which gcc &> /dev/null
if [ $? == 0 ]
then
    COMPILER=gcc
else
    echo gcc not found
    exit
fi

mkdir -p build
gcc -nostdlib -m64 -g src/entry.c -o build/slim64.out
cd build
./slim64.out m "hello.slm"
