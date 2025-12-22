int main() {
    char* msg = "Hello World!";
    /* Expected return: 'H' + ' ' + 'W' = 0x48 + 0x20 + 0x57 = 0xBF. */
    return msg[0] + msg[5] + msg[6];
}
