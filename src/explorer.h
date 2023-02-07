#include "common.h"
#include "m_alloc.c"

typedef enum Commands {
    c_invalid,
    c_quit,
    c_help,
    c_clear,
    c_list,
    c_change_directory,
    c_current_directory,
    c_make_directory,
    c_make_file,
    c_rename,
    c_copy,
    c_move,
    c_import,
    c_search,
    c_find,
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

typedef struct SearchArgs {
    char *name;
} SearchArgs;

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
        "mkfile",
        "ren",
        "copy",
        "move",
        "import",
        "search",
        "find",
        "del"
};


typedef struct file {
    size_t size;
    const char *name;
    const char *ext;

    u32 isDirectory;

    void *content;
} file;

typedef struct explorer_state {
    file *current_working_directory;
    u32 isAdmin;
    char path[256];
    Vector prev_commands;
} explorer_state;