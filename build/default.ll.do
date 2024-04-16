
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


LLVM_DIR="$(cat llvm-dir)"

redo-ifchange llvm-dir ../compile_flags.txt "$SOURCE"

FLAGS="$(cat ../compile_flags.txt)"

"$LLVM_DIR"/bin/clang \
    $FLAGS \
    -fno-discard-value-names \
    -emit-llvm \
    -S -O1 \
    "$SOURCE" \
    -o "$3"

