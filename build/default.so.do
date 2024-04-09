
SOURCE="../src/${2}.cpp"
DEPFILE="${2}.deps"

redo-ifchange llvm-dir ../compile_flags.txt "$SOURCE"

LLVM_DIR="$(cat llvm-dir)"
FLAGS="$(cat ../compile_flags.txt)"

"$LLVM_DIR"/bin/clang++ \
    $FLAGS \
    -Wall -fdiagnostics-color=always -fvisibility-inlines-hidden \
    -g -std=gnu++17 -fPIC \
    --write-user-dependencies -MF"$DEPFILE" \
    -o "$3" \
    -l LLVM \
    -shared \
    "$SOURCE"

