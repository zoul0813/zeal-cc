int test_semantic_types(void) {
    char c = 1;
    int x = c;
    int* p = 0;
    p = 1;
    return x;
}

int main(void) {
    return test_semantic_types();
}
