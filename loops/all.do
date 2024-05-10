
redo-ifchange $(
    # Alt: stripped="${string%"$suffix"}"
    for F in $(find ../testdata -type f -name '*.cpp' -or -name '*.c')
    do
        # TODO: Fix these:
        if echo "$F" | grep -P '(factorial|collatz)'
        then
            continue
        fi
        echo $(basename "$F" | sed 's/c\(pp\)\?$/loops/')
    done
)

