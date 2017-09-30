#pragma once
#include "./MemoryPoolCore_v2.h"

#pragma pack(push , 4)
#pragma warning(push)
#pragma warning(disable: 4201)
#pragma warning(disable: 4324)
namespace UTIL{
namespace MEM{
    // OS���� ���� �޸� ����������� �� ū ������ �����ּ� ������ ���� ������� ��ü
    class _DEF_CACHE_ALIGN CVMEM_ReservedMemory_Manager{
    public:
        static const size_t sc_Max_VMEM_Count = 117; // 117 = ������ 0.64%, TVMEM_GROUP ��ü ũ�� x86(512) x64(1024)
        static const size_t sc_VMEM_AllocationSize = GLOBAL::gc_minAllocationSize;
        static const size_t sc_VMEM_TotalAllocationSize = sc_Max_VMEM_Count * sc_VMEM_AllocationSize;
        struct _Calculated_Efficiency{
            // ȿ���� ���
            // �ڵ忡�� ����ؼ��� �ȵ�, �ٸ� ����ġ�� ���ϱ� ���� ����
            static const size_t sc_Size_Minimal_ReservationMem = 64 * 1024;
            static const size_t sc_nNeed_Minimal_ReservationMem = sc_VMEM_AllocationSize / sc_Size_Minimal_ReservationMem - 1;
            static const size_t sc_Size_Waste = sc_Size_Minimal_ReservationMem * sc_nNeed_Minimal_ReservationMem;
            _MACRO_STATIC_ASSERT(sc_VMEM_AllocationSize % sc_Size_Minimal_ReservationMem == 0);
            static const size_t sc_Size_Valid = sc_VMEM_TotalAllocationSize;
            static const size_t sc_Size_ValidKB = sc_Size_Valid / 1024;
            static const size_t sc_Size_ValidMB = sc_Size_Valid / 1024 / 1024;
            static const size_t sc_Size_Total = sc_Size_Valid + sc_Size_Waste;
            // static const float �Ǵ� double �� ������ ����...
            struct _WasteRate__Valid{
                static const size_t sc_temp = 10000 * sc_Size_Waste / sc_Size_Valid;
                // ���Ұ����� / ��ȿ����
                // HHH.LL
                static const size_t sc_Percent_H = sc_temp / 100;
                static const size_t sc_Percent_L = sc_temp % 100;
            };
            struct _WasteRate__Total{
                static const size_t sc_temp = 10000 * sc_Size_Waste / sc_Size_Total;
                // ���Ұ����� / (��ȿ����+���Ұ�����)
                // HHH.LL
                static const size_t sc_Percent_H = sc_temp / 100;
                static const size_t sc_Percent_L = sc_temp % 100;
            };
            //static const double sc_Percent_Waste_Valid = sc_Size_Waste / sc_Size_Valid;

            //static const size_t sc_WasteSize = 
        };

        struct TVMEM_GROUP{
            CVMEM_ReservedMemory_Manager* m_pOwner;
            TVMEM_GROUP* m_pPrev;
            TVMEM_GROUP* m_pNext;
            //--------------------------------
            void* m_Slot[sc_Max_VMEM_Count];
            size_t m_nVMEM;

            byte* m_pValidAddressS;
            byte* m_pValidAddressE;

            void* m_pReservedAddress;
            struct{
                // ���ϴ� ������ ���ĵ����ʴ� ����� �� ���� �ּ�
                byte* m_pFront;// [m_pFront] ~ [m_pFront + m_SizeFront]
                byte* m_pBack;// [m_pBack] ~ [m_pBack + m_SizeBack]
                size_t m_SizeFront;
                size_t m_SizeBack;
            }m_NotAligned;
        }; // TVMEM_GROUP ��ü�� �Ϲ� malloc�� ����Ѵ�(����ڿ� ħ������ ���ɼ��� ū ������ �ִ�)
        static const size_t sc_SizeChunk = sizeof(TVMEM_GROUP);

    private:
        BOOL m_bInitialized;
        BOOL : 32;

        size_t m_System_VirtualMemory_Minimal_ReservationSize;
        size_t m_System_VirtualMemory_Minimal_CommitSize;
        size_t m_VMEM_ReservationSize;



        _DEF_CACHE_ALIGN ::UTIL::LOCK::CSpinLock m_Lock;
        // �� �׷��� ���� �ڷ� �Ű�, ���ʿ��� Ž���� ���δ�
        TVMEM_GROUP* m_pVMEM_Group_First;
        TVMEM_GROUP* m_pVMEM_Group_Last;

    public:
        CVMEM_ReservedMemory_Manager();
        ~CVMEM_ReservedMemory_Manager();

        BOOL mFN_Initialize();
        BOOL mFN_Shutdown();
        
        TDATA_VMEM* mFN_Pop_VMEM();
        void mFN_Push_VMEM(TDATA_VMEM* p);

    private:
        TVMEM_GROUP* mFN_Make_Chunk();
        void mFN_Destroy_Chunk(TVMEM_GROUP* p);

        void mFN_Push__VMEM_GROUP(TVMEM_GROUP* pGroup);
        void mFN_Pop__VMEM_GROUP(TVMEM_GROUP* pGroup);
        void mFN_PushFront__VMEM_GROUP(TVMEM_GROUP* pDest, TVMEM_GROUP* pGroup);
        void mFN_PushBack__VMEM_GROUP(TVMEM_GROUP* pDest, TVMEM_GROUP* pGroup);
        void mFN_Request_Move_to_Front__VMEM_GROUP(TVMEM_GROUP* pGroup);
        void mFN_Move_to_FrontEnd__VMEM_GROUP(TVMEM_GROUP* pGroup);
        void mFN_Move_to_BackEnd__VMEM_GROUP(TVMEM_GROUP* pGroup);

        BOOL mFN_Test_VMEM(const void* p, const TVMEM_GROUP* pOwner);
        void mFN_ExceptionProcess_from_mFN_Test_VMEM(const void* p);
    };


    struct _DEF_CACHE_ALIGN CVMEM_CACHE{
    public:
        struct TStats{
            size_t m_Max_cnt_VMEM = 0;
            ::UTIL::NUMBER::TCounting_UINT64 m_Counting_VMEM_Push = 0;

            size_t m_Current_MemorySize = 0;
            size_t m_Max_Storage_MemorySize = 0;
        };
    public:
        CVMEM_CACHE(size_t sizeKB_per_VMEM);
        ~CVMEM_CACHE();

        BOOL mFN_Shutdown();

        size_t mFN_Query__Size_per_VMEM() const;
        size_t mFN_Query__SizeKB_per_VMEM() const;
        const TStats& mFN_Get_Stats() const;
        BOOL mFN_Set_MaxStorageMemorySize(size_t Size);
        size_t mFN_Add_MaxStorageMemorySize(size_t Size);

        TDATA_VMEM* mFN_Pop();
        void mFN_Push(TDATA_VMEM* p);

        BOOL mFN_Free_AnythingHeapMemory();

    private:
        const size_t mc_Size_per_VMEM;

        ::UTIL::LOCK::CSpinLockQ m_Lock;
        
        TDATA_VMEM* m_pFirst;// �ܹ��� ����
        size_t m_cnt_VMEM;

        TStats m_stats;
    };


    struct _DEF_CACHE_ALIGN TVMEM_CACHE_LIST{
        TVMEM_CACHE_LIST();
        BOOL mFN_Shutdown();
        CVMEM_CACHE* mFN_Find_VMEM_CACHE(size_t Size_per_VMEM);

        BOOL mFN_Free_AnythingHeapMemory();
        BOOL mFN_Free_AnythingHeapMemory(size_t ProtectedSize);

        // CACHE �� ���Ǵ� �⺻ũ��� UTIL::MEM::GLOBAL::gc_minAllocationSize �� ���� ����
        // ������ KB
        CVMEM_CACHE m_256KB = 256;
        CVMEM_CACHE m_512KB = 512;
        CVMEM_CACHE m_1024KB = 1024;
        CVMEM_CACHE m_2048KB = 2048;
        CVMEM_CACHE m_4096KB = 4096;

        static const size_t sc_NumCache = 5;
    };
    _MACRO_STATIC_ASSERT(sizeof(TVMEM_CACHE_LIST) == sizeof(CVMEM_CACHE) * TVMEM_CACHE_LIST::sc_NumCache);
}
}


namespace UTIL{
namespace MEM{
    namespace GLOBAL{
        extern CVMEM_ReservedMemory_Manager g_VEM_ReserveMemory;
        extern TVMEM_CACHE_LIST g_VMEM_Recycle;
    }
}
}
#pragma warning(pop)
#pragma pack(pop)