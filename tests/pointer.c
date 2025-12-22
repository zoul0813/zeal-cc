int main() {
    int x;
    int *p;

    x = 5;
    p = &x;
    *p = 7;

    return x; /* Expected return: 7 (0x07). */
}
