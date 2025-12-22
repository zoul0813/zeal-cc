int main() {
    /* Expected return: 'l' + ' ' + '!' = 0x6C + 0x20 + 0x21 = 0xAD. */
    return "Hello"[3] + " "[0] + "World!"[5];
}
