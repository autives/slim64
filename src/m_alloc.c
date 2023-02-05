#include <stddef.h>
#if !defined(M_ALLOC)

#include "common.h"
#include "platform.c"

#define ARENA_ALLOCATION_SIZE GigaBytes(1)
#define CHUNK_LIST_SIZE 1024

typedef struct memory_arena {
    size_t total_size;
    size_t used_size;
    char *mem;
} Arena;

int InitMemArena(Arena *arena, size_t size) {
    arena->total_size = size;
    arena->used_size = 0;
    arena->mem = MemAlloc(size);
    if(!arena->mem)
        return 0;
    return 1;
}

void* PushSize(Arena *arena, size_t size) {
    if(arena->used_size + size >= arena->total_size)
        return 0;

    void *res= arena->mem + arena->used_size;
    arena->used_size += size;
    return res;
}

#define PushStruct(arena, type) PushSize(arena, sizeof(type))
#define PushArray(arena, type, count) PushSize(arena, count * sizeof(type))

typedef struct {
    size_t total_size;
    size_t used_size;
    void *start;

    void *last_allocated_address;

    u32 allocated_chunk_count;
    void *allocated_chunks[CHUNK_LIST_SIZE];
} Allocator;

void InitAllocator(Allocator *arena) {
    arena->start = MemAlloc(ARENA_ALLOCATION_SIZE);
    if(!arena->start)
        return;
    
    arena->total_size = ARENA_ALLOCATION_SIZE;
    arena->used_size = 0;
    arena->last_allocated_address = arena->start;
    arena->allocated_chunk_count = 0;
}

void *m_alloc(Allocator *arena, size_t size) {
    u32 *chunk_addr = arena->last_allocated_address;
    while(*chunk_addr == 1) {
        chunk_addr += 2 + (*(chunk_addr + 1));
        if((ptrdiff_t)chunk_addr > (ptrdiff_t)arena->start + arena->total_size)
            chunk_addr = arena->start;
    }
    *chunk_addr = 1;
    *(chunk_addr + 1) = size;
    arena->last_allocated_address = (char*)(chunk_addr + 2) + size;
    return (chunk_addr + 2);    
}

void m_free(Allocator *arena, void *ptr) {
    u32 *chunk_addr = ptr;
    chunk_addr -= 2;
    *chunk_addr = 0;
}

void m_copy(void *src, void *dst, size_t size) {
    if(!(size & 0x03)) {            //multiple of 4
        u32 *_src = src, *_dst = dst;
        u32 _size = size >> 2;
        for(int i = 0; i < _size; ++i) {
            _dst[i] = _src[i];
        }
    }
    else {
        char *_src = src, *_dst = dst;
        for(int i = 0; i < size; ++i) {
            _dst[i] = _src[i];
        }
    }
}

void *m_realloc(Allocator *arena, void *ptr, size_t size) {
    u32 *old_chunk_addr = (u32*)ptr - 2;
    u32 old_size = *(old_chunk_addr + 1);
    if(old_size > size) {
        u32 *free_chunk = (u32*)((char*)ptr + old_size);
        *free_chunk = 0;
        *(free_chunk + 1) = old_size - size - 2;
        *(old_chunk_addr + 1) = size;   

        return ptr;     
    }

    u32 *new_chunk_addr = arena->last_allocated_address;
    while(*new_chunk_addr == 1) {
        new_chunk_addr += 2 + (*(new_chunk_addr + 1));
        if((ptrdiff_t)new_chunk_addr > (ptrdiff_t)arena->start + arena->total_size)
            new_chunk_addr = arena->start;
    }
    *old_chunk_addr = 0; // free old memory
    *new_chunk_addr = 1;
    *(new_chunk_addr + 1) = size;
    m_copy(ptr, new_chunk_addr + 2, old_size);

    return (new_chunk_addr + 2);
}


typedef struct Vector {
    void *mem;
    u32 count;
    u32 total;
    size_t unit_size;
    Allocator *arena;
} Vector;

Vector VectorBegin(Allocator *arena, u32 init_count, size_t unit_size) {
    Vector vec = { 0 };

    vec.arena = arena;
    vec.unit_size = unit_size;
    vec.total = init_count;
    vec.count = 0;
    vec.mem = m_alloc(arena, unit_size * init_count);

    return vec;
}

void VectorPush(Vector *vec, void *val) {
    if(vec->count >= vec->total) {
        vec->mem = m_realloc(vec->arena, vec->mem, 2 * vec->total * vec->unit_size);
        vec->total *= 2;
    }

    m_copy(val, (char*)vec->mem + vec->unit_size * vec->count++, vec->unit_size);
}

void VectorFree(Vector *vec) {
    m_free(vec->arena, vec->mem);
    vec->total = 0;
    vec->count = 0;
}


#define M_ALLOC
#endif