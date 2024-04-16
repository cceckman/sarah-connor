
volatile int x = 0;

int get_value() {
    return ++x;
}

int main() {
    x++;
    [[clang::noinline]] x += get_value();
    return x;
}
