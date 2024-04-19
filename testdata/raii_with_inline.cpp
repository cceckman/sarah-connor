
#include <cstdio>

class Ray {
public:
  [[clang::noinline]] Ray() { printf("Started"); }
  [[clang::noinline]] ~Ray() { printf("Destroyed"); }

private:
  int x = 1;
};

class InlineRay {
public:
  InlineRay() { printf("Started inline"); }
  ~InlineRay() { printf("Destroyed inline"); }

private:
  int y = 2;

};

int main() {
  InlineRay inl;
  Ray ray;
}