int main() {
    int x;
    x = 0;
    do {
        x = x + 1;
    } while (x < 3);
    return x; /* Expected-fail until do/while is supported. */
}
