

int factorial(int n) {
    int rec;
    if (n == 1) return n;

    [[clang::noinline]] rec = factorial(n - 1);
    return n * rec;
}

int main() {
    int x = 5;
    [[clang::noinline]] return factorial(x);
}