int put_s(char* str) {
    int i = 0;
    while (str[i] != 0) {
        putchar(str[i]);
        i = i + 1;
    }

    fflush_stdout();
    return 0xEAEA;
}

void put_hex_u16(int value) {
    char* digits = "0123456789ABCDEF";
    int pow = 4096;
    int i = 0;
    while (i < 4) {
        int nibble = (value / pow) % 16;
        putchar(digits[nibble]);
        pow = pow / 16;
        i = i + 1;
    }
}

void put_hex_u8(int value) {
    char* digits = "0123456789ABCDEF";
    int v = value;
    int high;
    int low;
    if (v < 0) v = v + 256;
    high = v / 16;
    low = v - (high * 16);
    putchar(digits[high]);
    putchar(digits[low]);
}

int main() {
    char* msg = "Hello World\n";
    int x = put_s(msg);

    char* path = "h:/tests/zealos.txt";
    char dev = open(path, 0);
    char err;
    int size = 32;
    char buffer[33];
    err = read(dev, buffer, &size);
    char* msg_size = "size=0x";
    put_s(msg_size);
    put_hex_u16(size);
    putchar('\n');
    if (err) {
        char* msg_err = "err=0x";
        put_s(msg_err);
        put_hex_u8(err);
        putchar('\n');
    }
    if (size < 32) buffer[size] = 0;
    else buffer[32] = 0;
    err = close(dev);
    putchar('"');
    put_s(buffer);
    putchar('"');
    putchar('\n');
    {
        int i = 0;
        int col = 0;
        while (i < size) {
            put_hex_u8(buffer[i]);
            putchar(' ');
            i = i + 1;
            col = col + 1;
            if (col == 16) {
                putchar('\n');
                col = 0;
            }
        }
        if (col != 0) putchar('\n');
    }

    exit(x);

    return x;
}
