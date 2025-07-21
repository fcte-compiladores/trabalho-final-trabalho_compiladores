/* Exemplo 2: Fibonacci - Demonstração de função recursiva */

int fibonacci(int n) {
    if (n <= 1) {
        return n;
    }
    return fibonacci(n - 1) + fibonacci(n - 2);
}

int main() {
    int num = 10;
    int result = fibonacci(num);
    return result;
}
