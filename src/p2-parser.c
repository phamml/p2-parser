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
 * NODE-LEVEL PARSING FUNCTIONS
 */

// Prototype required because of mutual recursion
ASTNode* parse_loc(TokenQueue* input);
ASTNode* parse_expr(TokenQueue* input);
ASTNode* parse_block(TokenQueue* input);

/**
 * @brief Parse and return a Decaf type
 * 
 * @param input Token queue to modify
 * @returns Parsed type (it is also removed from the queue)
 */
DecafType parse_type (TokenQueue* input)
{
    if (TokenQueue_is_empty(input)) {
        Error_throw_printf("Unexpected end of input (expected int, bool, or void)\n");
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
    if (TokenQueue_is_empty(input)) {
        Error_throw_printf("Unexpected end of input (expected id token)\n");
    }

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
        Error_throw_printf("Unexpected end of input (expected type)\n");
    }
    
    int line = get_next_token_line(input);
    DecafType type = parse_type(input);

    char* buffer = malloc (MAX_TOKEN_LEN);
    parse_id(input, buffer);

    ASTNode* n = NULL;
    // if next token is symbol -> VarDecl is an array assignment
    if (check_next_token(input, SYM, "[")) {
        match_and_discard_next_token(input, SYM, "[");
        Token* token = TokenQueue_remove(input);
        // convert dec string to int
        int length = (int) strtol(token->text, NULL, 10);
        n = VarDeclNode_new(buffer, type, true, length, line);
        match_and_discard_next_token(input, SYM, "]");
        Token_free(token);
    } else {
        n = VarDeclNode_new(buffer, type, false, 1, line);
    }
    match_and_discard_next_token(input, SYM, ";");
    free(buffer);
    return n;
}

ParameterList* parse_params(TokenQueue* input) {
    // check if empty
    if (TokenQueue_is_empty(input)) {
        Error_throw_printf("Unexpected end of input (expected type)\n");
    }

    // parse first param
    ParameterList* params = ParameterList_new();
    DecafType type = parse_type(input);
    char* buffer = malloc (MAX_TOKEN_LEN);
    parse_id(input, buffer);
    ParameterList_add_new(params, buffer, type);

    // if next token is "," then more params present -> keep parsing
    // and adding to param list until ")" is seen
    if (check_next_token(input, SYM, ",")) {
        while (!check_next_token(input, SYM, ")")) {
            match_and_discard_next_token(input, SYM, ",");
            type = parse_type(input);
            buffer = malloc (MAX_TOKEN_LEN);
            parse_id(input, buffer);
            ParameterList_add_new(params, buffer, type);
        }
    }

    free(buffer);

    return params;
}

ASTNode* parse_lit(TokenQueue* input) {
    if (TokenQueue_is_empty(input)) {
        Error_throw_printf("Unexpected end of input (expected DEC, HEX, STR, false, or true)\n");
    }

    // get line number of literal
    int line = get_next_token_line(input);
    Token* token = NULL;

    // check if next token type is DECLIT, HEXLIT, KEY, or STRLIT 
    // and return corresponding node
    ASTNode* n = NULL;
    if (check_next_token_type(input, DECLIT)) {
        token = TokenQueue_remove(input);
        // convert dec string to int
        int i = (int) strtol(token->text, NULL, 10); 
        n = LiteralNode_new_int(i, line);
    
    } else if (check_next_token_type(input, HEXLIT)) {
        token = TokenQueue_remove(input);
        // convert hex string to int
        int hex = (int) strtol(token->text, NULL, 16); 
        n = LiteralNode_new_int(hex, line);

    } else if (check_next_token_type(input, KEY)) {
        if (check_next_token(input, KEY, "true")) {
            n = LiteralNode_new_bool(true, line);
            TokenQueue_remove(input);
        } else {
            n = LiteralNode_new_bool(false, line);
            TokenQueue_remove(input);
        }

    } else {
        token = TokenQueue_remove(input);
        // remove surrounding quotes from string lit
        char *p = token->text + 1;
        p[strlen(p)-1] = 0;

        // fix escape character present in string
        char* p2 = NULL;
        char* escp = NULL;
        if ((p2 = strstr( p, "\\n"))) {
            escp = "\n";
        } else if ((p2 = strstr( p, "\\t"))) {
            escp = "\t";
        } else if ((p2 = strstr( p, "\\\\"))) {
            escp = "\\";
        } else if ((p2 = strstr( p, "\\\""))) {
            escp = "\"";
        } else {
            return LiteralNode_new_string(p, line);
        }

        p[p2 - p] = '\0';
        p2 = p2 + 2;
        strncat(p, escp, 3);
        strncat(p, p2, strlen(p2) + 1);
        n = LiteralNode_new_string(p, line);
    }
    Token_free(token);
    return n;
}

NodeList* parse_args(TokenQueue* input) {
    if (TokenQueue_is_empty(input)) {
        Error_throw_printf("Unexpected end of input (expected expr)\n");
    }

    // get line number of args
    NodeList* args = NodeList_new();

    ASTNode* expr = parse_expr(input);
    NodeList_add(args, expr);

    // if next token is "," then more args present -> keep parsing
    // and adding to args list until ")" is seen
    if (check_next_token(input, SYM, ",")) {
        while (!check_next_token(input, SYM, ")")) {
            match_and_discard_next_token(input, SYM, ",");
            expr = parse_expr(input);
            NodeList_add(args, expr);
        }
    }
    return args;
}

ASTNode* parse_funccall(TokenQueue* input) {
    if (TokenQueue_is_empty(input)) {
        Error_throw_printf("Unexpected end of input (expected ID)\n");
    }

    // get line number of func call
    int line = get_next_token_line(input);

    // parse func call id
    char* buffer = malloc (MAX_TOKEN_LEN);
    parse_id(input, buffer);
    
    // get args of func call
    NodeList* args  = NodeList_new();
    match_and_discard_next_token(input, SYM, "(");
    if (!check_next_token(input, SYM, ")")) {
        args = parse_args(input);
    } 

    match_and_discard_next_token(input, SYM, ")");
    ASTNode* n = FuncCallNode_new(buffer, args, line);

    free(buffer);
    return n;
}

ASTNode* parse_base_expr(TokenQueue* input) {
    if (TokenQueue_is_empty(input)) {
        Error_throw_printf("Unexpected end of input (expected another expression, location, function call, or literal");
    }

    // get line number of base expression
    int line = get_next_token_line(input);
    ASTNode* n = NULL;

    // if next token is "(" then there is another expr and should parse expr
    if (check_next_token(input, SYM, "(")) {
        match_and_discard_next_token(input, SYM, "(");
        n = parse_expr(input);
        match_and_discard_next_token(input, SYM, ")");

    // if next token is keyword def then parse next token as FuncCall
    } else if (check_next_token_type(input, ID) && token_str_eq(input->head->next->text, "(")) {
        n = parse_funccall(input);
    
    // if next token is ID then parse next token as location   
    } else if (check_next_token_type(input, ID)) {
        n = parse_loc(input);

    // if next token is literal then parse next token as Lit
    } else if (check_next_token_type(input, DECLIT) || check_next_token_type(input, HEXLIT)
                || check_next_token_type(input, STRLIT) || check_next_token(input, KEY, "true")
                || check_next_token(input, KEY, "false")){
        n = parse_lit(input);
    
    // If token doesn't match any statements above -> invalid base expr throw error
    } else {
        Token* t = TokenQueue_remove(input);
        Error_throw_printf("Invalid base expression \'%s\' on line %d\n", t->text, line);
        Token_free(t);
    }
    return n;
}

ASTNode* parse_neg(TokenQueue* input) {
    if (TokenQueue_is_empty(input)) {
        Error_throw_printf("Unexpected end of input (expected unary operator: - or !)\n");
    }

    // get line number of negate expression
    int line = get_next_token_line(input);
    ASTNode* root = NULL;

    if (check_next_token(input, SYM, "-") || check_next_token(input, SYM, "!")) {
        ASTNode* new_root = NULL;
        ASTNode* child = NULL;

        if (check_next_token(input, SYM, "-")) { 
            match_and_discard_next_token(input, SYM, "-");
            child = parse_base_expr(input);
            new_root = UnaryOpNode_new(NEGOP, child, line);
            root = new_root;
        } else {
            match_and_discard_next_token(input, SYM, "!");
            child = parse_base_expr(input);
            new_root = UnaryOpNode_new(NOTOP, child, line);
            root = new_root;
        }
    } else {
        root = parse_base_expr(input);
    }
    return root;
}

ASTNode* parse_mult(TokenQueue* input) {
    if (TokenQueue_is_empty(input)) {
        Error_throw_printf("Unexpected end of input\n");
    }

    // get line number of multiplicative expression
    int line = get_next_token_line(input);
    ASTNode* root = parse_neg(input);

    while (check_next_token(input, SYM, "*") || check_next_token(input, SYM, "/")
            || check_next_token(input, SYM, "%")) {
        ASTNode* new_root = NULL;
        ASTNode* right = NULL;
        if (check_next_token(input, SYM, "*")) {
            match_and_discard_next_token(input, SYM, "*");
            right = parse_neg(input);
            new_root = BinaryOpNode_new(MULOP, root, right, line);
            root = new_root;
        } else if (check_next_token(input, SYM, "/")) {
            match_and_discard_next_token(input, SYM, "/");
            right = parse_neg(input);
            new_root = BinaryOpNode_new(DIVOP, root, right, line);
            root = new_root;
        } else {
            match_and_discard_next_token(input, SYM, "%");
            right = parse_neg(input);
            new_root = BinaryOpNode_new(MODOP, root, right, line);
            root = new_root;
        }
    }

    return root; 
}

ASTNode* parse_arith(TokenQueue* input) {
    if (TokenQueue_is_empty(input)) {
        Error_throw_printf("Unexpected end of input\n");
    }

    // get line number of arithmetic expression
    int line = get_next_token_line(input);
    ASTNode* root = parse_mult(input);

    while (check_next_token(input, SYM, "+") || check_next_token(input, SYM, "-")) {
        ASTNode* new_root = NULL;
        ASTNode* right = NULL;
        if (check_next_token(input, SYM, "+")) {
            match_and_discard_next_token(input, SYM, "+");
            right = parse_mult(input);
            new_root = BinaryOpNode_new(ADDOP, root, right, line);
            root = new_root;
        } else {
            match_and_discard_next_token(input, SYM, "-");
            right = parse_mult(input);
            new_root = BinaryOpNode_new(SUBOP, root, right, line);
            root = new_root;
        }
    }

    return root;
}

ASTNode* parse_relational(TokenQueue* input) {
    if (TokenQueue_is_empty(input)) {
        Error_throw_printf("Unexpected end of input\n");
    }

    // get line number of relational expression
    int line = get_next_token_line(input);
    ASTNode* root = parse_arith(input);

    while (check_next_token(input, SYM, "<") || check_next_token(input, SYM, "<=")
            || check_next_token(input, SYM, ">") || check_next_token(input, SYM, ">=")) {
        ASTNode* new_root = NULL;
        ASTNode* right = NULL;
        if (check_next_token(input, SYM, "<")) {
            match_and_discard_next_token(input, SYM, "<");
            right = parse_arith(input);
            new_root = BinaryOpNode_new(LTOP, root, right, line);
            root = new_root;
        } else if (check_next_token(input, SYM, "<=")){
            match_and_discard_next_token(input, SYM, "<=");
            right = parse_arith(input);
            new_root = BinaryOpNode_new(LEOP, root, right, line);
            root = new_root;
        } else if (check_next_token(input, SYM, ">")){
            match_and_discard_next_token(input, SYM, ">");
            right = parse_arith(input);
            new_root = BinaryOpNode_new(GTOP, root, right, line);
            root = new_root;
        } else {
            match_and_discard_next_token(input, SYM, ">=");
            right = parse_arith(input);
            new_root = BinaryOpNode_new(GEOP, root, right, line);
            root = new_root;
        }
    }
    return root;
}

ASTNode* parse_equality(TokenQueue* input) {
    if (TokenQueue_is_empty(input)) {
        Error_throw_printf("Unexpected end of input\n");
    }

    // get line number of equality expression
    int line = get_next_token_line(input);
    ASTNode* root = parse_relational(input);

    while (check_next_token(input, SYM, "==") || check_next_token(input, SYM, "!=")) {
        ASTNode* new_root = NULL;
        ASTNode* right = NULL;
        if (check_next_token(input, SYM, "==")) {
            match_and_discard_next_token(input, SYM, "==");
            right = parse_relational(input);
            new_root = BinaryOpNode_new(EQOP, root, right, line);
            root = new_root;
        } else {
            match_and_discard_next_token(input, SYM, "!=");
            right = parse_relational(input);
            new_root = BinaryOpNode_new(NEQOP, root, right, line);
            root = new_root;
        }
    }
    return root;
}

ASTNode* parse_and(TokenQueue* input) {
    if (TokenQueue_is_empty(input)) {
        Error_throw_printf("Unexpected end of input\n");
    }

    // get line number of logical OR expression
    int line = get_next_token_line(input);
    ASTNode* root = parse_equality(input);

    while (check_next_token(input, SYM, "&&")) {
        ASTNode* new_root = NULL;
        ASTNode* right = NULL;
        if (check_next_token(input, SYM, "&&")) {
            match_and_discard_next_token(input, SYM, "&&");
            right = parse_equality(input);
            new_root = BinaryOpNode_new(ANDOP, root, right, line);
            root = new_root;
        }
    }
    return root;
}

ASTNode* parse_or(TokenQueue* input) {
    if (TokenQueue_is_empty(input)) {
        Error_throw_printf("Unexpected end of input\n");
    }

    // get line number of logical OR expression
    int line = get_next_token_line(input);
    ASTNode* root = parse_and(input);

    while (check_next_token(input, SYM, "||")) {
        ASTNode* new_root = NULL;
        ASTNode* right = NULL;
        if (check_next_token(input, SYM, "||")) {
            match_and_discard_next_token(input, SYM, "||");
            right = parse_and(input);
            new_root = BinaryOpNode_new(OROP, root, right, line);
            root = new_root;
        }
    }
    return root;
}

ASTNode* parse_expr(TokenQueue* input) {
    if (TokenQueue_is_empty(input)) {
        Error_throw_printf("Unexpected end of input\n");
    }
    ASTNode* n = parse_or(input);
    return n;
}

ASTNode* parse_conditional(TokenQueue* input) {

    if (TokenQueue_is_empty(input)) {
        Error_throw_printf("Conditional expected but not found at line 1\n");
    }

    int line = get_next_token_line(input);

    // grammar: if ‘(’ Expr ‘)’ Block (else Block)?
 
    // call parse_expr
    // call parse_block (possibly twice if there's an else block)
    // n = ConditionalNode_new
    ASTNode* n = NULL;
    ASTNode* condition = NULL;
    ASTNode* if_block = NULL;
    ASTNode* else_block = NULL;

    Token_free(TokenQueue_remove(input));

    match_and_discard_next_token(input, SYM, "(");

    condition = parse_expr(input);

    match_and_discard_next_token(input, SYM, ")");
 

    if_block = parse_block(input);

    if (check_next_token(input, KEY, "else")) {

        match_and_discard_next_token(input, KEY, "else");

        else_block = parse_expr(input);


    }

    n = ConditionalNode_new(condition, if_block, else_block, line);

    return n;
}

ASTNode* parse_loc(TokenQueue* input) {
    if (TokenQueue_is_empty(input)) {
        Error_throw_printf("Location expected but not found at line 1\n");
    }

    // get line number of location
    int line = get_next_token_line(input);

    // get name of location
    char* buffer = malloc (MAX_TOKEN_LEN);
    parse_id(input, buffer);

    ASTNode* n = NULL;
    // if next token is SYM -> Loc is an array assignment
    if (check_next_token(input, SYM, "[")) {
        match_and_discard_next_token(input, SYM, "[");
        ASTNode* index = parse_expr(input);
        n = LocationNode_new(buffer, index, line);
        match_and_discard_next_token(input, SYM, "]");
    } else {
        n = LocationNode_new(buffer, NULL, line);
    }

    free(buffer);
    return n;
}

ASTNode* parse_while(TokenQueue* input) {
    
    if (TokenQueue_is_empty(input)) {
        Error_throw_printf("While loop expected but not found at line 1\n");
    }

    // grammar: while ‘(’ Expr ‘)’ Block
    // call parse_expr
    // call parse_block
    // n = WhileLoopNode_new

    int line = get_next_token_line(input);
    
    ASTNode* n = NULL;
    ASTNode* expr = NULL;
    ASTNode* block = NULL;

    Token_free(TokenQueue_remove(input));

    match_and_discard_next_token(input, SYM, "(");
    expr = parse_expr(input);

    match_and_discard_next_token(input, SYM, ")");
    block = parse_block(input);
    n = WhileLoopNode_new(expr, block, line);

    return n;
}

ASTNode* parse_statement(TokenQueue* input) {
    if (TokenQueue_is_empty(input)) {
        Error_throw_printf("Unexpected end of input\n");
    }

    // get line number of statement
    int line = get_next_token_line(input);

    ASTNode* n = NULL;
    // check what kind of statement and return appropriate statement node
    if (check_next_token(input, KEY, "break")) {
       Token_free(TokenQueue_remove(input));
        n = BreakNode_new(line);
        match_and_discard_next_token(input, SYM, ";");

    } else if (check_next_token(input, KEY, "continue")) {
       Token_free(TokenQueue_remove(input));
        n = ContinueNode_new(line);
        match_and_discard_next_token(input, SYM, ";");

    } else if (check_next_token(input, KEY, "return")) {
        // get val from calling parse_expr
        ASTNode* val = NULL;
        Token_free(TokenQueue_remove(input));
        // if ";" is not next token then parse return val
        if (!check_next_token(input, SYM, ";")) {
            val = parse_expr(input);
        }
        n = ReturnNode_new(val, line);
        match_and_discard_next_token(input, SYM, ";");

    } else if (check_next_token(input, KEY, "while")) {
       // TokenQueue_remove(input);
        n = parse_while(input);
       // n = WhileLoopNode_new()
    
    } else if (check_next_token(input, KEY, "if")) {
        
        n = parse_conditional(input);
       //  match_and_discard_next_token(input, SYM, "?");
    
    // if next token is keyword def then parse next token as FuncCall
    } else if (check_next_token_type(input, ID) && token_str_eq(input->head->next->text, "(")) {
        n = parse_funccall(input);
        match_and_discard_next_token(input, SYM, ";");

    // parse location
    } else if (check_next_token_type(input, ID)) {
        ASTNode* loc = parse_loc(input);
        match_and_discard_next_token(input, SYM, "=");
        ASTNode* value = parse_expr(input);
        n = AssignmentNode_new(loc, value, line);
        match_and_discard_next_token(input, SYM, ";");
    
    // If token does not match any statements above -> invalid statement throw error
    } else {
        Error_throw_printf("Invalid statement on line %d\n", line);
    }
    return n;
}

ASTNode* parse_block(TokenQueue* input) {
    // check if empty
    if (TokenQueue_is_empty(input)) {
        Error_throw_printf("Unexpected end of input (expected '{')\n");
    }

    // get line number of block
    int line = get_next_token_line(input);

    match_and_discard_next_token(input, SYM, "{");
    NodeList* vars = NodeList_new();
    NodeList* stmnts = NodeList_new();
    ASTNode* var = NULL;
    ASTNode* stmnt = NULL;

    // if next toke is "}" then func body is empty just return
    if (check_next_token(input, SYM, "}")) {
        match_and_discard_next_token(input, SYM, "}");
        return BlockNode_new(vars, stmnts, line);
    } else {
        // parse func body until another "}" is seen
        while (!check_next_token(input, SYM, "}")) {
            // if line of body starts with Type then parse VarDecl
            if (check_next_token(input, KEY, "int") || check_next_token(input, KEY, "bool")
                            || check_next_token(input, KEY, "void") ) {
                var = parse_vardecl(input);
                NodeList_add(vars, var);
            // if not parse statements
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
        Error_throw_printf("Unexpected end of input (expected 'def')\n");
    }

    // get line number of func decl
    int line = get_next_token_line(input);

    // get return type of func decl
    match_and_discard_next_token(input, KEY, "def");
    DecafType type = parse_type(input);
    // printf("%d\n", type);

    // get func name
    char* buffer = malloc (MAX_TOKEN_LEN);
    parse_id(input, buffer);

    // get params and body of func and create new funcdecl node
    ParameterList* params = ParameterList_new();
    match_and_discard_next_token(input, SYM, "(");
    if (!check_next_token(input, SYM, ")")) {
        params = parse_params(input);
    }

    match_and_discard_next_token(input, SYM, ")");
    ASTNode* body = parse_block(input);
    ASTNode* n = FuncDeclNode_new(buffer, type, params, body, line);

    free(buffer);
    return n;
}

// Parses the program non terminal
ASTNode* parse_program (TokenQueue* input)
{
    NodeList* vars = NodeList_new();
    NodeList* funcs = NodeList_new();

    while (!TokenQueue_is_empty(input)) {
        // checks next token to determine whether to parse VarDecl or FuncDecl
        ASTNode* n = NULL;
        if (check_next_token(input, KEY, "def")) {
            n = parse_funcdecl(input);
            NodeList_add(funcs, n);
        } else {
            n = parse_vardecl(input);
            NodeList_add(vars, n);
        }
    }
    return ProgramNode_new(vars, funcs);
}

ASTNode* parse (TokenQueue* input)
{
    if (input == NULL) {
        Error_throw_printf("TokenQueue is NULL there are no tokens to parse\n");
    }
    return parse_program(input);
}