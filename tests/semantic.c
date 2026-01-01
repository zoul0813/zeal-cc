int test_semantic_lvalue(void) {
    int x = 0;
    1 = x;
    return x;
}

int main(void) {
    return test_semantic_lvalue();
}
