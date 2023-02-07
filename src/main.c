#include "console_io.c"
#include "string.c"
#include "parser.c"
#include "slim64.c"
#include <stddef.h>


int mine(int argc, char **argv) {
    Arena a;


    FileSystem fs = SLM_CreateNewFileSystem("hello.slm", KiloBytes(10));
    SLM_File root = SLM_ReadUnitMetaData(&fs, fs.header.root);

    SLM_DirectoryEntry entry = { 0 };
    _strcpy("new_file", entry.name, 9);
    SLM_DirectoryAddEntry(&fs, root.self, &entry);

    file_offset size = SLM_ReadUsedSize(&fs, root.self);
    int s = size;
    print("%d\n", s);
    return 5;
}



