
# This script is invoked as:
#   redo build/BlockGraphPass.ll
# ->
#   cd build
#   default.ll.do BlockGraphPass.ll BlockGraphPass temp >temp
# So $1 is "the path of the file being built",
#    $2 is "that, minus the target extension"
#    $3 a temporary file, where we should put our output
#    alternatively, stdout is also routed to that output
SOURCE="../src/${2}.cpp"

LLVM_DIR="$(cat llvm-dir)"

redo-ifchange llvm-dir ../compile_flags.txt "$SOURCE"

FLAGS="$(cat ../compile_flags.txt)"

"$LLVM_DIR"/bin/clang++ \
    $FLAGS \
    -fno-discard-value-names \
    -emit-llvm \
    -S -O1 \
    "$SOURCE" \
    -o "$3"

