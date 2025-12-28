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

char open(char* path, char flag) {
    return _open(path, flag);
}

char close(char dev) {
    return _close(dev);
}

int main() {
    char* msg = "Hello World\n";
    int x = put_s(msg);

    char* path = "h:/tests/zealos.txt";
    char dev = open(path, 0);
    char err = close(dev);

    return x;
}