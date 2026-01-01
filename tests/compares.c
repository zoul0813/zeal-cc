int main() {
    if (!(3 == 3)) return 0x01;
    if (!(3 != 4)) return 0x02;
    if (!(2 < 3)) return 0x03;
    if (!(3 <= 3)) return 0x04;
    if (!(4 > 3)) return 0x05;
    if (!(4 >= 4)) return 0x06;

    return 0x3F; /* Should return 63 (0x3F) */
}
