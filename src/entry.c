#include "main.c"
#include <processthreadsapi.h>
#include <winnt.h>

#if defined(_WIN32)
#include <Windows.h>

int WeDontNeedMain()
{
    int argc; char **argv;
    wchar_t ** wargv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!wargv) {
        ExitProcess(mine(0, 0));
    }
    
    int n = 0;
    for (int i = 0; i < argc; ++i)
        n += WideCharToMultiByte(CP_UTF8, 0, wargv[i], -1, NULL, 0, NULL, NULL) + 1;
    
    argv = VirtualAlloc(0, (argc + 1) * sizeof(char *) + n, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
    if (!argv) {
        ExitProcess(mine(0, 0));
    }
    
    char *arg = (char*)&((argv)[argc + 1]);
    for (int i = 0; i < argc; ++i)
    {
        argv[i] = arg;
        arg += WideCharToMultiByte(CP_UTF8, 0, wargv[i], -1, arg, n, NULL, NULL) + 1;
    }
    argv[argc] = NULL;
    ExitProcess(mine(argc, argv));
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