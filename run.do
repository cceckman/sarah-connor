

if uname -a | grep -q Linux
then
    PASS_TARGET="build/BlockGraphPass.so"
else
    # Assume OS X
    PASS_TARGET="build/BlockGraphPass.dylib"
fi



redo-ifchange \
  build/llvm-dir \
  build/compile_flags.txt \
  "$PASS_TARGET" \
  build/BlockGraphPass.ll

LLVM_DIR="$(cat build/llvm-dir)"
FLAGS="$(cat build/compile_flags.txt)"

"$LLVM_DIR"/bin/opt -load-pass-plugin \
    "$PASS_TARGET" \
    -passes="print<block-graph-pass>" \
    -disable-output build/BlockGraphPass.ll

