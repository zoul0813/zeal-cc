/* Test while loop */
int main() {
    int x;
    int sum;
    
    x = 0;
    sum = 0;
    
    while (x < 5) {
        sum = sum + x;
        x = x + 1;
    }
    
    return sum;  /* Should return 0+1+2+3+4 = 10 */
}
