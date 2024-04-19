
#include <cstdio>
[[clang::noinline]] int print_or_throw(int i) {
    if(i > 0) { 
        throw i;
    } else {
        return i;
    }
}


int main() {
    int i;
    scanf("%d", &i);
    try {
        printf("%d\n", print_or_throw(i));
    } catch(int i) {
        printf("uh oh: %d\n", i);
    } catch (float f) {
        printf("hey we got a weird result: %f\n", f);
    }
}