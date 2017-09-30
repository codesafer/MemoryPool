#include "stdafx.h"
#include "./LogWriter.h"

#include <atlpath.h>

#pragma warning(disable: 6255)
#ifdef MAX_PATH
#undef MAX_PATH
#define MAX_PATH _DEF_COMPILE_MSG__WARNING("CLogWriter::sc_MaxPath �� ����Ͻʽÿ�")
#endif
namespace UTIL
{
    namespace{
        static const BOOL   gc_bUse_Alloca = FALSE;
        static const size_t gc_LenTempStrBuffer__LogWrite = 1024 * 4;
    }
    //----------------------------------------------------------------
    //  size_t nLimit_WriteCount
    //      ��ϼ� ����(WriteLog ȣ���)
    //      ���� ���α׷��� �ߴ��� ������ ����ġ�� ���� �αװ� �ۼ��ȴٸ�,
    //      ��κ� ���� ������ ����ؼ� �ߺ��Ǵ� ��찡 ������
    //      ������ġ�� ������ ���� �ȴ�
    //      �� ���� �����ϰ� �����ؾ� �Ѵ�
    //      (0 : ���� ����)
    //  LPCTSTR _LogFileName
    //      �α����ϸ�
    //      �� ���� ��ο� ���� ���ϸ��� ����ؼ��� �ȵȴ�
    //  LPCTSTR _LogFolder
    //      �̰��� _Rootpath �� ���� ����̴�
    //  LPCTSTR _Rootpath
    //      �������� �ʰų� ��ȿ�� ����� ��� ���������� ��θ� ���
    //      �� �̰��� �α������� �ƴ� �� �θ������̴�
    //  BOOL _bEncoding_is_UniCode
    //      �α����� ���ڵ�(Ascii / UniCode)
    //----------------------------------------------------------------
    CLogWriter::CLogWriter(size_t nLimit_WriteCount, LPCTSTR _LogFileName, LPCTSTR _LogFolder, LPCTSTR _Rootpath, BOOL _bEncoding_is_UniCode)
        : m_bEncoding_is_UniCode(_bEncoding_is_UniCode)
        , m_bStopWriteLog(FALSE)
        , m_nLimit_WriteLog(nLimit_WriteCount)
        , m_Counting_WriteLog(0)
    {
        _DEF_CACHE_ALIGNED_TEST__THIS();
        if(!_LogFileName)
            _LogFileName = _T("log.txt");

        m_strPath[0] = 0;

        // ���� ��� ����
        if(_Rootpath && ATLPath::IsDirectory(_Rootpath))
            _tcscpy_s(m_strPath, _Rootpath);
        else
            ::GetModuleFileName(NULL, m_strPath, sc_MaxPath);
        
        ATLPath::RemoveFileSpec(m_strPath);

        if(_LogFolder)
            ATLPath::Append(m_strPath, _LogFolder);

    #pragma warning(push)
    #pragma warning(disable: 6031)
        // ������ �����
        _tmkdir(m_strPath);
    #pragma warning(pop)

        // + ���ϸ�
        if(!_LogFileName)
            _Error(_T("CLogWriter::CLogWriter �������� ���� ���ϸ�"));
        ATLPath::Append(m_strPath, _LogFileName);

        // ������ �����.
        FILE* pFile = NULL;
        if(m_bEncoding_is_UniCode)
            _tfopen_s(&pFile, m_strPath, _T("wt, ccs=UNICODE"));
        else
            _tfopen_s(&pFile, m_strPath, _T("wt"));
        if(pFile)
            fclose(pFile);
        else
            _Error(_T("Failed Create File : %s"), m_strPath);

        // ���۽ð� ���
        mFN_WriteLog__Time_StartEnd(TRUE);
    }

    CLogWriter::~CLogWriter()
    { 
        // ����ð� ���
        mFN_WriteLog__Time_StartEnd(FALSE);
    }

    /*----------------------------------------------------------------
    /   ����� �������̽�
    ----------------------------------------------------------------*/
    // m_Counting_WriteLog �����÷ο� ���ɼ�
    // ������ ����ϴ� ��쿡 ���Ͽ�
    //      �ѹ��� ������� 1����Ʈ�� ���(\0)�Ѵٰ� ����,
    //      x86 max size_t �� �ƽ�Ű ���� �ּ� 4GB -> ������ ���� �ʴ´�
    DECLSPEC_NOINLINE void CLogWriter::mFN_WriteLog(BOOL _bWriteTime, LPCSTR _szText)
    {
        if(!_szText || _szText[0] == NULL)
            return;

        if(!m_nLimit_WriteLog)
            mFN_WriteLog_Internal(_bWriteTime, _szText);

        //std::_Atomic_thread_fence(std::memory_order::memory_order_consume);// noinline �̱� ������ ���ʿ�
        if(m_bStopWriteLog)
            return;

        const auto prev = ::InterlockedExchangeAdd(&m_Counting_WriteLog, 1);
        if(prev < m_nLimit_WriteLog)
            mFN_WriteLog_Internal(_bWriteTime, _szText);
        else
            mFN_Stop_WriteLog(); // now == 0 : �����÷ο�
    }
    DECLSPEC_NOINLINE void CLogWriter::mFN_WriteLog(BOOL _bWriteTime, LPCWSTR _szText)
    {
        if(!_szText || _szText[0] == NULL)
            return;

        if(!m_nLimit_WriteLog)
            mFN_WriteLog_Internal(_bWriteTime, _szText);

        //std::_Atomic_thread_fence(std::memory_order::memory_order_consume);// noinline �̱� ������ ���ʿ�
        if(m_bStopWriteLog)
            return;

        const auto prev = ::InterlockedExchangeAdd(&m_Counting_WriteLog, 1);
        if(prev < m_nLimit_WriteLog)
            mFN_WriteLog_Internal(_bWriteTime, _szText);
        else
            mFN_Stop_WriteLog(); // now == 0 : �����÷ο�
    }

    LPCTSTR CLogWriter::mFN_Get_LogPath() const
    {
        return m_strPath;
    }
    //----------------------------------------------------------------
    //�α׸� ����մϴ�. MULTIBYTE -> MULTIBYTE / UNICODE
    BOOL CLogWriter::mFN_WriteLog_Internal(BOOL _bWriteTime, LPCSTR _szText)
    {
        if(!_szText || _szText[0] == NULL)
            return FALSE;

        if(!m_bEncoding_is_UniCode)
        {
            return mFN_WriteLog__MULTIBYTE(_bWriteTime, _szText);
        }
        else
        {
            if(gc_bUse_Alloca)
            {
                int LenW = MultiByteToWideChar(CP_ACP, 0
                    , _szText, -1
                    , NULL, 0);

                WCHAR* __szTempBuff_W = (WCHAR*)_alloca(sizeof(WCHAR) * LenW);
                MultiByteToWideChar(CP_ACP, 0
                    , _szText, -1
                    , __szTempBuff_W, LenW);

                return mFN_WriteLog__UNICODE(_bWriteTime, __szTempBuff_W);
            }
            else
            {
                WCHAR __szTempBuff_W[gc_LenTempStrBuffer__LogWrite];
                MultiByteToWideChar(CP_ACP, 0
                    , _szText, -1
                    , __szTempBuff_W, _MACRO_ARRAY_COUNT(__szTempBuff_W));

                return mFN_WriteLog__UNICODE(_bWriteTime, __szTempBuff_W);
            }
        }
    }
    //�α׸� ����մϴ�. UNICODE -> MULTIBYTE / UNICODE
    BOOL CLogWriter::mFN_WriteLog_Internal(BOOL _bWriteTime, LPCWSTR _szText)
    {
        if(!_szText || _szText[0] == NULL)
            return FALSE;

        if(m_bEncoding_is_UniCode)
        {
            return mFN_WriteLog__UNICODE(_bWriteTime, _szText);
        }
        else
        {
            if(gc_bUse_Alloca)
            {
                int LenA = WideCharToMultiByte(CP_ACP, 0
                    , _szText, -1
                    , NULL, 0
                    , NULL, FALSE);
                char* __szTempBuff_A = (char*)_alloca(sizeof(char) * LenA);
                WideCharToMultiByte(CP_ACP, 0
                    , _szText, -1
                    , __szTempBuff_A, LenA
                    , NULL, FALSE);

                return mFN_WriteLog__MULTIBYTE(_bWriteTime, __szTempBuff_A);
            }
            else
            {
                char __szTempBuff_A[gc_LenTempStrBuffer__LogWrite];
                WideCharToMultiByte(CP_ACP, 0
                    , _szText, -1
                    , __szTempBuff_A, _MACRO_ARRAY_COUNT(__szTempBuff_A)
                    , NULL, FALSE);

                return mFN_WriteLog__MULTIBYTE(_bWriteTime, __szTempBuff_A);
            }
        }
    }
    //----------------------------------------------------------------
    BOOL CLogWriter::mFN_WriteLog__MULTIBYTE(BOOL _bWriteTime, LPCSTR _szText)
    {
        if(m_bEncoding_is_UniCode)
            return FALSE;

        CHAR szTemp[gc_LenTempStrBuffer__LogWrite];
        LPCSTR pSTR_output;
        if(_bWriteTime)
        {
            SYSTEMTIME st;
            GetLocalTime(&st);
            sprintf_s(szTemp, "[%d-%02d-%02d %02d:%02d:%02d] %s"
                , st.wYear, st.wMonth, st.wDay
                , st.wHour, st.wMinute, st.wSecond
                , _szText);
            pSTR_output = szTemp;
        }
        else
        {
            pSTR_output = _szText;
        }

        m_Lock.Lock();
        FILE* pFile = mFN_OpenFile();
        BOOL bDone  = (pFile)? TRUE : FALSE;
        if(pFile)
        {
            fputs(pSTR_output, pFile);
            if(fclose(pFile))
                bDone = FALSE;
        }
        m_Lock.UnLock();

        return bDone;
    }

    BOOL CLogWriter::mFN_WriteLog__UNICODE(BOOL _bWriteTime, LPCWSTR _szText)
    {
        if(!m_bEncoding_is_UniCode)
            return FALSE;

        WCHAR szTemp[gc_LenTempStrBuffer__LogWrite];
        szTemp[0] = L'\0';

        LPCWSTR pSTR_output;
        if(_bWriteTime)
        {
            SYSTEMTIME st;
            GetLocalTime(&st);
            swprintf_s(szTemp, L"[%d-%02d-%02d %02d:%02d:%02d] %s"
                , st.wYear, st.wMonth, st.wDay
                , st.wHour, st.wMinute, st.wSecond
                , _szText);
            pSTR_output = szTemp;
        }
        else
        {
            pSTR_output = _szText;
        }

        m_Lock.Lock();
        FILE* pFile = mFN_OpenFile();
        BOOL bDone  = (pFile)? TRUE : FALSE;
        if(pFile)
        {
            fputws(pSTR_output, pFile);
            if(fclose(pFile))
                bDone = FALSE;
        }
        m_Lock.UnLock();

        return bDone;
    }

    FILE* CLogWriter::mFN_OpenFile()
    {
        FILE* pFile;
        if(m_bEncoding_is_UniCode)
            _tfopen_s(&pFile, m_strPath, _T("at, ccs=UNICODE"));
        else
            _tfopen_s(&pFile, m_strPath, _T("at"));

        return pFile;
    }

    void CLogWriter::mFN_WriteLog__Time_StartEnd(BOOL isStart)
    {
        if(isStart)
        {
            if(!m_nLimit_WriteLog)
            {
                mFN_WriteLog_Internal(TRUE, _T("Begin Log - Unlimited\n"));
            }
            else
            {
                TCHAR szTemp[256];
                _stprintf_s(szTemp, _T("Begin Log - Limited(%Iu)\n"), m_nLimit_WriteLog);
                mFN_WriteLog_Internal(TRUE, szTemp);
            }
        }
        else
        {
            if(!m_nLimit_WriteLog)
            {
                mFN_WriteLog_Internal(TRUE, _T("End Log\n"));
            }
            else
            {
                TCHAR szTemp[256];
                _stprintf_s(szTemp, _T("End Log - WriteCount(%Iu)\n"), m_Counting_WriteLog);
                mFN_WriteLog_Internal(TRUE, szTemp);
            }
        }
    }
    DECLSPEC_NOINLINE void CLogWriter::mFN_Stop_WriteLog()
    {
        if(FALSE == InterlockedExchange((LONG*)&m_bStopWriteLog, (LONG)TRUE))
            mFN_WriteLog_Internal(TRUE, _T("Stop Log\n"));

        if(m_Counting_WriteLog != m_nLimit_WriteLog)
            ::InterlockedExchange(&m_Counting_WriteLog, m_nLimit_WriteLog);
    }


};