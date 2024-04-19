
redo-always

find . >&2 \
    \( \
    -name '*.ll' -or \
    -name '*.deps' -or \
    -name '*.dylib' -or \
    -name '*.dSYM' -or \
    -name 'llvm-dir' -or \
    -name '*.so' -or \
    -name '*.did' -or \
    -name '*.tmp' -or \
    -name '*.svg' -or \
    -name 'compile_flags.txt' \
    \) \
    -print \
    -delete

