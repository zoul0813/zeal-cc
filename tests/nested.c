int sum(int a, int b) { return a + b; }
int identity(int a) { return a; }
int main() { return sum(11, identity(22)); }
