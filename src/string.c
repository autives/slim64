#include <stddef.h>
#if !defined(STRING)
    
#include "common.h"

int _strcmp(const char *str1, const char *str2) {
    if(!str1 || !str2)
        return 0;
    
    while(*str1 && *str2) {
        if(*str1 != *str2)
            return 0;
        str1++;
        str2++;
    }
    if(*str1 || *str2)
        return 0;

    return 1;
}

int _strlen(const char *src) {
    if(!src)
        return -1;
    
    int count = 0;
    while(*src) {
        count++;
        src++;;
    }
    return count;
}

int _strcpy(const char *src, char *dst, size_t size) {
    if(!src || !dst)
        return 0;

    int len = _strlen(dst) + 1, count = 0;
    for(; count < size + len; ++count) {
        if(src[count] == '\0') 
            break;
        dst[count] = src[count];
    }
    return count;
}

int contains(const char *str, const char *token) {
    int index = 0;
    while(str[index]) {
        int token_index = 0;
        if(str[index] == token[token_index]) {
            int tmp_index = index;
            while(str[tmp_index++] && token[token_index++]) {
                if(str[tmp_index] != token[token_index])
                    break;
            }
            if(!token[token_index])
                return index;
        }
        index++;
    }
    return 0;
}

#define STRING
#endif