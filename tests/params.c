int add(int a, int b) {
    return a + b;
}

int main() {
    int x = 5;
    int y = 10;
    int sum = add(x, y);

    sum = sum + add(2, 3);
    /* Expected return: 20 (0x14). */
    return sum;
}
