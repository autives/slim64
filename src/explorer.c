#include "explorer.h"
#include "console_io.c"
#include "slim64.c" 
#include "parser.c"

#define PATH   "\x1B[38;5;190m"
#define RESET "\x1B[0m"

static const char *usage_msg = "Usage: slim64 <mode> <file name>\n";
static const char *modes_msg = "mode:\n  m[ount] = mount existing instance of the file system\n  n[ew]   = create new instance of the file system\n";

static explorer_state ExplorerBegin(Arena *arena, char *name, int create_new) {
    explorer_state Explorer = { 0 };

    Explorer.fs = create_new ? SLM_CreateNewFileSystem(name, DEFAULT_FS_SIZE) :
                               SLM_OpenExistingFileSystem(name);
    Explorer.arena = arena;
    
    SLM_File root = SLM_ReadRoot(&Explorer.fs);
    Explorer.current_working_directory = PushStruct(arena, file);
    Explorer.current_working_directory->isDirectory = 1;
    Explorer.current_working_directory->base_block = Explorer.fs.header.root;
    Explorer.current_working_directory->size = SLM_ReadUsedSize(&Explorer.fs, root.self);
    SLM_ReadName(&Explorer.fs, root.self, Explorer.current_working_directory->name, 124);
    
    int char_copied = _strcpy(Explorer.current_working_directory->name, Explorer.path, 256);
    Explorer.path[char_copied] = '>';
    Explorer.path[char_copied + 1] = '\0';
    return Explorer;
}

static void ChangeWorkingDirectory(explorer_state *Explorer, block_index cwd_new) {
    Explorer->current_working_directory->base_block = cwd_new;
    Explorer->current_working_directory->size = SLM_ReadUsedSize(&Explorer->fs, cwd_new);
    SLM_ReadName(&Explorer->fs, cwd_new, Explorer->current_working_directory->name, 123);

    int char_copied = SLM_BuildPath(&Explorer->fs, cwd_new, Explorer->path, 256);
    Explorer->path[char_copied] = '>';
    Explorer->path[char_copied + 1] = '\0';
}

static ExecutionBlock ExplorerProcessInput(Arena *arena) {
    char *read_buf = PushString(arena, 1024);
    int bytes_read = scan("%s", read_buf, 1023);
    read_buf[bytes_read] = '\0';

    return ExtractCommand(arena, read_buf);   
}

static inline void DisplayChild(char *name, u32 is_dircetory) {
    print("\t%s\t%s\n", is_dircetory ? "<dir>" : "     ", name);
}

static void ExplorerRun(Arena *arena, int argc, char **argv) {
    if(argc < 3) {
        print("%s", usage_msg);
        print("%s", modes_msg);
        
        return;
    }

    if(argc > 3) {
        print("Unknown argument \"%s\"", argv[3]);
        return;
    }

    explorer_state Explorer = { 0 };
    if(_strcmp(argv[1], "m") || _strcmp(argv[1], "mount")) {
        Explorer = ExplorerBegin(arena, argv[2], 0);
    }
    else if(_strcmp(argv[1], "n") || _strcmp(argv[1], "new")){
        Explorer = ExplorerBegin(arena, argv[2], 1);
    }
    else {
        print("Invalid mode \"%s\"\n", argv[1]);
        print("%s", modes_msg);
    }

    u32 running = 1;
    while(running) {
        print(PATH "%s " RESET, Explorer.path);
        ExecutionBlock input = ExplorerProcessInput(arena);
        
        switch(input.command) {
            case c_invalid:
            {
                print("Unknown command \"%s\"\n", input.arg);
            } break;

            case c_quit:
            {
                running = 0;
            } break;

            case c_change_directory:
            {
                if(!input.arg)
                    print("No path provided\n");
                else {
                    Path *arg = input.arg;
                    block_index cwd_new = SLM_GetChild(&Explorer.fs, Explorer.current_working_directory->base_block, arg->target);

                    if(_strcmp(arg->target, "..")) {
                        cwd_new = SLM_ReadParent(&Explorer.fs, Explorer.current_working_directory->base_block);
                        ChangeWorkingDirectory(&Explorer, cwd_new);
                        break;
                    }
                    else if(_strcmp(arg->target, "."))
                        break;
                    
                    u32 is_directory = SLM_ReadIsDirectory(&Explorer.fs, cwd_new);
                    if(!is_directory) {
                        print("\"%s\" is not a directory\n", arg->target);
                        break;
                    }
                    if(cwd_new == 0) {
                        print("No such directory \"%s\" in \"%s\"\n", arg->target, Explorer.current_working_directory->name);
                        break;
                    }
                    ChangeWorkingDirectory(&Explorer, cwd_new);
                }
            } break;

            case c_list:
            {
                DisplayChild(".", 1);
                DisplayChild("..", 1);

                u32 n_children = SLM_ReadNEntries(&Explorer.fs, Explorer.current_working_directory->base_block);
                for(int i = 0; i < n_children; ++i) {
                    SLM_DirectoryEntry entry = SLM_ReadEntry(&Explorer.fs, Explorer.current_working_directory->base_block, i);
                    DisplayChild(entry.name, SLM_ReadIsDirectory(&Explorer.fs, entry.base_block));
                }
            } break;

            default:
            {

            } break;
        }
    }
}