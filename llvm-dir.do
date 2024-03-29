
if uname -a | grep -q Linux
then
    echo "$(llvm-config-17 --prefix)"
else
    # Fill in OS X flags here?
    echo "your LLVM prefix path here!"
fi
