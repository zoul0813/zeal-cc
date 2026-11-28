int main() {
    signed char c = 255;
    int prior = 1234;
    signed int value = c;
    if (value == 65535) return 42;
    return 1;
}
