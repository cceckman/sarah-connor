
redo-ifchange $(
    # Alt: stripped="${string%"$suffix"}"
    for F in $(find ../testdata -type f -name '*.cpp' -or -name '*.c')
    do
        echo $(basename "$F" | sed 's/c\(pp\)\?$/loops/')
    done
)

