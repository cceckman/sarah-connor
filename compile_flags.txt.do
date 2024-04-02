
# https://github.com/apenwarr/redo script for filling compile_flags.txt

if uname -a | grep -q Linux
then
    cat <<EOF
-I/usr/include/llvm-17
-I/usr/include/llvm-c-17
EOF
else
    # Fill in OS X flags here?
    cat <<EOF
-I/opt/homebrew/opt/llvm@17/include
-L/opt/homebrew/opt/llvm@17/lib
EOF

fi
