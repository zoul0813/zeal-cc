int add(int a, int b) {
    return a + b;
}

int mul(int a, int b) {
    return a * b;
}

int is_even(int v) {
    if ((v % 2) == 0) {
        return 1;
    }
    return 0;
}

int chooser(int a, int b) {
    if (a > b) {
        return a - b;
    } else {
        return b - a;
    }
}

int sum_to_n(int n) {
    int sum;
    int i;
    sum = 0;
    for (i = 1; i <= n; i = i + 1) {
        sum = sum + i;
    }
    return sum;
}

int count_down(int n) {
    int acc;
    acc = 0;
    while (n > 0) {
        acc = acc + n;
        n = n - 1;
    }
    return acc;
}

int factorial(int n) {
    if (n <= 1) {
        return 1;
    }
    return n * factorial(n - 1);
}

int sum_and_fact(int n) {
    return sum_to_n(n) + factorial(n);
}

int main() {
    int x = 5;
    int y = 3;
    int total = 0;
    int v;

    v = add(x, y);
    if (v != 8) return 0x01;
    total = total + v;
    v = mul(x, y);
    if (v != 15) return 0x02;
    total = total + v;
    v = sum_to_n(4);
    if (v != 10) return 0x03;
    total = total + v;
    v = count_down(3);
    if (v != 6) return 0x04;
    total = total + v;
    v = sum_and_fact(4);
    if (v != 34) return 0x05;
    total = total + v;
    v = chooser(2, 5);
    if (v != 3) return 0x06;
    total = total + v;

    if (total != 76) return 0x07;
    if (is_even(total) != 0) {
        total = total + 2;
    } else {
        total = total + 1;
    }
    if (total != 78) return 0x08;

    return 0x4E;  /* Should return 78 (0x4E) */
}
