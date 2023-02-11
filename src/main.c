#include "console_io.c"
#include "string.c"
#include "parser.c"
#include "slim64.c"
#include "explorer.c"


int mine(int argc, char **argv) {
    Arena a;
    InitTempArena(&a, MegaBytes(1));

    // FileSystem fs = SLM_CreateNewFileSystem("hello.slm", MegaBytes(10));
    // SLM_File root = SLM_ReadFileMetaData(&fs, fs.header.root);

    // block_index dir = SLM_InsertNewDirectory(&fs, "dir", root.self);
    // block_index dir1 = SLM_InsertNewDirectory(&fs, "dir1", root.self);
    // block_index dir2 = SLM_InsertNewDirectory(&fs, "dir2", root.self);
    // block_index dir3 = SLM_InsertNewDirectory(&fs, "dir3", root.self);
    // print("%d\n", dir);

    // char *name = PushString(&a, 128);
    // name[0] = 'n';
    // name[1] = 'e';
    // name[2] = 'w';
    // name[3] = '.';
    // name[4] = 'e';
    // name[5] = 'x';
    // name[6] = 'e';
    // block_index file = SLM_InsertNewFile(&fs, name, dir);

    // char *buf = "It says I need to type at least ten characters, so here's this. Y'know what? I'm gonna type one hundred characters instead. Actually, I'm going to type five hundred characters. I'm definitely not going to type anywhere near one thousand characters, because that'd be ridiculous. Even if I wanted to type one thousand characters, I have to go to bed now anyway, so I simply don't have the time. I mean, I could just type a bunch of random letters or hold down one key, but that would be no fun at all.";
    // SLM_WriteToFile(&fs, file, buf, _strlen(buf));
    // SLM_WriteToFileAtOffset(&fs, file, buf, _strlen(buf), 490);

    // file_offset size = SLM_ReadUsedSize(&fs, file);
    // int s = size;
    // print("%d\n", s);

    // SLM_ReadFromFileAtOffset(&fs, file, name, 127, 485);
    // print("%s\n", name);
    // return 5;

    ExplorerRun(&a, argc, argv);

    return 0;
}



