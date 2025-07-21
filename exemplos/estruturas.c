/* Exemplo 3: Estruturas e Arrays - Demonstração de estruturas de dados */

struct Pessoa {
    char nome[50];
    int idade;
    float altura;
};

int main() {
    struct Pessoa pessoas[3];
    int numeros[5] = {1, 2, 3, 4, 5};
    
    pessoas[0].idade = 25;
    pessoas[0].altura = 1.75;
    
    int soma = 0;
    int i;
    for (i = 0; i < 5; i++) {
        soma = soma + numeros[i];
    }
    
    return soma;
}
