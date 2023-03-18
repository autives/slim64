#if !defined(SLIM64_C)

#include "slim64.h"
#include "string.c"
#include <stddef.h>


/*
    Block Structure:
    u32 in_use
    u32 used_before
    u32 prev_block
    u32 next_block
    data [block_size - 8]

    prev_block is 0 for the first block of a file
    if not in_use (free block) next_block gives the index of the next free block
*/

#define BLOCK_SIZE (512)
#define BLOCK_METADATA (4*sizeof(u32))
#define USABLE_BLOCK_SIZE ((BLOCK_SIZE) - (BLOCK_METADATA))


#define IN_USE(block) GlobalFileOffset(block, 0) - BLOCK_METADATA
#define BLOCK_BEGIN(block) IN_USE(block)
#define USED_BEFORE(block) GlobalFileOffset(block, sizeof(u32)) - BLOCK_METADATA
#define PREV(block) GlobalFileOffset(block, sizeof(u32) * 2) - BLOCK_METADATA
#define NEXT(block) GlobalFileOffset(block, sizeof(u32) * 3) - BLOCK_METADATA
#define CONTENT(block) GlobalFileOffset(block, 0)

#define INIT_USED_SIZE sizeof(SLM_File)

static inline file_offset GlobalFileOffset(block_index block, file_offset off) {
    return block * BLOCK_SIZE + off + sizeof(SLM_Header) + BLOCK_METADATA;
}

static inline void SLM_UpdateHeader(FileSystem *fs) {
    WriteToFileAtOffset(&fs->file, &fs->header, sizeof(fs->header), 0);
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

        u32 used_before = 0;
        ReadFromFileAtOffset(&fs->file, &used_before, sizeof(used_before), USED_BEFORE(next_block));
        if(used_before)
            ReadFromFileAtOffset(&fs->file, &fs->header.next_free_block, sizeof(block_index), NEXT(fs->header.next_free_block));
        else
            fs->header.next_free_block++;
        
        used_before = 1;
        WriteToFileAtOffset(&fs->file, &used_before, sizeof(used_before), USED_BEFORE(next_block));

        if(i == count - 1)
            WriteToFileAtOffset(&fs->file, &zero, sizeof(block_index), NEXT(next_block));
        else
            WriteToFileAtOffset(&fs->file, &fs->header.next_free_block, sizeof(block_index), NEXT(next_block));
    }

    fs->header.used_size += count * fs->header.block_size;
    fs->header.nfree_blocks -= count;
    SLM_UpdateHeader(fs);
    return res;
}

void SLM_FreeBlocks(FileSystem *fs, block_index first) {
    block_index next_block = first;

    do {
        u32 in_use;
        ReadFromFileAtOffset(&fs->file, &in_use, sizeof(in_use), IN_USE(next_block));
        Assert(in_use);

        in_use = 0;
        WriteToFileAtOffset(&fs->file, &in_use, sizeof(in_use), IN_USE(next_block));

        block_index tmp;
        ReadFromFileAtOffset(&fs->file, &tmp, sizeof(tmp), NEXT(next_block));

        WriteToFileAtOffset(&fs->file, &fs->header.next_free_block, sizeof(block_index), NEXT(next_block));
        fs->header.next_free_block = next_block;

        next_block = tmp;
    } while(next_block);
}

static void SLM_InitBlocks(FileSystem *fs) {
    WriteToFile(&fs->file, &fs->header, sizeof(fs->header));

    fs->header.used_size += sizeof(fs->header);
    // MoveFilePointer(&fs->file, sizeof(fs->header), FOFFSET_BEGIN, FPOINTER_WRITE);
    // block_index i = 0;
    // for(; i < fs->header.total_blocks - 1; ++i) {
    //     block_index next = i+1;
    //     block_index prev = i-1;
    //     WriteToFileAtOffset(&fs->file, &prev, sizeof(block_index), PREV(i));
    //     WriteToFileAtOffset(&fs->file, &next, sizeof(block_index), NEXT(i));
    // }
}

static FileSystem SLM_CreateNewFileSystem(char *name, size_t total_size) {
    FileSystem result = { 0 };
    result.file = CreateLargeFile(name, total_size);

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
    root.content = GlobalFileOffset(root.self, sizeof(SLM_File));
    WriteToFileAtOffset(&result.file, &root, sizeof(root), CONTENT(result.header.root));
    
    return result;    
}

static FileSystem SLM_OpenExistingFileSystem(char *name) {
    FileSystem result = { 0 };

    result.file = OpenExistingFile(name);
    ReadFromFile(&result.file, &result.header, sizeof(result.header));

    return result;
}


static inline void SLM_GetBlock(FileSystem *fs, block_index block, char buf[BLOCK_SIZE]) {
    ReadFromFileAtOffset(&fs->file, buf, BLOCK_SIZE, BLOCK_BEGIN(block));
}

typedef struct Block {
    u32 in_use;
    u32 used_before;
    u32 prev;
    u32 next;
    char *content;
} Block;

static inline Block ParseBlock(char buf[BLOCK_SIZE]) {
    Block result = { 0 };

    u32 *_buf = (u32*)buf;
    result.in_use = *_buf++;
    result.used_before = *_buf++;
    result.prev = *_buf++;
    result.next = *_buf++;
    result.content = buf;

    return result;
}

static SLM_File SLM_ReadFileMetaData(FileSystem *fs, block_index block) {
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

static inline SLM_File SLM_ReadRoot(FileSystem *fs) {
    return SLM_ReadFileMetaData(fs, fs->header.root);
}

static inline size_t SLM_ReadUsedSize(FileSystem *fs, block_index file) {
    size_t res;
    ReadFromFileAtOffset(&fs->file, &res, sizeof(res), GlobalFileOffset(file, OffsetOf(SLM_File, used_size)));
    return res;
}

static inline void SLM_ReadName(FileSystem *fs, block_index file, char *buf, size_t size) {
    ReadFromFileAtOffset(&fs->file, buf, size, GlobalFileOffset(file, OffsetOf(SLM_File, name)));
}

static inline void SLM_ReadExt(FileSystem *fs, block_index file, char *buf, size_t size) {
    ReadFromFileAtOffset(&fs->file, buf, size, GlobalFileOffset(file, OffsetOf(SLM_File, ext)));
}

static inline void SLM_WriteUsedSize(FileSystem *fs, block_index file, size_t used_size) {
    WriteToFileAtOffset(&fs->file, &used_size, sizeof(used_size), GlobalFileOffset(file, OffsetOf(SLM_File, used_size)));
}

static inline size_t SLM_ReadNBlocks(FileSystem *fs, block_index file) {
    size_t res;
    ReadFromFileAtOffset(&fs->file, &res, sizeof(res), GlobalFileOffset(file, OffsetOf(SLM_File, nblocks)));
    return res;
}

static inline void SLM_WriteNBlocks(FileSystem *fs, block_index file, size_t nblocks) {
    WriteToFileAtOffset(&fs->file, &nblocks, sizeof(nblocks), GlobalFileOffset(file, OffsetOf(SLM_File, nblocks)));
}

static inline block_index SLM_ReadNextBlockIndex(FileSystem *fs, block_index block) {
    block_index res;
    ReadFromFileAtOffset(&fs->file, &res, sizeof(res), NEXT(block));
    return res;
}

static inline u32 SLM_ReadIsDirectory(FileSystem *fs, block_index file) {
    u32 res;
    ReadFromFileAtOffset(&fs->file, &res, sizeof(res), GlobalFileOffset(file, OffsetOf(SLM_File, is_directory)));
    return res;
}

static inline block_index SLM_ReadParent(FileSystem *fs, block_index file) {
    block_index res;
    ReadFromFileAtOffset(&fs->file, &res, sizeof(res), GlobalFileOffset(file, OffsetOf(SLM_File, parent)));
    return res;
}

static inline void SLM_WriteParent(FileSystem *fs, block_index file, block_index parent) {
    WriteToFileAtOffset(&fs->file, &parent, sizeof(parent), GlobalFileOffset(file, OffsetOf(SLM_File, parent)));
}

static inline void SLM_WriteSelf(FileSystem *fs, block_index file) {
    WriteToFileAtOffset(&fs->file, &file, sizeof(file), GlobalFileOffset(file, OffsetOf(SLM_File, self)));
}

static inline void SLM_WriteContentOffset(FileSystem *fs, block_index file) {
    file_offset off = GlobalFileOffset(file, INIT_USED_SIZE);
    WriteToFileAtOffset(&fs->file, &off, sizeof(off), GlobalFileOffset(file, OffsetOf(SLM_File, content)));
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

static inline block_index SLM_GetNthBlock(FileSystem *fs, block_index base_block, u32 n) {
    block_index next_block = base_block;
    u32 index = 1;
    while(SLM_ReadNextBlockIndex(fs, next_block) != 0) {
        if(index == n)
            break;
        next_block = SLM_ReadNextBlockIndex(fs, next_block);
        index++;
    }
    return next_block;
}

static inline file_offset SLM_GetFileOffset(FileSystem *fs, block_index file) {
    size_t used_size = SLM_ReadUsedSize(fs, file);
    used_size += BLOCK_METADATA * SLM_ReadNBlocks(fs, file);

    block_index next_block = file;
    while(used_size >= BLOCK_SIZE) {
        next_block = SLM_ReadNextBlockIndex(fs, next_block);
        used_size -= BLOCK_SIZE;
    }

    return GlobalFileOffset(next_block, used_size) - BLOCK_METADATA;
}

static void SLM_WriteToFile(FileSystem *fs, block_index base_block, char *data, size_t size) {
    SLM_File file = SLM_ReadFileMetaData(fs, base_block);
    size_t available_size = SLM_GetAvailableSize(file);

    block_index next_block;
    if(available_size < size) {
        size_t additional_blocks = RoundUpDivision(size - available_size, USABLE_BLOCK_SIZE);
        file.nblocks += additional_blocks;
        next_block = SLM_ReserveBlocks(fs, RoundUpDivision(size - available_size, USABLE_BLOCK_SIZE));
        block_index prev_last_block = SLM_GetLastBlock(fs, base_block);
        WriteToFileAtOffset(&fs->file, &next_block, sizeof(next_block), NEXT(prev_last_block));
        WriteToFileAtOffset(&fs->file, &prev_last_block, sizeof(prev_last_block), PREV(next_block));
    }

    size_t size_to_write = MIN(size, available_size);
    file_offset write_at = SLM_GetFileOffset(fs, base_block);
    WriteToFileAtOffset(&fs->file, data, size_to_write, write_at);
    size_t written_size = size_to_write;

    if(size_to_write < size) {
        while(written_size < size) {
            write_at = CONTENT(next_block);
            size -= size_to_write;
            size_to_write = MIN(size, USABLE_BLOCK_SIZE);
            WriteToFileAtOffset(&fs->file, data + written_size, size_to_write, write_at);
            written_size += size_to_write;
            next_block = SLM_ReadNextBlockIndex(fs, next_block);
        }
    }

    file.used_size += written_size;
    SLM_WriteUsedSize(fs, base_block, file.used_size);
    SLM_WriteNBlocks(fs, base_block, file.nblocks);
}

static void SLM_WriteToFileAtOffset(FileSystem *fs, block_index base_block, char *data, size_t size, file_offset off) {
    SLM_File file = SLM_ReadFileMetaData(fs, base_block);
    off += INIT_USED_SIZE;
    if(off > file.used_size)
        return;

    size_t overflowed_size = (off + size > file.used_size) ? (off + size - file.used_size) : 0;

    u32 block_containing_off = RoundUpDivision(off, USABLE_BLOCK_SIZE);
    u32 offset_in_block = (off - (USABLE_BLOCK_SIZE * (block_containing_off - 1)));
    size_t available_size_in_block = USABLE_BLOCK_SIZE - offset_in_block;
    size_t total_available_size = file.used_size - off;

    if(total_available_size < size) {
        size_t additional_blocks = RoundUpDivision(size - total_available_size, USABLE_BLOCK_SIZE);
        file.nblocks += additional_blocks;
        block_index next_block = SLM_ReserveBlocks(fs, RoundUpDivision(size - total_available_size, USABLE_BLOCK_SIZE));
        block_index prev_last_block = SLM_GetLastBlock(fs, base_block);
        WriteToFileAtOffset(&fs->file, &next_block, sizeof(next_block), NEXT(prev_last_block));
        WriteToFileAtOffset(&fs->file, &prev_last_block, sizeof(prev_last_block), PREV(next_block));
    }

    size_t size_to_write = MIN(size, available_size_in_block);
    block_index write_in_block = SLM_GetNthBlock(fs, base_block, block_containing_off);
    file_offset write_at = GlobalFileOffset(write_in_block, offset_in_block);
    WriteToFileAtOffset(&fs->file, data, size_to_write, write_at);
    size_t written_size = size_to_write;

    if(size_to_write < size) {
        write_in_block = SLM_ReadNextBlockIndex(fs, write_in_block);
        while(written_size < size) {
            write_at = CONTENT(write_in_block);
            size_to_write = MIN(size - written_size, USABLE_BLOCK_SIZE);
            WriteToFileAtOffset(&fs->file, data + written_size, size_to_write, write_at);
            written_size += size_to_write;
            write_in_block = SLM_ReadNextBlockIndex(fs, write_in_block);
        }
    }

    file.used_size += overflowed_size;
    SLM_WriteUsedSize(fs, base_block, file.used_size);
    SLM_WriteNBlocks(fs, base_block, file.nblocks);
}

static void SLM_ReadFromFileAtOffset(FileSystem *fs, block_index base_block, char *buf, size_t size, file_offset off) {
    SLM_File file = SLM_ReadFileMetaData(fs, base_block);
    off += INIT_USED_SIZE;
    if(off > file.used_size)
        return;

    u32 block_containing_off = RoundUpDivision(off, USABLE_BLOCK_SIZE);
    u32 offset_in_block = (off - (USABLE_BLOCK_SIZE * (block_containing_off - 1)));
    size_t available_size_in_block = USABLE_BLOCK_SIZE - offset_in_block;
    size_t total_available_size = file.used_size - off;

    size_t total_size_to_read = MIN(total_available_size, size);
    size_t size_to_read = MIN(available_size_in_block, size);

    block_index read_from_block = SLM_GetNthBlock(fs, base_block, block_containing_off);
    file_offset read_from = GlobalFileOffset(read_from_block, offset_in_block);
    ReadFromFileAtOffset(&fs->file, buf, size_to_read, read_from);

    size_t size_read = size_to_read;

    if(size_read < total_size_to_read) {
        read_from_block = SLM_ReadNextBlockIndex(fs, read_from_block);
        while(size_read < total_size_to_read) {
            read_from = CONTENT(read_from_block);
            size_to_read = MIN(total_size_to_read - size_read, USABLE_BLOCK_SIZE);
            ReadFromFileAtOffset(&fs->file, buf + size_read, size_to_read, read_from);

            size_read += size_to_read;
            read_from_block = SLM_ReadNextBlockIndex(fs, read_from_block);
        }
    }

}

static inline u32 SLM_ReadNEntries(FileSystem *fs, block_index directory) {
    u32 res;
    ReadFromFileAtOffset(&fs->file, &res, sizeof(res), GlobalFileOffset(directory, INIT_USED_SIZE));
    return res;
}

static inline void SLM_WriteNEntries(FileSystem *fs, block_index directory, u32 nentries) {
    WriteToFileAtOffset(&fs->file, &nentries, sizeof(nentries), GlobalFileOffset(directory, INIT_USED_SIZE));
}

static inline void SLM_WriteFileName(FileSystem *fs, block_index file, char *name) {
    WriteToFileAtOffset(&fs->file, name, 128, GlobalFileOffset(file, OffsetOf(SLM_File, name)));
}

static inline SLM_DirectoryEntry SLM_ReadEntry(FileSystem *fs, block_index directory, u32 index) {
    SLM_DirectoryEntry entry;
    SLM_ReadFromFileAtOffset(fs, directory, (void*)&entry, sizeof(entry), index * sizeof(SLM_DirectoryEntry) + sizeof(u32));
    return entry;
}

static block_index SLM_GetChild(FileSystem *fs, block_index directory, char *child_name) {
    u32 nentries = SLM_ReadNEntries(fs, directory);
    for(u32 i = 0; i < nentries; ++i) {
        SLM_DirectoryEntry entry = SLM_ReadEntry(fs, directory, i);
        if(_strcmp(entry.name, child_name))
            return entry.base_block;
    }
    return 0;
}

static void SLM_DirectoryAddEntry(FileSystem *fs, block_index directory, SLM_DirectoryEntry *entry) {
    SLM_File directory_metadata = SLM_ReadFileMetaData(fs, directory);
    Assert(directory_metadata.is_directory);

    u32 nentries = SLM_ReadNEntries(fs, directory);
    if(nentries == 0) {
        SLM_WriteToFile(fs, directory, (void*)&nentries, sizeof(nentries));
    }

    SLM_WriteToFile(fs, directory, (void*)entry, sizeof(*entry));
    SLM_WriteNEntries(fs, directory, ++nentries);
}

static inline void SLM_ReplaceEntry(FileSystem *fs, block_index directory, u32 index, SLM_DirectoryEntry *entry) {
    SLM_WriteToFileAtOffset(fs, directory, (char*)entry, sizeof(*entry), index * sizeof(SLM_DirectoryEntry) + sizeof(u32));
}

static void SLM_DirectoryRemoveEntry(FileSystem *fs, block_index directory, block_index base_block) {
    u32 is_directory = SLM_ReadIsDirectory(fs, directory);
    Assert(is_directory);

    int i;
    u32 nentries = SLM_ReadNEntries(fs, directory);
    for(i = 0; i < nentries; ++i) {
        SLM_DirectoryEntry entry = SLM_ReadEntry(fs, directory, i);
        if(entry.base_block == base_block)
            break;
    }

    for(; i < nentries - 1; ++i) {
        SLM_DirectoryEntry next_entry = SLM_ReadEntry(fs, directory, i + 1);
        SLM_ReplaceEntry(fs, directory, i, &next_entry);
    }
    
    if(nentries) {
        nentries--;
        SLM_WriteNEntries(fs, directory, nentries);
    }
}

static u32 SLM_EntryExists(FileSystem *fs, block_index directory, char *name) {
    u32 nentries = SLM_ReadNEntries(fs, directory);
    for(int i = 0; i < nentries; ++i) {
        SLM_DirectoryEntry entry = SLM_ReadEntry(fs, directory, i);
        if(_strcmp(name, entry.name))
            return 1;
    }
    return 0;
 }

static inline SLM_File SLM_CreateEmptyDirectory(FileSystem *fs, char *name) {
    SLM_File result = { 0 };

    result.is_directory = 1;
    result.parent = 0;
    result.used_size = INIT_USED_SIZE;
    result.nblocks = 1;
    result.self = SLM_ReserveBlocks(fs, 1);
    result.content = GlobalFileOffset(result.self, INIT_USED_SIZE);
    _strcpy(name, result.name, _strlen(name));

    return result;
}

static inline char* ExtractExtension(char *name, size_t length) {
    Assert(length >= 5);

    char *res = name + length - 1;
    int size = 0;
    while(*res != '.' && size < 4) {
        res--;
        size++;
    }

    if(*res == '.') {
        *res = '\0';
        return res + 1;
    }
    return 0;
}

static inline SLM_File SLM_CreateEmptyFile(FileSystem *fs, char *name) {
    SLM_File file = { 0 };
    
    file.is_directory = 0;
    file.parent = 0;
    file.nblocks = 1;
    file.used_size = INIT_USED_SIZE;
    file.self = SLM_ReserveBlocks(fs, 1);
    file.content = GlobalFileOffset(file.self, INIT_USED_SIZE);

    char *ext = ExtractExtension(name, _strlen(name));
    _strcpy(name, file.name, _strlen(name));
    _strcpy(ext, file.ext, _strlen(ext));
    
    return file;
}

static block_index SLM_InsertNewDirectory(FileSystem *fs, char *name, block_index parent) {
    SLM_File directory = SLM_CreateEmptyDirectory(fs, name);
    directory.parent = parent;

    SLM_DirectoryEntry directory_entry = { 0 };
    directory_entry.base_block = directory.self;
    _strcpy(name, directory_entry.name, _strlen(name));

    WriteToFileAtOffset(&fs->file, &directory, sizeof(directory), CONTENT(directory.self));
    SLM_DirectoryAddEntry(fs, parent, &directory_entry);
    
    return directory.self;
}

static block_index SLM_InsertNewFile(FileSystem *fs, char *name, block_index parent) {
    SLM_DirectoryEntry entry = { 0 };
    _strcpy(name, entry.name, _strlen(name));

    SLM_File file = SLM_CreateEmptyFile(fs, name);
    file.parent = parent;
    entry.base_block = file.self;

    WriteToFileAtOffset(&fs->file, &file, sizeof(file), CONTENT(file.self));
    SLM_DirectoryAddEntry(fs, parent, &entry);

    return file.self;
}


static int SLM_BuildPath(FileSystem *fs, block_index file, char *res, size_t size) {
    char names[16][128] = { 0 };
    u32 depth = 0;
    block_index current = file;
    while(current != fs->header.root) {
        SLM_File current_file = SLM_ReadFileMetaData(fs, current);
        current = current_file.parent;
        _strcpy(current_file.name, names[depth++], 128);
    }
    SLM_File root = SLM_ReadRoot(fs);
    _strcpy(root.name, names[depth], 128);

    int bytes_written = 0;
    for(int i = depth; i >= 0; --i) {
        bytes_written += _strcpy(names[i], res + bytes_written, 128);
        res[bytes_written++] = '/';
    }

    return bytes_written - 1;
}

static void SLM_RenameEntry(FileSystem *fs, block_index parent, char *old_name, char *new_name) {
    u32 is_directory = SLM_ReadIsDirectory(fs, parent);
    if(!is_directory)
        return;

    u32 n_entries = SLM_ReadNEntries(fs, parent);
    for(int i = 0; i < n_entries; ++i) {
        SLM_DirectoryEntry entry = SLM_ReadEntry(fs, parent, i);
        if(_strcmp(entry.name, old_name)) {
            u32 char_copied = _strcpy(new_name, entry.name, 127);
            entry.name[char_copied] = '\0';
            
            SLM_WriteToFileAtOffset(fs, parent, (char*)&entry, sizeof(entry), i * sizeof(entry) + sizeof(u32));
            SLM_WriteFileName(fs, entry.base_block, entry.name);
            
            break;
        }
    }
}

static inline void SLM_ReadBlock(FileSystem *fs, block_index block, char buf[USABLE_BLOCK_SIZE]) {
    ReadFromFileAtOffset(&fs->file, buf, USABLE_BLOCK_SIZE, GlobalFileOffset(block, 0));
}

static inline void SLM_WriteBlock(FileSystem *fs, block_index block, char buf[USABLE_BLOCK_SIZE]) {
    WriteToFileAtOffset(&fs->file, buf, USABLE_BLOCK_SIZE, GlobalFileOffset(block, 0));
}


static void SLM_Copy(FileSystem *fs, block_index src, block_index dst) {
    u32 is_directory = SLM_ReadIsDirectory(fs, dst);
    Assert(is_directory);

    block_index parent = SLM_ReadParent(fs, src);
    u32 n_children = SLM_ReadNEntries(fs, parent);

    SLM_DirectoryEntry entry = { 0 };
    for(int i = 0; i < n_children; ++i) {
        entry = SLM_ReadEntry(fs, parent, i);
        if(entry.base_block == src)
            break;
    }

    if(SLM_EntryExists(fs, dst, entry.name)) {
        _strcpy("-copy", entry.name + _strlen(entry.name), 128);
    }

    is_directory = SLM_ReadIsDirectory(fs, src);
    if(is_directory){
        block_index src_copy = SLM_InsertNewDirectory(fs, entry.name, dst);
        u32 n_children = SLM_ReadNEntries(fs, src);

        for(int i = 0; i < n_children; ++i) {
            SLM_DirectoryEntry entry = SLM_ReadEntry(fs, src, i);
            SLM_Copy(fs, entry.base_block, src_copy);
        }
    }

    else {
        u32 n_blocks = SLM_ReadNBlocks(fs, src);
        block_index src_copy = SLM_ReserveBlocks(fs, n_blocks);
        entry.base_block = src_copy;
        SLM_DirectoryAddEntry(fs, dst, &entry);

        char buf[USABLE_BLOCK_SIZE];
        for(int i = 0; i < n_blocks; ++i) {
            SLM_ReadBlock(fs, src, buf);
            SLM_WriteBlock(fs, src_copy, buf);
            src = SLM_ReadNextBlockIndex(fs, src);
            src_copy = SLM_ReadNextBlockIndex(fs, src_copy);
        }

        SLM_WriteParent(fs, entry.base_block, dst);
        SLM_WriteSelf(fs, entry.base_block);
        SLM_WriteContentOffset(fs, entry.base_block);            
    }

}

static void SLM_Move(FileSystem *fs, block_index src, block_index dst) {
    u32 is_directory = SLM_ReadIsDirectory(fs, dst);
    Assert(is_directory);

    block_index parent = SLM_ReadParent(fs, src);
    u32 n_children = SLM_ReadNEntries(fs, parent);

    SLM_DirectoryEntry entry = { 0 };
    for(int i = 0; i < n_children; ++i) {
        entry = SLM_ReadEntry(fs, parent, i);
        if(entry.base_block == src)
            break;
    }

    if(SLM_EntryExists(fs, dst, entry.name)) {
        _strcpy("-copy", entry.name + _strlen(entry.name), 128);
    }

    SLM_DirectoryRemoveEntry(fs, parent, src);
    SLM_DirectoryAddEntry(fs, dst, &entry);
    SLM_WriteParent(fs, src, dst);
}

static void SLM_DeleteFile(FileSystem *fs, block_index file) {
    block_index parent = SLM_ReadParent(fs, file);
    SLM_DirectoryRemoveEntry(fs, parent, file);

    u32 is_directory = SLM_ReadIsDirectory(fs, file);
    if(is_directory) {
        u32 nentries = SLM_ReadNEntries(fs, file);
        for(u32 i = 0; i < nentries; ++i) {
            SLM_DirectoryEntry entry = SLM_ReadEntry(fs, file, i);
            SLM_DeleteFile(fs, entry.base_block);
        }
    }
    SLM_FreeBlocks(fs, file);
}

#define SLIM64_C
#endif