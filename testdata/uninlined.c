
#include <stdio.h>
#include <stdlib.h>

int get_int() __attribute__((noinline)) {
    char buffer[32];
    char *data = fgets(buffer, sizeof(buffer) - 1, stdin);
    int value = atoi(data);
    return value;
}

int get_sum() {
    int count = get_int();
    int sum = 0;
    for(int i = 0; i < count; i++) {
        sum += get_int();
    }
    return sum;
}

int main(int argc, char** argv) {
    int sum = get_sum();
    printf("%d\n", sum);
    return 0;
}