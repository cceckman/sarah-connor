
# https://github.com/apenwarr/redo script for filling in compiler include paths

if uname -a | grep -q Linux
then
    cat >"$3" <<EOF
-I/usr/include/llvm-17
-I/usr/include/llvm-c-17
-L/usr/lib/llvm-17/lib
EOF

else
    # Fill in OS X flags here?
    cat >"$3" <<EOF
-I/opt/homebrew/opt/llvm@17/include
-L/opt/homebrew/opt/llvm@17/lib
EOF

fi

redo-always
redo-stamp <"$3"
