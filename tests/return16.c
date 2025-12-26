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

    if (a != 0xBEEF) return 0x01;
    if (b != 0xBEEF) return 0x02;
    if (c != 0xBEEF) return 0x03;
    if (d != 0xBEEF) return 0x04;

    /* Actual return: 0xBEEF */
    /* Expected return: 0xEF */
    return d;
}
