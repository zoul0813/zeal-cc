int main() {
    int x;
    int *p;
    char* msg = "Hello\"how are you\" \\World\\!";
    int sum;

    x = 5;
    p = &x;
    *p = 7;

    sum = msg[0] + msg[5] + msg[6];
    sum = sum + "Hello"[3] + " "[0] + "World!"[5];
    sum = sum + x;

    /* Actual return: 0x186 */
    /* Expected return: 0x86. */
    return sum;
}
