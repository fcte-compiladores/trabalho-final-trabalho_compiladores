/* Exemplo 4: Algoritmo de Ordenação - Bubble Sort */

void bubble_sort(int arr[], int n) {
    int i, j, temp;
    
    for (i = 0; i < n - 1; i++) {
        for (j = 0; j < n - i - 1; j++) {
            if (arr[j] > arr[j + 1]) {
                temp = arr[j];
                arr[j] = arr[j + 1];
                arr[j + 1] = temp;
            }
        }
    }
}

int main() {
    int arr[5] = {64, 34, 25, 12, 22};
    int n = 5;
    
    bubble_sort(arr, n);
    
    return 0;
}
