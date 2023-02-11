#include <stddef.h>
#include <inttypes.h>

#define UINT_MAX 0xffffffff

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t   i8;
typedef int16_t  i16;
typedef int32_t  i32;
typedef int64_t  i64;

typedef u64 file_offset;
typedef u32 block_index;

#define Assert(exp)                 \
    if(exp == 0) {                  \
        *(int*)1 = 0;               \
    }

#define OffsetOf(structure, element) (ptrdiff_t)&(((structure*)0)->element)
#define RoundUpDivision(a, b) ((a) + (b) - 1 ) / (b)

#define MIN(a, b) ((a) < (b)) ? (a) : (b)
#define MAX(a, b) ((a) > (b)) ? (a) : (b)

#define KiloBytes(n) ((n) * 1024LL)
#define MegaBytes(n) (KiloBytes(n) * 1024LL)
#define GigaBytes(n) (MegaBytes(n) * 1024LL)
