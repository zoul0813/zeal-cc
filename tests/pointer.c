int main() {
    int x;
    int *p;

    x = 5;
    p = &x;
    *p = 7;

    return x; /* Expect 7 once pointers are supported */
}
