
// "Add !annotation metadata for entries in @llvm.global.annotations, generated using __attribute((annotate("_name"))) on functions in Clang."
//
// Let's see how it works!
// The code says "Only add !annotation metadata if the corresponding remarks pass is also enabled"-
// named "annotation-remarks".
//
// This seems to be talking about https://llvm.org/docs/Remarks.html, so we'd need `pass-remarks`
int __attribute__((annotate("_name"))) get_value()  {
    volatile int  var = 1;
    return var;
}

int main() {
    return get_value();
}
