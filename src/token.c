#include "compiler.h"

bool token_is_keyword(struct token *token, const char *value)
{
    return token->type == TOKEN_TYPE_KEYWORD && S_EQ(token->sval, value);
}

bool token_is_symbol(struct token *token, char value)
{
    return (token->type == TOKEN_TYPE_SYMBOL) && (token->cval == value);
}

bool discart_token(struct token *token)
{
    return (token->type == TOKEN_TYPE_NEWLINE) ||
           (token->type == TOKEN_TYPE_COMMENT);
}

bool token_is_operator(struct token* token, const char* val) {
    if (!token) return false;
    
    // Caso especial para "]" - aceitar tanto como operador quanto como símbolo
    if (S_EQ(val, "]")) {
        return (token->type == TOKEN_TYPE_OPERATOR && S_EQ(token->sval, val)) ||
               (token->type == TOKEN_TYPE_SYMBOL && token->cval == ']');
    }
    
    // Para outros operadores, manter o comportamento original
    return token->type == TOKEN_TYPE_OPERATOR && S_EQ(token->sval, val);
}