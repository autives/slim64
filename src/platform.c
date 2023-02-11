#if !defined(PLATFORM_C)

#include "common.h"
#include <stddef.h>

#define FILE_READONLY  0b00000001
#define FILE_WRITEONLY 0b00000010
#define FILE_READWRITE 0b00000100

#define INVALID_FILE_OFFSET UINT_MAX

#define FPOINTER_READ  0
#define FPOINTER_WRITE 1


#if defined _WIN32

#include <Windows.h>

typedef HANDLE file_handle;

#define FOFFSET_BEGIN FILE_BEGIN
#define FOFFSET_CURRENT FILE_CURRENT
#define FOFFSET_END FILE_END


#elif defined __linux__
#include <stdarg.h>

#define FOFFSET_BEGIN 0   // SEEK_SET
#define FOFFSET_CURR  1   // SEEK_CURRENT
#define FOFFSET_END   2   // SEEK_END

#include <sys/stat.h>
#include <sys/fcntl.h>
#include <sys/mman.h>

typedef uint32_t DWORD;
typedef void* HANDLE;
typedef i32 file_handle;

int _write(u32 fd, const char *buf, size_t count)
{
    asm("mov $1, %rax;"
        "syscall");
}

int _read(u32 fd, char *buf, size_t count)
{
    asm("mov $0, %rax;"
        "syscall");
}

void end(int code)
{
    asm("mov $60, %rax;"
        "syscall");
}


typedef u16 umode_t;
int _open(const char *filename, int flags, umode_t mode)
{
    asm("mov $0x02, %rax;"
        "syscall");    
}

int _close(u32 fd) {
    asm("mov $0x03, %rax;"
        "syscall");    
}

int _lseek(int fd, int offset, int whence)
{
    asm("mov $0x08, %rax;"
        "syscall");
}

int _stat(u32 fd, struct stat *statbuf)
{
    asm("mov $0x05, %rax;"
        "syscall");
}

void *_mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset)
{
    asm("mov $0x9, %rax;"
        "mov %rcx, %r10;"
        "syscall");
}

int _munmap(void *addr, size_t length)
{
    asm("mov $0x0b, %rax;"
        "syscall");
}
#endif

#if defined(_WIN32)
static inline DWORD ConsoleOut(HANDLE Console, const char *buf, DWORD nchar){
    DWORD bytes;
    WriteConsoleA(Console, buf, nchar, &bytes, 0);
    return bytes;
}
#elif defined(__linux__)
static inline DWORD ConsoleOut(u32 fd, const char *buf, size_t nchar){
    DWORD bytes;
    bytes = _write(fd, buf, nchar);
    return bytes;
}
#endif

#if defined(_WIN32)

static inline DWORD ConsoleIn(HANDLE Console, char *buf, DWORD nchar) {
    DWORD bytes;
    ReadConsoleA(Console, buf, nchar, &bytes, 0);
    return bytes;
}
#elif defined(__linux__)
static inline DWORD ConsoleIn(u32 fd, char *buf, size_t nchar) {
    DWORD bytes;
    bytes = _read(fd, buf, nchar);
    return bytes;
}
#endif

typedef struct loaded_file {
    void *content;
    u64 size;
} loaded_file;

typedef struct active_file {
    file_handle handle;
    u32 permissions;
    file_offset read_offset;
    file_offset write_offset;
    file_offset end;
} active_file;

#if defined(_WIN32) 



static loaded_file ReadEntireFile(const char *file_name) {
    loaded_file result = { 0 };
    HANDLE FileHandle = CreateFileA(file_name, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
    if(FileHandle == INVALID_HANDLE_VALUE)
        return result;


    LARGE_INTEGER FileSizeResult;
    if(!GetFileSizeEx(FileHandle, &FileSizeResult)) {
        CloseHandle(FileHandle);
        return result;
    }
    result.size = (DWORD)FileSizeResult.QuadPart;
    result.content = VirtualAlloc(0, result.size, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);

    if(result.content) {
        DWORD BytesRead;
        if(!ReadFile(FileHandle, result.content, result.size, &BytesRead, 0)){
            VirtualFree(result.content, 0, MEM_RELEASE);
            result.content = 0;
        }
    }
    CloseHandle(FileHandle);
    return result;
}

static inline void FreeFileMemory(loaded_file file) {
    VirtualFree(file.content, 0, MEM_RELEASE);
}

static int WriteEntireFile(loaded_file file, const char *file_name) {
    if(!file.size)
        return 0;
    
    HANDLE FileHandle = CreateFileA(file_name, GENERIC_WRITE, FILE_SPECIAL_ACCESS, 0, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
    if(FileHandle == INVALID_HANDLE_VALUE)
        return 0;

    DWORD bytes_written = 0;
    WriteFile(FileHandle, file.content, file.size, &bytes_written, 0);

    if(!bytes_written)
        return 0;
    while(bytes_written < file.size) {
        DWORD res = 0;
        WriteFile(FileHandle, (char*)file.content + bytes_written, file.size - bytes_written, &res, 0);
        bytes_written += res;        
    }

    return 1;
}

#elif defined(__linux__)

static inline loaded_file ReadEntireFile(const char *file_name) {
    loaded_file result = { 0 };

    int fd = _open(file_name, O_RDONLY, 0);
    if(fd < 0) 
        return result;

    struct stat fstat;
    _stat(fd, &fstat);

    result.size = fstat.st_size;
    result.content = _mmap(0, fstat.st_size, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, 0, 0);
    _read(fd, result.content, result.size);
    _close(fd);
    
    return result;
}

static inline void FreeFileMemory(loaded_file file) {
    _munmap(file.content, file.size);
}

static int WriteEntireFile(loaded_file file, char *file_name) {
    if(!file.size)
        return 0;

    int fd = _open(file_name, O_WRONLY | O_CREAT, S_IRWXU);
    if(fd < 0)
        return 0;
    
    _write(fd, file.content, file.size);
} 

#endif


active_file CreateNewFile(const char *file_name) {
    active_file result;

#if defined(_WIN32)   
    result.handle = CreateFileA(file_name, GENERIC_READ|GENERIC_WRITE, FILE_SHARE_READ, 0, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
#elif defined(__linux__)
    result.handle = _open(file_name, O_RDWR|O_CREAT, S_IRWXU);
#endif
    result.read_offset = 0;
    result.write_offset = 0;
    result.end = 0;
    result.permissions = FILE_READWRITE;

    return result;
}

active_file CreateLargeFile(const char *file_name, size_t size) {
    active_file result = CreateNewFile(file_name);

#if defined(_WIN32)
    SetFilePointer(result.handle, size, 0, FOFFSET_BEGIN);
    SetEndOfFile(result.handle);
    SetFilePointer(result.handle, 0, 0, FOFFSET_BEGIN);
#elif defined(__linux__)
    _lseek(result.handle, size, SEEK_SET);
    u32 dummy = 0;
    _write(result.handle, (char *)&dummy, sizeof(dummy));
    _lseek(result.handle, 0, SEEK_SET);
#endif
    result.end = size;
    
    return result;
}

active_file OpenExistingFile(const char *file_name) {
    active_file result = { 0 };

#if defined(_WIN32)
    result.handle = CreateFileA(file_name, GENERIC_READ|GENERIC_WRITE, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
    
    LARGE_INTEGER LI_size;
    GetFileSizeEx(result.handle, &LI_size);

    result.end = LI_size.QuadPart;
#elif defined(__linux__)
    result.handle = _open(file_name, O_RDWR, 0);

    struct stat fstat;
    _stat(result.handle, &fstat);

    result.end = fstat.st_size;
#endif

    result.read_offset = 0;
    result.write_offset = 0;
    result.permissions = FILE_READWRITE;


    return result;
}

int WriteToFile(active_file *file, void *buf, size_t size) {
    if(!(file->permissions & (FILE_READWRITE | FILE_WRITEONLY)))
        return 0;

    Assert(size >= 0);
    
    DWORD bytes_written = 0;
#if defined(_WIN32)
    SetFilePointer(file->handle, file->write_offset, 0, FOFFSET_BEGIN);
    if(!WriteFile(file->handle, buf, size, &bytes_written, 0))
        return 0;
#elif defined(__linux__)
    _lseek(file->handle, file->write_offset, FOFFSET_BEGIN);
    bytes_written = _write(file->handle, buf, size);
    if(bytes_written < 0)
        return 0;
#endif
    file->write_offset += bytes_written;
    file->end += bytes_written;
    return bytes_written;
}

int WriteToFileAtOffset(active_file *file, void *buf, size_t size, file_offset off) {
    if(off > file->end) 
        return 0;
    
    file_offset prev_offset = file->write_offset;
    file->write_offset = off;
    int res = WriteToFile(file, buf, size);
    file->write_offset = prev_offset;

    return res;
}

int ReadFromFile(active_file *file, void *buf, size_t size) {
    if(!(file->permissions & (FILE_READWRITE | FILE_READONLY)))
        return 0;
    
    DWORD bytes_read = 0;
#if defined(_WIN32)
    SetFilePointer(file->handle, file->read_offset, 0, FOFFSET_BEGIN);
    if(!ReadFile(file->handle, buf, size, &bytes_read, 0))
        return 0;
#elif defined(__linux__)
    _lseek(file->handle, file->read_offset, FOFFSET_BEGIN);
    bytes_read = _read(file->handle, buf, size);
    if(bytes_read < 0)
        return 0;
#endif

    file->read_offset += bytes_read;
    return bytes_read;

}

int ReadFromFileAtOffset(active_file *file, void *buf, size_t size, file_offset off) {
    if(off > file->end)
        return 0;
    
    file_offset prev_offset = file->read_offset;
    file->read_offset = off;
    int res = ReadFromFile(file, buf, size);
    file->read_offset = prev_offset;

    return res;
}



void CloseFile(active_file *file) {
#if defined(_WIN32)
    CloseHandle(file->handle);
#elif defined(__linux__)
    _close(file->handle);
#endif
}

int MoveFilePointer(active_file *file, int offset, int relative, int fpointer) {
    if(relative == FOFFSET_BEGIN && offset < 0)
        return 0;
    if(relative == FOFFSET_END && offset > file->end)
        return 0;

    if(fpointer == FPOINTER_READ) {
        if(file->read_offset + offset > file->end)
            return 0;
        file->read_offset += offset;
#if defined(_WIN32) 
        return SetFilePointer(file->handle, offset, 0, relative);
#elif defined(__linux__)
        return  _lseek(file->handle, offset, relative);
#endif
    }
    else if(fpointer == FPOINTER_WRITE) {
        if(file->write_offset + offset > file->end)
            return 0;
        file->write_offset += offset;
#if defined(_WIN32) 
        return SetFilePointer(file->handle, offset, 0, relative);
#elif defined(__linux__)
        return _lseek(file->handle, offset, relative);
#endif
    }
    return 0;
}

void *MemAlloc(size_t size) {
#if defined(_WIN32)
    return VirtualAlloc(0, size, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
#elif defined(__linux__)
    return _mmap(0, size, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, 0, 0);
#endif
}

#define PLATFORM_C
#endif