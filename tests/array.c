int int_array_sum(void) {
    int a[3];

    a[0] = 1;
    a[1] = 2;
    a[2] = 3;

    if (a[0] + a[1] + a[2] != 6) return 0x01;
    return 0;
}

int char_array_element(void) {
    char c[3];

    c[0] = 1;
    c[1] = 2;
    c[2] = 3;

    if (c[2] != 3) return 0x02;
    return 0;
}

int pointer_index(void) {
    int x;
    int* p;

    x = 7;
    p = &x;

    if (p[0] != 7) return 0x03;
    return 0;
}

int string_literal_index(void) {
    char* msg;

    msg = "Hi";
    if (msg[0] != 'H') return 0x04;
    if (msg[1] != 'i') return 0x05;
    if (msg[2] != 0) return 0x06;
    return 0;
}

int string_pointer_index(void) {
    char* msg_ptr;

    msg_ptr = "Yo";
    if (msg_ptr[0] != 'Y') return 0x07;
    if (msg_ptr[1] != 'o') return 0x08;
    return 0;
}

int get_second_int(int a[]) {
    return a[1];
}

int array_param_decay(void) {
    int a[3];

    a[0] = 3;
    a[1] = 4;
    a[2] = 5;

    if (get_second_int(a) != 4) return 0x09;
    return 0;
}

int pointer_arithmetic(void) {
    int a[3];
    int* p;

    a[0] = 7;
    a[1] = 8;
    a[2] = 9;

    p = a;
    if (p[2] != 9) return 0x0A;
    return 0;
}

int int_array_16bit(void) {
    int a[2];

    a[0] = 0x1234;
    if (a[0] != 0x1234) return 0x0B;
    return 0;
}

int main() {
    int result = 0;

    result = int_array_sum();
    if (result) return result;
    result = char_array_element();
    if (result) return result;
    result = pointer_index();
    if (result) return result;
    result = string_literal_index();
    if (result) return result;
    result = string_pointer_index();
    if (result) return result;
    result = array_param_decay();
    if (result) return result;
    result = pointer_arithmetic();
    if (result) return result;
    result = int_array_16bit();
    if (result) return result;

    return 0xEF;
}
