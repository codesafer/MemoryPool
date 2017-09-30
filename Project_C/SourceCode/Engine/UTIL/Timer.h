#pragma once
/*----------------------------------------------------------------
/	[Timer]
/----------------------------------------------------------------
/	���ػ� Ÿ�̸ӶǴ� timeGetTime���� ����մϴ�.
/----------------------------------------------------------------
/
/	����:
/		#1) �ʱ�ȭ �մϴ�.
/			mFN_Init( ���ػ� Ÿ�̸� �������)
/			�̸� ������ �����츦 ���� / ������ ����
/
/       #2) �� �����Ӹ��� �ֿ켱������ ���Լ��� ȣ���մϴ�.
/           mFN_Frame()
/----------------------------------------------------------------
/
/	�ۼ���: lastpenguin83@gmail.com
/	�ۼ���: 09-01-16(��)
/   ������:
----------------------------------------------------------------*/

namespace UTIL{

    class CTimer : public CUnCopyAble{
    public:
        CTimer();
        ~CTimer();

    private:
        DWORD64 m_dw64TotalPlayTime_Stack;  // ���� ���α׷� ����ð�   (ǥ������ ȸ���� ����)
        DWORD   m_dwStartingTime;           // ���α׷� ���۽ð�        (ǥ������ ȸ���� ����ȴ�)
        float   m_fTotalPlayTimeSec;        // ���α׷� ����ð�

        float   m_fFPS;             // Frame Per Seoncd
        float   m_fSPF;             // Second Per Frame

        struct{
            struct{
                LARGE_INTEGER   m_Time_Prev;        // ���ػ� Ÿ�̸�: ������
                LARGE_INTEGER   m_Time_Now;         // ���ػ� Ÿ�̸�: ���簪
                LONGLONG        m_Delay_Now;        // ���� ������
                LONGLONG        m_llCycle_Per_Second;
            }mt_Large;

            union{
                struct{
                    DWORD           m_Time_Prev;    // ���ػ� Ÿ�̸�: ������
                    DWORD           m_Time_Now;     // ���ػ� Ÿ�̸�: ���簪
                    DWORD           m_Delay_Now;    // ���� ������
                    DWORD           m_dwCycle_Per_Second;
                }mt_Small;

                struct{
                    DWORD           m_Time_Prev;    // �ý��� Ÿ�̸�: ���簪
                    DWORD           m_Time_Now;     // �ý��� Ÿ�̸�: ������
                    DWORD           m_Delay_Now;    // ���� ������
                    DWORD           m_dwCycle_Per_Second;
                }mt_System;
            };

            float   m_fCycle_Per_Second;    // �ʴ� �ֱ�
            float   m_fDelay_Now;           // float - ���� ������
        }mt_Time;

        struct {
            float   m_fFPS_Average;     // ��� FPS
            float   m_Array_fFPS[30];
            float   m_fSum_FPS;
            DWORD   m_Focus_Array_FPS;
        }mt_Statistic; // ���

        /*----------------------------------------------------------------
        /       �ɼ�
        ----------------------------------------------------------------*/
        BOOL    m_bUsing_Performance_Counter;   // ���ػ� Ÿ�̸� ����÷���
        BOOL    m_bLimitFPS;                    // ������ ���� �ɼ�
        float   m_fLimitFPS;                    // ������ ����

        union{
            LONGLONG    m_llDelay_LimitFPS;     // ������ ���� ������
            DWORD       m_dwDelay_LimitFPS;     // ������ ���� ������
        };

    public:
        /*----------------------------------------------------------------
        /       ���� �����ӿ�ũ ���� �������̽�
        ----------------------------------------------------------------*/
        void mFN_Init(BOOL _bUse_Performance_Counter);
        BOOL mFN_Frame();
                
        // Timer ����(�ε��� �����ɸ��� �۾� �Ϸ��� ȣ���մϴ�)
        void mFN_ResetTimer();

        /*----------------------------------------------------------------
        /       ���� �������̽�
        ----------------------------------------------------------------*/        
        BOOL mFN_Set_LimitFPS(BOOL _bUseLimit, float _fFPS = 60.f);     // Set: ������ ����
        _DEF_MAKE_GET_METHOD(Frame_Per_Second, float, m_fFPS);          // Get: �ʴ� ������
        _DEF_MAKE_GET_METHOD(Second_Per_Frame, float, m_fSPF);          // Get: �����Ӵ� �ð�
        _DEF_MAKE_GET_METHOD(Delay_System, DWORD, mt_Time.mt_System.m_Delay_Now);   // Get: System Delay
        _DEF_MAKE_GET_METHOD(Time_System, DWORD, mt_Time.mt_System.m_Time_Now);     // Get: System Time
        _DEF_MAKE_GET_METHOD(PlayTime, float, m_fTotalPlayTimeSec);                 // Get: Play Time(Sec)
        _DEF_MAKE_GET_METHOD(Avarage_FPS, float, mt_Statistic.m_fFPS_Average);      // Get: ��� FPS
        _DEF_MAKE_GET_METHOD(UsingPerformance_Counter, BOOL, m_bUsing_Performance_Counter);
        BOOL  mFN_Use_Performance_Counter(BOOL _bUsing);// ���ػ� Ÿ�̸� ���(On / Off)

        __declspec(property(get = mFN_Get_Frame_Per_Second)) float FPS;
        __declspec(property(get = mFN_Get_Second_Per_Frame)) float SPF;


    private:
        BOOL mFN_QueryPerformanceCounter ();
        BOOL mFN_Test_Performance_Counter();    // ���ػ� Ÿ�̸� �׽�Ʈ
        __forceinline void mFN_Frame__System_Timer();
        BOOL mFN_Frame__Large_Timer();          // ���������� - ���ػ� Ÿ�̸�
        BOOL mFN_Frame__Small_Timer();          // ���������� - ���ػ� Ÿ�̸�

        void mFN_Cal_Statistic();
    };

};