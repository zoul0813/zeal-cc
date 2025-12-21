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
    int x;
    int y;
    int total;

    x = 5;
    y = 3;
    total = 0;

    total = total + add(x, y);        /* 8 */
    total = total + mul(x, y);        /* 15 => 23 */
    total = total + sum_to_n(4);      /* 10 => 33 */
    total = total + count_down(3);    /* 6 => 39 */
    total = total + sum_and_fact(4);  /* 10 + 24 = 34 => 73 */
    total = total + chooser(2, 5);    /* 3 => 76 */

    if (is_even(total) != 0) {
        total = total + 2;
    } else {
        total = total + 1;
    }

    return total;  /* Should return 78 (0x4E) */
}
