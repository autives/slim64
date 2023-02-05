#if !defined(CONSOLE_IO)

#include "parse_and_build.c"
#include "platform.c"

#define READ_BUFFER_SIZE 1024

int len(const char *buf){
    int size = 0;
    while(*buf) {
        size++; buf++;
    }
    return size;
}



DWORD print(const char *format, ...) {   
#if defined(_WIN32)
    HANDLE OutputConsole = GetStdHandle(STD_OUTPUT_HANDLE);
#elif defined(__linux__)
    unsigned int OutputConsole = 1;
#endif

    va_list args;
    va_start(args, format);

    DWORD bytes_written = 0;
    const char *tmp = format;
    const char *next_buffer_begin = tmp;
    int nchar_to_print = 0;

    while(1){
        char ch = *tmp;

        switch(ch){
            case 0:
            {   
                bytes_written += ConsoleOut(OutputConsole, next_buffer_begin, nchar_to_print);
                return bytes_written;
            } break;

            case '%':
            {
                tmp++;
                char specifier = *tmp++;

                bytes_written += ConsoleOut(OutputConsole, next_buffer_begin, nchar_to_print);

                int length_to_print = -1;
                int decimals_to_print = -1;

                if(isDigit(specifier)) {
                    tmp--;
                    ExtractInteger((char **)&tmp, (int*)&bytes_written);
                    specifier = *tmp++;
                }

                if(specifier == '.') {
                    if(isDigit(*tmp)) {
                        ExtractInteger((char **)&tmp, &decimals_to_print);
                        if(decimals_to_print > FLOAT_PRECISION) decimals_to_print = FLOAT_PRECISION;
                    }
                    specifier = *tmp++;
                }

                nchar_to_print = 0;
                next_buffer_begin = tmp;

                if(specifier == 's') {
                    const char *buf_arg = va_arg(args, char *);
                    int buf_size = len(buf_arg);

                    if(length_to_print != -1) {
                        if(length_to_print <= buf_size)
                            buf_size = length_to_print;
                        else{
                            for(int i = 0; i < length_to_print - buf_size; ++i)
                                bytes_written += ConsoleOut(OutputConsole, " ", 1);
                        }
                    }

                    bytes_written += ConsoleOut(OutputConsole, buf_arg, buf_size);
                }

                else if(specifier == 'd') {
                    char str_int[MAX_INT_LENGTH];
                    int len_str_int = IntegerDumps(va_arg(args, int), str_int);

                    if(length_to_print != -1) {
                        if(length_to_print > len_str_int) {
                            for(int i = 0; i < length_to_print - len_str_int; ++i)
                                bytes_written += ConsoleOut(OutputConsole, " ", 1);
                        }
                    }

                    bytes_written += ConsoleOut(OutputConsole, str_int, len_str_int);
                }

                else if(specifier == 'u') {
                    char str_int[MAX_INT_LENGTH];
                    int len_str_int = UIntegerDumps(va_arg(args, int), str_int);

                    if(length_to_print != -1) {
                        if(length_to_print > len_str_int) {
                            for(int i = 0; i < length_to_print - len_str_int; ++i)
                                bytes_written += ConsoleOut(OutputConsole, " ", 1);
                        }
                    }

                    bytes_written += ConsoleOut(OutputConsole, str_int, len_str_int);
                }

                else if(specifier == 'f') {
                    char str_int[30];
                    int len_str_int = FloatDumps(va_arg(args, double), decimals_to_print, str_int);

                    if(length_to_print != -1) {
                        if(length_to_print > len_str_int) {
                            for(int i = 0; i < length_to_print - len_str_int; ++i)
                                bytes_written += ConsoleOut(OutputConsole, " ", 1);
                        }
                    }

                    bytes_written += ConsoleOut(OutputConsole, str_int, len_str_int);
                }


            } break;

            default:
            {
                nchar_to_print++;
                tmp++;
            }break;
        }
    }

    va_end(args);

    return bytes_written;
}

static inline int CopyBuffer(char *src, char *dst, int dst_size) {
    int count = 0;
    while(*src && *src != '\n' && *src != '\r' && count < dst_size) {
        *dst++ =*src++;
        count++;
    }
    return count;
}

static int scan(const char *format, ...) {
#if defined(_WIN32)
    HANDLE InputConsole = GetStdHandle(STD_INPUT_HANDLE);
#elif defined(__linux__)
    unsigned int InputConsole = 0;
#endif
    va_list args;
    va_start(args, format);

    int bytes_read = 0;
    const char *tmp = format;
    while(1) {
        char ch = *tmp;

        switch(ch) {
            case 0:
            {
                return bytes_read;
            } break;

            case '%':
            {
                tmp++;
                char specifier = *tmp++;

                char buffer[READ_BUFFER_SIZE];
                char *read_buffer = buffer;
                DWORD characters_read = ConsoleIn(InputConsole, read_buffer, READ_BUFFER_SIZE);

                if(specifier == 's') {
                    char *buf_to_write = va_arg(args, char *);
                    if(!buf_to_write)
                        return -1;

                    int max_buf_size = va_arg(args, int);
                    max_buf_size--;     //making space for null terminator

                    int buf_written = 0;
                    
                    while (characters_read == READ_BUFFER_SIZE) {
                        if(buf_written + characters_read < max_buf_size) {
                            buf_written += CopyBuffer(read_buffer, buf_to_write + buf_written, max_buf_size - buf_written);
                        }
                        characters_read = ConsoleIn(InputConsole, read_buffer, READ_BUFFER_SIZE);
                    }
                    buf_written += CopyBuffer(read_buffer, buf_to_write + buf_written, max_buf_size - buf_written);

                    buf_to_write[buf_written++] = '\0';
                    bytes_read += buf_written;   
                }

                if(specifier == 'd') {
                    int *buf_to_write = va_arg(args, int*);
                    if(!buf_to_write)
                        return -1;
                
                    while(!isDigitWithSign(*read_buffer)) {
                        ConsoleIn(InputConsole, read_buffer, READ_BUFFER_SIZE);
                    }

                    bytes_read += ExtractInteger((char **)&read_buffer, buf_to_write);
                }

                if(specifier == 'f') {
                    double *buf_to_write = va_arg(args, double*);
                    if(!buf_to_write)
                        return -1;
                    
                    while(!isDigitWithSign(*read_buffer)) {
                        ConsoleIn(InputConsole, read_buffer, READ_BUFFER_SIZE);
                    }

                    bytes_read += ExtractFloat((char **)&read_buffer, buf_to_write);
                }

            } break;

            default:
            {
                tmp++;
            } break;
        }

    }
    va_end(args);
}

#define CONSOLE_IO
#endif
