#include "compiler.h"
#include "helpers/vector.h"
#include <assert.h>

struct vector *node_vector = NULL;
struct vector *node_vector_root = NULL;

void node_set_vector(struct vector *vec, struct vector *root_vec)
{
    node_vector = vec;
    node_vector_root = root_vec;
}

// Adiciona o node ao final da lista.
void node_push(struct node *node)
{
    vector_push(node_vector, &node);
}

// Pega o node da parte de tras do vetor. Se nao tiver nada, retorna NULL.
struct node *node_peek_or_null()
{
    return vector_back_ptr_or_null(node_vector);
}

struct node *node_peek()
{
    if (vector_empty(node_vector))
    {
        return NULL;
    }
    return vector_back_ptr(node_vector);
}

struct node *node_pop()
{
    // Check if vector is empty before any operations
    if (vector_empty(node_vector))
    {
        return NULL;
    }

    struct node *last_node = vector_back_ptr(node_vector);

    // Safe check for root vector
    struct node *last_node_root = vector_empty(node_vector_root) ? NULL : vector_back_ptr(node_vector_root);

    vector_pop(node_vector);

    // Only pop from root vector if needed and possible
    if (last_node == last_node_root && !vector_empty(node_vector_root))
    {
        vector_pop(node_vector_root);
    }

    return last_node;
}

void print_tree(struct node *node, int level)
{
}

// Tipos de nos que podem ser colocados dentro de uma expressao.
bool node_is_expressionable(struct node *node)
{
    return node->type == NODE_TYPE_EXPRESSION ||
           node->type == NODE_TYPE_EXPRESSION_PARENTHESES ||
           node->type == NODE_TYPE_UNARY ||
           node->type == NODE_TYPE_IDENTIFIER ||
           node->type == NODE_TYPE_NUMBER ||
           node->type == NODE_TYPE_STRING;
}

// Verifica se o node informado pode ser colocado dentro de uma expressao.
struct node *node_peek_expressionable_or_null()
{
    struct node *last_node = node_peek_or_null();
    return node_is_expressionable(last_node) ? last_node : NULL;
}

// Cria um node de expressao.
void make_exp_node(struct node *node_left, struct node *node_right, const char *op)
{
    assert(node_left);
    assert(node_right);
    node_create(&(struct node){.type = NODE_TYPE_EXPRESSION, .exp.left = node_left, .exp.right = node_right, .exp.op = op});
}

// Funcao parar criar um node.
struct node *node_create(struct node *_node)
{
    struct node *node = malloc(sizeof(struct node));
    memcpy(node, _node, sizeof(struct node));
    node_push(node);
    return node;
}