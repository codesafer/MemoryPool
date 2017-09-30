#pragma once
/*----------------------------------------------------------------
/
/----------------------------------------------------------------
/
/   ������ �ڷḦ �����Ͽ���.
/       - http://serious-code.net/moin.cgi/MiniDump
/----------------------------------------------------------------
/   ����ڴ� ���α׷� ������ ������ ���� �ñ� CMiniDump::Install �� ȣ���Ͽ� ���
/----------------------------------------------------------------
/   �޸� ���� ����
/       new ���� �ڵ鷯�� ����ڰ� �����ؾ� �մϴ�.
/       std::set_new_handler
/       �ʿ��� ��� WriteDump_OutofMemory�� ���� �Ͻʽÿ�
/       E_OUTOFMEMORY ���ܷν� ó���մϴ�.
/----------------------------------------------------------------
/   �޸� ���� ����
/       Stack Memory
/           ���� ����
/       Heap Memory
/           dbghelp �Լ��� �����޸� �ʿ�� ���� ����
/           Install �Ķ���� _pFN_Reduce_HeapMemory �� ����
/
/           ����ڴ� LPFUNCTION_Request_Reduce_HeapMemory Ÿ�� �Լ��� ����
/           ���� BOOL
/               ������ Heap �޸𸮰� ���� ���� TRUE�� �����ؾ� ��
/               (����� �����ϰ� TRUE�� �����ϸ� ���ѷ��� ���ɼ��� ����)
/           ����
/               ������ VirtualFree�� ���� ȣ���ϴ� ���� ����
/               �Ϲ����� �޸��Ҵ����� free/delete �� �޸𸮰� OS�� ��� �ݳ��ȴٴ� ������ ����
/----------------------------------------------------------------
/   ��������
/       �����޸� �������� ������� ����ó���� ����
/       ���� �̴ϴ�����ü�� �̸� ����޸𸮸� Ȯ���صδ� ���´� �������� �ʾ�,
/       ����ڰ� ����޸𸮸� �����ϴ� ���·� ����
----------------------------------------------------------------*/
#pragma warning(push)
#pragma warning(disable: 4091)
#include <DbgHelp.h>
#pragma warning(pop)
#include "../../Core/UTIL/DebugMemory.h"
#include "../../BasisClass/BasisClass.h"

class CMiniDump{
public:
    _DEF_FRIEND_SINGLETON;
    // ���ܺ��� �Լ� : ���� �������� ������ �����Ͽ��ٸ�, _szDumpFile : nullptr
    typedef void (__stdcall *LPFUNCTION_ERROR_REPORT)(LPCTSTR _szDumpFile);
    typedef BOOL(__stdcall *LPFUNCTION_Request_Reduce_HeapMemory)(void);
    typedef BOOL(__stdcall *LPFUNCTION_MINIDUMPWRITEDUMP)(
        IN HANDLE hProcess,
        IN DWORD ProcessId,
        IN HANDLE hFile,
        IN MINIDUMP_TYPE DumpType,
        IN CONST PMINIDUMP_EXCEPTION_INFORMATION ExceptionParam,
        IN CONST PMINIDUMP_USER_STREAM_INFORMATION UserStreamParam,
        IN CONST PMINIDUMP_CALLBACK_INFORMATION CallbackParam
        );
protected:
    CMiniDump();
    ~CMiniDump();

private:
    BOOL        m_bConnected_dbghelp;
    BOOL        m_bInstalled;

    HANDLE      m_hCallbackThread;
    HANDLE      m_hEventDump;
    UTIL::LOCK::CCompositeLock m_Lock_Request_Dump;
    UTIL::LOCK::CSpinLock m_Lock_ThisOBJ;

    struct TSignal{
        TSignal();
        BOOL    m_bCMD_ExitThread;
        LONG    m_Exception_Continue__Code;

        MINIDUMP_EXCEPTION_INFORMATION* m_pExParam;
    }m_signal;

    LPTOP_LEVEL_EXCEPTION_FILTER    m_pPrev_Filter;
    MINIDUMP_TYPE                   m_DumpLevel;
    TCHAR                           m_szWriteFolder[MAX_PATH];

    HMODULE m_hDLL_dbghelp;
    LPFUNCTION_MINIDUMPWRITEDUMP m_pFN_WriteDump;
    LPFUNCTION_ERROR_REPORT      m_pFN_ErrorReportFunction;
    LPFUNCTION_Request_Reduce_HeapMemory m_pFN_Reduce_VirtualMemory;

public:
    //----------------------------------------------------------------
    //  ���� ����� �Լ�
    //----------------------------------------------------------------

    // Minidump ��� �غ�
    // �ӽ� �޸� ����: Out Of Memory�� ��� ������ ����Ҷ�,
    //                   �޸� �������� �����ϰ� �Ǵµ�,
    //                   �̸� �Ҵ��� �޸𸮸� �������ָ� �ذ�ȴ�.
    // Parameter
    //      ����� ���(����� ������ Current Directory ���� ����)
    //      ���޸� �����Լ� ����
    //      ���� �����Լ� ����
    //      ���� ����
    static BOOL Install(LPCTSTR _szWriteFolder
        , LPFUNCTION_Request_Reduce_HeapMemory _FN_Reduce_HeapMemory = NULL
        , LPFUNCTION_ERROR_REPORT _FN_ErrorReport = NULL
        , MINIDUMP_TYPE _DumpLevel = MiniDumpNormal
    );

    // ����� �Լ�: MiniDump ����(�Ҹ��ڿ��� �ڵ����� ȣ��ȴ�)
    static BOOL Uninstall();
    // ����� �Լ�: ������ ������ ����Ѵ�.
    static void ForceDump(BOOL _bExitProgram=FALSE);

    // Write Dump : Out of Memory
    static void __stdcall WriteDump_OutofMemory__stdcall();
    static void __cdecl WriteDump_OutofMemory__cdecl();

    //----------------------------------------------------------------
    //  ���� �Լ��� ���ο��� ����� ����Ѵ�
    //  CMiniDump �� Internal �Լ����� �� �Լ����� ���� ��������¿� ������
    //----------------------------------------------------------------
    // ������� ��� ����
    static BOOL Change_WriteFolder(LPCTSTR _szWriteFolder);
    // ���� ���� ����
    static BOOL Change_DumpLevel(MINIDUMP_TYPE _DumpLevel);
    // ���� �����Լ� ����
    static BOOL Change_ErrorReport_Function(LPFUNCTION_ERROR_REPORT _ErrorReportFunction);
    // Get: ���� �����Լ�
    static LPFUNCTION_ERROR_REPORT Get_ErrorReport_Function();
    // ���޸� �����Լ� ����
    static BOOL Change_ReduceHeapMemory_Function(LPFUNCTION_Request_Reduce_HeapMemory _ReduceHeapMemory_Function);
    // Get: ���޸� �����Լ�
    static LPFUNCTION_Request_Reduce_HeapMemory Get_ReduceHeapMemory_Function();

private:
    // Write Dump
    static LONG __stdcall WriteDump(MINIDUMP_EXCEPTION_INFORMATION* pExParam);
    // Dump Thread
    static unsigned __stdcall CallbackThread(void* pParam);
    // Exception Filter
    static LONG __stdcall ExceptionFilter(PEXCEPTION_POINTERS exPtrs);

    BOOL _Uninstall();
    BOOL Connect_dbghelp();
    void Disconnect_dbghelp();

    // ������� ��� ����
    BOOL _Change_WriteFolder(LPCTSTR _szWriteFolder);
    // ���� ���� ����
    BOOL _Change_DumpLevel(MINIDUMP_TYPE _DumpLevel);
    // ���� �����Լ� ����
    BOOL _Change_ErrorReport_Function(LPFUNCTION_ERROR_REPORT _ErrorReportFunction);
    // Get: ���� �����Լ�
    LPFUNCTION_ERROR_REPORT _Get_ErrorReport_Function();
    // ���޸� �����Լ� ����
    BOOL _Change_ReduceHeapMemory_Function(LPFUNCTION_Request_Reduce_HeapMemory _ReduceHeapMemory_Function);
    // Get: ���޸� �����Լ�
    LPFUNCTION_Request_Reduce_HeapMemory _Get_ReduceHeapMemory_Function();

    HMODULE LoadDebugHelpLibrary();

    HANDLE CreateDumpFile(LPTSTR _out_FileName, size_t _len);

public:
    void WriteLog(PEXCEPTION_POINTERS exPtrs);
    static LPCTSTR Get_ExceptionName(DWORD dwExceptionCode);            // Get: �����ڵ��� �̸�
    static void WriteLog__FaultReason(PEXCEPTION_POINTERS exPtrs);      // LOG: ���� ������ ����Ѵ�.
    static void WriteLog__Os();
    static void WriteLog__CpuInfo();
    static void WriteLog__ProcessInfo();
    static void WriteLog__MemoryInfo();
};