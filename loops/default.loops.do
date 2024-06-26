#!/bin/bash
#
set -eux

if uname -a | grep -q Linux
then
    PASS_TARGET="../build/BoundedTerminationPass.so"
else
    # Assume OS X
    PASS_TARGET="../build/BoundedTerminationPass.dylib"
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

set -o pipefail
timeout 10s \
"$LLVM_DIR"/bin/opt -load-pass-plugin \
    "$PASS_TARGET" \
    -passes="print<bounded-termination>" \
    -disable-output \
    "$ANALYSIS_FILE" \
    2>&1 | tee "$3" >&2

