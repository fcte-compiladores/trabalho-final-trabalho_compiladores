#include "compiler.h"
#include <string.h>
#include "helpers/vector.h"
#include "helpers/buffer.h"
#include <assert.h>
#include <ctype.h>

#define LEX_GETC_IF(buffer, c, exp)     \
    for (c = peekc(); exp; c = peekc()) \
    {                                   \
        buffer_write(buffer, c);        \
        nextc();                        \
    }

struct token *read_next_token();
static struct lex_process *lex_process;
static struct token tmp_token;
bool lex_is_in_expresssion();

// Pegar um caracter do arquivo, sem mudar a posicao de leitura.
static char peekc()
{
    return lex_process->function->peek_char(lex_process);
}

static char nextc()
{
    char c = lex_process->function->next_char(lex_process);

    if (lex_is_in_expresssion())
    {
        buffer_write(lex_process->parentheses_buffer, c);
    }
    
    lex_process->pos.col += 1;
    if (c == '\n')
    {
        lex_process->pos.line += 1;
        lex_process->pos.col = 1;
    }
    return c;
}

// Adicionar um caracter no arquivo.
static void pushc(char c)
{
    lex_process->function->push_char(lex_process, c);
}

static char assert_next_char(char c)
{
    char next_c = nextc();
    assert(c == next_c);

    return next_c;
}

static struct pos lex_file_position()
{
    return lex_process->pos;
}

struct token *token_create(struct token *_token)
{
    memcpy(&tmp_token, _token, sizeof(struct token));
    tmp_token.pos = lex_file_position();

    if (lex_is_in_expresssion())
    {
        tmp_token.between_brackets = buffer_ptr(lex_process->parentheses_buffer);
    }

    return &tmp_token;
}

static struct token *lexer_last_token()
{
    return vector_back_or_null(lex_process->token_vec);
}

static struct token *handle_whitespace()
{
    struct token *last_token = lexer_last_token();
    if (last_token)
    {
        last_token->whitespace = true;
    }

    nextc();
    return read_next_token();
}

const char *read_number_str()
{
    const char *num = NULL;
    struct buffer *buffer = buffer_create();
    char c = peekc();
    LEX_GETC_IF(buffer, c, (c >= '0' && c <= '9'));

    // Finaliza a string.
    buffer_write(buffer, 0x00);

    return buffer_ptr(buffer);
}

unsigned long long read_number()
{
    const char *s = read_number_str();
    return atoll(s);
}

struct token *token_make_number_for_value(unsigned long number)
{
    return token_create(&(struct token){.type = TOKEN_TYPE_NUMBER, .llnum = number});
}

struct token *token_make_number()
{
    return token_make_number_for_value(read_number());
}

static struct token *token_make_string(char start_delim, char end_delim)
{
    struct buffer *buf = buffer_create();
    assert(nextc() == start_delim); // verifica se o caracter inicial eh aspas duplas
    char c = nextc();

    for (; c != end_delim && c != EOF; c = nextc())
    {
        if (c == '\\')
        { // Retira o enter do final da string (se tiver).
            continue;
        }
        buffer_write(buf, c);
    }
    buffer_write(buf, 0x00);

    struct token *token = token_create(&(struct token){.type = TOKEN_TYPE_STRING, .sval = buffer_ptr(buf)});

    return token;
}

/* BEGIN - TOKEN_TYPE_OPERATOR */
static bool op_treated_as_one(char op)
{
    return op == '(' || op == '[' || op == ',' || op == '.' || op == '*' || op == '?';
}

static bool is_single_operator(char op)
{
    return op == '+' ||
           op == '-' ||
           op == '/' ||
           op == '*' ||
           op == '=' ||
           op == '>' ||
           op == '<' ||
           op == '|' ||
           op == '&' ||
           op == '^' ||
           op == '%' ||
           op == '!' ||
           op == '(' ||
           op == '[' ||
           op == ',' ||
           op == '.' ||
           op == '~' ||
           op == '?' ||
           op == '\\';
}

bool op_valid(const char *op)
{
    return S_EQ(op, "+") ||
           S_EQ(op, "-") ||
           S_EQ(op, "*") ||
           S_EQ(op, "/") ||
           S_EQ(op, "!") ||
           S_EQ(op, "^") ||
           S_EQ(op, "+=") ||
           S_EQ(op, "-=") ||
           S_EQ(op, "*=") ||
           S_EQ(op, "/=") ||
           S_EQ(op, ">>") ||
           S_EQ(op, "<<") ||
           S_EQ(op, ">=") ||
           S_EQ(op, "<=") ||
           S_EQ(op, ">") ||
           S_EQ(op, "<") ||
           S_EQ(op, "||") ||
           S_EQ(op, "&&") ||
           S_EQ(op, "|") ||
           S_EQ(op, "&") ||
           S_EQ(op, "++") ||
           S_EQ(op, "--") ||
           S_EQ(op, "=") ||
           S_EQ(op, "!=") ||
           S_EQ(op, "==") ||
           S_EQ(op, "->") ||
           S_EQ(op, "(") ||
           S_EQ(op, "[") ||
           S_EQ(op, "]") ||
           S_EQ(op, ",") ||
           S_EQ(op, ".") ||
           S_EQ(op, "...") ||
           S_EQ(op, "~") ||
           S_EQ(op, "?") ||
           S_EQ(op, "%") ||
           S_EQ(op, "\\");
}

void read_op_flush_back_keep_first(struct buffer *buffer)
{
    const char *data = buffer_ptr(buffer);
    int len = buffer->len;
    for (int i = len - 1; i >= 1; i--)
    {
        if (data[i] == 0x00)
            continue;

        pushc(data[i]);
    }
}

const char *read_op()
{
    bool single_operator = true;
    char op = nextc();
    struct buffer *buffer = buffer_create();
    buffer_write(buffer, op);

    if (!op_treated_as_one(op))
    {
        op = peekc();
        if (is_single_operator(op))
        {
            buffer_write(buffer, op);
            nextc();
            single_operator = false;
        }
    }
    buffer_write(buffer, 0x00);
    // printf("Token: %s\n", buffer->data);
    char *ptr = buffer_ptr(buffer);
    if (!single_operator)
    {

        if (!op_valid(ptr))
        {
            read_op_flush_back_keep_first(buffer);
            ptr[1] = 0x00;
        }
    }
    else if (!op_valid(ptr))
    {
        compiler_error(lex_process->compiler, "O operador %s nao eh valido!\n", ptr);
    }

    return ptr;
}

static void lex_new_expression()
{
    lex_process->current_expression_count++;
    if (lex_process->current_expression_count == 1)
    {
        lex_process->parentheses_buffer = buffer_create();
    }
}

static void lex_fisish_expression()
{
    lex_process->current_expression_count--;

    if (lex_process->current_expression_count < 0)
    {
        compiler_error(lex_process->compiler, "Voce fechou uma expressao nunca iniciada!\n");
    }
}

bool lex_is_in_expresssion()
{
    return lex_process->current_expression_count > 0;
}
/* END - TOKEN_TYPE_OPERATOR */

bool is_keyword(const char *str)
{
    return S_EQ(str, "unsigned") ||
           S_EQ(str, "signed") ||
           S_EQ(str, "char") ||
           S_EQ(str, "short") ||
           S_EQ(str, "int") ||
           S_EQ(str, "long") ||
           S_EQ(str, "float") ||
           S_EQ(str, "double") ||
           S_EQ(str, "void") ||
           S_EQ(str, "struct") ||
           S_EQ(str, "union") ||
           S_EQ(str, "static") ||
           S_EQ(str, "__ignore_typecheck") ||
           S_EQ(str, "return") ||
           S_EQ(str, "include") ||
           S_EQ(str, "sizeof") ||
           S_EQ(str, "if") ||
           S_EQ(str, "else") ||
           S_EQ(str, "while") ||
           S_EQ(str, "for") ||
           S_EQ(str, "do") ||
           S_EQ(str, "break") ||
           S_EQ(str, "continue") ||
           S_EQ(str, "switch") ||
           S_EQ(str, "case") ||
           S_EQ(str, "default") ||
           S_EQ(str, "goto") ||
           S_EQ(str, "typedef") ||
           S_EQ(str, "const") ||
           S_EQ(str, "extern") ||
           S_EQ(str, "retrict") ||
           S_EQ(str, "igual") ||
           S_EQ(str, "bool");
}

static struct token *token_make_operator_or_string()
{
    char op = peekc();
    // Tratar o caso do #include <abc.h>
    if (op == '<')
    {
        struct token *last_token = lexer_last_token();
        if (token_is_keyword(last_token, "include"))
        {
            return token_make_string('<', '>');
        }
    }

    struct token *token = token_create(&(struct token){.type = TOKEN_TYPE_OPERATOR, .sval = read_op()});
    if (op == '(')
    {
        lex_new_expression();
    }

    return token;
}

static struct token *token_make_symbol()
{
    char c = nextc();
    if (c == ')')
    {
        lex_fisish_expression();
    }

    struct token *token = token_create(&(struct token){.type = TOKEN_TYPE_SYMBOL, .cval = c});
    // printf("Token: %c\n", c);
    return token;
}

static struct token *token_make_identifier_or_keyword()
{
    struct buffer *buffer = buffer_create();
    char c = 0;
    LEX_GETC_IF(buffer, c, (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_')

    buffer_write(buffer, 0x00);
    // printf("Token: %s\n", buffer->data);

    if (is_keyword(buffer_ptr(buffer)))
    {
        return token_create(&(struct token){.type = TOKEN_TYPE_KEYWORD, .sval = buffer_ptr(buffer)});
    }

    return token_create(&(struct token){.type = TOKEN_TYPE_IDENTIFIER, .sval = buffer_ptr(buffer)});
}

struct token *read_special_token()
{
    char c = peekc();
    if (isalpha(c) || c == '_')
    {
        return token_make_identifier_or_keyword();
    }

    return NULL;
}

struct token *token_make_newline()
{
    nextc();
    return token_create(&(struct token){.type = TOKEN_TYPE_NEWLINE});
}

struct token *token_make_one_line_comment()
{
    struct buffer *buffer = buffer_create();
    char c = 0;
    LEX_GETC_IF(buffer, c, c != '\n' && c != EOF);
    buffer_write(buffer, 0x00);

    return token_create(&(struct token){.type = TOKEN_TYPE_COMMENT, .sval = buffer_ptr(buffer)});
}

struct token *token_make_multiline_comment()
{
    struct buffer *buffer = buffer_create();
    char c = 0;
    while (1)
    {
        LEX_GETC_IF(buffer, c, c != '*' && c != EOF);

        if (c == EOF)
        {
            compiler_error(lex_process->compiler, "O comentario nao foi fechado\n");
        }
        else if (c == '*')
        {
            nextc(); // Pula para o proximo caractere do arquivo.

            if (peekc() == '/')
            {
                // Finaliza o comentario
                nextc();
                break;
            }
        }
    }
    buffer_write(buffer, 0x00);

    return token_create(&(struct token){.type = TOKEN_TYPE_COMMENT, .sval = buffer_ptr(buffer)});
}

struct token *handle_comment()
{
    char c = peekc();
    if (c == '/')
    {
        nextc();
        if (peekc() == '/')
        {
            nextc();
            return token_make_one_line_comment();
        }
        else if (peekc() == '*')
        {
            nextc();
            return token_make_multiline_comment();
        }

        pushc('/');
        return token_make_operator_or_string();
    }

    return NULL;
}

/* BEGIN - HEXADECIMAL AND BINARY NUMBERS ***************************************************************/
bool is_hex_char(char c)
{
    return (c >= '0' && c <= '9') ||
           (c >= 'A' && c <= 'F') ||
           (c >= 'a' && c <= 'f');
}

const char *read_hex_number_str()
{
    struct buffer *buffer = buffer_create();
    char c = peekc();
    LEX_GETC_IF(buffer, c, is_hex_char(c))
    buffer_write(buffer, 0x00);

    return buffer_ptr(buffer);
}

struct token *token_make_special_number_hexadecimal()
{
    nextc(); // Retirar o 'x' do input stream

    unsigned long number = 0;
    const char *numer_str = read_hex_number_str();
    number = strtol(numer_str, 0, 16);

    return token_make_number_for_value(number);
}

struct token *token_make_special_number_binary()
{
    nextc(); // Retirar o 'x' do input stream

    unsigned long number = 0;
    const char *number_str = read_number_str();

    size_t len = strlen(number_str);
    for (int i = 0; i < len; i++)
    {
        if (number_str[i] != '0' && number_str[i] != '1')
        {
            compiler_error(lex_process->compiler, "Numero binario incorreto!\n");
        }
    }

    number = strtol(number_str, 0, 2);

    return token_make_number_for_value(number);
}

struct token *token_make_special_number()
{
    struct token *token = NULL;
    struct token *last_token = lexer_last_token();

    if (!last_token || !(last_token->type == TOKEN_TYPE_NUMBER && last_token->llnum == 0))
    {
        return token_make_identifier_or_keyword();
    }

    vector_pop(lex_process->token_vec); // Basicamente retira o último token da pilha

    char c = peekc();
    if (c == 'x')
    {
        token = token_make_special_number_hexadecimal();
    }
    else if (c == 'b')
    {
        token = token_make_special_number_binary();
    }

    return token;
}
/* END - HEXADECIMAL AND BINARY NUMBERS ***************************************************************/

/* BEGIN - QUOTES ************************************************************************************/
char lex_get_escaped_char(char c)
{
    char co = 0;
    switch (c)
    {
    case 'n':
        co = '\n';
        break;
    case '\\':
        co = '\\';
        break;
    case 't':
        co = '\t';
        break;
    case '\'':
        co = '\'';
        break;
    }
    return co;
}

struct token *token_make_quote()
{
    assert_next_char('\'');
    char c = nextc();
    if (c == '\\')
    {
        c = nextc();
        c = lex_get_escaped_char(c);
    }

    if (nextc() != '\'')
    {
        compiler_error(lex_process->compiler, "Voce abriu um ' mas nao fechou com um caracter ' \n");
    }

    return token_create(&(struct token){.type = TOKEN_TYPE_NUMBER, .cval = c});
}
/* END - QUOTES ************************************************************************************/

void print_token_list(struct lex_process *process)
{
    struct token *token = NULL;

    for (int i = 0; i < process->token_vec->count; i++)
    {
        token = vector_at(process->token_vec, i);

        switch (token->type)
        {
        case TOKEN_TYPE_IDENTIFIER:
            printf("TOKEN\tID: %s\n", token->sval);
            break;
        case TOKEN_TYPE_KEYWORD:
            printf("TOKEN\tKE: %s\n", token->sval);
            break;
        case TOKEN_TYPE_NEWLINE:
            printf("TOKEN\tNL \n");
            break;
        case TOKEN_TYPE_COMMENT:
            printf("TOKEN\tCO: \n");
            break;
        case TOKEN_TYPE_NUMBER:
            printf("TOKEN\tNU: %llu \t PARENTESES: %s\n", token->llnum, token->between_brackets);
            break;
        case TOKEN_TYPE_OPERATOR:
            printf("TOKEN\tOP: %s\n", token->sval);
            break;
        case TOKEN_TYPE_STRING:
            printf("TOKEN\tST: %s\n", token->sval);
            break;
        case TOKEN_TYPE_SYMBOL:
            printf("TOKEN\tSY: %c\n", token->cval);
            break;
        default:
            break;
        }
    }
}

// Funcao responsavel por ler o proximo token do arquivo.
struct token *read_next_token()
{
    struct token *token = NULL;
    char c = peekc();

    token = handle_comment();
    if (token)
        return token;

    switch (c)
    {
    case EOF: /* */
        break;
    NUMERIC_CASE:
        token = token_make_number();
        break;
    case 'x':
    case 'b':
        token = token_make_special_number();
        break;
    OPERATOR_CASE:
        token = token_make_operator_or_string();
        break;
    SYMBOL_CASE:
        token = token_make_symbol();
        break;
    case '"':
        token = token_make_string('"', '"');
        break;
    case '\'':
        token = token_make_quote();
        break;
    case '\t':
    case ' ':
        token = handle_whitespace();
        break;
    case '\n':
        token = token_make_newline();
        break;
    default:
        token = read_special_token();
        if (!token)
        {
            compiler_error(lex_process->compiler, "Token invalido!\n");
        }
        break;
    }
    return token;
}

int lex(struct lex_process *process)
{
    process->current_expression_count = 0;
    process->parentheses_buffer = NULL;
    lex_process = process;
    process->pos.filename = process->compiler->cfile.abs_path;

    struct token *token = read_next_token();

    // Ler todos os tokens do arquivo de imput
    while (token)
    {
        vector_push(process->token_vec, token);
        token = read_next_token();
    }

    print_token_list(process);

    return LEXICAL_ANALYSIS_ALL_OK;
}

char lexer_string_buffer_next_char(struct lex_process *process)
{
    struct buffer *buf = lex_process_private(process);
    return buffer_read(buf);
}

char lexer_string_buffer_peek_char(struct lex_process *process)
{
    struct buffer *buf = lex_process_private(process);
    return buffer_peek(buf);
}

void lexer_string_buffer_push_char(struct lex_process *process, char c)
{
    struct buffer *buf = lex_process_private(process);
    buffer_write(buf, c);
}

struct lex_process_functions lexer_string_buffer_functions = {
    .next_char = lexer_string_buffer_next_char,
    .peek_char = lexer_string_buffer_peek_char,
    .push_char = lexer_string_buffer_push_char};

// Permite a criacao de tokens a partir de strings e sem a necessidade do inputfile.
struct lex_process *tokens_build_for_string(struct compile_process *compiler, const char *str)
{
    struct buffer *buffer = buffer_create();
    buffer_printf(buffer, str);
    struct lex_process *lex_process = lex_process_create(compiler, &lexer_string_buffer_functions, buffer);

    if (!lex_process)
        return NULL;

    if (lex(lex_process) != LEXICAL_ANALYSIS_ALL_OK)
        return NULL;

    return lex_process;
}