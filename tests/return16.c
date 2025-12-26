int ret16() {
    return 0xBEEF;
}

int main() {
    /* Actual return: 0xBEEF */
    /* Expected return: 0xEF */
    return ret16();
}
