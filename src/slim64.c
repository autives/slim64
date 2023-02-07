#include "common.h"
#include "m_alloc.c"
#include "string.c"
#include <stddef.h>


/*
    Block Structure
    u32 in_use
    u32 prev_block
    u32 next_block
    data [block_size - 8]

    prev_block is 0 for the first block of a file
    if not in_use (free block) next_block gives the index of the next free block
*/

#define IN_USE(block) GlobalFileOffset(block, 0)
#define PREV(block) GlobalFileOffset(block, sizeof(u32))
#define NEXT(block) GlobalFileOffset(block, sizeof(u32) * 2)
#define CONTENT(block) GlobalFileOffset(block, sizeof(u32) * 3)


#define BLOCK_SIZE 512
#define BLOCK_METADATA 3*sizeof(u32)
#define USABLE_BLOCK_SIZE BLOCK_SIZE - BLOCK_METADATA

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

    // size_t nfiles;
    // size_t ndirectories; 

    block_index root;
} SLM_Header;
#pragma pack(pop)

typedef struct FileSystem{
    SLM_Header header;
    active_file file;
} FileSystem;

static inline file_offset GlobalFileOffset(block_index block, file_offset off) {
    return block * BLOCK_SIZE + off + sizeof(SLM_Header);
}

block_index SLM_ReserveBlocks(FileSystem *fs, u32 count) {
    Assert(fs->header.used_size + count * fs->header.block_size < fs->header.total_size);

    block_index res = fs->header.next_free_block;
    for(int i = 0; i < count; ++i) {        
        u32 in_use = 1;
        block_index zero = 0;
        block_index next_block = fs->header.next_free_block;

        WriteToFileAtOffset(&fs->file, &in_use, sizeof(in_use), IN_USE(next_block));
        if(i == 0)
            WriteToFileAtOffset(&fs->file, &zero, sizeof(block_index), PREV(next_block));
        
        ReadFromFileAtOffset(&fs->file, &fs->header.next_free_block, sizeof(block_index), NEXT(fs->header.next_free_block));
        if(i == count - 1)
            WriteToFileAtOffset(&fs->file, &zero, sizeof(block_index), NEXT(next_block));
    }

    fs->header.used_size += count * fs->header.block_size;
    fs->header.nfree_blocks -= count;
    return res;
}

static void SLM_InitBlocks(FileSystem *fs) {
    WriteToFile(&fs->file, &fs->header, sizeof(fs->header));

    char dummy_buf[BLOCK_SIZE] = { 0 };
    for(int i = 0; i < fs->header.total_blocks; ++i) {
        WriteToFile(&fs->file, dummy_buf, BLOCK_SIZE);
    }

    fs->header.used_size += sizeof(fs->header);
    MoveFilePointer(&fs->file, sizeof(fs->header), FOFFSET_BEGIN, FPOINTER_WRITE);
    block_index i = 0;
    for(; i < fs->header.total_blocks - 1; ++i) {
        block_index next = i+1;
        block_index prev = i-1;
        WriteToFileAtOffset(&fs->file, &prev, sizeof(block_index), GlobalFileOffset(i, sizeof(u32)));
        WriteToFileAtOffset(&fs->file, &next, sizeof(block_index), GlobalFileOffset(i, 2 * sizeof(u32)));
    }
}

static FileSystem SLM_CreateNewFileSystem( char *name, size_t total_size) {
    FileSystem result = { 0 };
    result.file = CreateNewFile(name);

    // rounding up to nearest multiple of BLOCK_SIZE = 512
    total_size >>= 9;
    total_size++;
    total_size <<= 9;

    result.header.block_size = BLOCK_SIZE;
    result.header.total_size = total_size;
    result.header.total_blocks = total_size / BLOCK_SIZE;
    result.header.nfree_blocks = result.header.total_blocks;
    result.header.header_block_size = sizeof(SLM_Header);

    SLM_InitBlocks(&result);
    result.header.next_free_block = 0;

    result.header.root = SLM_ReserveBlocks(&result, 1);

    SLM_File root = { 0 };
    root.used_size = sizeof(SLM_File);
    root.is_directory = 1;
    _strcpy("room", root.name, 5);
    root.self = result.header.root;
    root.nblocks = 1;
    root.parent = 0;   
    root.content = BLOCK_METADATA + GlobalFileOffset(root.self, sizeof(SLM_File));
    WriteToFileAtOffset(&result.file, &root, sizeof(root), CONTENT(result.header.root));
    
    return result;    
}

static inline void SLM_GetBlock(FileSystem *fs, block_index block, char buf[BLOCK_SIZE]) {
    ReadFromFileAtOffset(&fs->file, buf, BLOCK_SIZE, GlobalFileOffset(block, 0));
}

typedef struct Block {
    u32 in_use;
    u32 prev;
    u32 next;
    char *content;
} Block;

static inline Block ParseBlock(char buf[BLOCK_SIZE]) {
    Block result = { 0 };

    result.in_use = *(u32*)buf++;
    result.prev = *(u32*)buf++;
    result.next = *(u32*)buf++;
    result.content = buf;

    return result;
}

static SLM_File SLM_ReadUnitMetaData(FileSystem *fs, block_index block) {
    char buf[BLOCK_SIZE];
    SLM_GetBlock(fs, block, buf);

    Block block_data = ParseBlock(buf);
    if(!block_data.in_use) {
        // Invalid block, not in use
        return (SLM_File){ 0 };
    }
    if(block_data.prev) {
        // The block is not the initial block
        return (SLM_File){ 0 };
    }

    SLM_File *block_contents = (SLM_File*)((char*)buf + BLOCK_METADATA);
    return *block_contents;
}

static inline void ReadFromBlockOffseted(FileSystem *fs, void *buf, size_t size, file_offset off) {
    ReadFromFileAtOffset(&fs->file, buf, size, BLOCK_METADATA + off);
}

static inline void WriteToBlockOffseted(FileSystem *fs, void *buf, size_t size, file_offset off) {
    WriteToFileAtOffset(&fs->file, buf, size, BLOCK_METADATA + off);
}

static inline size_t SLM_ReadUsedSize(FileSystem *fs, block_index file) {
    size_t res;
    ReadFromBlockOffseted(fs, &res, sizeof(res), GlobalFileOffset(file, OffsetOf(SLM_File, used_size)));
    return res;
}

static inline void SLM_WriteUsedSize(FileSystem *fs, block_index file, size_t used_size) {
    WriteToBlockOffseted(fs, &used_size, sizeof(used_size), GlobalFileOffset(file, OffsetOf(SLM_File, used_size)));
}

static inline size_t SLM_ReadNBlocks(FileSystem *fs, block_index file) {
    size_t res;
    ReadFromBlockOffseted(fs, &res, sizeof(res), GlobalFileOffset(file, OffsetOf(SLM_File, nblocks)));
    return res;
}

static inline void SLM_WriteNBlocks(FileSystem *fs, block_index file, size_t nblocks) {
    WriteToBlockOffseted(fs, &nblocks, sizeof(nblocks), GlobalFileOffset(file, OffsetOf(SLM_File, nblocks)));
}

static inline block_index SLM_ReadNextBlockIndex(FileSystem *fs, block_index block) {
    block_index res;
    ReadFromFileAtOffset(&fs->file, &res, sizeof(res), NEXT(block));
    return res;
}

static inline size_t SLM_GetAvailableSize(SLM_File file) {
    return file.nblocks * USABLE_BLOCK_SIZE - file.used_size;
}

static inline block_index SLM_GetLastBlock(FileSystem *fs, block_index base_block) {
    block_index next_block = base_block;
    while(SLM_ReadNextBlockIndex(fs, next_block) != 0) {
        next_block = SLM_ReadNextBlockIndex(fs, next_block);
    }
    return next_block;
}

static inline file_offset SLM_GetFileOffset(FileSystem *fs, block_index file) {
    size_t used_size = SLM_ReadUsedSize(fs, file);
    used_size += BLOCK_METADATA * SLM_ReadNBlocks(fs, file);

    block_index next_block = file;
    while(used_size > BLOCK_SIZE) {
        next_block = SLM_ReadNextBlockIndex(fs, file);
    }

    return GlobalFileOffset(next_block, used_size);
}

static void SLM_WriteToFile(FileSystem *fs, block_index base_block, char *data, size_t size) {
    SLM_File file = SLM_ReadUnitMetaData(fs, base_block);
    size_t available_size = SLM_GetAvailableSize(file);

    if(available_size < size) {
        size_t additional_blocks = RoundUpDivision(size - available_size, USABLE_BLOCK_SIZE);
        file.nblocks += additional_blocks;
        block_index next_block = SLM_ReserveBlocks(fs, RoundUpDivision(size - available_size, USABLE_BLOCK_SIZE));
        block_index prev_last_block = SLM_GetLastBlock(fs, base_block);
        WriteToFileAtOffset(&fs->file, &next_block, sizeof(next_block), NEXT(prev_last_block));
    }

    size_t written_size = 0;
    size_t size_to_write = MIN(size, available_size);
    file_offset write_at = SLM_GetFileOffset(fs, base_block);
    while(written_size < size) {
        WriteToFileAtOffset(&fs->file, data + written_size, size_to_write, write_at);
        size -= size_to_write;
        written_size += size_to_write;
        size_to_write = MIN(size, USABLE_BLOCK_SIZE);
    }
    file.used_size += written_size;
    SLM_WriteUsedSize(fs, base_block, file.used_size);
    SLM_WriteNBlocks(fs, base_block, file.nblocks);

}

static void SLM_DirectoryAddEntry(FileSystem *fs, block_index directory, SLM_DirectoryEntry *entry) {
    SLM_File directory_metadata = SLM_ReadUnitMetaData(fs, directory);
    Assert(directory_metadata.is_directory);

    SLM_WriteToFile(fs, directory, (void*)entry, sizeof(*entry));
}