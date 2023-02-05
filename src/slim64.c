#include "common.h"
#include "m_alloc.c"

#pragma pack(push, 1)

typedef struct  SLM_FileContentOffset {
    u32 size;
    u32 nreferences;
    u32 offset;
    file_offset references[16];
    void *data;
} SLM_Content;

typedef struct SLM_File{
    size_t size;
    size_t nblocks;

    char name[124];
    char ext[4];

    u32 is_directory;

    SLM_Content *parent;
    SLM_Content *self;
    SLM_Content *content;
} SLM_File;

typedef struct SLM_Directory {
    size_t nchildren;

    SLM_Content *self;
    SLM_Content *parent;
    SLM_Content *children;
} SLM_Directory;

typedef struct {
    size_t total_size;
    size_t block_size;
    size_t used_size;
    size_t used_blocks;
    size_t padding;

    size_t nfiles;
    size_t ndirectories; 

    SLM_Content *root;
} SLM_Header;
#pragma pack(pop)

SLM_Content* SLM_GetEmptyDirectory(Arena *arena, SLM_Content *self, SLM_Content *parent) {
    SLM_Content *result = PushStruct(arena, SLM_Content);
    result->data = PushStruct(arena, SLM_Content);
    result->nreferences = 0;
    result->offset = INVALID_FILE_OFFSET;
    result->size = sizeof(SLM_Directory);

    SLM_Directory *data = result->data;
    data->nchildren = 0;
    data->children = 0;
    data->parent = parent;
    data->self = self;

    return result;
}

SLM_Header *SLM_CreateNewFileSystem(Arena *arena, size_t block_size, size_t total_size) {
    SLM_Header *result = PushStruct(arena, SLM_Header);
    if(!result)
        return 0;

    result->block_size = block_size;
    result->total_size = total_size;
    result->ndirectories = 1;
    result->nfiles = 1;
    result->padding = 0;
    result->root = PushStruct(arena, SLM_Content);
    result->root->size = (u32)(sizeof(SLM_File) - 3 * sizeof(SLM_Content*));
    result->root->data = PushStruct(arena, SLM_File);
    if(!result->root->data)
        return 0;
    
    SLM_File *root = result->root->data;
    _strcpy("root", root->name, 5);
    root->is_directory = 1;
    root->nblocks = 1;
    root->parent = 0;
    root->self = result->root;
    root->size = sizeof(SLM_File) + sizeof(SLM_Directory);
    root->content = SLM_GetEmptyDirectory(arena, result->root, 0);

    return result;    
}

void AddSLMContentReference(active_file *file, SLM_Content *content) {
    content->references[content->nreferences++] = file->write_offset;
    
    file_offset dummy = INVALID_FILE_OFFSET;
    WriteToFile(file, &dummy, sizeof(file_offset));
}

int ResolveSLMContentReference(active_file *file, SLM_Content *content) {
    if(content->offset == INVALID_FILE_OFFSET)
        return 0;

    for(int i = 0; i < content->nreferences; ++i) {
        WriteToFileAtOffset(file, &content->offset, sizeof(content->offset), content->references[i]);
    }
    return 1;
}

void WriteSLMContentToFile(active_file *file, SLM_Content *content) {
    content->offset = file->write_offset;
    ResolveSLMContentReference(file, content);
    WriteToFile(file, content->data, content->size);
}

void SLM_WriteHeader(active_file *file, SLM_Header *header) {
    WriteToFile(file, "SLM FILE", 8);
    WriteToFile(file, header, sizeof(*header) - sizeof(header->root));
    AddSLMContentReference(file, header->root);

    WriteSLMContentToFile(file, header->root);
    SLM_File *root = header->root->data;
    WriteToFile(file, "\0\0\0\0", sizeof(root->parent->offset));
    AddSLMContentReference(file, root->self);
    AddSLMContentReference(file, root->content);
    
    ResolveSLMContentReference(file, root->self);
}