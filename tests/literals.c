int main() {
    if (010 != 8) return 1;
    if (0XffU != 255) return 2;
    if (65535u != 0xffff) return 3;
    if ('\0' != 0) return 4;
    return 0xA7;
}
