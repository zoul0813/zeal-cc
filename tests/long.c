int main() {
    long a;
    long b;
    long sum;
    a = 0xFFFF;
    b = 2;
    sum = a + b; /* 0x0000FFFF + 0x00000002 = 0x00010001 */
    sum = sum - 0x10000;
    return sum; /* Expected return: 1 (0x01). */
}
