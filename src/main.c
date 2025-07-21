#include <stdio.h>
#include "helpers/vector.h"
#include "helpers/buffer.h"
#include "compiler.h"

int main(int argc, char const *argv[])
{
    char *input_file = NULL;
    char *output_file = NULL;

    printf("Compilador C\n\n");

    if (argc > 1)
    {
        input_file = (char *)argv[1];

        if (argc > 2)
            output_file = (char *)argv[2];
    }

    printf("#Input file: %s \n#Output file: %s \n\n", input_file, output_file);

    int res = compile_file(input_file, output_file, 0);
    if (res == COMPILER_FILE_COMPILED_OK)
    {
        printf("Todos os arquivos foram compilados com sucesso!\n");
    }
    else if (res == COMPILER_FAILED_WITH_ERRORS)
    {
        printf("Erro de compilacao!\n");
    }
    else
    {
        printf("Erro desconhecido!\n");
    }

    return 0;
}
