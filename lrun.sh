which gcc &> /dev/null
if [ $? == 0 ]
then
    COMPILER=gcc
else
    echo gcc not found
    exit
fi

mkdir -p build
gcc -nostdlib -m64 -g src/entry.c -o build/slim64
build/slim64 "hello there"
