#include "Engine_include.h"
#pragma message("---- Import : ENGINE LIB ----")
// �� ������ ���� cpp���� 1ȸ�� ���� �Ǿ�� �մϴ�


#if __X64
    #ifdef _DEBUG
        #pragma comment(lib, "Engine_x64D.lib")
    #else
        #pragma comment(lib, "Engine_x64.lib")
    #endif
#elif __X86
    #ifdef _DEBUG
        #pragma comment(lib, "Engine_x86D.lib")
    #else
        #pragma comment(lib, "Engine_x86.lib")
    #endif
#else
    #error
#endif