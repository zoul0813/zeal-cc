int test_semantic_addr_lvalue(void) {
    int x = 0;
    int* p = &(x + 1);
    return (p != 0);
    return 0;
}

int main(void) {
    int a[4];
    a = 0;
    return test_semantic_addr_lvalue();
}
