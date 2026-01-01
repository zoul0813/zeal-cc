int test_continue(void) {
    int i = 0;
    int sum = 0;

    while (i < 5) {
        i = i + 1;
        if (i == 3) continue;
        sum = sum + i;
    }

    if (sum != 12) return 0x01;
    return 0;
}

int test_break(void) {
    int i = 0;

    while (1) {
        i = i + 1;
        if (i == 4) break;
    }

    if (i != 4) return 0x02;
    return 0;
}

int test_break_nested(void) {
    int i = 0;
    int j = 0;
    int total = 0;

    for (i = 0; i < 3; i = i + 1) {
        for (j = 0; j < 3; j = j + 1) {
            if (j == 1) break;
            total = total + 1;
        }
    }

    if (total != 3) return 0x03;
    return 0;
}

int main() {
    int result = 0;

    result = test_continue();
    if (result) return result;
    result = test_break();
    if (result) return result;
    result = test_break_nested();
    if (result) return result;

    return 0xB1;
}
