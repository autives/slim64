#include "console_io.c"

int mine(int argc, char **argv) {
    for(int i = 0; i < argc; ++i) {
        print("%s\n", argv[i]);
    }
    return 5;
}



