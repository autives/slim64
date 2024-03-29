#if !defined(EXPLORER)

#include "common.h"
#include "slim64.h"
#include "m_alloc.c"
#include "string.c"

#define DEFAULT_FS_SIZE GigaBytes(1)

typedef enum Commands {
    c_invalid,
    c_quit,
    c_help,
    c_clear,
    c_list,
    c_change_directory,
    c_current_directory,
    c_make_directory,
    c_rename,
    c_copy,
    c_move,
    c_import,
    c_open,
    c_delete,
    
    c_total
} Commands;

typedef struct ChangeDirectoryArgs {
    char *target;
} Path;

// #define MAX_DIRECTORY_ARGS 16
// #define MAX_FILE_ARGS MAX_DIRECTORY_ARGS
// #define MAX_COPY_ARGS 16
// #define MAX_MOVE_ARGS MAX_COPY_ARGS
// #define MAX_DELETE_ARGS 16
typedef struct MakeDirectoryArgs {
    Vector names;
} MakeDirectoryArgs, MakeFileArgs, CopyArgs, MoveArgs, DeleteArgs;

typedef struct RenameFileArgs {
    char *old_name;
    char *new_name;
} ChangeName;

typedef struct ImportArgs {
    char *src;
    char *dst;
} ImportArgs;

typedef struct {
    char *name;
} SearchArgs, OpenArgs;

// #define MAX_FIND_FILES 16
typedef struct FindArgs {
    char *str_to_search;
    Vector files;
} FindArgs;

static char *Command_Strings[c_total] = 
{       "",
        "quit",
        "help",
        "clear",
        "list",
        "cd",
        "pwd",
        "mkdir",
        "ren",
        "copy",
        "move",
        "import",
        "open",
        "del"
};


typedef struct file {
    size_t size;
    block_index base_block;
    char name[124];
    char ext[4];

    u32 isDirectory;

    void *content;
} file;

typedef struct explorer_state {
    file *current_working_directory;
    u32 isAdmin;
    char path[256];
    Vector prev_commands;

    Arena *arena;
    FileSystem fs;
} explorer_state;

#define EXPLORER
#endif