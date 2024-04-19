
if uname -a | grep -q Linux
then
    llvm-config-17 --prefix >"$3"
else
    # Assume OS X
    echo "/opt/homebrew/opt/llvm@17" >"$3"
fi

redo-always
redo-stamp <"$3"
