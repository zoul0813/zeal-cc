int main() {
    unsigned int u;
    signed char c;

    u = 255u;
    c = -1;

    if (u != 255u) return 0x01;
    if (c != -1) return 0x02;
    return 0xEE;
}
