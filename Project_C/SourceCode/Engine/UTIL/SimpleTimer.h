#pragma once

namespace UTIL{


    class CSimpleTimer{
    public:
        CSimpleTimer();

    private:
        // ���ػ� Ÿ�̸� �׽�Ʈ
        BOOL mFN_Test_Performance_Counter();

    private:
        BOOL    m_bUsing_Performance_Counter;

        DWORD   m_dwTimeNow;
        DWORD   m_dwCycle_Per_Second;
        float   m_fCycle_Per_Second;

    public:
        void  mFN_Frame();                                  // ���� �ð��� ������Ʈ
        DWORD mFN_Get_Cycle() const;                        // �ʴ� ����Ŭ�� ����
        DWORD mFN_Get_Time() const;                         // ���� �ð��� ����
        float mFN_Get_DelaySec(DWORD _dwPrevTime) const;    // ���� �ð��� ���ڷ� �޾� ���� �ð��� ����(Sec)
        
        _DEF_MAKE_GET_METHOD(UsingPerformance_Counter, BOOL, m_bUsing_Performance_Counter);
        BOOL  mFN_Use_Performance_Counter(BOOL _bUsing);    // ���ػ� Ÿ�̸� ���(On / Off)
    };


};