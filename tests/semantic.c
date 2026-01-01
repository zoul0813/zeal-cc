int test_semantic_goto_scope(void) {
    goto inner;
    {
        int x = 0;
inner:
        x = 1;
    }
    return 0;
}

int main(void) {
    return test_semantic_goto_scope();
}
