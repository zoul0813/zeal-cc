int main() {
    int a = 0x5A;
    int b = 0x0F;
    int v;

    v = a & b;
    if (v != 0x000A) return 0x01;
    v = a | b;
    if (v != 0x005F) return 0x02;
    v = a ^ b;
    if (v != 0x0055) return 0x03;
    v = (~b) & 0x00FF;
    if (v != 0x00F0) return 0x04;
    v = (1 << 3);
    if (v != 0x0008) return 0x05;
    v = (0x80 >> 3);
    if (v != 0x0010) return 0x06;
    v = (0x0100 >> 4);
    if (v != 0x0010) return 0x07;

    v = (1 && 0);
    if (v != 0) return 0x08;
    v = (0 || 5);
    if (v != 1) return 0x09;
    v = ((1 << 1) == 2) && ((4 >> 1) == 2);
    if (v != 1) return 0x0A;

    return 0xE4;
}
