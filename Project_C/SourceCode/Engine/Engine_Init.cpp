#include "stdafx.h"
#include "./Engine.h"


namespace ENGINE{
    void CEngine::mFN_Write_EngineInformation(::UTIL::ILogWriter* pLog)
    {
        if(!pLog) return;
        #define _LOCAL_MACRO_LOG(bTime, fmt, ...) ::UTIL::TLogWriter_Control::sFN_WriteLog(pLog, bTime, fmt, __VA_ARGS__)

        auto pT = mFN_Query_CompileTime();
        if(pT)
        {
            auto t = *pT;
            _LOCAL_MACRO_LOG(FALSE, _T("Compile Time : %d-%02d-%02d  %02d:%02d:%02d (Engine LIB)")
                , t.Year
                , t.Month
                , t.Day
                , t.Hour
                , t.Minute
                , t.Second);
        }
        if(!mFN_Query_Initialized_Engine())
        {
            // ����ڿ��� �ȳ��Ѵ�
            _MACRO_MESSAGEBOX__ERROR(_T("from Engine"), _T("Failed Initialized Engine...\nFailed Code : 0x%016I64X"), m_stats_Engine_Initialized.m_Failed_Something);


            // �ʱ�ȭ�� �����Ͽ��ٸ� ������ ����Ѵ�
            _LOCAL_MACRO_LOG(TRUE, _T("Failed Initialized Engine... Failed Code : 0x%016I64X")
                , m_stats_Engine_Initialized.m_Failed_Something);
            

            const ::UTIL::TCpu_InstructionSet* pCpuIS = nullptr;
            const SYSTEM_INFO* pSysInfo = nullptr;
            if(::UTIL::g_pSystem_Information)
            {
                pCpuIS = ::UTIL::g_pSystem_Information->mFN_Get_CPU_InstructionSet();
                pSysInfo = ::UTIL::g_pSystem_Information->mFN_Get_SystemInfo();
            }
            //----------------------------------------------------------------
            // �� ����Ʈ(�ʿ��� ������ ���)
            //----------------------------------------------------------------
            const auto& code = m_stats_Engine_Initialized.m_FailedCode;
            if(code._ConnectCore){
                _LOCAL_MACRO_LOG(FALSE, _T("\tFailed Connect Core"));
            }
            if(code._Intrinsics && pCpuIS){
                _LOCAL_MACRO_LOG(FALSE, "\t%s\n\t%s\n", pCpuIS->NameVendor(), pCpuIS->NameBrand());
            }
            if(code._OS && ::UTIL::g_pSystem_Information){
                auto osv = *::UTIL::g_pSystem_Information->mFN_Get_OsInfo();
                _LOCAL_MACRO_LOG(FALSE, _T("\tOS : %s\n\tOS Version(%u.%u) ServicePack(%hu.%hu) Build(%u)")
                    , ::UTIL::g_pSystem_Information->mFN_Get_OsName()
                    , osv.dwMajorVersion, osv.dwMinorVersion
                    , osv.wServicePackMajor, osv.wServicePackMinor
                    , osv.dwBuildNumber);
            }
            if(code._SystemMemory){
                _LOCAL_MACRO_LOG(FALSE, _T("\t---- System Memory Information ----"));
                if(pSysInfo)
                    _LOCAL_MACRO_LOG(FALSE, _T("\tPageSize(%u) AllocationGranularity(%u)"), pSysInfo->dwPageSize, pSysInfo->dwAllocationGranularity);

                MEMORYSTATUS MemInfo;
                MemInfo.dwLength = sizeof(MemInfo);
            #pragma warning(push)
            #pragma warning(disable: 28159)
                ::GlobalMemoryStatus(&MemInfo);
            #pragma warning(pop)
                _LOCAL_MACRO_LOG(FALSE, _T("\tMemoryLoad              : %10u %%"), MemInfo.dwMemoryLoad);
                _LOCAL_MACRO_LOG(FALSE, _T("\tPhysical Memory(Total)  : %10Iu KB"), MemInfo.dwTotalPhys / 1024);
                _LOCAL_MACRO_LOG(FALSE, _T("\tPhysical Memory(Free)   : %10Iu KB"), MemInfo.dwAvailPhys / 1024);
                _LOCAL_MACRO_LOG(FALSE, _T("\tPaging File(Total)      : %10Iu KB"), MemInfo.dwTotalPageFile / 1024);
                _LOCAL_MACRO_LOG(FALSE, _T("\tPaging File(Free)       : %10Iu KB"), MemInfo.dwAvailPageFile / 1024);
                _LOCAL_MACRO_LOG(FALSE, _T("\tVirtual Memory(Total)   : %10Iu KB"), MemInfo.dwTotalVirtual / 1024);
                _LOCAL_MACRO_LOG(FALSE, _T("\tVirtual Memory(Free)    : %10Iu KB"), MemInfo.dwAvailVirtual / 1024);
            }
            if(code._InitializeMemoryPool)
                _LOCAL_MACRO_LOG(FALSE, _T("\tFailed preinitialize MemoryPool"));
        }
        #undef _LOCAL_MACRO_LOG
    }
}


namespace ENGINE{
    BOOL CEngine::mFN_Test_System_Intrinsics()
    {
        // �ڵ忡�� ����� �����Լ��� ��� üũ�� ��
        auto pIntrinsics = UTIL::g_pSystem_Information->mFN_Get_CPU_InstructionSet();
        if(!pIntrinsics->SSE2())
            return FALSE;

        return TRUE;
    }
    BOOL CEngine::mFN_Test_System_OS()
    {
        // x64 ���������� x32 OS���� �����ϴ� ��� Ȯ��
        // ���ʿ� ������ �Ǵ°�?
        #if __X64
        if(!UTIL::g_pSystem_Information->mFN_Query_Os64Bit())
            return FALSE;
        #endif

        // OS ���� üũ
        {
            // �� ���� UTIL::E_OS_TYPE���ο� ���ǵ� �Ͱ� ���ϴ� ���� �����ؾ��Ѵ�
            // ���ο� �������� OS�� ��������, �ڵ尡 ������Ʈ �����ʾҴٸ� ������ �������� ���ϱ� ������
            // �����ϴٸ� OSVERSIONINFO �� ������ ���� ���Ѵ�
            // ���� ���� XP SP3 �̻�
            // XP SP3 ver(5, 1, 3, 0)
            if(!UTIL::g_pSystem_Information->mFN_Query_OSVersion_Test_Above(5, 1, 3, 0))
                return FALSE;
        }
        return TRUE;
    }
    BOOL CEngine::mFN_Test_System_Memory()
    {
        auto pSysInfo = UTIL::g_pSystem_Information->mFN_Get_SystemInfo();
        // �ּ� ���� 64KB ��� Ȯ��
        {
            const size_t iAddress_per_Reserve = 64 *1024;
            if(1 > pSysInfo->dwAllocationGranularity / iAddress_per_Reserve)
                return FALSE;
            else if(0 != pSysInfo->dwAllocationGranularity % iAddress_per_Reserve)
                return FALSE;
        }
        // �Ҵ� ���� 4KB ��� Ȯ��
        {
            const size_t iMinPageSize = 4 *1024;
            if(1 > pSysInfo->dwPageSize / iMinPageSize)
                return FALSE;
            else if(0 != pSysInfo->dwPageSize % iMinPageSize)
                return FALSE;
        }
        // �ʿ��� �ּ� �޸� üũ
        {
            MEMORYSTATUS MemInfoSys;
            MemInfoSys.dwLength = sizeof(MemInfoSys);
        #pragma warning(push)
        #pragma warning(disable: 28159)
            ::GlobalMemoryStatus(&MemInfoSys);
        #pragma warning(pop)

            // �� 512MB ����(� ȯ�濡�� ������� �𸥴� �����Ǵ� �ڵ带 ���� �ϱ� ����...)
            size_t limit = 512 * 1024 * 1024;
            // ���� �޸𸮴� �ణ ���� ������ ������ �ణ �������� ����
            limit = limit * 95 / 100;
            if(MemInfoSys.dwTotalPhys < limit)
                return FALSE;
        }

        return TRUE;
    }
    BOOL CEngine::mFN_Initialize_MemoryPool()
    {
        // to do
        // XML ���� ���������� �Ľ��Ͽ� ����
        //  ���������� ������ ��
        //  �ֱ� n���� ���� ��踦 �����Ͽ�
        //  ������ �޸�Ǯ �ʱ�ȭ�� ����ؼ� ���

        return TRUE;
    }
    

}