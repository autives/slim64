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

void* DoNothing(Arena *arena, char **str) {
    return 0;
}

void* ExtractChangeDirectoryArgs(Arena *arena, char **str) {
    Path *args = PushStruct(arena, Path);
    
    args->target = GetString(str);
    if(**str != '\0')
        return 0;

    return args;
}

void* ExtractMakeDirectoryArgs(Arena *arena, char **str) {
    MakeDirectoryArgs *args = PushStruct(arena, *args);
    args->names = VectorBegin(arena, 2, sizeof(char*));

    while(**str != 0) {
        char *name = GetString(str);
        VectorPush(&args->names, &name);
    }
    if(!args->names.count) {
        VectorFree(&args->names);
        args = 0;
    }

    return args;
}

void* ExtractMakeFileArgs(Arena *arena, char **str) {
    return ExtractMakeDirectoryArgs(arena, str);
}

void* ExtractRenameArgs(Arena *arena, char **str) {
    ChangeName *args = PushStruct(arena, ChangeName);

    args->old_name = GetString(str);
    args->new_name = GetString(str);

    return args;
}

void* ExtractCopyArgs(Arena *arena, char **str) {
    CopyArgs *args = PushStruct(arena, CopyArgs);
    args->names = VectorBegin(arena, 2, sizeof(char*));

    while(**str != 0) {
        char *name = GetString(str);
        VectorPush(&args->names, &name);
    }
    if(!args->names.count) {
        VectorFree(&args->names);
        args = 0;
    }

    return args;
}

void* ExtractMoveArgs(Arena *arena, char **str) {
    return ExtractCopyArgs(arena, str);
}

void* ExtractImportArgs(Arena *arena, char **str) {
    ImportArgs *args = PushStruct(arena, ImportArgs);
    
    args->src = GetString(str);
    args->dst = GetString(str);

    return args;
}

void* ExtractSearchArgs(Arena *arena, char **str) {
    SearchArgs *args = PushStruct(arena, SearchArgs);
    args->name = GetString(str);
    return args;
}

void* ExtractFindArgs(Arena *arena, char **str) {
    FindArgs *args = PushStruct(arena, FindArgs);
    args->files = VectorBegin(arena, 2, sizeof(char*));

    args->str_to_search = GetString(str);
    while(**str != 0) {
        char *name = GetString(str);
        VectorPush(&args->files, name);
    }
    if(!args->files.count) {
        VectorFree(&args->files);
        args = 0;
    }

    return args;
}

void* ExtractDeleteArgs(Arena *arena, char **str) {
    DeleteArgs *args = PushStruct(arena, DeleteArgs);
    args->names = VectorBegin(arena, 2, sizeof(char*));

    while(**str != 0) {
        char *name = GetString(str);
        VectorPush(&args->names, &name);
    }
    if(!args->names.count) {
        VectorFree(&args->names);
        args = 0;
    }
    
    return args;
}

void* (*argument_extractor[c_total]) (Arena *arena, char **str) = 
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


static ExecutionBlock ExtractCommand(Arena *arena, char *input) {
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
