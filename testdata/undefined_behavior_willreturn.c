

int definitely_returns(int a, int b) {
    volatile int x = 0;
    while(1) {
        x += a / b;
    }
    return x;
}

int main() {
    volatile int y = definitely_returns(1, 2);
    return 0;
}