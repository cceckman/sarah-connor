int collatz(int n);

int collatz_even(int n) {
    return collatz(n / 2);
}

int collatz_odd(int n) {
    return collatz(3 * n + 1);
}

int collatz(int n) {
    if (n == 1) return 1;

    if (n % 2 == 0) {
        return 1 + collatz_even(n);
    } else {
        return 1 + collatz_odd(n);
    }
}

int main() {
    int x = 5;
    [[clang::noinline]] return collatz(x);
}