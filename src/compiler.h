#ifndef COMPILER_H
#define COMPILER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

struct pos
{
    int line;
    int col;
    const char *filename;
};

#define S_EQ(str, str2) \
    (str && str2 && (strcmp(str, str2) == 0))

#define NUMERIC_CASE \
    case '0':        \
    case '1':        \
    case '2':        \
    case '3':        \
    case '4':        \
    case '5':        \
    case '6':        \
    case '7':        \
    case '8':        \
    case '9'

enum
{
    LEXICAL_ANALYSIS_ALL_OK,
    LEXICAL_ANALYSIS_INPUT_ERROR
};

#define OPERATOR_CASE \
    case '+':         \
    case '-':         \
    case '*':         \
    case '>':         \
    case '<':         \
    case '^':         \
    case '%':         \
    case '!':         \
    case '=':         \
    case '~':         \
    case '|':         \
    case '&':         \
    case '(':         \
    case '[':         \
    case ',':         \
    case '.':         \
    case '?'

#define SYMBOL_CASE \
    case '{':       \
    case '}':       \
    case ':':       \
    case ';':       \
    case '#':       \
    case '\\':      \
    case ')':       \
    case ']'

enum
{
    TOKEN_TYPE_KEYWORD,
    TOKEN_TYPE_IDENTIFIER,
    TOKEN_TYPE_OPERATOR,
    TOKEN_TYPE_SYMBOL,
    TOKEN_TYPE_NUMBER,
    TOKEN_TYPE_STRING,
    TOKEN_TYPE_COMMENT,
    TOKEN_TYPE_NEWLINE
};

#define TOTAL_OPERADOR_GROUPS 14
#define MAX_OPERATORS_IN_GROUP 12
enum
{
    ASSOCIATIVITY_LEFT_TO_RIGTH,
    ASSOCIATIVITY_RIGHT_TO_LEFT
};
struct expressionable_op_precedence_group
{
    char *operators[MAX_OPERATORS_IN_GROUP];
    int associativity;
};

struct token
{
    int type;
    int flags;

    struct pos pos; // Identificar onde o token esta no arquivo.

    union
    {
        char cval;
        const char *sval;
        unsigned int inum;
        unsigned long lnum;
        unsigned long long llnum;
        void *any;
    };
    // Sera 'true' se tiver um espaço entre um token e o próximo token.
    bool whitespace;

    // Retira a string que estiver dentro de parênteses. Ex: (1+2+3) resulta em 1+2+3.
    const char *between_brackets;
};

struct lex_process;

// Definicao de ponteiros para funcoes.
typedef char (*LEX_PROCESS_NEXT_CHAR)(struct lex_process *process);
typedef char (*LEX_PROCESS_PEEK_CHAR)(struct lex_process *process);
typedef void (*LEX_PROCESS_PUSH_CHAR)(struct lex_process *process, char c);

struct lex_process_functions
{
    LEX_PROCESS_NEXT_CHAR next_char;
    LEX_PROCESS_PEEK_CHAR peek_char;
    LEX_PROCESS_PUSH_CHAR push_char;
};

struct lex_process
{
    struct pos pos;
    struct vector *token_vec;
    struct compile_process *compiler;

    int current_expression_count; // Qts parenteses existem no momento.

    struct buffer *parentheses_buffer;
    struct lex_process_functions *function;

    void *private; // Dados privados que o lexer nao entende mas o programador entende.
};

enum
{
    COMPILER_FILE_COMPILED_OK,
    COMPILER_FAILED_WITH_ERRORS
};

struct compile_process
{
    // Como o arquivo deve ser compilado
    int flags;

    struct pos pos;

    struct compile_process_input_file
    {
        FILE *fp;
        const char *abs_path;
    } cfile;

    struct vector *token_vec;
    struct vector *node_vec;
    struct vector *node_tree_vec;

    FILE *ofile;

    struct {
        struct scope* root;
        struct scope* current;
    } scope;

    struct {
        struct vector* table;
        struct vector* tables;
    } symbols;
};

    enum {
    DATATYPE_FLAG_IS_SIGNED = 0b00000001,
    DATATYPE_FLAG_IS_STATIC = 0b00000010,
    DATATYPE_FLAG_IS_CONST = 0b00000100,
    DATATYPE_FLAG_IS_POINTER = 0b00001000,
    DATATYPE_FLAG_IS_ARRAY = 0b00010000,
    DATATYPE_FLAG_IS_EXTERN = 0b00100000,
    DATATYPE_FLAG_IS_RESTRICT = 0b01000000,
    DATATYPE_FLAG_IS_IGNORE_TYPE_CHECKING = 0b10000000,
    DATATYPE_FLAG_IS_SECONDARY = 0b100000000,
    DATATYPE_FLAG_IS_STRUCT_UNION_NO_NAME = 0b1000000000,
    DATATYPE_FLAG_IS_LETERAL = 0b10000000000
};

enum {
    DATATYPE_VOID,
    DATATYPE_CHAR,
    DATATYPE_SHORT,
    DATATYPE_INTEGER,
    DATATYPE_LONG,
    DATATYPE_FLOAT,
    DATATYPE_DOUBLE,
    DATATYPE_STRUCT,
    DATATYPE_UNION,
    DATATYPE_UNKNOWN
};

enum {
    DATATYPE_EXPECT_PRIMITIVE,
    DATATYPE_EXPECT_UNION,
    DATATYPE_EXPECT_STRUCT
};

struct datatype {
    int flags;
    // EX: long, int, float, etc.
    int type;
    const char* type_str;
    // EX: long int, sendo int o secundário.
    struct datatype* datatype_secondary;
    // Tamanho do datatype. EX: long tem 8 bytes.
    size_t size;
    // Quantidades de ponteiros alinhados. Ex: int** a, pointer_depth == 2.
    int pointer_depth;
    union {
        struct node* struct_node;
        struct node* union_node;
    };
};

enum {
    NODE_TYPE_EXPRESSION,
    NODE_TYPE_EXPRESSION_PARENTHESES,
    NODE_TYPE_NUMBER,
    NODE_TYPE_IDENTIFIER,
    NODE_TYPE_STRING,
    NODE_TYPE_VARIABLE,
    NODE_TYPE_VARIABLE_LIST,
    NODE_TYPE_FUNCTION,
    NODE_TYPE_BODY,
    NODE_TYPE_STATEMENT_RETURN,
    NODE_TYPE_STATEMENT_IF,
    NODE_TYPE_STATEMENT_ELSE,
    NODE_TYPE_STATEMENT_WHILE,
    NODE_TYPE_STATEMENT_DO_WHILE,
    NODE_TYPE_STATEMENT_FOR,
    NODE_TYPE_STATEMENT_BREAK,
    NODE_TYPE_STATEMENT_CONTINUE,
    NODE_TYPE_STATEMENT_SWITCH,
    NODE_TYPE_STATEMENT_CASE,
    NODE_TYPE_STATEMENT_DEFAULT,
    NODE_TYPE_STATEMENT_GOTO,
    NODE_TYPE_UNARY,
    NODE_TYPE_TENARY,
    NODE_TYPE_LABEL,
    NODE_TYPE_STRUCT,
    NODE_TYPE_UNION,
    NODE_TYPE_BRACKET,
    NODE_TYPE_CAST,
    NODE_TYPE_BLANK
};

enum {
    PARSE_ALL_OK,
    PARSE_GENERAL_ERROR
};

enum {
    NODE_FLAG_INSIDE_EXPRESSION = 0b00000001
};

struct node
{
    int type;
    int flags;
    struct pos pos;
    struct node_binded
    {
        // Ponteiro para o body node.
        struct node *owner;
        // Ponteiro para a funcao que o node esta.
        struct node *funtion;
    } binded;
    // Estrutura similar ao token
    union
    {
        char cval;
        const char *sval;
        unsigned int inum;
        unsigned long lnum;
        unsigned long long llnum;
        void *any;
    };
    union
    {
        struct exp
        {
            struct node *left;
            struct node *right;
            const char *op;
        } exp;
        struct var {
            struct datatype type;
            const char* name;
            struct node* val;
        } var;
        struct varlist {
            struct vector* list;
        } var_list;
    };
};

enum {
    SYMBOL_TYPE_NODE,
    SYMBOL_TYPE_NATIVE_FUNCTION,
    SYMBOL_TYPE_UNKNOWN
};

struct symbol {
    const char* name;
    int type;
    void* data;
};

struct scope {
    int flags;
    struct vector* entities;
    // Quantidade total de bytes do escopo.
    size_t size;
    // NULL se nao tiver pai.
    struct scope* parent;
};

bool token_is_operator(struct token* token, const char* val);

/* FUNCOES DO ARQUIVO CPROCESS.C */
char compile_process_next_char(struct lex_process *lex_process);
char compile_process_peek_char(struct lex_process *lex_process);
void compile_process_push_char(struct lex_process *lex_process, char c);

/* FUNCOES DO ARQUIVO LEX_PROCESS.C */
struct lex_process *lex_process_create(struct compile_process *compiler, struct lex_process_functions *functions, void *private);
void lex_process_free(struct lex_process *process);
void *lex_process_private(struct lex_process *process);
struct vector *lex_process_tokens(struct lex_process *process);

/* FUNCOES DO ARQUIVO LEXER.C */
int lex(struct lex_process *process);

/* FUNCOES DO ARQUIVO COMPILER.C */
int compile_file(const char *filename, const char *out_finename, int flags);
struct compile_process *compile_process_create(const char *filename, const char *filename_out, int flags);
void compiler_error(struct compile_process *compiler, const char *msg, ...);
void compiler_warning(struct compile_process *compiler, const char *msg, ...);

/* CONTROI UM TOKEN A PARTIR DE UMA STRING */
struct lex_process *tokens_build_for_string(struct compile_process *compiler, const char *str);

/* Estrutura para passar comandos atraves de funcoes recursivas. */
struct history {
    int flags;
};

/* FUNCOES DO ARQUIVO PARSER.C */
int parse(struct compile_process *process);
void parse_variable(struct datatype* dtype, struct token* name_token, struct history* history);
void make_variable_node_and_register(struct history* history, struct datatype* dtype, struct token* name_token, struct node* value_node);
void make_variable_node(struct datatype* dtype, struct token* name_token, struct node* value_node);
void parse_variable_function_or_struct_union(struct history* history);
void parse_expressionable_root(struct history* history);
void parse_struct(struct history* history);
void parse_if(struct history* history);

/* FUNCOES DO ARQUIVO TOKEN.C */
bool token_is_keyword(struct token *token, const char *value);
bool token_is_symbol(struct token *token, const char value);
bool discart_token(struct token *token);

/* FUNCOES DO ARQUIVO NODE.C */
void node_set_vector(struct vector *vec, struct vector *root_vec);
void node_push(struct node *node);
struct node *node_peek_or_null();
struct node *node_peek();
struct node *node_pop();
struct node *node_peek_expressionable_or_null();
bool node_is_expressionable(struct node *node);
void make_exp_node(struct node *node_left, struct node *node_right, const char *op);
struct node *node_create(struct node *_node);

#endif
