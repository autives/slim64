#include "string.c"
#include "m_alloc.c"
#include "explorer.h"


char* GetCommand(char **str) {
    char *src_str = *str;
    while(*src_str == ' ' || *src_str == '\t')
        src_str++;

    char *result = src_str;
    int index = 0;
    while(src_str[index]) {
        if(src_str[index] == ' ' || src_str[index] == '\t'){
            src_str[index] = 0;
            break;
        }
        index++;
    }
    *str = src_str + index + 1;
    return src_str;
}

char* GetString(char **str) {
    int insideQuotes = 0;
    int escaping = 0;
    char *escapeCharaterToBeRemoved = 0;

    char *src_str = *str;
    char *result = src_str;

    while(*src_str) {
        if(*src_str == '\\'){
            if(insideQuotes){
                escaping = !escaping;
                src_str++;
            }
        }

        if(*src_str == '"') {
            if(!escaping) {
                if(insideQuotes)
                    *src_str = 0;
                else{
                    src_str++;
                    result++;
                }
                insideQuotes = !insideQuotes;
                
                if(escapeCharaterToBeRemoved) {
                    while(*(escapeCharaterToBeRemoved + 1)) {
                        *escapeCharaterToBeRemoved = *(escapeCharaterToBeRemoved + 1);
                        escapeCharaterToBeRemoved++;
                    }
                    *escapeCharaterToBeRemoved = *(escapeCharaterToBeRemoved + 1);
                    escapeCharaterToBeRemoved = 0;
                }
            }
            else
                escapeCharaterToBeRemoved = src_str - 1;
        }

        if(*src_str == ' '){
            if(!insideQuotes) {
                *src_str = '\0';
                src_str++;
                break;
            }
        }

        escaping = 0;
        src_str++;
    }
    *str = src_str;
    return result;
}

void* DoNothing(Allocator *arena, char **str) {
    return 0;
}

void* ExtractChangeDirectoryArgs(Allocator *arena, char **str) {
    Path *args = m_alloc(arena, sizeof(Path));
    
    args->target = GetString(str);
    if(**str != '\0')
        return 0;

    return args;
}

void* ExtractMakeDirectoryArgs(Allocator *arena, char **str) {
    MakeDirectoryArgs *args = m_alloc(arena, sizeof(*args));
    args->names = VectorBegin(arena, 2, sizeof(char*));

    while(**str != 0) {
        char *name = GetString(str);
        VectorPush(&args->names, &name);
    }
    if(!args->names.count) {
        VectorFree(&args->names);
        m_free(arena, args);
    }

    return args;
}

void* ExtractMakeFileArgs(Allocator *arena, char **str) {
    return ExtractMakeDirectoryArgs(arena, str);
}

void* ExtractRenameArgs(Allocator *arena, char **str) {
    ChangeName *args = m_alloc(arena, sizeof(ChangeName));

    args->old_name = GetString(str);
    args->new_name = GetString(str);

    return args;
}

void* ExtractCopyArgs(Allocator *arena, char **str) {
    CopyArgs *args = m_alloc(arena, sizeof(*args));
    args->names = VectorBegin(arena, 2, sizeof(char*));

    while(**str != 0) {
        char *name = GetString(str);
        VectorPush(&args->names, &name);
    }
    if(!args->names.count) {
        VectorFree(&args->names);
        m_free(arena, args);
    }

    return args;
}

void* ExtractMoveArgs(Allocator *arena, char **str) {
    return ExtractCopyArgs(arena, str);
}

void* ExtractImportArgs(Allocator *arena, char **str) {
    ImportArgs *args = m_alloc(arena, sizeof(*args));
    
    args->src = GetString(str);
    args->dst = GetString(str);

    return args;
}

void* ExtractSearchArgs(Allocator *arena, char **str) {
    SearchArgs *args = m_alloc(arena, sizeof(*args));
    args->name = GetString(str);
    return args;
}

void* ExtractFindArgs(Allocator *arena, char **str) {
    FindArgs *args = m_alloc(arena, sizeof(*args));
    args->files = VectorBegin(arena, 2, sizeof(char*));

    args->str_to_search = GetString(str);
    while(**str != 0) {
        char *name = GetString(str);
        VectorPush(&args->files, name);
    }
    if(!args->files.count) {
        VectorFree(&args->files);
        m_free(arena, args);
    }

    return args;
}

void* ExtractDeleteArgs(Allocator *arena, char **str) {
    DeleteArgs *args = m_alloc(arena, sizeof(*args));
    args->names = VectorBegin(arena, 2, sizeof(char*));

    while(**str != 0) {
        char *name = GetString(str);
        VectorPush(&args->names, &name);
    }
    if(!args->names.count) {
        VectorFree(&args->names);
        m_free(arena, args);
    }
    
    return args;
}

void* (*argument_extractor[c_total]) (Allocator *arena, char **str) = 
{
    DoNothing,
    DoNothing,
    DoNothing,
    DoNothing,
    DoNothing,
    ExtractChangeDirectoryArgs,
    DoNothing,
    ExtractMakeDirectoryArgs,
    ExtractMakeFileArgs,
    ExtractRenameArgs,
    ExtractCopyArgs,
    ExtractMoveArgs,
    ExtractImportArgs,
    ExtractSearchArgs,
    ExtractFindArgs,
    ExtractDeleteArgs,
};


typedef struct ExecutionBlock {
    Commands command;
    void *arg;
} ExecutionBlock;


static ExecutionBlock ExtractCommand(Allocator *arena, char *input) {
    ExecutionBlock result =  { 0 };

    char *command = GetCommand(&input);
    for(int i = 0; i < c_total; ++i) {
        if(_strcmp(command, Command_Strings[i])) {
            result.command = i;
            result.arg = argument_extractor[i](arena, &input);
            break;
        }
    }

    return result;
}
