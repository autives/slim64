#include "console_io.c"
#include "string.c"
#include "parser.c"
#include "slim64.c"
#include "explorer.c"


int mine(int argc, char **argv) {
    Arena a;
    InitTempArena(&a, MegaBytes(512));
    ExplorerRun(&a, argc, argv);

    return 0;
}



