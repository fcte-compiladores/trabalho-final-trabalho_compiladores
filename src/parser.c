#include "compiler.h"
#include "helpers/vector.h"
#include <assert.h>

static struct compile_process *current_process;
static struct token *parser_last_token;

void parse_expressionable(struct history *history);
int parse_expressionable_single(struct history *history);

static bool parser_left_op_has_priority(const char *op_left, const char *op_right);
extern struct expressionable_op_precedence_group op_precedence[TOTAL_OPERADOR_GROUPS];

int parse(struct compile_process *process);

void parse_identifier(struct history* history);
static bool keyword_is_datatype(const char* val);
static bool is_keyword_variable_modifier(const char* val);
bool token_is_primitive_keyword(struct token* token);
void parser_get_datatype_tokens(struct token** datatype_token, struct token** datatype_secundary_token);
int parser_datatype_expected_for_type_string(const char* str);
int parser_get_random_type_index();
struct token*parser_build_random_type_name();
int parser_get_pointer_depth();
bool parser_datatype_is_secondary_allowed_for_type(const char* type);
void parser_datatype_adjust_size_for_secondary(struct datatype* datatype, struct token* datatype_secondary_token);
void parser_datatype_init_type_and_size_for_primitive(struct token* datatype_token, struct token* datatype_secondary_token, struct datatype* datatype_out);
void parser_datatype_init_type_and_size(struct token* datatype_token, struct token* datatype_secondary_token, struct datatype* datatype_out, int pointer_depth, int expected_type);
void parser_datatype_init(struct token* datatype_token, struct token* datatype_secondary_token, struct datatype* datatype_out, int pointer_depth, int expected_type);
void parse_datatype_type(struct datatype* dtype);
void parse_datatype_modifiers(struct datatype* dtype);
void parse_datatype(struct datatype* dtype);
void parse_variable_function_or_struct_union(struct history* history);
void parse_keyword(struct history *history);
void parse_variable_igual_syntax(struct history* history, struct token* name_token);
void parse_struct(struct history* history);
void parse_struct_tradicional(struct history* history, struct token* struct_name);
void parse_struct_nova_sintaxe(struct history* history, struct token* struct_name);
void parse_if(struct history* history);
void parse_if_tradicional(struct history* history);
void parse_if_nova_sintaxe(struct history* history);
void parse_expressionable_until_token(struct history* history, const char* stop_token);

void make_variable_node_and_register(struct history* history, struct datatype* dtype, struct token* name_token, struct node* value_node);
void make_variable_node(struct datatype* dtype, struct token* name_token, struct node* value_node);
void make_variable_node_simple(struct datatype* dtype, struct token* name_token, struct node* value_node);

struct history *history_begin(int flags)
{
    struct history *history = calloc(1, sizeof(struct history));
    history->flags = flags;
    return history;
}
struct history *history_down(struct history *history, int flags)
{
    struct history *new_history = calloc(1, sizeof(struct history));
    memcpy(new_history, history, sizeof(struct history));
    new_history->flags = flags;
    return new_history;
}

static void parser_ignore_nl_or_comment(struct token *token)
{
    while (token && discart_token(token))
    {
        // Pula o token que deve ser descartado no vetor.
        vector_peek(current_process->token_vec);
        // Pega o proximo token para ver se ele tambem sera descartado.
        token = vector_peek_no_increment(current_process->token_vec);
    }
}

static struct token *token_next()
{
    struct token *next_token = vector_peek_no_increment(current_process->token_vec);
    parser_ignore_nl_or_comment(next_token);
    current_process->pos = next_token->pos; // Atualiza a posicao do arquivo de compilacao.
    parser_last_token = next_token;
    return vector_peek(current_process->token_vec);
}

static struct token *token_peek_next()
{
    struct token *next_token = vector_peek_no_increment(current_process->token_vec);
    parser_ignore_nl_or_comment(next_token);
    return vector_peek_no_increment(current_process->token_vec);
}

void parse_single_token_to_node()
{
    struct token *token = token_next();
    struct node *node = NULL;
    switch (token->type)
    {
    case TOKEN_TYPE_NUMBER:
        node = node_create(&(struct node){.type = NODE_TYPE_NUMBER, .llnum = token->llnum});
        break;
    case TOKEN_TYPE_IDENTIFIER:
        node = node_create(&(struct node){.type = NODE_TYPE_IDENTIFIER, .sval = token->sval});
        break;
    case TOKEN_TYPE_STRING:
        node = node_create(&(struct node){.type = NODE_TYPE_STRING, .sval = token->sval});
        break;
    default:
        compiler_error(current_process, "Esse token nao pode ser convertido para node!\n");
        break;
    }

    // Verifica se o nó foi criado com sucesso
    if (node)
    {
        node_push(node);
    }
    else
    {
        printf("ERRO: Falha ao criar nó para token: %d\n", token->type);
    }
}

void parse_expressionable_for_op(struct history *history, const char *op)
{
    if (!op) return;
    parse_expressionable(history);
}

void parser_node_shift_children_left(struct node *node)
{
    assert(node->type == NODE_TYPE_EXPRESSION);
    assert(node->exp.right == NODE_TYPE_EXPRESSION);

    const char *right_op = node->exp.right->exp.op;
    struct node *new_exp_left_node = node->exp.left;
    struct node *new_exp_right_node = node->exp.right->exp.left;
    make_exp_node(new_exp_left_node, new_exp_right_node, node->exp.op);

    // EX: 50*E(20+50) -> E(50*20)+50
    struct node *new_left_operand = node_pop();
    struct node *new_right_operand = node->exp.right->exp.right;
    node->exp.left = new_left_operand;
    node->exp.right = new_right_operand;
    node->exp.op = right_op;
}

void parser_reorder_expression(struct node **node_out)
{
    struct node *node = *node_out;

    // Only reorder expression nodes
    if (!node || node->type != NODE_TYPE_EXPRESSION)
        return;

    // Recurse into sub-expressions first
    if (node->exp.left && node->exp.left->type == NODE_TYPE_EXPRESSION)
        parser_reorder_expression(&node->exp.left);

    if (node->exp.right && node->exp.right->type == NODE_TYPE_EXPRESSION)
        parser_reorder_expression(&node->exp.right);

    // Now check if the right expression has higher precedence
    if (node->exp.right && node->exp.right->type == NODE_TYPE_EXPRESSION)
    {
        const char *current_op = node->exp.op;
        const char *right_op = node->exp.right->exp.op;

        if (parser_left_op_has_priority(current_op, right_op))
        {
            // Get nodes A, B, C
            struct node *A = node->exp.left;
            struct node *B = node->exp.right->exp.left;
            struct node *C = node->exp.right->exp.right;

            // Save the operator from the right expression
            const char *new_op = right_op;

            // Create (A op B) subtree
            make_exp_node(A, B, current_op);
            struct node *new_left = node_pop();

            // Update current node: (A op B) new_op C
            node->exp.left = new_left;
            node->exp.right = C;
            node->exp.op = new_op;

            // Check if we need to further reorder the new left subtree
            parser_reorder_expression(&node->exp.left);
        }
    }
}

static int parser_get_precedence_for_operator(const char *op, struct expressionable_op_precedence_group **group_out)
{
    *group_out = NULL;
    for (int i = 0; i < TOTAL_OPERADOR_GROUPS; i++)
    {
        for (int j = 0; op_precedence[i].operators[j]; j++)
        {
            const char *_op = op_precedence[i].operators[j];
            if (S_EQ(op, _op))
            {
                *group_out = &op_precedence[i];
                return i;
            }
        }
    }
    return -1;
}

static bool parser_left_op_has_priority(const char *op_left, const char *op_right)
{
    struct expressionable_op_precedence_group *group_left = NULL;
    struct expressionable_op_precedence_group *group_right = NULL;

    int precedence_left = parser_get_precedence_for_operator(op_left, &group_left);
    int precedence_right = parser_get_precedence_for_operator(op_right, &group_right);

    // Compara os precedentes
    if (precedence_left != precedence_right)
    {
        return precedence_left < precedence_right;
    }

    // Igual -> Confere associatividade
    if (group_left && group_left->associativity == ASSOCIATIVITY_LEFT_TO_RIGTH)
    {
        return false;
    }

    // Associatividade direita -> esquerda prioridade
    if (group_left && group_left->associativity == ASSOCIATIVITY_RIGHT_TO_LEFT)
    {
        return true;
    }

    return false;
}

void parse_exp_normal(struct history *history)
{
    struct token *op_token = token_peek_next();
    if (!op_token) return;
    const char *op = op_token->sval;
    if (!op) return;

    struct node *node_left = node_peek_expressionable_or_null();
    if (!node_left) return;

    // Retira da lista de tokens o token de operador. Ex: "123+456", retira o token "+".
    token_next();

    // Retira da lista de nodes, o ultimo node inserido.
    node_pop();

    node_left->flags |= NODE_FLAG_INSIDE_EXPRESSION;

    // Nesse momento, temos o node da esquerda e o operador. Essa funcao ira criar o node da direita.
    parse_expressionable_for_op(history_down(history, history->flags), op);
    struct node *node_right = node_pop();
    node_right->flags |= NODE_FLAG_INSIDE_EXPRESSION;

    // Cria o node de expressao, passando o node da esquerda, node da direita e operador.
    make_exp_node(node_left, node_right, op);
    struct node *exp_node = node_pop();

    // TODO: Inserir AQUI codigo para reordenador a expressao.
    parser_reorder_expression(&exp_node);

    node_push(exp_node);
}

int parse_exp(struct history *history)
{
    parse_exp_normal(history);
    return 0;
}

void parse_keyword_for_global() {
    parse_keyword(history_begin(0));
    struct node* node = node_pop();
    if (node) {
        // Adiciona o node à árvore global
        vector_push(current_process->node_tree_vec, &node);
    }
}

void parse_identifier(struct history* history) {
    struct token* token = token_peek_next();
    if (!token || token->type != TOKEN_TYPE_IDENTIFIER) {
        compiler_error(current_process, "Esperado identificador\n");
        return;
    }
    parse_single_token_to_node();
}

static bool keyword_is_datatype(const char* val) {
    return S_EQ(val, "void") ||
           S_EQ(val, "char") ||
           S_EQ(val, "int") ||
           S_EQ(val, "short") ||
           S_EQ(val, "float") ||
           S_EQ(val, "double") ||
           S_EQ(val, "long") ||
           S_EQ(val, "struct") ||
           S_EQ(val, "union") ||
           S_EQ(val, "bool");
}

static bool is_keyword_variable_modifier(const char* val) {
    return S_EQ(val, "unsigned") ||
           S_EQ(val, "signed") ||
           S_EQ(val, "static") ||
           S_EQ(val, "const") ||
           S_EQ(val, "extern") ||
           S_EQ(val, "__ignore_typecheck__");
}

bool token_is_primitive_keyword(struct token* token) {
    if (!token) return false;
    if (token->type != TOKEN_TYPE_KEYWORD) return false;
    if (S_EQ(token->sval, "void")) {
        return true;
    } else if (S_EQ(token->sval, "char")) {
        return true;
    } else if (S_EQ(token->sval, "short")) {
        return true;
    } else if (S_EQ(token->sval, "int")) {
        return true;
    } else if (S_EQ(token->sval, "long")) {
        return true;
    } else if (S_EQ(token->sval, "float")) {
        return true;
    } else if (S_EQ(token->sval, "double")) {
        return true;
    } else if (S_EQ(token->sval, "bool")) {
        return true;
    }
    return false;
}

// Essa funcao trata o caso de 2 datatypes seguidos. Ex: long int A.
void parser_get_datatype_tokens(struct token** datatype_token, struct token** datatype_secundary_token) {
    *datatype_token = token_next();
    struct token* next_token = token_peek_next();
    if (token_is_primitive_keyword(next_token)) {
        *datatype_secundary_token = next_token;
        token_next();
    }
}

int parser_datatype_expected_for_type_string(const char* str) {
    int type = DATATYPE_EXPECT_PRIMITIVE;
    if (S_EQ(str, "union")) {
        type = DATATYPE_EXPECT_UNION;
    } else if (S_EQ(str, "struct")) {
        type = DATATYPE_EXPECT_STRUCT;
    }
    return type;
}

int parser_get_random_type_index() {
    static int x = 0;
    x++;
    return x;
}

struct token* parser_build_random_type_name() {
    static int counter = 0;
    char tmp_name[32];
    snprintf(tmp_name, sizeof(tmp_name), "__anonymous_type_%d", counter++);
    
    // Aloca memória para o nome e copia
    char* sval = malloc(strlen(tmp_name) + 1);
    if (!sval) {
        compiler_error(current_process, "Falha ao alocar memória para nome de tipo anônimo\n");
        return NULL;
    }
    strcpy(sval, tmp_name);
    
    // Cria o token
    struct token* token = calloc(1, sizeof(struct token));
    if (!token) {
        free(sval);
        compiler_error(current_process, "Falha ao alocar memória para token de tipo anônimo\n");
        return NULL;
    }
    
    token->type = TOKEN_TYPE_IDENTIFIER;
    token->sval = sval;
    token->pos = current_process->pos; // Mantém a posição atual do arquivo
    
    return token;
}

int parser_get_pointer_depth() {
    int depth = 0;
    struct token* token = token_peek_next();
    while (token && token->type == TOKEN_TYPE_OPERATOR && S_EQ(token->sval, "*")) {
        depth++;
        token_next();
    }
    return depth;
}

bool parser_datatype_is_secondary_allowed_for_type(const char* type) {
    return S_EQ(type, "long") || S_EQ(type, "short") || S_EQ(type, "double") || S_EQ(type, "float") || S_EQ(type, "bool");
}

int parse_expressionable_single(struct history *history) {
    struct token *token = token_peek_next();
    if (!token) return -1;
    
    history->flags |= NODE_FLAG_INSIDE_EXPRESSION;
    int res = -1;
    
    switch (token->type) {
        case TOKEN_TYPE_NUMBER:
            parse_single_token_to_node();
            res = 0;
            break;
        case TOKEN_TYPE_IDENTIFIER:
            parse_identifier(history);
            res = 0;
            break;
        case TOKEN_TYPE_OPERATOR:
            parse_exp(history);
            res = 0;
            break;
        case TOKEN_TYPE_KEYWORD:
            // Para keywords dentro de expressões, apenas avança
            token_next();
            res = 0;
            break;
        default:
            // Para outros tipos, apenas avança
            token_next();
            res = 0;
            break;
    }
    return res;
}

void parse_expressionable(struct history *history)
{
    int max_iterations = 100; // Limite para evitar loop infinito
    int iteration_count = 0;
    
    while (iteration_count < max_iterations && parse_expressionable_single(history) == 0)
    {
        iteration_count++;
    }
    
    if (iteration_count >= max_iterations) {
        printf("Aviso: Número máximo de iterações atingido em parse_expressionable\n");
    }
}

int parse_next()
{
    struct token *token = token_peek_next();
    if (!token)
        return -1;

    int res = 0;
    switch (token->type)
    {
    case TOKEN_TYPE_KEYWORD:
        if (S_EQ(token->sval, "struct")) {
            token_next(); // Consome "struct"
            parse_struct(history_begin(0));
            res = 0;
        } else if (S_EQ(token->sval, "if")) {
            token_next(); // Consome "if"
            parse_if(history_begin(0));
            res = 0;
        } else if (S_EQ(token->sval, "return")) {
            token_next(); // Consome "return"
            // Processa o valor de retorno
            parse_expressionable_root(history_begin(0));
            struct node* return_value = node_pop();
            if (return_value) {
                struct node return_node = {
                    .type = NODE_TYPE_STATEMENT_RETURN,
                    .pos = token->pos
                };
                struct node* created_return = node_create(&return_node);
                if (created_return) {
                    node_push(created_return);
                    printf("Return statement processado\n");
                }
            }
            // Verifica ponto e vírgula
            struct token* semicolon = token_next();
            if (!token_is_symbol(semicolon, ';')) {
                compiler_error(current_process, "Esperado ';' após return\n");
            }
            res = 0;
        } else if (keyword_is_datatype(token->sval)) {
            parse_variable_function_or_struct_union(history_begin(0));
            res = 0;
        } else {
            // Para outros keywords, apenas avança
            token_next();
            res = 0;
        }
        break;
    case TOKEN_TYPE_NUMBER:
    case TOKEN_TYPE_STRING:
        parse_expressionable(history_begin(0));
        res = 0;
        break;
    case TOKEN_TYPE_IDENTIFIER:
        // Verificar se o próximo token é "igual" para usar a nova sintaxe
        {
            // Consumir o token atual (identificador)
            struct token* current_token = token_next();
            
            // Verificar o próximo token
            struct token* next_token = token_peek_next();
            
            if (next_token && token_is_keyword(next_token, "igual")) {
                parse_variable_igual_syntax(history_begin(0), current_token);
            } else {
                // Processar a expressão normalmente
                parse_expressionable(history_begin(0));
            }
        }
        res = 0;
        break;
    case TOKEN_TYPE_SYMBOL:
        // Ignora símbolos como chaves, parênteses, etc.
        token_next();
        res = 0;
        break;
    case TOKEN_TYPE_OPERATOR:
        // Processa operadores como parte de expressões
        parse_expressionable(history_begin(0));
        res = 0;
        break;
    default:
        // Avança o token se não conseguir processar
        token_next();
        res = 0;
        break;
    }
    return res;
}

// Para imprimir árvore e seus nós
void print_node_type(struct node *node)
{
    if (!node)
    {
        printf("NULL");
        return;
    }

    switch (node->type)
    {
    case NODE_TYPE_NUMBER:
        printf("NUMBER (%llu)", node->llnum);
        break;
    case NODE_TYPE_IDENTIFIER:
        printf("IDENTIFIER (%s)", node->sval ? node->sval : "null");
        break;
    case NODE_TYPE_STRING:
        printf("STRING (\"%s\")", node->sval ? node->sval : "null");
        break;
    case NODE_TYPE_EXPRESSION:
        printf("EXPRESSION (op: %s)", node->exp.op ? node->exp.op : "null");
        break;
    case NODE_TYPE_UNARY:
        printf("UNARY (op: %s)", node->exp.op ? node->exp.op : "null");
        break;
    case NODE_TYPE_EXPRESSION_PARENTHESES:
        printf("PAREN_EXPR");
        break;
    case NODE_TYPE_VARIABLE:
        {
            printf("VARIABLE (name: %s, type: %s", 
                   node->var.name ? node->var.name : "null",
                   node->var.type.type_str ? node->var.type.type_str : "null");
            
            // Mostrar informações de array se aplicável
            if (node->var.type.flags & DATATYPE_FLAG_IS_ARRAY) {
                printf(", ARRAY[%zu", node->var.type.size);
                
                // Mostrar dimensões adicionais se houver
                struct datatype* current = node->var.type.datatype_secondary;
                while (current && current->size > 0) {
                    printf("][%zu", current->size);
                    current = current->datatype_secondary;
                }
                printf("]");
            }
            
            // Mostrar valor inicial se houver
            if (node->var.val) {
                printf(", initialized");
            }
            
            printf(")");
        }
        break;
    case NODE_TYPE_VARIABLE_LIST:
        printf("VARIABLE_LIST (count: %d)", 
               node->var_list.list ? vector_count(node->var_list.list) : 0);
        break;
    case NODE_TYPE_STRUCT:
        printf("STRUCT (name: %s)", node->sval ? node->sval : "null");
        break;
    case NODE_TYPE_STATEMENT_IF:
        printf("IF_STATEMENT (nova sintaxe)");
        break;
    case NODE_TYPE_STATEMENT_RETURN:
        printf("RETURN_STATEMENT");
        break;
    default:
        printf("UNKNOWN (type %d)", node->type);
        break;
    }
}

void print_node_tree(struct node *node, int indent, bool is_last)
{
    if (!node)
    {
        for (int i = 0; i < indent; ++i)
        {
            printf("│   ");
        }
        printf("%sNULL\n", is_last ? "└── " : "├── ");
        return;
    }

    for (int i = 0; i < indent; ++i)
    {
        printf("│   ");
    }

    printf("%s", is_last ? "└── " : "├── ");
    print_node_type(node);
    printf("\n");

    // Imprimir filhos baseado no tipo do nó
    switch (node->type)
    {
        case NODE_TYPE_EXPRESSION:
        case NODE_TYPE_UNARY:
        case NODE_TYPE_EXPRESSION_PARENTHESES:
            print_node_tree(node->exp.left, indent + 1, false);
            print_node_tree(node->exp.right, indent + 1, true);
            break;
            
        case NODE_TYPE_VARIABLE:
            if (node->var.val)
            {
                print_node_tree(node->var.val, indent + 1, true);
            }
            break;
            
        case NODE_TYPE_VARIABLE_LIST:
            if (node->var_list.list)
            {
                for (int i = 0; i < vector_count(node->var_list.list); i++)
                {
                    struct node** var_ptr = vector_at(node->var_list.list, i);
                    if (var_ptr && *var_ptr)
                    {
                        print_node_tree(*var_ptr, indent + 1, i == vector_count(node->var_list.list) - 1);
                    }
                }
            }
            break;
            
        case NODE_TYPE_STRUCT:
            // Aqui você pode adicionar a impressão dos membros da struct se necessário
            break;
            
        case NODE_TYPE_STATEMENT_IF:
            // Imprimir informações detalhadas sobre o if
            for (int i = 0; i < indent + 1; ++i)
            {
                printf("│   ");
            }
            printf("├── CONDITION\n");
            if (node->exp.left)
                print_node_tree(node->exp.left, indent + 2, true);

            for (int i = 0; i < indent + 1; ++i)
            {
                printf("│   ");
            }
            printf("├── TRUE_ACTION\n");
            if (node->exp.right)
                print_node_tree(node->exp.right, indent + 2, true);

            for (int i = 0; i < indent + 1; ++i)
            {
                printf("│   ");
            }
            printf("└── FALSE_ACTION\n");
            if (node->any)
                print_node_tree((struct node*)node->any, indent + 2, true);
            break;
    }
}

int parse(struct compile_process *process)
{
    // Retorna caso tenha algum erro
    if (!process || !process->node_tree_vec || !process->token_vec || !process->node_vec)
    {
        return PARSE_GENERAL_ERROR;
    }
    current_process = process;
    parser_last_token = NULL;
    node_set_vector(process->node_vec, process->node_tree_vec);
    vector_set_peek_pointer(process->token_vec, 0);

    // Imprime arvore de nodes
    printf("\n\nArvore de nodes:\n");
    
    // Contador de segurança para evitar loop infinito
    int max_iterations = 1000; // Número máximo de iterações
    int iteration_count = 0;
    
    // Processa cada token até o final
    while (iteration_count < max_iterations)
    {
        struct token* token = token_peek_next();
        if (!token) break;

        // Processa o próximo token
        if (parse_next() != 0) break;

        // Obtém o nó criado
        struct node* node = node_peek();
        if (!node)
        {
            printf("Warning: node_peek() returned NULL (iteração %d)\n", iteration_count);
            // Incrementa o contador mesmo quando node_peek retorna NULL
            iteration_count++;
            continue;
        }

        // Adiciona o nó à árvore
        vector_push(process->node_tree_vec, &node);

        // Imprime a árvore
        print_node_tree(node, 0, true);
        printf("\n\n");
        
        iteration_count++;
    }

    if (iteration_count >= max_iterations) {
        printf("Aviso: Número máximo de iterações atingido. Possível loop infinito detectado.\n");
    }

    return PARSE_ALL_OK;
}

void parser_datatype_adjust_size_for_secondary(struct datatype* datatype, struct token* datatype_secondary_token) {
    if (!datatype_secondary_token) return;
    struct datatype* secondary_data_type = calloc(1, sizeof(struct datatype));
    parser_datatype_init_type_and_size_for_primitive(datatype_secondary_token, NULL, secondary_data_type);
    datatype->size += secondary_data_type->size;
    datatype->datatype_secondary = secondary_data_type;
    datatype->flags |= DATATYPE_FLAG_IS_SECONDARY;
}

void parser_datatype_init_type_and_size_for_primitive(struct token* datatype_token, struct token* datatype_secondary_token, struct datatype* datatype_out) {
    if (!parser_datatype_is_secondary_allowed_for_type(datatype_token->sval) && datatype_secondary_token) {
        compiler_error(current_process, "Voce utilizou um datatype secundario invalido!\n");
    }
    if (S_EQ(datatype_token->sval, "void")) {
        datatype_out->type = DATATYPE_VOID;
        datatype_out->size = 0;
    } else if (S_EQ(datatype_token->sval, "char")) {
        datatype_out->type = DATATYPE_CHAR;
        datatype_out->size = 1; // 1 BYTE
    } else if (S_EQ(datatype_token->sval, "short")) {
        datatype_out->type = DATATYPE_SHORT;
        datatype_out->size = 2; // 2 BYTES
    } else if (S_EQ(datatype_token->sval, "int")) {
        datatype_out->type = DATATYPE_INTEGER;
        datatype_out->size = 4; // 4 BYTES
    } else if (S_EQ(datatype_token->sval, "long")) {
        datatype_out->type = DATATYPE_LONG;
        datatype_out->size = 8; // 8 BYTES
    } else if (S_EQ(datatype_token->sval, "float")) {
        datatype_out->type = DATATYPE_FLOAT;
        datatype_out->size = 4; // 4 BYTES
    } else if (S_EQ(datatype_token->sval, "double")) {
        datatype_out->type = DATATYPE_DOUBLE;
        datatype_out->size = 8; // 8 BYTES
    } else {
        compiler_error(current_process, "BUG: Datatype invalido!\n");
    }
    parser_datatype_adjust_size_for_secondary(datatype_out, datatype_secondary_token);
}

void parser_datatype_init_type_and_size(struct token* datatype_token, struct token* datatype_secondary_token, struct datatype* datatype_out, int pointer_depth, int expected_type) {
    if (!(expected_type == DATATYPE_EXPECT_PRIMITIVE) && datatype_secondary_token) {
        compiler_error(current_process, "Voce utilizou um datatype secundario invalido!\n");
    }
    switch (expected_type) {
        case DATATYPE_EXPECT_PRIMITIVE:
            parser_datatype_init_type_and_size_for_primitive(datatype_token, datatype_secondary_token, datatype_out);
            break;
        case DATATYPE_EXPECT_STRUCT:
        case DATATYPE_EXPECT_UNION:
            compiler_error(current_process, "Struct e Unions ainda nao estao implementados!\n");
            break;
        default:
            compiler_error(current_process, "BUG: Erro desconhecido!\n");
    }
}

void parser_datatype_init(struct token* datatype_token, struct token* datatype_secondary_token, struct datatype* datatype_out, int pointer_depth, int expected_type) {
    parser_datatype_init_type_and_size(datatype_token, datatype_secondary_token, datatype_out, pointer_depth, expected_type);
    datatype_out->type_str = datatype_token->sval;
}

void parse_datatype_type(struct datatype* dtype) {
    struct token* datatype_token = NULL;
    struct token* datatype_secundary_token = NULL;
    parser_get_datatype_tokens(&datatype_token, &datatype_secundary_token);
    
    if (!datatype_token) {
        compiler_error(current_process, "Esperado tipo após modificadores\n");
        return;
    }

    int expected_type = parser_datatype_expected_for_type_string(datatype_token->sval);
    if (S_EQ(datatype_token->sval, "union") || S_EQ(datatype_token->sval, "struct")) {
        // Caso da struct com nome.
        struct token* next_token = token_peek_next();
        if (!next_token) {
            compiler_error(current_process, "Esperado identificador após struct/union\n");
            return;
        }
        if (next_token->type == TOKEN_TYPE_IDENTIFIER) {
            datatype_token = token_next();
        } else { // Caso da struct sem nome -> gerar nome aleatorio.
            datatype_token = parser_build_random_type_name();
            if (!datatype_token) {
                compiler_error(current_process, "Falha ao gerar nome para tipo anônimo\n");
                return;
            }
            dtype->flags |= DATATYPE_FLAG_IS_STRUCT_UNION_NO_NAME;
        }
    }
    // Descobre a quantidade de ponteiros.
    int pointer_depth = parser_get_pointer_depth();
    parser_datatype_init(datatype_token, datatype_secundary_token, dtype, pointer_depth, expected_type);
}

void parse_datatype_modifiers(struct datatype* dtype) {
    struct token* token = token_peek_next();
    if (!token) return;

    while (token && token->type == TOKEN_TYPE_KEYWORD) {
        if (!is_keyword_variable_modifier(token->sval)) break;
        if (S_EQ(token->sval, "signed")) {
            dtype->flags |= DATATYPE_FLAG_IS_SIGNED;
        } else if (S_EQ(token->sval, "unsigned")) {
            dtype->flags &= ~DATATYPE_FLAG_IS_SIGNED;
        } else if (S_EQ(token->sval, "static")) {
            dtype->flags |= DATATYPE_FLAG_IS_STATIC;
        } else if (S_EQ(token->sval, "const")) {
            dtype->flags |= DATATYPE_FLAG_IS_CONST;
        } else if (S_EQ(token->sval, "extern")) {
            dtype->flags |= DATATYPE_FLAG_IS_EXTERN;
        } else if (S_EQ(token->sval, "__ignore_typecheck__")) {
            dtype->flags |= DATATYPE_FLAG_IS_IGNORE_TYPE_CHECKING;
        }
        token_next();
        token = token_peek_next();
    }
}

void parse_datatype(struct datatype* dtype) {
    memset(dtype, 0, sizeof(struct datatype));
    // Flag padrao.
    dtype->flags |= DATATYPE_FLAG_IS_SIGNED;
    parse_datatype_modifiers(dtype);
    parse_datatype_type(dtype);
    parse_datatype_modifiers(dtype);
}

void parse_variable_function_or_struct_union(struct history* history) {
    struct datatype dtype;
    parse_datatype(&dtype);
    struct token* name_token = token_next();
    if (name_token->type != TOKEN_TYPE_IDENTIFIER) {
        compiler_error(current_process, "Variavel declarada sem nome!\n");
        return;
    }
    parse_variable(&dtype, name_token, history);
}

// Nova função para processar declarações usando a sintaxe "igual"
void parse_variable_igual_syntax(struct history* history, struct token* name_token) {
    // Verificar se o token do nome é válido
    if (name_token->type != TOKEN_TYPE_IDENTIFIER) {
        compiler_error(current_process, "Esperado nome da variável antes de 'igual'\n");
        return;
    }
    
    // Segundo token deve ser "igual"
    struct token* igual_token = token_next();
    if (!token_is_keyword(igual_token, "igual")) {
        compiler_error(current_process, "Esperado 'igual' após nome da variável\n");
        return;
    }
    
    // Terceiro token deve ser o tipo de dados
    struct token* type_token = token_next();
    if (!keyword_is_datatype(type_token->sval)) {
        compiler_error(current_process, "Esperado tipo de dados após 'igual'\n");
        return;
    }
    
    // Criar o datatype baseado no tipo especificado
    struct datatype dtype;
    memset(&dtype, 0, sizeof(struct datatype));
    
    if (S_EQ(type_token->sval, "int")) {
        dtype.type = DATATYPE_INTEGER;
        dtype.type_str = "int";
        dtype.size = sizeof(int);
    } else if (S_EQ(type_token->sval, "float")) {
        dtype.type = DATATYPE_FLOAT;
        dtype.type_str = "float";
        dtype.size = sizeof(float);
    } else if (S_EQ(type_token->sval, "double")) {
        dtype.type = DATATYPE_DOUBLE;
        dtype.type_str = "double";
        dtype.size = sizeof(double);
    } else if (S_EQ(type_token->sval, "char")) {
        dtype.type = DATATYPE_CHAR;
        dtype.type_str = "char";
        dtype.size = sizeof(char);
    } else if (S_EQ(type_token->sval, "bool")) {
        dtype.type = DATATYPE_INTEGER; // bool é tratado como int
        dtype.type_str = "bool";
        dtype.size = sizeof(int);
    } else if (S_EQ(type_token->sval, "void")) {
        dtype.type = DATATYPE_VOID;
        dtype.type_str = "void";
        dtype.size = 0;
    } else {
        compiler_error(current_process, "Tipo de dados não suportado: %s\n", type_token->sval);
        return;
    }
    
    // Processar a variável normalmente
    parse_variable(&dtype, name_token, history);
}

void parse_keyword(struct history *history) { 
    struct token* token = token_peek_next();
    if (!token) return;

    if (is_keyword_variable_modifier(token->sval) || keyword_is_datatype(token->sval)) {
        // Cria um novo node para a declaração
        struct node* node = node_create(&(struct node){
            .type = NODE_TYPE_VARIABLE,
            .pos = token->pos
        });
        
        if (!node) {
            compiler_error(current_process, "Falha ao criar node para declaração\n");
            return;
        }

        // Parse o datatype
        struct datatype dtype;
        parse_datatype(&dtype);
        
        // Armazena o datatype no node
        node->any = malloc(sizeof(struct datatype));
        if (!node->any) {
            compiler_error(current_process, "Falha ao alocar memória para datatype\n");
            return;
        }
        memcpy(node->any, &dtype, sizeof(struct datatype));

        // Adiciona o node à pilha
        node_push(node);
        return;
    }

    // TODO: Implementar outros tipos de keywords (if, while, etc)
    compiler_error(current_process, "Keyword não implementada: %s\n", token->sval);
}

static bool token_next_is_operator(const char* op) {
    struct token* token = token_peek_next();
    return token_is_operator(token, op);
}

void parse_expressionable_root(struct history* history) {
    parse_expressionable(history);
    struct node* result_node = node_pop();
    if (result_node) {
        node_push(result_node);
    }
}

void parse_variable(struct datatype* dtype, struct token* name_token, struct history* history) {
    // Criar lista de variáveis usando a estrutura varlist
    struct node var_list_node = {
        .type = NODE_TYPE_VARIABLE_LIST,
        .var_list.list = vector_create(sizeof(struct node*))
    };
    
    if (!var_list_node.var_list.list) {
        compiler_error(current_process, "Falha ao criar lista de variáveis\n");
        return;
    }
    
    // Adicionar a primeira variável
    struct node* value_node = NULL;
    int array_dims[8] = {0}; // Suporta até 8 dimensões
    int array_dim_count = 0;
    
    // Processa colchetes para arrays
    while (token_next_is_operator("[")) {
        token_next(); // Consome o '['
        struct token* size_token = token_next();
        if (size_token->type != TOKEN_TYPE_NUMBER) {
            compiler_error(current_process, "Esperado número como tamanho do array\n");
            break;
        }
        array_dims[array_dim_count++] = size_token->inum;
        struct token* close_bracket = token_next();
        if (!token_is_operator(close_bracket, "]")) {
            compiler_error(current_process, "Esperado ']' após tamanho do array\n");
            break;
        }
    }
    
    if (token_next_is_operator("=")) {
        token_next();
        parse_expressionable_root(history);
        value_node = node_pop();
    }
    
    // Criar node para a primeira variável
    make_variable_node(dtype, name_token, value_node);
    struct node* var_node = node_pop();
    if (!var_node) {
        compiler_error(current_process, "Falha ao criar node para variável\n");
        vector_free(var_list_node.var_list.list);
        return;
    }
    // Armazenar as dimensões do array no node
    if (array_dim_count > 0) {
        var_node->var.type.flags |= DATATYPE_FLAG_IS_ARRAY;
        var_node->var.type.pointer_depth = array_dim_count;
        for (int i = 0; i < array_dim_count; i++) {
            // Usar o campo size para a primeira dimensão, e o campo datatype_secondary para as demais
            if (i == 0) {
                var_node->var.type.size = array_dims[0];
            } else {
                // Para múltiplas dimensões, pode-se criar uma cadeia de datatypes secundários
                if (!var_node->var.type.datatype_secondary) {
                    var_node->var.type.datatype_secondary = calloc(1, sizeof(struct datatype));
                }
                var_node->var.type.datatype_secondary->size = array_dims[i];
            }
        }
    }
    vector_push(var_list_node.var_list.list, &var_node);
    
    // Verificar se há mais variáveis (separadas por vírgula)
    while (token_next_is_operator(",")) {
        token_next();
        struct token* next_name_token = token_next();
        if (next_name_token->type != TOKEN_TYPE_IDENTIFIER) {
            compiler_error(current_process, "Esperado identificador após vírgula\n");
            break;
        }
        // Processa colchetes para arrays
        array_dim_count = 0;
        while (token_next_is_operator("[")) {
            token_next();
            struct token* size_token = token_next();
            if (size_token->type != TOKEN_TYPE_NUMBER) {
                compiler_error(current_process, "Esperado número como tamanho do array\n");
                break;
            }
            array_dims[array_dim_count++] = size_token->inum;
            struct token* close_bracket = token_next();
            if (!token_is_operator(close_bracket, "]")) {
                compiler_error(current_process, "Esperado ']' após tamanho do array\n");
                break;
            }
        }
        value_node = NULL;
        if (token_next_is_operator("=")) {
            token_next();
            parse_expressionable_root(history);
            value_node = node_pop();
        }
        make_variable_node(dtype, next_name_token, value_node);
        var_node = node_pop();
        if (!var_node) {
            compiler_error(current_process, "Falha ao criar node para variável\n");
            break;
        }
        if (array_dim_count > 0) {
            var_node->var.type.flags |= DATATYPE_FLAG_IS_ARRAY;
            var_node->var.type.pointer_depth = array_dim_count;
            for (int i = 0; i < array_dim_count; i++) {
                if (i == 0) {
                    var_node->var.type.size = array_dims[0];
                } else {
                    if (!var_node->var.type.datatype_secondary) {
                        var_node->var.type.datatype_secondary = calloc(1, sizeof(struct datatype));
                    }
                    var_node->var.type.datatype_secondary->size = array_dims[i];
                }
            }
        }
        vector_push(var_list_node.var_list.list, &var_node);
    }
    
    struct token* semicolon = token_next();
    if (!token_is_symbol(semicolon, ';')) {
        compiler_error(current_process, "Esperado ';' após declaração de variáveis\n");
        vector_free(var_list_node.var_list.list);
        return;
    }
    
    struct node* created_node = node_create(&var_list_node);
    if (!created_node) {
        compiler_error(current_process, "Falha ao criar node da lista de variáveis\n");
        vector_free(var_list_node.var_list.list);
        return;
    }
    for (int i = 0; i < vector_count(var_list_node.var_list.list); i++) {
        struct node** var_ptr = vector_at(var_list_node.var_list.list, i);
        if (!var_ptr || !*var_ptr) continue;
        struct token temp_token = {
            .type = TOKEN_TYPE_IDENTIFIER,
            .sval = (*var_ptr)->var.name,
            .pos = (*var_ptr)->pos
        };
        make_variable_node_and_register(history, dtype, &temp_token, (*var_ptr)->var.val);
    }
    node_push(created_node);
}

void make_variable_node_and_register(struct history* history, struct datatype* dtype, struct token* name_token, struct node* value_node) {
    make_variable_node(dtype, name_token, value_node);
    struct node* var_node = node_pop();
    // 1 - Calcular o escopo offset
    // 2 - Adicionar a variavel no escopo correto
    node_push(var_node);
}

// Versão simplificada de make_variable_node que não chama parse_datatype
void make_variable_node_simple(struct datatype* dtype, struct token* name_token, struct node* value_node) {
    if (!name_token || !name_token->sval) {
        compiler_error(current_process, "Token de nome inválido em make_variable_node_simple\n");
        return;
    }
    char* name_str = malloc(strlen(name_token->sval) + 1);
    if (!name_str) {
        compiler_error(current_process, "Falha ao alocar memória para nome da variável\n");
        return;
    }
    strcpy(name_str, name_token->sval);
    node_create(&(struct node){.type = NODE_TYPE_VARIABLE, .var.name = name_str, .var.type = *dtype, .var.val = value_node});
}

void make_variable_node(struct datatype* dtype, struct token* name_token, struct node* value_node) {
    const char* name_str = NULL;
    if (name_token) name_str = name_token->sval;
    node_create(&(struct node){.type = NODE_TYPE_VARIABLE, .var.name = name_str, .var.type = *dtype, .var.val = value_node});
}

// Função para processar structs
void parse_struct(struct history* history) {
    printf("Processando struct:\n");
    struct token* struct_name = token_next();
    if (struct_name->type != TOKEN_TYPE_IDENTIFIER) {
        compiler_error(current_process, "Nome da struct esperado\n");
        return;
    }
    printf("Nome da struct: %s\n", struct_name->sval);
    
    // Verificar se é a nova sintaxe ou a sintaxe tradicional
    struct token* next_token = token_peek_next();
    if (next_token && token_is_operator(next_token, "/")) {
        // Nova sintaxe: struct ABC /int A\ /int B\ /int C\;
        parse_struct_nova_sintaxe(history, struct_name);
    } else {
        // Sintaxe tradicional: struct ABC { ... };
        parse_struct_tradicional(history, struct_name);
    }
}

// Nova função para processar structs com a sintaxe tradicional
void parse_struct_tradicional(struct history* history, struct token* struct_name) {
    struct token* open_brace = token_next();
    if (!token_is_symbol(open_brace, '{')) {
        compiler_error(current_process, "Esperado '{' após nome da struct\n");
        return;
    }
    
    struct vector* members = vector_create(sizeof(struct node*));
    if (!members) {
        compiler_error(current_process, "Falha ao criar lista de membros da struct\n");
        return;
    }
    
    int member_count = 0;
    while (1) {
        struct token* next_token = token_peek_next();
        if (!next_token) {
            compiler_error(current_process, "Struct não foi fechada corretamente\n");
            break;
        }
        
        if (token_is_symbol(next_token, '}')) {
            token_next(); // Consome o '}'
            break;
        }
        
        if (!keyword_is_datatype(next_token->sval)) {
            compiler_error(current_process, "Esperado tipo de dados para membro da struct\n");
            break;
        }
        
        struct datatype member_type;
        parse_datatype(&member_type);
        
        struct token* member_name = token_next();
        if (member_name->type != TOKEN_TYPE_IDENTIFIER) {
            compiler_error(current_process, "Esperado nome do membro da struct\n");
            break;
        }
        
        make_variable_node_simple(&member_type, member_name, NULL);
        struct node* member_node = node_pop();
        if (!member_node) {
            compiler_error(current_process, "Falha ao criar node para membro da struct\n");
            break;
        }
        
        // Verificar se o próximo token é ';' (fim da struct)
        struct token* next_after_member = token_peek_next();
        if (next_after_member && token_is_symbol(next_after_member, ';')) {
            vector_push(members, &member_node);
            member_count++;
            printf("Membro adicionado: %s %s\n", member_type.type_str, member_name->sval);
            token_next(); // Consome o ';'
            break;
        }
        
        vector_push(members, &member_node);
        member_count++;
        printf("Membro adicionado: %s %s\n", member_type.type_str, member_name->sval);
    }
    
    struct token* struct_semicolon = token_next();
    if (!token_is_symbol(struct_semicolon, ';')) {
        compiler_error(current_process, "Esperado ';' após struct\n");
        vector_free(members);
        return;
    }
    
    struct node struct_node = {
        .type = NODE_TYPE_STRUCT,
        .sval = struct_name->sval,
        .pos = struct_name->pos
    };
    
    struct node* created_struct = node_create(&struct_node);
    if (!created_struct) {
        compiler_error(current_process, "Falha ao criar node da struct\n");
        vector_free(members);
        return;
    }
    
    printf("Struct criada com %d membros\n", member_count);
    node_push(created_struct);
    return;
}

// Nova função para processar structs com a nova sintaxe
void parse_struct_nova_sintaxe(struct history* history, struct token* struct_name) {
    struct vector* members = vector_create(sizeof(struct node*));
    if (!members) {
        compiler_error(current_process, "Falha ao criar lista de membros da struct\n");
        return;
    }
    
    int member_count = 0;
    while (1) {
        // Ignorar quebras de linha e comentários
        struct token* slash_open = token_next();
        while (slash_open && (slash_open->type == TOKEN_TYPE_NEWLINE || slash_open->type == TOKEN_TYPE_COMMENT)) {
            slash_open = token_next();
        }
        if (!slash_open) break;
        if (token_is_symbol(slash_open, ';')) {
            // Fim da struct logo após o último membro
            break;
        }
        if (!token_is_operator(slash_open, "/")) {
            compiler_error(current_process, "Esperado '/' para iniciar membro da struct\n");
            break;
        }
        // Ler o tipo de dados
        struct token* type_token = token_next();
        if (!keyword_is_datatype(type_token->sval)) {
            compiler_error(current_process, "Esperado tipo de dados após '/'\n");
            break;
        }
        
        // Criar o datatype
        struct datatype member_type;
        memset(&member_type, 0, sizeof(struct datatype));
        
        if (S_EQ(type_token->sval, "int")) {
            member_type.type = DATATYPE_INTEGER;
            member_type.type_str = "int";
            member_type.size = sizeof(int);
        } else if (S_EQ(type_token->sval, "float")) {
            member_type.type = DATATYPE_FLOAT;
            member_type.type_str = "float";
            member_type.size = sizeof(float);
        } else if (S_EQ(type_token->sval, "double")) {
            member_type.type = DATATYPE_DOUBLE;
            member_type.type_str = "double";
            member_type.size = sizeof(double);
        } else if (S_EQ(type_token->sval, "char")) {
            member_type.type = DATATYPE_CHAR;
            member_type.type_str = "char";
            member_type.size = sizeof(char);
        } else if (S_EQ(type_token->sval, "bool")) {
            member_type.type = DATATYPE_INTEGER;
            member_type.type_str = "bool";
            member_type.size = sizeof(int);
        } else {
            compiler_error(current_process, "Tipo de dados não suportado: %s\n", type_token->sval);
            break;
        }
        
        // Ler o nome do membro
        struct token* member_name = token_next();
        if (!member_name) {
            compiler_error(current_process, "member_name é NULL\n");
            break;
        }
        
        // Verificar se é um ponteiro
        if (member_name->type == TOKEN_TYPE_OPERATOR && S_EQ(member_name->sval, "*")) {
            // É um ponteiro, ler o nome do membro
            member_type.pointer_depth = 1;
            member_name = token_next();
            if (!member_name || member_name->type != TOKEN_TYPE_IDENTIFIER) {
                compiler_error(current_process, "Esperado nome do membro após '*'\n");
                break;
            }
        }
        
        if (member_name->type != TOKEN_TYPE_IDENTIFIER) {
            compiler_error(current_process, "Esperado nome do membro da struct\n");
            break;
        }
        
        // Processar arrays se houver
        struct datatype* current_dtype = &member_type;
        while (token_next_is_operator("[")) {
            struct token* abre = token_next(); // Consome o '['
            struct token* size_token = token_next();
            if (size_token->type != TOKEN_TYPE_NUMBER) {
                compiler_error(current_process, "Esperado número como tamanho do array\n");
                break;
            }
            current_dtype->flags |= DATATYPE_FLAG_IS_ARRAY;
            current_dtype->size = size_token->inum;
            struct token* close_bracket = token_next();
            if (!token_is_operator(close_bracket, "]")) {
                compiler_error(current_process, "Esperado ']' após tamanho do array\n");
                break;
            }
            // Se houver mais dimensões, criar datatype_secondary
            if (token_next_is_operator("[")) {
                current_dtype->datatype_secondary = calloc(1, sizeof(struct datatype));
                current_dtype = current_dtype->datatype_secondary;
                memset(current_dtype, 0, sizeof(struct datatype));
                current_dtype->type = member_type.type;
                current_dtype->type_str = member_type.type_str;
                current_dtype->size = 0;
            }
        }
        
        // Criar o node do membro
        make_variable_node_simple(&member_type, member_name, NULL);
        struct node* member_node = node_pop();
        if (!member_node) {
            compiler_error(current_process, "Falha ao criar node para membro da struct\n");
            break;
        }
        vector_push(members, &member_node);
        member_count++;
        printf("Membro adicionado: %s %s\n", member_type.type_str, member_name->sval);
        
        // Agora, garantir que o próximo token seja '\\' e consumi-lo
        struct token* slash_close = token_peek_next();
        if (!slash_close) {
            compiler_error(current_process, "token_peek_next() retornou NULL\n");
            break;
        }
        
        if (slash_close->type != TOKEN_TYPE_SYMBOL || slash_close->cval != '\\') {
            compiler_error(current_process, "Esperado '\\' para fechar membro da struct, encontrado: %c\n", slash_close->cval);
            break;
        }
        
        slash_close = token_next(); // Consome o '\\'
        
        // Ignorar quebras de linha e comentários após o fechamento
        struct token* next_token = token_peek_next();
        while (next_token && (next_token->type == TOKEN_TYPE_NEWLINE || next_token->type == TOKEN_TYPE_COMMENT)) {
            token_next();
            next_token = token_peek_next();
        }
        
        // Verificar o próximo token após ignorar quebras de linha/comentários
        if (next_token && token_is_operator(next_token, "/")) {
            // Há mais membros, continuar o loop
            continue;
        }
        if (next_token && token_is_symbol(next_token, ';')) {
            // Fim da struct
            token_next(); // Consome o ';'
            break;
        }
        if (next_token) {
            compiler_error(current_process, "Esperado '/' para novo membro ou ';' para finalizar struct\n");
            break;
        }
        // Se não há próximo token, finalizar a struct
        break;
    }
    
    struct node struct_node = {
        .type = NODE_TYPE_STRUCT,
        .sval = struct_name->sval,
        .pos = struct_name->pos
    };
    
    struct node* created_struct = node_create(&struct_node);
    if (!created_struct) {
        compiler_error(current_process, "Falha ao criar node da struct\n");
        vector_free(members);
        return;
    }
    
    printf("Struct criada com %d membros\n", member_count);
    node_push(created_struct);
    return;
}

// Função auxiliar para verificar se um token é parênteses
static bool token_is_parenthesis(struct token* token, char paren) {
    if (!token) return false;
    
    if (token->type == TOKEN_TYPE_OPERATOR && S_EQ(token->sval, &paren)) {
        return true;
    }
    
    if (token->type == TOKEN_TYPE_SYMBOL && token->cval == paren) {
        return true;
    }
    
    return false;
}

void parse_if(struct history* history) {
    // Verificar se é a nova sintaxe ou a sintaxe tradicional
    struct token* next_token = token_peek_next();
    if (!next_token) {
        compiler_error(current_process, "Token inesperado após if\n");
        return;
    }
    
    // Para a nova sintaxe, não há parênteses, então vamos processar a condição diretamente
    // e verificar se o próximo token é '?'
    
    // Processar a condição de verdade: ler tokens até encontrar '?'
    parse_expressionable_until_token(history, "?");
    struct node* condition = node_pop();
    if (!condition) {
        compiler_error(current_process, "Falha ao processar a condição do if\n");
        return;
    }
    
    // Verificar o próximo token após a condição
    struct token* after_condition = token_peek_next();
    if (after_condition && token_is_operator(after_condition, "?")) {
        // É a nova sintaxe: if condição ? ação_verdadeiro ? ação_falso;
        // Restaurar a condição na pilha para a nova função processar
        node_push(condition);
        parse_if_nova_sintaxe(history);
    } else {
        // É a sintaxe tradicional: if (condição) { ... }
        // Restaurar a condição na pilha para a função tradicional processar
        node_push(condition);
        parse_if_tradicional(history);
    }
}

void parse_if_tradicional(struct history* history) {
    // A condição já foi processada, apenas pegá-la da pilha
    struct node* condition = node_pop();
    if (!condition) {
        compiler_error(current_process, "Condição do if não encontrada na pilha\n");
        return;
    }
    
    // Verifica parênteses de abertura
    struct token* next_token = token_peek_next();
    if (!next_token) {
        compiler_error(current_process, "Token inesperado após if\n");
        return;
    }
    
    // Verifica se o próximo token é '('
    if (!token_is_parenthesis(next_token, '(')) {
        // Se não encontrou '(', avança para o próximo token
        token_next();
        next_token = token_peek_next();
        if (!next_token || !token_is_parenthesis(next_token, '(')) {
            compiler_error(current_process, "Esperado '(' após if\n");
            return;
        }
    }
    
    // Consome o '('
    token_next();
    
    // Verifica parênteses de fechamento
    struct token* close_paren = token_next();
    if (!close_paren || !token_is_parenthesis(close_paren, ')')) {
        compiler_error(current_process, "Esperado ')' após condição do if\n");
        return;
    }
    
    // Verifica chave de abertura
    struct token* open_brace = token_next();
    if (!open_brace || !token_is_symbol(open_brace, '{')) {
        compiler_error(current_process, "Esperado '{' após condição do if\n");
        return;
    }
    
    // Processa o corpo do if
    struct vector* body_statements = vector_create(sizeof(struct node*));
    if (!body_statements) {
        compiler_error(current_process, "Falha ao criar lista de statements do if\n");
        return;
    }
    
    while (true) {
        struct token* next_token = token_peek_next();
        if (!next_token) break;
        
        // Verifica se é o fechamento do if
        if (token_is_symbol(next_token, '}')) {
            token_next(); // Consome o '}'
            break;
        }
        
        // Processa statement
        if (parse_next() == 0) {
            struct node* statement = node_pop();
            if (statement) {
                vector_push(body_statements, &statement);
            }
        } else {
            // Se não conseguiu processar, avança o token
            token_next();
        }
    }
    
    // Cria o node do if
    struct node if_node = {
        .type = NODE_TYPE_STATEMENT_IF,
        .pos = condition ? condition->pos : (struct pos){0, 0, NULL}
    };
    
    struct node* created_if = node_create(&if_node);
    if (!created_if) {
        compiler_error(current_process, "Falha ao criar node do if\n");
        vector_free(body_statements);
        return;
    }
    
    node_push(created_if);
}

void parse_if_nova_sintaxe(struct history* history) {
    // A condição já foi processada, apenas pegá-la da pilha
    struct node* condition = node_pop();
    if (!condition) {
        compiler_error(current_process, "Condição do if não encontrada na pilha\n");
        return;
    }
    
    // Consumir o primeiro '?'
    struct token* first_question = token_next();
    if (!first_question || !token_is_operator(first_question, "?")) {
        compiler_error(current_process, "Esperado '?' após condição do if\n");
        return;
    }
    
    // Processar a ação se verdadeiro de forma simples
    struct node* true_action = NULL;
    
    // Ler tokens até encontrar o segundo '?'
    char action_buffer[256] = {0};
    bool first_token = true;
    
    while (1) {
        struct token* next_token = token_peek_next();
        if (!next_token) break;
        
        if (token_is_operator(next_token, "?")) {
            break;
        }
        
        // Concatenar tokens para formar a ação completa
        if (!first_token) {
            strcat(action_buffer, " ");
        }
        
        if (next_token->type == TOKEN_TYPE_KEYWORD) {
            strcat(action_buffer, next_token->sval);
        } else if (next_token->type == TOKEN_TYPE_IDENTIFIER) {
            strcat(action_buffer, next_token->sval);
        } else if (next_token->type == TOKEN_TYPE_OPERATOR) {
            strcat(action_buffer, next_token->sval);
        } else if (next_token->type == TOKEN_TYPE_NUMBER) {
            char numbuf[32];
            snprintf(numbuf, sizeof(numbuf), "%llu", next_token->llnum);
            strcat(action_buffer, numbuf);
        }
        
        first_token = false;
        token_next(); // Consumir o token
    }
    
    if (strlen(action_buffer) > 0) {
        true_action = node_create(&(struct node){
            .type = NODE_TYPE_IDENTIFIER,
            .sval = strdup(action_buffer)
        });
    }
    
    if (!true_action) {
        true_action = node_create(&(struct node){
            .type = NODE_TYPE_IDENTIFIER,
            .sval = "ação_verdadeira"
        });
    }
    
    // Consumir o segundo '?'
    struct token* second_question = token_next();
    if (!second_question || !token_is_operator(second_question, "?")) {
        compiler_error(current_process, "Esperado '?' após ação se verdadeiro\n");
        return;
    }
    
    // Processar a ação se falso de forma simples
    struct node* false_action = NULL;
    
    // Ler tokens até encontrar o ';'
    char false_action_buffer[256] = {0};
    first_token = true;
    
    while (1) {
        struct token* next_token = token_peek_next();
        if (!next_token) break;
        
        if (token_is_symbol(next_token, ';')) {
            break;
        }
        
        // Concatenar tokens para formar a ação completa
        if (!first_token) {
            strcat(false_action_buffer, " ");
        }
        
        if (next_token->type == TOKEN_TYPE_KEYWORD) {
            strcat(false_action_buffer, next_token->sval);
        } else if (next_token->type == TOKEN_TYPE_IDENTIFIER) {
            strcat(false_action_buffer, next_token->sval);
        } else if (next_token->type == TOKEN_TYPE_OPERATOR) {
            strcat(false_action_buffer, next_token->sval);
        } else if (next_token->type == TOKEN_TYPE_NUMBER) {
            char numbuf[32];
            snprintf(numbuf, sizeof(numbuf), "%llu", next_token->llnum);
            strcat(false_action_buffer, numbuf);
        }
        
        first_token = false;
        token_next(); // Consumir o token
    }
    
    if (strlen(false_action_buffer) > 0) {
        false_action = node_create(&(struct node){
            .type = NODE_TYPE_IDENTIFIER,
            .sval = strdup(false_action_buffer)
        });
    }
    
    if (!false_action) {
        false_action = node_create(&(struct node){
            .type = NODE_TYPE_IDENTIFIER,
            .sval = "ação_falsa"
        });
    }
    
    // Consumir o ';'
    struct token* semicolon = token_next();
    if (!semicolon || !token_is_symbol(semicolon, ';')) {
        compiler_error(current_process, "Esperado ';' após ação se falso\n");
        return;
    }
    
    // Criar o node do if com a nova sintaxe, armazenando os filhos
    struct node if_node = {
        .type = NODE_TYPE_STATEMENT_IF,
        .pos = condition ? condition->pos : (struct pos){0, 0, NULL},
        .exp = {
            .left = condition,
            .right = true_action,
            .op = NULL
        },
        .any = false_action
    };
    
    struct node* created_if = node_create(&if_node);
    if (!created_if) {
        compiler_error(current_process, "Falha ao criar node do if\n");
        return;
    }
    
    node_push(created_if);
}

// Função para processar expressões até encontrar um token específico
void parse_expressionable_until_token(struct history* history, const char* stop_token) {
    // Primeiro, processar todos os tokens até encontrar o stop_token
    while (1) {
        struct token* next_token = token_peek_next();
        if (!next_token) {
            break;
        }
        
        // Verificar se chegamos ao token de parada
        if (token_is_operator(next_token, stop_token) || token_is_symbol(next_token, stop_token[0])) {
            break;
        }
        
        // Processar o token atual diretamente
        if (next_token->type == TOKEN_TYPE_IDENTIFIER) {
            parse_single_token_to_node();
        } else if (next_token->type == TOKEN_TYPE_NUMBER) {
            parse_single_token_to_node();
        } else if (next_token->type == TOKEN_TYPE_OPERATOR) {
            // Para operadores, apenas consumir por enquanto
            token_next();
        } else {
            // Para outros tipos, apenas consumir
            token_next();
        }
    }
    
    // Agora tentar processar a expressão se houver nós na pilha
    struct node* result = node_peek_or_null();
    if (!result) {
        // Se não há nada na pilha, criar um placeholder
        node_create(&(struct node){
            .type = NODE_TYPE_IDENTIFIER,
            .sval = "expressão_processada"
        });
    }
}