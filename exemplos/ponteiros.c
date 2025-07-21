/* Exemplo 6: Ponteiros e Manipulação de Memória */

void trocar(int *a, int *b) {
    int temp = *a;
    *a = *b;
    *b = temp;
}

int main() {
    int x = 10;
    int y = 20;
    int *ptr_x = &x;
    int *ptr_y = &y;
    
    trocar(ptr_x, ptr_y);
    
    return x + y;  // Deve retornar 30 (valores trocados)
}
