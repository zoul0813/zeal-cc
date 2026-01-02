int test_semantic_types(void) {
    int a[2];
    int* p = a;
    return a[2];
}

int main(void) {
    return test_semantic_types();
}
