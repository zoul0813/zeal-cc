void put_c(char c) {
    _putchar(c);
    _fflush_stdout();
}

void put_s(char* str) {
    int i = 0;
    while (str[i] != 0) {
        put_c(str[i]);
        i = i + 1;
    }
}

int main() {
    char* msg = "Hello World\n";
    put_s(msg);
    return 0xEA;
}