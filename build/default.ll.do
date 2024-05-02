
# This script is invoked as:
#   redo build/BlockGraphPass.ll
# ->
#   cd build
#   default.ll.do BlockGraphPass.ll BlockGraphPass temp >temp
# So $1 is "the path of the file being built",
#    $2 is "that, minus the target extension"
#    $3 a temporary file, where we should put our output
#    alternatively, stdout is also routed to that output
for F in "../testdata/${2}.cpp" "../testdata/${2}.c" "../src/${2}.c" "../src/${2}.cpp"
do
    if test -f "$F"
    then
        SOURCE="$F"
        break
    fi
done

if test -z "$SOURCE"
then
    echo >&2 "No source found for $2"
    exit 1
fi

redo-ifchange llvm-dir ../compile_flags.txt "$SOURCE"
LLVM_DIR="$(cat llvm-dir)"
FLAGS="$(cat ../compile_flags.txt)"

T="$(mktemp)"

"$LLVM_DIR"/bin/clang \
    $FLAGS \
    -fno-discard-value-names \
    -emit-llvm \
    -O1 \
    -S \
    "$SOURCE" \
    -o "$3"

