#include "Core_include.h"
#pragma message("---- Import : CORE DLL ----")
// �� ������ ���� cpp���� 1ȸ�� ���� �Ǿ�� �մϴ�
//
// #1. Connect_CORE ȣ���� ���� ����
// #2. Disconnect_CORE �� �ڵ����� ȣ���(���Ѵٸ� ���� ȣ��)
//     ���� Connect_CORE ���� ILogWriter �� ���� �Ͽ��ٸ�
//     ILogWriter ���� �Ļ��� �ν��Ͻ��� �ı��Ǳ��� Disconnect_CORE �� ��������� ȣ���ؾ���

namespace CORE{
    HMODULE g_hDLL_Core = nullptr;
    ICore* g_pCore      = nullptr;
}
namespace UTIL{
    const ISystem_Information* g_pSystem_Information = nullptr;
#ifndef CORE_EXPORTS
    TUTIL* g_pUtil                      = nullptr;
    MEM::IMemoryPool_Manager* g_pMem    = nullptr;
#endif
}

namespace CORE{
    BOOL Connect_CORE_PRIVATE(::UTIL::ILogWriter* pLogSystem, ::UTIL::ILogWriter* pLogDebug);
    void Disconnect_CORE();

    class CAutoDisconnectCore{
    private:
        CAutoDisconnectCore()
        {
        }
        ~CAutoDisconnectCore()
        {
            Disconnect_CORE();
        }

    private:
        static CAutoDisconnectCore s_Instance;
    };
    CAutoDisconnectCore CAutoDisconnectCore::s_Instance;

    __declspec(noinline) BOOL Connect_CORE(LPCSTR path_DLL, ::UTIL::ILogWriter* pLogSystem, ::UTIL::ILogWriter* pLogDebug)
    {
        _Assert(NULL == CORE::g_hDLL_Core);
        CORE::g_hDLL_Core = LoadLibraryA(path_DLL);
        if(!CORE::g_hDLL_Core)
        {
            CHAR str[256];
            sprintf_s(str, "file not found : %s", path_DLL);
            ::MessageBoxA(NULL, str, "Error : Connect_CORE", MB_ICONERROR);
        }
        return Connect_CORE_PRIVATE(pLogSystem, pLogDebug);
    }
    __declspec(noinline) BOOL Connect_CORE(LPCWSTR path_DLL, ::UTIL::ILogWriter* pLogSystem, ::UTIL::ILogWriter* pLogDebug)
    {
        _Assert(NULL == CORE::g_hDLL_Core);
        CORE::g_hDLL_Core = LoadLibraryW(path_DLL);
        if(!CORE::g_hDLL_Core)
        {
            WCHAR str[256];
            str[0] = L'\0';
            swprintf_s(str, L"file not found : %s\nError Code: %u", path_DLL, GetLastError());
            ::MessageBoxW(NULL, str, L"Error : Connect_CORE", MB_ICONERROR);
        }
        return Connect_CORE_PRIVATE(pLogSystem, pLogDebug);
    }

    __declspec(noinline) void Disconnect_CORE()
    {
        // ���� ����
        UTIL::g_pMem  = NULL;
        UTIL::g_pUtil = NULL;
        UTIL::g_pSystem_Information = NULL;

        CORE::g_pCore = NULL;

        if(CORE::g_hDLL_Core)
        {
            if(FALSE == ::FreeLibrary(CORE::g_hDLL_Core))
                _Error("Failed FreeLibrary CORE DLL");
            CORE::g_hDLL_Core = NULL;
        }
    }

#pragma region private
    __declspec(noinline) BOOL Connect_CORE_PRIVATE(::UTIL::ILogWriter* pLogSystem, ::UTIL::ILogWriter* pLogDebug)
    {
        if(!CORE::g_hDLL_Core)
            goto Label_failed;

        CORE::g_pCore = CORE::gFN_Get_CORE(CORE::g_hDLL_Core);
        if(!CORE::g_pCore)
            goto Label_failed;


        CORE::g_pCore->mFN_Set_LogWriterInstance__System(pLogSystem);
        CORE::g_pCore->mFN_Set_LogWriterInstance__Debug(pLogDebug);


        UTIL::g_pSystem_Information = CORE::g_pCore->mFN_Get_System_Information();
        if(!UTIL::g_pSystem_Information)
            goto Label_failed;

        UTIL::g_pUtil = CORE::g_pCore->mFN_Get_Util();
        if(!UTIL::g_pUtil)
            goto Label_failed;

        UTIL::g_pMem  = CORE::g_pCore->mFN_Get_MemoryPoolManager();
        if(!UTIL::g_pMem)
            goto Label_failed;
#if _DEF_USING_MEMORYPOOL_UPGRADE_MULTIPROCESSING__A_TLS && _DEF_MEMORYPOOL_MAJORVERSION == 1
        UTIL::g_pMem->mFN_Set_TlsCache_AccessFunction(UTIL::MEM::TMemoryPool_TLS_CACHE::sFN_Default_Function);
#endif

        CORE::g_pCore->mFN_Write_CoreInformation(pLogSystem);
        return TRUE;

    Label_failed:
        ::MessageBox(NULL, _T("failed initialize : Core DLL"), _T("Error : Connect_CORE"), MB_ICONERROR);
        Disconnect_CORE();
        return FALSE;
    }
#pragma endregion
}