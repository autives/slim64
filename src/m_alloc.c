#include <stddef.h>
#if !defined(M_ALLOC)

#include "common.h"
#include "platform.c"

#define ARENA_ALLOCATION_SIZE 512

typedef struct memory_arena {
    u32 is_temp;
    size_t total_size;
    size_t used_size;
    char *mem;
} Arena;

int InitMemArena(Arena *arena, size_t size) {
    arena->total_size = size;
    arena->used_size = 0;
    arena->mem = MemAlloc(size);
    arena->is_temp = 0;
    if(!arena->mem)
        return 0;
    return 1;
}

int InitTempArena(Arena *arena, size_t size) {
    int res = InitMemArena(arena, size);
    if(res) arena->is_temp = 1;
    return res;
}

void* PushSize(Arena *arena, size_t size) {
    if(arena->used_size + size >= arena->total_size){
        if(!arena->is_temp)
            return 0;
        if(size > arena->total_size)
            return 0;
        else
            arena->used_size = 0;
    }

    void *res= arena->mem + arena->used_size;
    arena->used_size += size;
    return res;
}

#define PushStruct(arena, type) PushSize(arena, sizeof(type))
#define PushArray(arena, type, count) PushSize(arena, count * sizeof(type))
#define PushString(arena, length) PushArray(arena, char, length)


typedef struct Vector {
    void *mem;
    u32 count;
    u32 total;
    size_t unit_size;
    Arena *arena;
} Vector;

Vector VectorBegin(Arena *arena, u32 init_count, size_t unit_size) {
    Vector vec = { 0 };

    vec.arena = arena;
    vec.unit_size = unit_size;
    vec.total = init_count;
    vec.count = 0;
    vec.mem = PushSize(arena, unit_size * init_count);

    return vec;
}

static void m_copy(void *src, void *dst, size_t size) {
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

void VectorPush(Vector *vec, void *val) {
    if(vec->count >= vec->total) {
        void *new = PushSize(vec->arena, 2 * vec->total * vec->unit_size);
        m_copy(vec->mem, new, vec->total * vec->unit_size);
        vec->total *= 2;
    }

    m_copy(val, (char*)vec->mem + vec->unit_size * vec->count++, vec->unit_size);
}

void VectorFree(Vector *vec) {
    vec->total = 0;
    vec->count = 0;
}

static void* VectorGet(Vector *vec, int index) {
    if(index > vec->count) 
        return 0;

    return ((char*)vec->mem) + vec->unit_size * index;
}


#define M_ALLOC
#endif