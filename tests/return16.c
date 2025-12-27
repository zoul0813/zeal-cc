int g_init = 0xBEEF;
int g_copy = 0;

int ret_local() {
    int local = 0xBEEF;
    return local;
}

int ret_param(int value) {
    return value;
}

int ret_global() {
    return g_init;
}

int ret_global_chain() {
    g_copy = g_init;
    return g_copy;
}

int main() {
    int a = ret_param(g_init);
    int b = ret_local();
    int c = ret_global();
    int d = ret_global_chain();
    int sum = 0x0102 + 0x0304;
    int diff = 0x0304 - 0x0102;
    int carry = 0x00FF + 0x0001;
    int borrow = 0x0100 - 0x0001;
    int mul16 = 0x0006 * 0x0007;
    int div16 = 0x0030 / 0x0005;
    int mod16 = 0x0031 % 0x0005;
    int cmp_gt = (0x0100 > 0x00FF);
    int cmp_le = (0x00FF <= 0x0100);

    if (a != 0xBEEF) return 0x01;
    if (b != 0xBEEF) return 0x02;
    if (c != 0xBEEF) return 0x03;
    if (d != 0xBEEF) return 0x04;
    if (sum != 0x0406) return 0x05;
    if (diff != 0x0202) return 0x06;
    if (carry != 0x0100) return 0x07;
    if (borrow != 0x00FF) return 0x08;
    if (mul16 != 0x002A) return 0x09;
    if (div16 != 0x0009) return 0x0A;
    if (mod16 != 0x0004) return 0x0B;
    if (cmp_gt != 1) return 0x0C;
    if (cmp_le != 1) return 0x0D;

    /* Actual return: 0xBEEF */
    /* Expected return: 0xEF */
    return d;
}
