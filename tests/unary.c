int main() {
    int x;
    x = 4;
    return -x; /* Expected-fail until unary ops are supported. */
}
