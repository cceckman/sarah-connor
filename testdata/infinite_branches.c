
void maybe_stall(int value) {
    if(value) {
      while(1) {}
    } else {
      return;
    }
}

void always_stall(int value) {
    if(value) {
      while(1) {}
    } else {
      while(1) {}
    }
}

int main() {
    maybe_stall(0);
    maybe_stall(1);
    always_stall(1);
    return 0;
}

