int main() {
    int total;
    total = 0;

    if (3 == 3) {
        total = total + 1;
    }
    if (3 != 4) {
        total = total + 2;
    }
    if (2 < 3) {
        total = total + 4;
    }
    if (3 <= 3) {
        total = total + 8;
    }
    if (4 > 3) {
        total = total + 16;
    }
    if (4 >= 4) {
        total = total + 32;
    }

    return total; /* Should return 63 (0x3F) */
}
