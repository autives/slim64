#include "main.c"

#if defined(_WIN32)
#include <Windows.h>

int WeDontNeedMain()
{
    LPSTR command_line_string = GetCommandLineA();

    int argc = 0; char *argv[1024];
    argv[argc++] = command_line_string;

    int insideQuotes = 0;
    int escaping = 0;
    char *escapeCharaterToBeRemoved = 0;

    while(*command_line_string) {
        if(*command_line_string == '\\'){
            if(insideQuotes){
                escaping = !escaping;
                command_line_string++;
            }
        }

        if(*command_line_string == '"') {
            if(!escaping) {
                if(insideQuotes)
                    *command_line_string = 0;
                else{
                    argv[argc - 1]++;
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
                escapeCharaterToBeRemoved = command_line_string - 1;
        }

        if(*command_line_string == ' '){
            if(!insideQuotes) {
                *command_line_string = '\0';
                if(argc == 1)
                    command_line_string++;
                argv[argc++] = command_line_string + 1;
            }
        }
        escaping = 0;
        command_line_string++;
    }

    int ret = mine(argc, argv);    
    ExitProcess(ret);
}

#elif defined(__linux__)

__attribute__((force_align_arg_pointer))
void _start() {
    asm("popq %rdi;"
        "popq %rdi;"
        "movq %rsp, %rsi;"
        "call mine;"
        "movq %rax, %rdi;"
        "call end");
}

#endif