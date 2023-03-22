#include "explorer.h"
#include "console_io.c"
#include "slim64.c" 
#include "parser.c"

#define PATH   "\x1B[38;5;190m"
#define RESET "\x1B[0m"

static const char *usage_msg = "Usage: slim64 <mode> <file name>\n";
static const char *modes_msg = "mode:\n  m[ount] = mount existing instance of the file system\n  n[ew]   = create new instance of the file system\n";
static const char *help_msg = \
"\
    This is a command line based explorer for Slim64 File System\n\n\
    Supported Commands:\n\
    help:\n\
    \tDisplays this message\n\
    pwd:\n\
    \tDisplays the current working directory\n\
    cd <target>:\n\
    \tChange the current working directory to <target>\n\
    mkdir <path>:\n\
    \tCreates a directory at path. Also creates intermediate directories if necessary\n\
    quit:\n\
    \tTerminates the program\n\
    clear:\n\
    \tClear the console\n\
    list:\n\
    \tLists the list of files and diectory in the current working directory\n\
    ren <old> <new>:\n\
    \tChanges the name of <old> to <new>\n\
    copy <files> <dst>:\n\
    \tCopies the items in <files> to <dst>\n\
    move <files> <dst>:\n\
    \tMoves the items in <files> to <dst>\n\
    import <src> <dst>\n\
    \tImports <src> from OS filesystem to <dst> in the Slim64 filesystem\n\
    open <file>\n\
    \tOpens the <file>\n\
    del <files>\n\
    \tDeletes the items listed in <files>\n\
";

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

typedef int (*comparator) (const void*, const void*);

typedef struct List {
    size_t size;
    u32 is_directory;
    char name[128];
} ListItem;

inline int list_item_comp(const void *_a, const void *_b){
    return compare_str(((ListItem*)_a)->name, ((ListItem*)_b)->name);
} 

void swap(Arena *arena, void* a, void* b, size_t size) {
    void *buf = PushSize(arena, size);
    m_copy(a, buf, size);
    m_copy(b, a, size);
    m_copy(buf, b, size);
}

int partition(Arena *arena, void *arr, int low, int high, size_t size,
              int (*cmp)(const void *, const void *)) {
    void *pivot = (char*)arr + high * size;
    int i = low - 1;

    for (int j = low; j <= high - 1; j++) {
        if (cmp((char*)arr + j * size, pivot) < 0) {
            i++;
            swap(arena, (char*)arr + i * size, (char*)arr + j * size, size);
        }
    }

    swap(arena, (char*)arr + (i+1) * size, (char*)arr + high * size, size);
    return (i + 1);
}

void q_sort(Arena *arena, void *arr, int low, int high, size_t size,
               int (*cmp)(const void *, const void *)) {
    if (low < high) {
        int pi = partition(arena, arr, low, high, size, cmp);

        q_sort(arena, arr, low, pi - 1, size, cmp);
        q_sort(arena, arr, pi + 1, high, size, cmp);
    }
}

void sort(Vector *v, comparator comp) {
    q_sort(v->arena, v->mem, 0, v->count - 1, v->unit_size, comp);
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

static inline void DisplayChild(char *name, u32 is_dircetory, u32 size) {
    if(!is_dircetory){
        if(size < 1024)
            print("%d B", size);
        else {
            float disp_size = size / 1024.0f;
            
            if(disp_size > 1024) {
                disp_size /= 1024;
                print("%.1f M", disp_size); 
            }
            else
                print("%.1f K", disp_size);

        }
    }
    else
        print("\t");
    print("%s\t%s\n", is_dircetory ? "<dir>" : "     ", name);
}

static Vector ParsePath(Arena *arena, char *path) {
    Vector res = VectorBegin(arena, 5, sizeof(char*));

    char *current = path;
    u32 len = _strlen(path);
    for(int i = 0; i < len; ++i) {
        if(path[i] == '/') {
            VectorPush(&res, &current);
            current = path + i + 1;
            path[i] = '\0';
        }
    }
    VectorPush(&res, &current);

    return res;
}

typedef enum {
    no_err, not_found, last_not_directory, middle_not_directory, ends_with_pnemonic 
} traverse_errors;

typedef struct {
    block_index terminating;
    block_index penultimate;
    
    traverse_errors err;
    char *str;
} traverse_result;

traverse_result TraversePath(Vector *path, FileSystem *fs, file *cwd) {
    traverse_result res = { 0 };

    res.terminating = cwd->base_block;
    res.penultimate = cwd->base_block;
    for(int i = 0; i < path->count; ++i) {
        char **_target = VectorGet(path, i);
        char *target = *_target;

        if(i > 1)
            res.penultimate = res.terminating;

        if(_strcmp(target, "..")) {
            res.terminating = SLM_ReadParent(fs, res.terminating);
            if(i == path->count - 1)
                res.err = ends_with_pnemonic;
            continue;
        }
        else if(_strcmp(target, ".")){
            if(i == path->count - 1)
                res.err = ends_with_pnemonic;
            continue;
        }

        char next_name[128];
        SLM_ReadName(fs, res.terminating, next_name, 128);
        res.terminating = SLM_GetChild(fs, res.terminating, target);

        u32 is_directory = SLM_ReadIsDirectory(fs, res.terminating);
        if(res.terminating == 0) {
            res.err = not_found;
            res.str = target;
            break;
        }
        if(!is_directory) {
            if(i == path->count - 1)
                res.err = last_not_directory;
            else
                res.err = middle_not_directory;
            res.str = target;
            break;
        }
    }

    return res;
}

static char* ExtractFileNameFromPath(char *path) {
    char *res = path;
    while(*path) {
        if(*path == '/')
            res = path + 1;
        path++;
    }
    return res;
}

static void ExportTmpFile(explorer_state *Explorer, char *file_name, loaded_file file) {
    CreateDirectoryA("tmp", 0);
    WriteEntireFile(file, file_name);
}

void append_path(char *path, const char *str) {
    int len = _strlen(path);
    if (len > 0 && path[len - 1] != '\\') {
        _strcpy("\\", path + len, 1);
    }
    _strcpy(str, path + len, _strlen(str));
}

void delete_folder(const char *folder) {
    char path[MAX_PATH];
    _strcpy(folder, path, MAX_PATH);

    WIN32_FIND_DATA find_data;
    append_path(path, "\\*");
    HANDLE find_handle = FindFirstFile(path, &find_data);

    if (find_handle == INVALID_HANDLE_VALUE) {
        return;
    }

    do {
        if (_strcmp(find_data.cFileName, ".") || _strcmp(find_data.cFileName, "..")) {
            continue;
        }

        path[_strlen(folder)] = '\0';

        append_path(path, "\\");
        path[_strlen(folder) + 1] = '\0';

        append_path(path, find_data.cFileName);
        path[_strlen(folder) + 1 + _strlen(find_data.cFileName)] = '\0';

        if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            delete_folder(path);
        } else {
            DeleteFile(path);
        }
    } while (FindNextFile(find_handle, &find_data));

    FindClose(find_handle);

    RemoveDirectory(folder);
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
        return;
    }

    delete_folder("tmp");

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
                if(!input.arg){
                    print("No path provided\n");
                    break;
                }
                Path *arg = input.arg;
                Vector path = ParsePath(arena, arg->target);
                traverse_result res = TraversePath(&path, &Explorer.fs, Explorer.current_working_directory);
                if(res.err && res.err != ends_with_pnemonic) {
                    print("Invalid path\n");
                    break;
                }

                block_index cwd_new = res.terminating;
                ChangeWorkingDirectory(&Explorer, cwd_new);
            } break;

            case c_list:
            {
                DisplayChild(".", 1, 0);
                DisplayChild("..", 1, 0);

                u32 n_children = SLM_ReadNEntries(&Explorer.fs, Explorer.current_working_directory->base_block);
                Vector list = VectorBegin(arena, n_children, sizeof(ListItem));
                
                for(int i = 0; i < n_children; ++i) {
                    SLM_DirectoryEntry entry = SLM_ReadEntry(&Explorer.fs, Explorer.current_working_directory->base_block, i);

                    u32 is_directory = SLM_ReadIsDirectory(&Explorer.fs, entry.base_block);
                    size_t total_size = SLM_ReadUsedSize(&Explorer.fs, entry.base_block);
                    size_t effective_size = total_size - INIT_USED_SIZE;

                    ListItem item = {effective_size, is_directory};
                    _strcpy(entry.name, item.name, 128);
                    VectorPush(&list, &item);

                }
                sort(&list, list_item_comp);

                for(int i = 0; i < n_children; ++i) {
                    ListItem *item = VectorGet(&list, i);
                    DisplayChild(item->name, item->is_directory, item->size);
                }
            } break;

            case c_make_directory:
            {
                MakeDirectoryArgs *arg = input.arg;
                if(!arg)
                    break;

                for(int i = 0; i < arg->names.count; ++i ){
                    char **_name = VectorGet(&arg->names, i);
                    char *name = *_name;

                    Vector levels = ParsePath(arena, name);
                    block_index parent = Explorer.current_working_directory->base_block;
                    char *final_name;
                    u32 is_valid = 1;

                    for(int i = 0; i < levels.count; ++i ){
                        char **_target = VectorGet(&levels, i);
                        char *target = *_target;

                        if(_strcmp(target, "..")) {
                            if(i == levels.count - 1) {
                                print("\"%s\" is not a valid directory name", target);
                                is_valid = 0;
                                break;
                            }
                            parent = SLM_ReadParent(&Explorer.fs, parent);
                            continue;
                        }

                        else if(_strcmp(target, ".")){
                            if(i == levels.count - 1) {
                                print("\"%s\" is not a valid directory name\n", target);
                                is_valid = 0;
                                break;
                            }
                            continue;
                        }

                        block_index old_parent = parent;
                        final_name = target;
                        parent = SLM_GetChild(&Explorer.fs, old_parent, final_name);

                        if(parent == 0) {
                            if(i < levels.count - 1) {
                                SLM_InsertNewDirectory(&Explorer.fs, final_name, old_parent);
                                parent = SLM_GetChild(&Explorer.fs, old_parent, final_name);
                            }
                            else {
                                parent = old_parent;
                            }
                        }
                        else {
                            if(i == levels.count - 1) {
                                print("\"%s\" already exists\n", final_name);
                                is_valid = 0;
                            }
                        }

                    }
                    if(is_valid) 
                        SLM_InsertNewDirectory(&Explorer.fs, final_name, parent);
                }
            } break;

            case c_clear:
            {
                ClearConsole();
            } break;

            case c_current_directory:
            {
                print("%s\b/\n", Explorer.path);
            } break;

            case c_rename:
            {
                ChangeName *arg = input.arg;
                if(!arg->old_name || arg->new_name) {
                    print("Insufficient arguments\n");
                }

                Vector old_path = ParsePath(arena, arg->old_name);
                traverse_result res = TraversePath(&old_path, &Explorer.fs, Explorer.current_working_directory);
                if(res.err == ends_with_pnemonic){
                    print("Invalid syntax\n");
                    break;
                }
                else if(res.err == not_found){
                    print("Path does not exist\n");
                    break;
                }

                char **old_name = VectorGet(&old_path, old_path.count - 1);
                SLM_RenameEntry(&Explorer.fs, res.penultimate, *old_name, arg->new_name);
            } break;

            case c_copy:
            case c_move:
            {
                CopyArgs *args = input.arg;
                if(!args) {
                    print("No files provided\n");
                    break;
                }      

                Vector *arg = &args->names;
                char **_dst = VectorGet(arg, arg->count - 1);
                char *dst = *_dst;

                Vector dst_path = ParsePath(arena, dst);
                traverse_result res = TraversePath(&dst_path, &Explorer.fs, Explorer.current_working_directory);
                if(res.err == last_not_directory)
                    print("%s is not a valid destination\n", res.str);
                else if(res.err == middle_not_directory)
                    print("Invalid destination provided\n");
                else if (res.err == not_found) {
                    print("No such directory \"%s\"\n", res.str);
                    break;
                }
                block_index dst_directory = res.terminating;                

                for(int i = 0; i < arg->count - 1; ++i) {
                    char **_src = VectorGet(arg, i);
                    char *src = *_src;

                    Vector src_path = ParsePath(arena, src);
                    traverse_result res = TraversePath(&src_path, &Explorer.fs, Explorer.current_working_directory);

                    if(res.err == middle_not_directory){
                        print("Invalid source path provided\n", res.str);
                        break;
                    }
                    else if (res.err == not_found){
                        print("No such directory \"%s\"\n", res.str);
                        break;
                    }
                    block_index src_item = res.terminating;

                    char **file_name = VectorGet(&src_path, src_path.count - 1);
                    if(SLM_EntryExists(&Explorer.fs, dst_directory, *file_name)) {
                        print("%s already exists\n", *file_name);
                        break;
                    }

                    if(input.command == c_copy)
                        SLM_Copy(&Explorer.fs, src_item, dst_directory);
                    else
                        SLM_Move(&Explorer.fs, src_item, dst_directory);
                }
            } break;

            case c_delete:
            {
                DeleteArgs *args = input.arg;
                if(!args) {
                    print("No files provided\n");
                    break;
                }      
                   
                Vector *arg = &args->names;
                for(int i = 0; i < arg->count; ++i) {
                    char **_path_str = VectorGet(arg, i);
                    char *path_str = *_path_str;

                    Vector path = ParsePath(arena, path_str);
                    traverse_result res = TraversePath(&path, &Explorer.fs, Explorer.current_working_directory);
                    if(res.err) {
                        if(res.err != last_not_directory && res.err != ends_with_pnemonic){
                            print("Invalid path for %dth argument\n", i);
                            break;   
                        }
                    }

                    block_index file_to_delete = res.terminating;
                    SLM_DeleteFile(&Explorer.fs, file_to_delete);
                }
            } break;

            case c_import:
            {
                ImportArgs *args = input.arg;
                if(!args) {
                    print("No arguments provided\n");
                    break;
                }

                loaded_file file = ReadEntireFile(args->src);
                char *file_name = ExtractFileNameFromPath(args->src);

                Vector dst_path = ParsePath(arena, args->dst);
                traverse_result res = TraversePath(&dst_path, &Explorer.fs, Explorer.current_working_directory);
                if(res.err && res.err != ends_with_pnemonic) {
                    print("Invalid destination\n");
                    break;
                }
                block_index dst = res.terminating;

                block_index new_file = SLM_InsertNewFile(&Explorer.fs, file_name, dst);
                SLM_WriteToFileAtOffset(&Explorer.fs, new_file, file.content, file.size, 0);

                FreeFileMemory(file);
            } break;

            case c_open:
            {
                OpenArgs *args = input.arg;
                char *file_path = args->name;

                Vector path = ParsePath(Explorer.arena, file_path);
                traverse_result res = TraversePath(&path, &Explorer.fs, Explorer.current_working_directory);

                if(res.err != last_not_directory) {
                    print("Invalid arguments provided\n");
                    break;
                }

                size_t file_size = SLM_ReadUsedSize(&Explorer.fs, res.terminating) - INIT_USED_SIZE;
                char *buf = PushString(Explorer.arena, file_size);
                SLM_ReadFromFileAtOffset(&Explorer.fs, res.terminating, buf, file_size, 0);

                loaded_file file = { 0 };
                file.content = buf;
                file.size = file_size;

                char tmp_file_path[256] = { 0 };
                _strcpy(".\\tmp\\", tmp_file_path, 6);
                _strcpy(ExtractFileNameFromPath(file_path), tmp_file_path + 6, 256 - 6);

                CreateDirectoryA("tmp", 0);
                WriteEntireFile(file, tmp_file_path);
                RunFile(tmp_file_path);
            } break;

            case c_help:
            {
                print("%s\n", help_msg);
            }
            default:
            {

            } break;
        }
    }
}