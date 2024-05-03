
volatile int x = 0;

int bounded_loop() {
    for(int i = 0; i < 10000; i++) {
        x += i;
    }
    return x;
}

int main() {
    int x;
    [[clang::noinline]] x = bounded_loop();
    x += 1;
    return x;
}
