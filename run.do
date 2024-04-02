redo-ifchange llvm-dir compile_flags.txt src/BlockGraphPass.dylib src/BlockGraphPass.ll

LLVM_DIR="$(cat llvm-dir)"

# This script is invoked as:
#   redo src/BlockGraphPass.ll
# ->
#   default.ll.do src/BlockGraphPass.ll src/BlockGraphPass temp >temp
# So $1 is "the path of the file being built",
#    $2 is "that, minus the target extension"
#    $3 a temporary file, where we should put our output
#    alternatively, stdout is also routed to that output

FLAGS="$(cat compile_flags.txt)"

$LLVM_DIR/bin/opt -load-pass-plugin \
    ./src/BlockGraphPass.dylib \
    -passes="print<block-graph-pass>" \
    -disable-output src/BlockGraphPass.ll
