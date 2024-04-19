
#include <cstdio>

class Ray {
public:
  [[clang::noinline]] Ray() { printf("Started"); }
  [[clang::noinline]] ~Ray() { printf("Destroyed"); }

private:
  int x = 1;
};

int main() {
  Ray ray;
}