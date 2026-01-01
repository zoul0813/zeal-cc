int main() {
    int total = 0;
    int a = 6;
    int b = 3;
    int v;

    v = (a + b);
    if (v != 9) return 0x01;
    total = total + v;
    v = (a - b);
    if (v != 3) return 0x02;
    total = total + v;
    v = (a * b);
    if (v != 18) return 0x03;
    total = total + v;
    v = (a / b);
    if (v != 2) return 0x04;
    total = total + v;
    v = (a % b);
    if (v != 0) return 0x05;
    total = total + v;
    v = ((2 + 3) * 4);
    if (v != 20) return 0x06;
    total = total + v;
    v = (20 / 5);
    if (v != 4) return 0x07;
    total = total + v;
    v = (17 % 5);
    if (v != 2) return 0x08;
    total = total + v;

    if (total != 58) return 0x09;
    return 0x3A; /* Should return 58 (0x3A) */
}
