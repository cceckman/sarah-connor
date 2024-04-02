
if uname -a | grep -q Linux
then
    echo "$(llvm-config-17 --prefix)"
else
    # Assume OS X
    echo "/opt/homebrew/opt/llvm@17"
fi

