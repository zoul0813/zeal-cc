int main() {
    int a[3];

    a[0] = 1;
    a[1] = 2;
    a[2] = 3;
    a[1] = 3;

    return a[0] + a[1] + a[2]; /* Should return 7 (0x07) */
}
