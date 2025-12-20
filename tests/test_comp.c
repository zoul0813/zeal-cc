/* Comprehensive test: factorial function */
int factorial(int n) {
    int result;
    result = 1;
    
    while (n > 1) {
        result = result * n;
        n = n - 1;
    }
    
    return result;
}

int main() {
    int x;
    x = 5;
    return factorial(x);  /* Should return 120 (0x78) */
}
