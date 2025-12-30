int test_int_unary() {
    int x;
    int y;
    int u;
    int p;
    int q;
    int r;
    int s;
    int t;

    x = 5;
    y = +x;
    if (y != x) return 0x01;
    if ((-x + x) != 0) return 0x02;

    u = 0x0100;
    y = !u;
    if (y != 0) return 0x03;
    u = 0;
    y = !u;
    if (y != 1) return 0x04;

    p = 1;
    q = p++;
    if (q != 1) return 0x05;
    if (p != 2) return 0x06;
    r = ++p;
    if (r != 3) return 0x07;
    if (p != 3) return 0x08;
    s = p--;
    if (s != 3) return 0x09;
    if (p != 2) return 0x0A;
    t = --p;
    if (t != 1) return 0x0B;
    if (p != 1) return 0x0C;

    return 0;
}

int test_char_unary() {
    char ch;
    int h;

    ch = 0;
    h = !ch;
    if (h != 1) return 0x0D;
    ch = 3;
    h = !ch;
    if (h != 0) return 0x0E;

    ch = 1;
    h = ch++;
    if (h != 1) return 0x0F;
    if (ch != 2) return 0x10;
    h = ++ch;
    if (h != 3) return 0x11;
    if (ch != 3) return 0x12;
    h = ch--;
    if (h != 3) return 0x13;
    if (ch != 2) return 0x14;
    h = --ch;
    if (h != 1) return 0x15;
    if (ch != 1) return 0x16;

    return 0;
}

int main() {
    int result;

    result = test_int_unary();
    if (result) return result;
    result = test_char_unary();
    if (result) return result;
    return 0xAA;
}
