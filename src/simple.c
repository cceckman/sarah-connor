
#include <stdlib.h>

volatile int return_value = 0;

int get_return_value() {
    return return_value;
}

int main(int argc, const char** argv) {
    if(argc > 0) {
        return_value = 1;
    }
    void *x = malloc(2);
    return_value += (size_t)(x);
    int v = get_return_value();
    if(v > 0) {
        return_value = 1;
    }

    return get_return_value();
}