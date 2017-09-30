#include "stdafx.h"
#include "./Timer.h"

#include <mmsystem.h>

namespace UTIL
{
    namespace{
        template<typename T>
        __forceinline T gFN_Cal_ElapsedTime(T prev, T now, T max)
        {
            if(prev <= now)
                return now - prev;

            return (max - prev) + now + 1; // �ð� 0�� �����ϱ� ���� 1 �߰�
        }
    }
    CTimer::CTimer()
        : m_dw64TotalPlayTime_Stack(0)
        , m_dwStartingTime(0)
        , m_fTotalPlayTimeSec(0.f)
        , m_fFPS(0.f)
        , m_fSPF(0.f)
        , m_bUsing_Performance_Counter(FALSE)
        , m_bLimitFPS(FALSE)
        , m_fLimitFPS(0.f)
        , m_llDelay_LimitFPS(0)                 // is m_Delay_LimitFPS
    {
        mFN_Init(TRUE);
        mFN_ResetTimer();
    }

    CTimer::~CTimer()
    {
    }

    /*----------------------------------------------------------------
    /       ���� �����ӿ�ũ ���� �������̽�
    ----------------------------------------------------------------*/
    void CTimer::mFN_Init(BOOL _bUse_Performance_Counter)
    {
        ::timeBeginPeriod(1);
        mt_Time.mt_System.m_Time_Now  = ::timeGetTime();
        ::timeEndPeriod(1);
        mt_Time.mt_System.m_Time_Prev = mt_Time.mt_System.m_Time_Now;
        mt_Time.mt_System.m_Delay_Now = 0;
        mt_Time.mt_System.m_dwCycle_Per_Second = 1000;

        // ���۽ð� ���
        m_dwStartingTime = mt_Time.mt_System.m_Time_Now;

        if(_bUse_Performance_Counter && mFN_Test_Performance_Counter())
        {
            /* ���ػ� Ÿ�̸� ��� */
            m_bUsing_Performance_Counter = TRUE;

            mFN_QueryPerformanceCounter();
            mt_Time.mt_Large.m_Time_Prev = mt_Time.mt_Large.m_Time_Now;
            mt_Time.mt_Large.m_Delay_Now = 0;

            // Cycle_Per_Second �� mFN_Test_Performance_Counter() �޼ҵ忡�� �ʱ�ȭ
        }
        else
        {
            /* timeGetTime ��� */
            m_bUsing_Performance_Counter = FALSE;

            // mt_System mt_Small ����ü
            mt_Time.m_fCycle_Per_Second = static_cast<float>(mt_Time.mt_System.m_dwCycle_Per_Second);
        }

        // ��Ÿ ����
        m_fFPS = 0.f;
        m_fSPF = 0.f;

        // ���
        mt_Statistic.m_fFPS_Average     = 0.f;
        mt_Statistic.m_fSum_FPS         = 0.f;
        mt_Statistic.m_Focus_Array_FPS  = MAXDWORD; // magic code
    }

    BOOL CTimer::mFN_Frame()
    {
        BOOL _bSuccess;
        mFN_Frame__System_Timer();
        if(m_bUsing_Performance_Counter)
            _bSuccess = mFN_Frame__Large_Timer();
        else
            _bSuccess = mFN_Frame__Small_Timer();

        if(_bSuccess)
        {
            // ������Ʈ
            m_fFPS = mt_Time.m_fCycle_Per_Second / mt_Time.m_fDelay_Now;
            m_fSPF = mt_Time.m_fDelay_Now / mt_Time.m_fCycle_Per_Second;

            // ��� ���
            mFN_Cal_Statistic();
        }
        else
        {
            // Ÿ�̸� ������ ������ �ɷ��ִ� ��쿡 ���Ͽ�,
            // ���ѽð��� ������ ���� ��� �⺻������ �ƹ��ϵ� ���� �ʾƾ� ������,
            // �̶� SPF, Delay_System���� ��ȿ �ϹǷ� ��� 0���� ���Ѵ�.
            //  ��, FPS�� ��踦 ���� ����ϱ⶧���� �״�� �д�.
            mt_Time.mt_System.m_Delay_Now = 0;
            m_fSPF                        = 0.f;
        }

        return _bSuccess;        
    }

    // Timer ����(�ε��� �����ɸ��� �۾� �Ϸ��� ȣ���մϴ�)
    void CTimer::mFN_ResetTimer()
    {
        if(m_bUsing_Performance_Counter)
        {
            mFN_QueryPerformanceCounter();
            mt_Time.mt_Large.m_Time_Prev = mt_Time.mt_Large.m_Time_Now;
            mt_Time.mt_Large.m_Delay_Now = 0;
        }
                
        ::timeBeginPeriod(1);
        mt_Time.mt_System.m_Time_Now  = ::timeGetTime();
        ::timeEndPeriod(1);
        mt_Time.mt_System.m_Time_Prev = mt_Time.mt_Small.m_Time_Now;
        mt_Time.mt_System.m_Delay_Now = 0;

        mt_Time.m_fDelay_Now = 0.f;

        //m_fFPS = 0.f;     // FPS�� ��踦 ���� �״�� �д�.
        m_fSPF = 0.f;
    }

    /*----------------------------------------------------------------
    /       ���� �������̽�
    ----------------------------------------------------------------*/

    // Set: ������ ����
    BOOL CTimer::mFN_Set_LimitFPS(BOOL _bUseLimit, float _fFPS)
    {
        m_bLimitFPS = _bUseLimit;
        if(FALSE == _bUseLimit)
            return TRUE;

        //����ó��
        if(0.f >= _fFPS || _fFPS > mt_Time.m_fCycle_Per_Second)
        {
            _DebugBreak( _T("FPS ���� ��ġ�� �߸��Ǿ����ϴ�.\n") );
            m_bLimitFPS = FALSE;
            return FALSE;
        }

        m_fLimitFPS = _fFPS;

        // 0.5 �� ���� �ݿø� ���� ������ ���ػ� Ÿ�̸�(�ʴ� 1000ƽ)�� ���
        // 60 ������������ ��� 62 �������� ������ �ȴ�
        // ������� �ǵ��� � ��ġ�� ���� �ʵ��� �����ϱ� ������ �̴� �ùٸ� ����� �ƴϴ�
        if(m_bUsing_Performance_Counter)
            m_llDelay_LimitFPS  = static_cast<LONGLONG>( mt_Time.m_fCycle_Per_Second / _fFPS + 0.5f);
        else
            m_dwDelay_LimitFPS  = static_cast<DWORD>( mt_Time.m_fCycle_Per_Second / _fFPS + 0.5f);

        return TRUE;
    }

    BOOL CTimer::mFN_QueryPerformanceCounter()
    {
        DWORD_PTR oldmask = ::SetThreadAffinityMask(::GetCurrentThread(), 1);
        BOOL bSuccessed = ::QueryPerformanceCounter(&mt_Time.mt_Large.m_Time_Now);
        ::SetThreadAffinityMask(::GetCurrentThread(), oldmask);
        return bSuccessed;
    }

    // ���ػ� Ÿ�̸� ���(On / Off)
    BOOL CTimer::mFN_Use_Performance_Counter(BOOL _bUsing)
    {
        if(m_bUsing_Performance_Counter == _bUsing)
            return TRUE;

        if(_bUsing)
        {
            if(!mFN_Test_Performance_Counter())
                return FALSE;

            if(m_bLimitFPS)
                m_llDelay_LimitFPS = static_cast<LONGLONG>(mt_Time.m_fCycle_Per_Second / m_fLimitFPS);
        }
        else
        {
            mt_Time.mt_Small.m_dwCycle_Per_Second   = 1000;
            mt_Time.m_fCycle_Per_Second             = 1000.f;

            if(m_bLimitFPS)
                m_dwDelay_LimitFPS = static_cast<DWORD>(mt_Time.m_fCycle_Per_Second / m_fLimitFPS);
        }
        m_bUsing_Performance_Counter = _bUsing;
        
        mFN_ResetTimer();
        return TRUE;
    }

    // ���ػ� Ÿ�̸� �׽�Ʈ
    BOOL CTimer::mFN_Test_Performance_Counter()
    {
        LARGE_INTEGER _Time_Frequency;
        if( !QueryPerformanceFrequency(&_Time_Frequency) )
            return FALSE;

        if(_Time_Frequency.QuadPart == 0)
            return FALSE;

        if(!mFN_QueryPerformanceCounter())
            return FALSE;
        
        mt_Time.mt_Large.m_llCycle_Per_Second   = _Time_Frequency.QuadPart;
        mt_Time.m_fCycle_Per_Second             = static_cast<float>(_Time_Frequency.QuadPart);
        return TRUE;
    }

    __forceinline void CTimer::mFN_Frame__System_Timer()
    {
        ::timeBeginPeriod(1);
        mt_Time.mt_System.m_Time_Now  = ::timeGetTime();
        ::timeEndPeriod(1);
        mt_Time.mt_System.m_Delay_Now = gFN_Cal_ElapsedTime<DWORD>(mt_Time.mt_System.m_Time_Prev, mt_Time.mt_System.m_Time_Now, MAXDWORD);


        // ���� �ð� ������Ʈ
        if(mt_Time.mt_System.m_Time_Prev > mt_Time.mt_System.m_Time_Now)
        {
            m_dw64TotalPlayTime_Stack += (MAXDWORD - m_dwStartingTime);
            m_dw64TotalPlayTime_Stack += 1;

            m_dwStartingTime = 0;
        }
        const DWORD64 dw64Total = m_dw64TotalPlayTime_Stack + (mt_Time.mt_System.m_Time_Now - m_dwStartingTime);
        m_fTotalPlayTimeSec = static_cast<float>(dw64Total) / mt_Time.mt_System.m_dwCycle_Per_Second;
    }

    // ���������� - ���ػ� Ÿ�̸�
    BOOL CTimer::mFN_Frame__Large_Timer()
    {
        mFN_QueryPerformanceCounter();
        mt_Time.mt_Large.m_Delay_Now = gFN_Cal_ElapsedTime<LONGLONG>(mt_Time.mt_Large.m_Time_Prev.QuadPart, mt_Time.mt_Large.m_Time_Now.QuadPart, MAXLONGLONG);

        if(m_bLimitFPS && mt_Time.mt_Large.m_Delay_Now < m_llDelay_LimitFPS)
            return FALSE;;

        mt_Time.mt_Large.m_Time_Prev = mt_Time.mt_Large.m_Time_Now;
        mt_Time.m_fDelay_Now = static_cast<float>(mt_Time.mt_Large.m_Delay_Now);
        return TRUE;
    }

    // ���������� - ���ػ� Ÿ�̸�
    BOOL CTimer::mFN_Frame__Small_Timer()
    {
        // mt_Time.mt_System mt_Time.mt_Small �� ����ü
        if(m_bLimitFPS && mt_Time.mt_Small.m_Delay_Now < m_dwDelay_LimitFPS)
            return FALSE;

        mt_Time.mt_Small.m_Time_Prev = mt_Time.mt_Small.m_Time_Now;
        mt_Time.m_fDelay_Now = static_cast<float>(mt_Time.mt_Small.m_Delay_Now);
        return TRUE;
    }

    void CTimer::mFN_Cal_Statistic()
    {
        const DWORD cntArray = sizeof(mt_Statistic.m_Array_fFPS) / sizeof(float);
        DWORD& Focus = mt_Statistic.m_Focus_Array_FPS;
        if(mt_Statistic.m_Focus_Array_FPS != MAXDWORD)
        {
            mt_Statistic.m_fSum_FPS -= mt_Statistic.m_Array_fFPS[Focus];
            mt_Statistic.m_fSum_FPS += mFN_Get_Frame_Per_Second();
            mt_Statistic.m_Array_fFPS[Focus] = mFN_Get_Frame_Per_Second();
            if(++Focus >= cntArray)
                Focus = 0;
        }
        else
        {
            Focus = 0;
            for(DWORD i=0; i<cntArray; i++)
                mt_Statistic.m_Array_fFPS[i] = mFN_Get_Frame_Per_Second();

            mt_Statistic.m_fSum_FPS = mFN_Get_Frame_Per_Second() * cntArray;
        }
        mt_Statistic.m_fFPS_Average = mt_Statistic.m_fSum_FPS / static_cast<float>(cntArray);
    }

};