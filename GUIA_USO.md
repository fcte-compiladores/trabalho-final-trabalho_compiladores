# Guia de Uso do Compilador C

## Como Usar

### Compilação do Compilador
```bash
cd src
make
```

### Executando o Compilador
```bash
./main <arquivo_entrada.c> [arquivo_saida]
```

### Exemplos de Uso
```bash
# Compilar um arquivo simples
./main ../exemplos/hello_world.c

# Compilar especificando arquivo de saída
./main ../exemplos/fibonacci.c output.out

# Compilar exemplo mais complexo
./main ../exemplos/bubble_sort.c
```

## Saída do Compilador

O compilador produz três tipos de saída:

1. **Lista de Tokens**: Mostra todos os tokens identificados na análise léxica
2. **Árvore de Nós**: Exibe a estrutura da árvore sintática abstrata (AST)
3. **Status de Compilação**: Informa se a compilação foi bem-sucedida

## Tipos de Tokens Reconhecidos

- **KE** (Keyword): Palavras-chave da linguagem C
- **ID** (Identifier): Identificadores de variáveis e funções  
- **OP** (Operator): Operadores aritméticos e lógicos
- **SY** (Symbol): Símbolos como chaves, parênteses, ponto e vírgula
- **NU** (Number): Números inteiros e decimais
- **ST** (String): Strings literais
- **CO** (Comment): Comentários
- **NL** (Newline): Quebras de linha

## Recursos Suportados

- Declaração de variáveis com tipos primitivos
- Funções com parâmetros e retorno
- Estruturas de controle (if, while, for)
- Arrays e ponteiros
- Structs e unions
- Expressões aritméticas e lógicas
- Comentários de linha e bloco

## Limitações Atuais

- Não gera código executável (apenas análise)
- Suporte limitado para algumas construções avançadas de C
- Preprocessamento básico apenas
