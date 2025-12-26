void put_c(char c) {
    _putchar(c);
    _fflush_stdout();
}

int put_s(char* str) {
    int i = 0;
    while (str[i] != 0) {
        put_c(str[i]);
        i = i + 1;
    }

    return 0xEAEA;
}

int main() {
    char* msg = "Hello World\n";
    int x = put_s(msg);

    return x;
}