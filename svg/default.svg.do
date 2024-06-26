
set -eux

if uname -a | grep -q Linux
then
    PASS_TARGET="../build/BlockGraphPass.so"
else
    # Assume OS X
    PASS_TARGET="../build/BlockGraphPass.dylib"
fi

ANALYSIS_TARGET="$2"
ANALYSIS_FILE="../build/${ANALYSIS_TARGET}.ll"


redo-ifchange \
  ../build/llvm-dir \
  ../compile_flags.txt \
  "$PASS_TARGET" \
  "$ANALYSIS_FILE"

LLVM_DIR="$(cat ../build/llvm-dir)"
FLAGS="$(cat ../compile_flags.txt)"

TEMP="$(mktemp)"

"$LLVM_DIR"/bin/opt -load-pass-plugin \
    "$PASS_TARGET" \
    -passes="print<block-graph-pass>" \
    -disable-output \
    "$ANALYSIS_FILE" \
    2>"$TEMP"

dot -Tsvg -o "$3" "$TEMP"
