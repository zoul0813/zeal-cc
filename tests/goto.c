int test_goto_forward(void) {
    int x = 0;

    x = 1;
    goto skip;
    x = 2;
skip:
    if (x != 1) return 0x01;
    return 0;
}

int test_goto_backward(void) {
    int x = 0;

start:
    x = x + 1;
    if (x < 3) goto start;
    if (x != 3) return 0x02;
    return 0;
}

int test_goto_over_label(void) {
    int x = 0;

    goto end;
middle:
    x = 5;
end:
    x = x + 1;
    if (x != 1) return 0x03;
    return 0;
}

int main() {
    int result = 0;

    result = test_goto_forward();
    if (result) return result;
    result = test_goto_backward();
    if (result) return result;
    result = test_goto_over_label();
    if (result) return result;

    return 0xB2;
}
