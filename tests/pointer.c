int main() {
    int x;
    int *p;
    char* msg = "Hello World!";
    int sum;

    x = 5;
    p = &x;
    *p = 7;

    sum = msg[0] + msg[5] + msg[6];
    sum = sum + "Hello"[3] + " "[0] + "World!"[5];
    sum = sum + x;

    /* Expected return: 0x73. */
    return sum;
}
