

redo-ifchange $(
    # Alt: stripped="${string%"$suffix"}"
    for F in $(find ../src ../testdata -type f)
    do
        echo $(basename "$F" | sed 's/c\(pp\)\?$/ll/')
    done
)