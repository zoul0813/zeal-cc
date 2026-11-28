void print_hex(char byte) {
    char *digits = "0123456789ABCDEF";
    putchar(digits[byte >> 4]);
    putchar(digits[byte & 15]);
}

int main() {
    char *path = "sample.dat";
    char dev = open(path, 0);
    char bytes[16];
    int count;
    int i;
    if (dev & 128) {
        char *message = "Cannot open sample.dat\n";
        i = 0;
        while (message[i]) {
            putchar(message[i]);
            i = i + 1;
        }
        return 2;
    }
    while (1) {
        count = 16;
        if (read(dev, bytes, &count)) {
            close(dev);
            return 3;
        }
        if (count == 0) break;
        for (i = 0; i < count; i = i + 1) {
            if (i) putchar(' ');
            print_hex(bytes[i]);
        }
        putchar('\n');
    }
    return close(dev);
}
