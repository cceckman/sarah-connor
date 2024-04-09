
volatile int return_value = 0;

int main(int argc, const char** argv) {
    if(argc > 0) {
        return_value = 1;
    }
    return return_value;
}