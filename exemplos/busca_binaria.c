/* Exemplo 5: Busca Binária - Algoritmo de busca eficiente */

int busca_binaria(int arr[], int l, int r, int x) {
    if (r >= l) {
        int mid = l + (r - l) / 2;
        
        if (arr[mid] == x) {
            return mid;
        }
        
        if (arr[mid] > x) {
            return busca_binaria(arr, l, mid - 1, x);
        }
        
        return busca_binaria(arr, mid + 1, r, x);
    }
    
    return -1;
}

int main() {
    int arr[6] = {2, 3, 4, 10, 40, 50};
    int n = 6;
    int x = 10;
    int resultado = busca_binaria(arr, 0, n - 1, x);
    
    if (resultado == -1) {
        return -1;
    } else {
        return resultado;
    }
}
