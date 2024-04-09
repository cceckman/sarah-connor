
SOURCE="../src/${2}.cpp"
DEPFILE="${2}.deps"

redo-ifchange llvm-dir ../compile_flags.txt "$SOURCE"

LLVM_DIR="$(cat llvm-dir)"
FLAGS="$(cat ../compile_flags.txt)"

"$LLVM_DIR"/bin/clang++ \
    $FLAGS \
    -Wall -fdiagnostics-color=always -fvisibility-inlines-hidden \
    -g -std=gnu++17 -arch arm64 \
    -isysroot /Library/Developer/CommandLineTools/SDKs/MacOSX13.0.sdk \
    -mmacosx-version-min=12.5 -fPIC \
    -D__STDC_CONSTANT_MACROS -D__STDC_FORMAT_MACROS -D__STDC_LIMIT_MACROS \
    --write-user-dependencies -MF"$DEPFILE" \
    -o "$3" \
    -l LLVM \
    -dynamiclib \
    "$SOURCE"

mv "$3".dSYM "$1".dSYM

