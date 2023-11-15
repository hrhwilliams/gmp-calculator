#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include <gmp.h>

#include <readline/readline.h>
#include <readline/history.h>

#ifdef _DEBUG
#include <malloc/malloc.h>
#include <dlfcn.h>

size_t malloc_bytes = 0;
size_t free_bytes = 0;
void* malloc(size_t size)
{
    void *(*libc_malloc)(size_t) = dlsym(RTLD_NEXT, "malloc");
    malloc_bytes += size;
    return libc_malloc(size);
}

void free(void *p)
{
    void (*libc_free)(void*) = dlsym(RTLD_NEXT, "free");
    free_bytes += malloc_size(p);
    libc_free(p);
}
#endif /* _DEBUG */

/*
 * ------------- Error reporting ------------- 
 */

void __attribute__((noreturn)) error(const char *type, const char *format, ...) {
    va_list argp;
    va_start(argp, format);

    fprintf(stderr, "[\033[31;1m%s\033[0m] ", type);
    vfprintf(stderr, format, argp);
    fprintf(stderr, "\n");

    va_end(argp);

    exit(-1);
}

/*
 * ------------- Tokenizer stuff ------------- 
 */

typedef enum {
    END,
    INTEGER,
    PLUS,
    MINUS,
    STAR,
    FORWARD_SLASH,
    PERCENT,
    CARET,
    LPAREN,
    RPAREN
} TokenIdentifier;

const char *token_names[] = {
    [END] = "END",
    [INTEGER] = "INTEGER",
    [PLUS] = "PLUS",
    [MINUS] = "MINUS",
    [STAR] = "STAR",
    [FORWARD_SLASH] = "FORWARD_SLASH",
    [PERCENT] = "PERCENT",
    [CARET] = "CARET",
    [LPAREN] = "LPAREN",
    [RPAREN] = "RPAREN",
};

typedef struct _Token {
    TokenIdentifier type;
    unsigned int str_length;
    char *string;
} Token;

typedef struct _TokenListNode {
    struct _TokenListNode *next;
    Token token;
} TokenListNode;

typedef struct _TokenList {
    TokenListNode *head;
    TokenListNode *end;
    char *string;
    unsigned int length;
} TokenList;

void add_token(TokenList *list, char *string, TokenIdentifier type, unsigned int length) {
    TokenListNode *node;
    node = malloc(sizeof *node);
    node->next = NULL;
    node->token.type = type;
    node->token.str_length = length;
    node->token.string = string;

    if (list->length == 0) { /* first element */
        list->head = node;
        list->end = node;
    } else {
        list->end->next = node;
        list->end = node;
    }

    list->length++;
}

void tokenize_number(TokenList *list, char *string, unsigned int *index) {
    unsigned int length = 0;
    while (string[length] >= '0' && string[length] <= '9') {
        *index = *index + 1;
        length++;
    }

    add_token(list, string, INTEGER, length);
    // *index = *index + 1;
}

/**
 * @param string String to be tokenized
 * @param list Token linked list to be filled
 * @return length of token list
 */
unsigned int tokenize(char *string, TokenList *list) {
    unsigned int line = 0;
    unsigned int row = 0;
    unsigned int string_index = 0;
    unsigned int string_length = strlen(string);

    list->head = NULL;
    list->end = NULL;
    list->string = strdup(string);
    list->length = 0;

    string = list->string;

    while (string_index <= string_length) {
        switch (string[string_index]) {
        case '\n':
            line++;
            row = 0;
        case ' ':
        case '\t':
        case '\r':
        case '\0':
            string_index++;
            break;
        case '0' ... '9': tokenize_number(list, &string[string_index], &string_index); break;
        case '+': add_token(list, &string[string_index++], PLUS, 1);  break;
        case '-': add_token(list, &string[string_index++], MINUS, 1); break;
        case '*': add_token(list, &string[string_index++], STAR, 1);  break;
        case '/': add_token(list, &string[string_index++], FORWARD_SLASH, 1); break;
        case '%': add_token(list, &string[string_index++], PERCENT, 1); break;
        case '^': add_token(list, &string[string_index++], CARET, 1); break;
        case '(': add_token(list, &string[string_index++], LPAREN, 1); break;
        case ')': add_token(list, &string[string_index++], RPAREN, 1); break;
        default:
            error("TokenizerError", "Unexpected character '%c' in expression at line %u", string[string_index], line);
            break;
        }
    }

    add_token(list, NULL, END, 0);

    return list->length;
}

void print_tokens(TokenList *list) {
    TokenListNode *node = list->head;
    for (unsigned int i = 0; i < list->length; i++) {
        Token token = node->token;
        printf("(%s: %.*s), ", token_names[token.type], token.str_length, token.string);
        node = node->next;
    }
    printf("\n");
}

void free_nodes(TokenListNode *node) {
    if (node->next != NULL) {
        free_nodes(node->next);
    }

    free(node);
}

void free_token_list(TokenList *list) {
    free_nodes(list->head);
    free(list->string);
}

/*
 * ------------- Tokenizer stuff ------------- 
 */
/*
 * ------------- Parser stuff ------------- 
 */

#define MAX_LITERALS 128

typedef enum {
    OP_ADD,
    OP_SUB,
    OP_MUL,
    OP_DIV,
    OP_MOD,
    OP_POW,
    OP_PUSH_INTEGER,
} OpcodeIdentifier;

typedef enum {
    VAL_NONE,
    VAL_INTEGER,
    VAL_RATIONAL,
    VAL_STRING,
    VAL_FLOAT,
    VAL_BOOL,
} ValueType;

const char *opcode_names[] = {
    [OP_ADD] = "OP_ADD",
    [OP_SUB] = "OP_SUB",
    [OP_MUL] = "OP_MUL",
    [OP_DIV] = "OP_DIV",
    [OP_MOD] = "OP_MOD",
    [OP_POW] = "OP_POW",
    [OP_PUSH_INTEGER] = "OP_PUSH_INTEGER",
};

typedef struct _Opcode {
    union {
        OpcodeIdentifier opcode;
        unsigned int literal;
    } as;
} Opcode;

typedef struct _Literal {
    union {
        mpz_t Integer;
        mpq_t Rational;
        char *String;
        double Float;
        bool Bool;
    } as;
    ValueType type;
} Literal;

typedef struct _OpcodeListNode {
    struct _OpcodeListNode *next;
    Opcode opcode;
} OpcodeListNode;

typedef struct _OpcodeList {
    Literal literals[MAX_LITERALS];
    OpcodeListNode *head;
    OpcodeListNode *end;
    unsigned int literals_top;
    unsigned int length;
} OpcodeList;


void expr(OpcodeList *opcodes, TokenListNode **current_node);
void term(OpcodeList *opcodes, TokenListNode **current_node);
void factor(OpcodeList *opcodes, TokenListNode **current_node);
void atom(OpcodeList *opcodes, TokenListNode **current_node);

bool peek_add_op(TokenListNode *current_node) {
    if (current_node == NULL) {
        return false;
    }

    TokenListNode *next_node = current_node->next;
    if (next_node == NULL) {
        return false;
    }

    if (next_node->token.type == PLUS || next_node->token.type == MINUS) {
        return true;
    }

    return false;
}

bool peek_mul_op(TokenListNode *current_node) {
    if (current_node == NULL) {
        return false;
    }

    TokenListNode *next_node = current_node->next;
    if (next_node == NULL) {
        return false;
    }

    if (next_node->token.type == STAR || next_node->token.type == FORWARD_SLASH || next_node->token.type == PERCENT) {
        return true;
    }

    return false;
}

bool peek_pow_op(TokenListNode *current_node) {
    if (current_node == NULL) {
        return false;
    }

    TokenListNode *next_node = current_node->next;
    if (next_node == NULL) {
        return false;
    }

    if (next_node->token.type == CARET) {
        return true;
    }

    return false;
}

void expect(TokenListNode *current_node, TokenIdentifier token) {
    if (current_node == NULL || current_node->token.type != token) {
        error("ParserError", "Expected %s: got %s", token_names[token], token_names[current_node->token.type]);
    }
}

bool is_number(TokenListNode *current_node) {
    return current_node->token.type == INTEGER;
}

bool is_open_paren(TokenListNode *current_node) {
    return current_node->token.type == LPAREN;
}

void push_opcode(OpcodeList *list, OpcodeIdentifier opcode) {
    OpcodeListNode *node;
    node = malloc(sizeof *node);
    node->next = NULL;
    node->opcode.as.opcode = opcode;

    if (list->length == 0) { /* first element */
        list->head = node;
        list->end = node;
    } else {
        list->end->next = node;
        list->end = node;
    }

    list->length++;
}

void push_literal_int_opcode(OpcodeList *list, char *num_string) {
    OpcodeListNode *node;
    node = malloc(sizeof *node);
    node->next = NULL;
    node->opcode.as.literal = list->literals_top;

    mpz_init(list->literals[list->literals_top].as.Integer);
    mpz_set_str(list->literals[list->literals_top].as.Integer, num_string, 10);
    list->literals_top++;

    if (list->length == 0) { /* first element */
        list->head = node;
        list->end = node;
    } else {
        list->end->next = node;
        list->end = node;
    }

    list->length++;
}

void push_opcode_from_token(OpcodeList *list, Token token) {
    if (token.type == INTEGER) {
        char *number = strndup(token.string, token.str_length);
        push_literal_int_opcode(list, number);
        free(number);
    } else {
        OpcodeListNode *node;
        node = malloc(sizeof *node);
        node->next = NULL;

        switch (token.type) {
            case PLUS:  node->opcode.as.opcode = OP_ADD; break;
            case MINUS: node->opcode.as.opcode = OP_SUB; break;
            case STAR:  node->opcode.as.opcode = OP_MUL; break;
            case FORWARD_SLASH: node->opcode.as.opcode = OP_DIV; break;
            case PERCENT: node->opcode.as.opcode = OP_MOD; break;
            case CARET: node->opcode.as.opcode = OP_POW; break;
            default: /* TODO signal an error */ break;
        }

        if (list->length == 0) { /* first element */
            list->head = node;
            list->end = node;
        } else {
            list->end->next = node;
            list->end = node;
        }

        list->length++;
    }
}

/* expr   := term {addOp expr}
 * term   := factor {mulOp factor}
 * factor := atom {powOp atom}
 * atom   := number | "(" expr ")"
 */

#define ADVANCE(node) *node = (*node)->next

void expr(OpcodeList *opcodes, TokenListNode **current_node) {
    term(opcodes, current_node);

    if (peek_add_op(*current_node)) {
        /* get op */
        ADVANCE(current_node);
        Token op = (*current_node)->token;

        /* parse next term */
        ADVANCE(current_node);
        expr(opcodes, current_node);

        /* push op */
        push_opcode_from_token(opcodes, op);
    }
}

void term(OpcodeList *opcodes, TokenListNode **current_node) {
    factor(opcodes, current_node);

    if (peek_mul_op(*current_node)) {
        /* get op */
        ADVANCE(current_node);
        Token op = (*current_node)->token;

        /* parse next factor */
        ADVANCE(current_node);
        term(opcodes, current_node);

        /* push op */
        push_opcode_from_token(opcodes, op);
    }
}

void factor(OpcodeList *opcodes, TokenListNode **current_node) {
    atom(opcodes, current_node);

    if (peek_pow_op(*current_node)) {
        /* get op */
        ADVANCE(current_node);
        Token op = (*current_node)->token;

        /* parse next atom */
        ADVANCE(current_node);
        factor(opcodes, current_node);

        /* push op */
        push_opcode_from_token(opcodes, op);
    }
}

void atom(OpcodeList *opcodes, TokenListNode **current_node) {
    if (is_number(*current_node)) {
        /* push number */
        Token op = (*current_node)->token;
        push_opcode(opcodes, OP_PUSH_INTEGER);
        push_opcode_from_token(opcodes, op);
    } else if (is_open_paren(*current_node)) {
        /* parse next expr */
        ADVANCE(current_node);
        expr(opcodes, current_node);

        ADVANCE(current_node);
        expect(*current_node, RPAREN);
    } // else if minus then parse unary minus

    // ADVANCE(current_node);
}

unsigned int parse(TokenList *list, OpcodeList *opcodes) {
    opcodes->head = NULL;
    opcodes->end = NULL;
    opcodes->length = 0;
    opcodes->literals_top = 0;
    memset(opcodes->literals, 0, MAX_LITERALS * sizeof *opcodes->literals);

    TokenListNode *node = list->head;
    expr(opcodes, &node);

    if (node->next != list->end) {
        error("ParserError", "Junk at end of expression");
    }

    return opcodes->length;
}

void print_opcodes(OpcodeList *list) {
    OpcodeListNode *node = list->head;
    bool int_next = false;
    for (unsigned int i = 0; i < list->length; i++) {
        Opcode op = node->opcode;
        if (int_next) {
            gmp_printf("%Zd", list->literals[op.as.literal].as.Integer);
        } else {
            printf("%s", opcode_names[op.as.opcode]);
        }

        if (op.as.opcode == OP_PUSH_INTEGER) {
            int_next = true;
            printf(" ");
        } else {
            int_next = false;
            printf(", ");
        }

        node = node->next;
    }

    printf("\n");
}

void free_opcode_nodes(OpcodeListNode *node) {
    if (node->next != NULL) {
        free_opcode_nodes(node->next);
    }

    free(node);
}

void free_opcode_list(OpcodeList *list) {
    free_opcode_nodes(list->head);

    for (int i = 0; i < list->literals_top; i++) {
        if (list->literals[i].type == VAL_INTEGER) {
            mpz_clear(list->literals[i].as.Integer);
        }
    }
}
/*
 * ------------- Parser stuff ------------- 
 */

/*
 * ------------- Interpreter stuff ------------- 
 */

#define MAX_STACK 64
#define GET_INT(pos) (stack[pos].as.literal.as.Integer)

#define PUSH_INTEGER(mpz) do {                                          \
    stack[stack_top].as.literal.as.Integer->_mp_alloc = mpz->_mp_alloc; \
    stack[stack_top].as.literal.as.Integer->_mp_size = mpz->_mp_size;   \
    stack[stack_top].as.literal.as.Integer->_mp_d = mpz->_mp_d;         \
    stack_top++;                                                        \
} while (0)

typedef struct _StackValue {
    union {
        Literal literal;
    } as;
} StackValue;

StackValue stack[MAX_STACK];
unsigned int stack_top = 0;

void add_numbers() {
    mpz_add(GET_INT(stack_top - 2), GET_INT(stack_top - 2), GET_INT(stack_top - 1));
    stack_top -= 1;
}

void sub_numbers() {
    mpz_sub(GET_INT(stack_top - 2), GET_INT(stack_top - 2), GET_INT(stack_top - 1));
    stack_top -= 1;
}

void mul_numbers() {
    mpz_mul(GET_INT(stack_top - 2), GET_INT(stack_top - 2), GET_INT(stack_top - 1));
    stack_top -= 1;
}

void div_numbers() {
    mpz_div(GET_INT(stack_top - 2), GET_INT(stack_top - 2), GET_INT(stack_top - 1));
    stack_top -= 1;
}

void mod_numbers() {
    mpz_mod(GET_INT(stack_top - 2), GET_INT(stack_top - 2), GET_INT(stack_top - 1));
    stack_top -= 1;
}

void pow_numbers() {
    const unsigned long pow = mpz_get_ui(GET_INT(stack_top - 1));
    mpz_pow_ui(GET_INT(stack_top - 2), GET_INT(stack_top - 2), pow);
    stack_top -= 1;
}

void interpret(OpcodeList *list) {
    Opcode code[list->length];

    /* compact code array */
    OpcodeListNode *node = list->head;
    for (int i = 0; i < list->length; i++) {
        code[i] = node->opcode;
        node = node->next;
    }

    for (unsigned int pc = 0; pc < list->length; pc++) {
        switch (code[pc].as.opcode) {
        case OP_PUSH_INTEGER: PUSH_INTEGER(list->literals[code[pc + 1].as.literal].as.Integer); pc++; break;
        case OP_ADD: add_numbers(); break;
        case OP_SUB: sub_numbers(); break;
        case OP_MUL: mul_numbers(); break;
        case OP_MOD: mod_numbers(); break;
        case OP_POW: pow_numbers(); break;
        // case OP_DIV: break;
        default: error("InterpreterError", "Unknown opcode '%d'", code[pc].as.opcode);
        }
    }

    if (stack_top == 1) {
        mpz_t x1;
        mpz_init(x1);

        mpz_set(x1, stack[--stack_top].as.literal.as.Integer);
        gmp_printf("%Zd\n", x1);
    }
}

int main(int argc, char *argv[]) {
    rl_bind_key('\t', rl_insert);

    char *buf;
    while ((buf = readline(">> ")) != NULL) {
        if (strlen(buf) > 0) {
            add_history(buf);
        }
        TokenList tokens;
        OpcodeList opcodes;

        unsigned int token_count = tokenize(buf, &tokens);
#ifdef _DEBUG
        print_tokens(&tokens);
#endif /* DEBUG */
        unsigned int opcode_count = parse(&tokens, &opcodes);
#ifdef _DEBUG
        printf("%d tokens read, %d opcodes generated\n", token_count, opcode_count);
        print_opcodes(&opcodes);
#endif /* _DEBUG */

        interpret(&opcodes);

        free_opcode_list(&opcodes);
        free_token_list(&tokens);


        // readline malloc's a new buffer every time.
        free(buf);
    }

    printf("\r\n");
#ifdef _DEBUG
    printf("Freed %zu bytes out of %zu bytes allocated\n", malloc_bytes, free_bytes);
#endif /* _DEBUG */

    return 0;
}

