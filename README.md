# Compilador C

## Turma
- Turma 18h 

## Integrantes
- Luiz Henrique Guimarães Soares - 222022144
- Gabriela Dourado França - 231026821
- Douglas Wilson Vieira dos Santos - 211030596
- Gabriel Santos Pinto - 222037675

## Introdução

Este projeto implementa um **compilador para a linguagem C** desenvolvido em linguagem C. O compilador implementa as principais etapas de compilação tradicionais:

- **Análise Léxica**: Tokenização do código fonte
- **Análise Sintática**: Construção da árvore sintática abstrata (AST)
- **Análise Semântica**: Verificação de tipos e resolução de símbolos
- **Preparação para Geração de Código**: Estruturas implementadas para futura geração

O compilador suporta um subconjunto da linguagem C, incluindo:
- Declarações de variáveis com tipos primitivos (int, char, float, double, etc.)
- Estruturas de controle (if, while, for)
- Expressões aritméticas e lógicas
- Funções
- Estruturas (struct) e uniões (union)
- Arrays e ponteiros

### Exemplo de Código Suportado

```c
int main() {
    int x = 10;
    int y = 20;
    if (x < y) {
        return x + y;
    }
    return x - y;
}
```

## Instalação

### Pré-requisitos
- GCC (GNU Compiler Collection)
- Make

### Compilação

1. Clone o repositório
2. Navegue até a pasta `src`
3. Execute o comando make:

```bash
cd src
make
```

### Execução

Para compilar um arquivo C:

```bash
./main arquivo_entrada.c [arquivo_saida]
```

Exemplo:
```bash
./main exemplo.c saida.out
```

## Exemplos

A pasta `exemplos/` contém diversos programas C que demonstram as funcionalidades suportadas pelo compilador:

### Exemplos Básicos
- **hello_world.c**: Programa básico "Hello World" 
- **fibonacci.c**: Implementação recursiva da sequência de Fibonacci
- **ponteiros.c**: Demonstração de ponteiros e passagem por referência

### Estruturas de Dados
- **estruturas.c**: Uso de structs, arrays e loops
  
### Algoritmos
- **bubble_sort.c**: Algoritmo de ordenação Bubble Sort
- **busca_binaria.c**: Implementação recursiva de busca binária

Para testar os exemplos:
```bash
cd src
make
./main ../exemplos/hello_world.c saida.out
```

## Estrutura do Código

### Módulos Principais

- **main.c**: Ponto de entrada do programa, processa argumentos da linha de comando
- **compiler.h/compiler.c**: Interface principal do compilador, coordena as fases
- **lexer.c**: Implementação da análise léxica (tokenização)
- **lex_process.c**: Gerenciamento do processo de análise léxica
- **parser.c**: Implementação da análise sintática (parsing)
- **node.c**: Manipulação dos nós da árvore sintática abstrata
- **token.c**: Definições e operações sobre tokens
- **scope.c**: Gerenciamento de escopos e tabelas de símbolos
- **symresolver.c**: Resolução de símbolos e análise semântica
- **expressionable.c**: Tratamento de expressões e precedência de operadores
- **output.c**: Preparação para saída/geração de código

### Estruturas de Dados Auxiliares

- **helpers/vector.c**: Implementação de vetor dinâmico
- **helpers/buffer.c**: Gerenciamento de buffers de caracteres

### Etapas de Compilação

1. **Análise Léxica** (lexer.c, lex_process.c): 
   - Tokenização do código fonte
   - Identificação de palavras-chave, operadores, símbolos, números e strings

2. **Análise Sintática** (parser.c, node.c):
   - Construção da árvore sintática abstrata
   - Verificação da gramática da linguagem
   - Tratamento de precedência de operadores

3. **Análise Semântica** (scope.c, symresolver.c):
   - Verificação de tipos
   - Resolução de símbolos
   - Gerenciamento de escopos

4. **Preparação para Geração de Código**:
   - Estruturas definidas para futura implementação
   - Interface preparada para diferentes backends

## Referências

[Adicionar referências utilizadas no desenvolvimento do compilador]

## Bugs/Limitações/Problemas Conhecidos

- Geração de código não implementada - o compilador apenas realiza as fases de análise
- Suporte limitado a algumas construções avançadas de C
- Tratamento de erros pode ser melhorado com mensagens mais descritivas
- Falta implementação completa de algumas verificações semânticas

### Melhorias Futuras

- Implementar geração de código assembly
- Adicionar otimizações básicas
- Melhorar mensagens de erro com contexto mais detalhado
- Expandir suporte para mais construções da linguagem C
- Adicionar testes unitários abrangentes 
