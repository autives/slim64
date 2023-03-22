#if !defined(SLIM64)

#include "common.h"
#include "platform.c"

#pragma pack(push, 1)

typedef struct SLM_File{
    size_t used_size;
    size_t nblocks;

    char name[124];
    char ext[4];

    u32 is_directory;

    block_index parent;
    block_index self;
    file_offset content;
} SLM_File;

typedef struct SLM_DirectoryEntry {
    char name[128];
    block_index base_block;
} SLM_DirectoryEntry;

typedef struct {
    size_t header_block_size;
    size_t total_size;
    size_t used_size;

    size_t block_size;
    size_t total_blocks;
    size_t nfree_blocks;

    block_index next_free_block;

    block_index root;
} SLM_Header;
#pragma pack(pop)

typedef struct FileSystem{
    SLM_Header header;
    active_file file;
} FileSystem;

static FileSystem SLM_CreateNewFileSystem(char *name, size_t total_size);
static FileSystem SLM_OpenExistingFileSystem(char *name);
static SLM_File SLM_ReadRoot(FileSystem *fs);

#define SLIM64
#endif