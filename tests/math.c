int main() {
    int total;
    int a;
    int b;

    total = 0;
    a = 6;
    b = 3;

    total = total + (a + b);   /* 9 */
    total = total + (a - b);   /* 3 -> 12 */
    total = total + (a * b);   /* 18 -> 30 */
    total = total + (a / b);   /* 2 -> 32 */
    total = total + (a % b);   /* 0 -> 32 */
    total = total + ((2 + 3) * 4); /* 20 -> 52 */
    total = total + (20 / 5);  /* 4 -> 56 */
    total = total + (17 % 5);  /* 2 -> 58 */

    return total; /* Should return 58 (0x3A) */
}
