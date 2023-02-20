/**
 * @file p2-parser.c
 * @brief Compiler phase 2: parser
 */

#include "p2-parser.h"

/*
 * helper functions
 */

/**
 * @brief Look up the source line of the next token in the queue.
 * 
 * @param input Token queue to examine
 * @returns Source line
 */
int get_next_token_line (TokenQueue* input)
{
    if (TokenQueue_is_empty(input)) {
        Error_throw_printf("Unexpected end of input\n");
    }
    return TokenQueue_peek(input)->line;
}

/**
 * @brief Check next token for a particular type and text and discard it
 * 
 * Throws an error if there are no more tokens or if the next token in the
 * queue does not match the given type or text.
 * 
 * @param input Token queue to modify
 * @param type Expected type of next token
 * @param text Expected text of next token
 */
void match_and_discard_next_token (TokenQueue* input, TokenType type, const char* text)
{
    if (TokenQueue_is_empty(input)) {
        Error_throw_printf("Unexpected end of input (expected \'%s\')\n", text);
    }
    Token* token = TokenQueue_remove(input);
    if (token->type != type || !token_str_eq(token->text, text)) {
        Error_throw_printf("Expected \'%s\' but found '%s' on line %d\n",
                text, token->text, get_next_token_line(input));
    }
    Token_free(token);
}

/**
 * @brief Remove next token from the queue
 * 
 * Throws an error if there are no more tokens.
 * 
 * @param input Token queue to modify
 */
void discard_next_token (TokenQueue* input)
{
    if (TokenQueue_is_empty(input)) {
        Error_throw_printf("Unexpected end of input\n");
    }
    Token_free(TokenQueue_remove(input));
}

/**
 * @brief Look ahead at the type of the next token
 * 
 * @param input Token queue to examine
 * @param type Expected type of next token
 * @returns True if the next token is of the expected type, false if not
 */
bool check_next_token_type (TokenQueue* input, TokenType type)
{
    if (TokenQueue_is_empty(input)) {
        return false;
    }
    Token* token = TokenQueue_peek(input);
    return (token->type == type);
}

/**
 * @brief Look ahead at the type and text of the next token
 * 
 * @param input Token queue to examine
 * @param type Expected type of next token
 * @param text Expected text of next token
 * @returns True if the next token is of the expected type and text, false if not
 */
bool check_next_token (TokenQueue* input, TokenType type, const char* text)
{
    if (TokenQueue_is_empty(input)) {
        return false;
    }
    Token* token = TokenQueue_peek(input);
    return (token->type == type) && (token_str_eq(token->text, text));
}

/*
 * node-level parsing functions
 */

/**
 * @brief Parse and return a Decaf type
 * 
 * @param input Token queue to modify
 * @returns Parsed type (it is also removed from the queue)
 */
DecafType parse_type (TokenQueue* input)
{
    if (TokenQueue_is_empty(input)) {
        Error_throw_printf("Variable declaration expected but not found at line 1\n");
    }

    Token* token = TokenQueue_remove(input);
    if (token->type != KEY) {
        Error_throw_printf("Invalid type '%s' on line %d\n", token->text, get_next_token_line(input));
    }
    DecafType t = VOID;
    if (token_str_eq("int", token->text)) {
        t = INT;
    } else if (token_str_eq("bool", token->text)) {
        t = BOOL;
    } else if (token_str_eq("void", token->text)) {
        t = VOID;
    } else {
        Error_throw_printf("Invalid type '%s' on line %d\n", token->text, get_next_token_line(input));
    }
    Token_free(token);
    return t;
}

/**
 * @brief Parse and return a Decaf identifier
 * 
 * @param input Token queue to modify
 * @param buffer String buffer for parsed identifier (should be at least
 * @c MAX_TOKEN_LEN characters long)
 */
void parse_id (TokenQueue* input, char* buffer)
{
    Token* token = TokenQueue_remove(input);
    if (token->type != ID) {
        Error_throw_printf("Invalid ID '%s' on line %d\n", token->text, get_next_token_line(input));
    }
    snprintf(buffer, MAX_ID_LEN, "%s", token->text);
    Token_free(token);
}

ASTNode* parse_vardecl(TokenQueue* input)
{
    // check if empty
    if (TokenQueue_is_empty(input)) {
        Error_throw_printf("Variable declaration expected but not found at line 1\n");
    }
    
    int line = get_next_token_line(input);
    DecafType type = parse_type(input);

    char* buffer = malloc (MAX_TOKEN_LEN);
    parse_id(input, buffer);
    match_and_discard_next_token(input, SYM, ";");

    ASTNode* n = VarDeclNode_new(buffer, type, false, 0, line);
    return n;
}

ParameterList* parse_params(TokenQueue* input) {
    // check if empty
    if (TokenQueue_is_empty(input)) {
        Error_throw_printf("Parameter expected but not found at line 1\n");
    }

    match_and_discard_next_token(input, SYM, "(");
    ParameterList* params = ParameterList_new();

    // if next token is ")" then param list is empty just return
    if (token_str_eq(TokenQueue_peek(input)->text, ")")) {
        match_and_discard_next_token(input, SYM, ")");
        return params;
    } else {
        // parse first param
        DecafType type = parse_type(input);
        char* buffer = malloc (MAX_TOKEN_LEN);
        parse_id(input, buffer);
        ParameterList_add_new(params, buffer, type);

        // if next token is "," then more params present -> keep parsing
        // and adding to param list until ")" is seen
        if (!token_str_eq(TokenQueue_peek(input)->text, ",")) {
            while (!token_str_eq(TokenQueue_peek(input)->text, ")")) {
                match_and_discard_next_token(input, SYM, ",");
                type = parse_type(input);
                buffer = malloc (MAX_TOKEN_LEN);
                parse_id(input, buffer);
                ParameterList_add_new(params, buffer, type);
            }
        }
    }
    match_and_discard_next_token(input, SYM, ")");
    return params;
}

ASTNode* parse_statement(TokenQueue* input) {
    if (TokenQueue_is_empty(input)) {
        Error_throw_printf("Statement expected but not found at line 1\n");
    }

    // get line number of statement
    int line = get_next_token_line(input);

    ASTNode* n = NULL;
    // check what kind of statement and return appropriate statement node
    if (token_str_eq(TokenQueue_peek(input)->text, "break")) {
        TokenQueue_remove(input);
        n = BreakNode_new(line);
    } else if (token_str_eq(TokenQueue_peek(input)->text, "continue")) {
        TokenQueue_remove(input);
        n = ContinueNode_new(line);
    } else if (token_str_eq(TokenQueue_peek(input)->text, "return")) {
        // get val from calling parse_expr
        ASTNode* val = NULL;
        TokenQueue_remove(input);
        n = ReturnNode_new(val, line);
    }
    match_and_discard_next_token(input, SYM, ";");
    return n;
}

ASTNode* parse_block(TokenQueue* input) {
    // check if empty
    if (TokenQueue_is_empty(input)) {
        Error_throw_printf("Block expected but not found at line 1\n");
    }

    // get line number of block
    int line = get_next_token_line(input);

    match_and_discard_next_token(input, SYM, "{");
    NodeList* vars = NodeList_new();
    NodeList* stmnts = NodeList_new();
    ASTNode* var = NULL;
    ASTNode* stmnt = NULL;

    // if next toke is "}" then func body is empty just return
    if (token_str_eq(TokenQueue_peek(input)->text, "}")) {
        match_and_discard_next_token(input, SYM, "}");
        return BlockNode_new(vars, stmnts, line);
    } else {
        // parse func body until another "}" is seen
        while (!token_str_eq(TokenQueue_peek(input)->text, "}")) {
            // if line of body starts with Type then parse VarDecl
            if (token_str_eq(TokenQueue_peek(input)->text, "int")
                    || token_str_eq(TokenQueue_peek(input)->text, "bool")
                    || token_str_eq(TokenQueue_peek(input)->text, "void")) {
                var = parse_vardecl(input);
                NodeList_add(vars, var);
            // else parse statements
            } else {
                stmnt = parse_statement(input);
                NodeList_add(stmnts, stmnt);
            }
        }
    }

    match_and_discard_next_token(input, SYM, "}");
    ASTNode* n = BlockNode_new(vars, stmnts, line);
    return n;
}

// All functions will start with def keyword
ASTNode* parse_funcdecl(TokenQueue* input) {
    // check if empty
    if (TokenQueue_is_empty(input)) {
        Error_throw_printf("Function declaration expected but not found at line 1\n");
    }

    // get line number of func decl
    int line = get_next_token_line(input);

    // get return type of func decl
    match_and_discard_next_token(input, KEY, "def");
    DecafType type = parse_type(input);
    printf("%d\n", type);

    // get func name
    char* buffer = malloc (MAX_TOKEN_LEN);
    parse_id(input, buffer);
    printf("%s\n", buffer);

    // get params and body of func and create new funcdecl node
    ParameterList* params = parse_params(input);
    ASTNode* body = parse_block(input);
    ASTNode* n = FuncDeclNode_new(buffer, type, params, body, line);
    return n;
}

// Parses the program non terminal
ASTNode* parse_program (TokenQueue* input)
{
    NodeList* vars = NodeList_new();
    NodeList* funcs = NodeList_new();

    // Program -> VarDecl
    // VarDecl -> ID
    while (!TokenQueue_is_empty(input)) {
        // Peeks at first token in input to determine whether to parse
        // VarDecl or FuncDecl
        Token* token = TokenQueue_peek(input);
        ASTNode* n = NULL;
        if (token_str_eq(token->text, "def")) {
            n = parse_funcdecl(input);
            NodeList_add(funcs, n);
        } else {
            n = parse_vardecl(input);
            // we want to add this to vars since it is a vars node
            NodeList_add(vars, n);
        }
    }

    return ProgramNode_new(vars, funcs);
}

ASTNode* parse (TokenQueue* input)
{
    return parse_program(input);
}