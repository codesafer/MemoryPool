#pragma once
#if _DEF_MEMORYPOOL_MAJORVERSION == 2
#include "./MemoryPool_v2/MemoryPoolCore_v2.h"
#elif _DEF_MEMORYPOOL_MAJORVERSION == 1
#include "./MemoryPool_Interface.h"
#include "../../BasisClass/LOCK/Lock.h"

#define _DEF_USING_MEMORYPOOL_GETPROCESSORNUMBER_SIDT_TLS_CACHE _DEF_USING_MEMORYPOOL_UPGRADE_MULTIPROCESSING__A_TLS

#define _DEF_USING_REGISTER_SMALLUNIT__TO__DBH_TABLE 1
#define _DEF_USING_MEMORYPOOL_TEST_SMALLUNIT__FROM__DBH_TABLE 1

// �޸� �̸��������� & ������� �ʴ� �޸� ĳ�ÿ��� ����
#define _DEF_USING_MEMORYPOOL_OPTIMIZE__CACHELINE_PREFETCH_AND_FLUSH 1

// Release ���� ���� �׽�Ʈ �÷���
// �׽�Ʈ���� ���
// #define _DEF_USING_MEMORYPOOL_DEBUG 1



// �޸�Ǯ �����
#if !defined(_DEF_USING_MEMORYPOOL_DEBUG)
  #if defined(_DEBUG)
    #define _DEF_USING_MEMORYPOOL_DEBUG 1
  #else
    #define _DEF_USING_MEMORYPOOL_DEBUG 0
  #endif
#endif

// �޸�Ǯ ����� - �޸� �����ڵ�
#if !defined(_DEF_USING_MEMORYPOOL_DEBUG__CHECK_OVERFLOW)
  #if defined(_DEBUG) && _DEF_USING_MEMORYPOOL_DEBUG
    #define _DEF_USING_MEMORYPOOL_DEBUG__CHECK_OVERFLOW 1
  #else
    #define _DEF_USING_MEMORYPOOL_DEBUG__CHECK_OVERFLOW 0
  #endif
#endif


#pragma warning(push)
#pragma warning(disable: 4324)
#pragma warning(disable: 4310)
namespace UTIL{
namespace MEM{
    namespace GLOBAL{
    #pragma region ����(����Ÿ�̹�)
        extern const size_t gc_SmallUnitSize_Limit;
        extern const size_t gc_minPageUnitSize;
    #pragma endregion
    #pragma region ���� ����
    #if _DEF_USING_DEBUG_MEMORY_LEAK
        extern BOOL g_bDebug__Trace_MemoryLeak;
    #endif
    #if _DEF_USING_MEMORYPOOL_DEBUG
        extern BOOL g_bDebug__Report_OutputDebug;
    #endif
    #pragma endregion
    }

    struct MemoryPool_UTIL{
        template<typename T>
        __forceinline static T* sFN_PopFront(T*& SourceF, T*& SourceB)
        {
            T* pReturn = SourceF;
            
            SourceF = SourceF->pNext;
            if(!SourceF)
                SourceB = nullptr;

            return pReturn;
        }

        template<typename T>
        __forceinline static void sFN_PushFront(T*& DestF, T*& DestB, T* p)
        {
            if(DestF)
            {
                p->pNext = DestF;
                DestF = p;
            }
            else
            {
                DestF = DestB = p;
                p->pNext = nullptr;
            }
        }
        template<typename T>
        __forceinline static void sFN_PushFrontAll(T*& DestF, T*& DestB, T*& SourceF, T*& SourceB)
        {
            if(DestB)
            {
                SourceB->pNext = DestF;
                DestF = SourceF;
            }
            else
            {
                DestF = SourceF;
                DestB = SourceB;
            }
            SourceF = nullptr;
            SourceB = nullptr;
        }

        // cnt �� �ʹ� ���Ƽ��� �ȵ˴ϴ�
        template<typename T>
        __forceinline static void sFN_PushFrontN(T*& DestF, T*& DestB, T*& SourceF, T*& SourceB, size_t cnt)
        {
            // cnt �� 1�̻��ϰ�
            _Assert(0 < cnt);

            T* pLast = SourceF;
            for(size_t i=1; i<cnt; i++)
                pLast = pLast->pNext;

            T* SourceF_After = pLast->pNext;

            pLast->pNext = DestF;
            DestF = SourceF;
            if(!DestB)
                DestB = pLast;

            SourceF = SourceF_After;
            if(!SourceF_After)
                SourceB = nullptr;
        }
        template<typename T>
        __forceinline static void sFN_PushBack(T*& DestF, T*& DestB, T* p)
        {
            if(DestB)
                DestB->pNext = p;
            else
                DestF = p;
            DestB = p;
            p->pNext = nullptr;
        }
        template<typename T>
        __forceinline static void sFN_PushBackN(T*& DestF, T*& DestB, T*& SourceF, T*& SourceB, size_t cnt)
        {
            // cnt �� 1�̻��ϰ�
            _Assert(0 < cnt);

            T* pLast = SourceF;
            for(size_t i=1; i<cnt; i++)
                pLast = pLast->pNext;

            if(DestB)
            {
                DestB->pNext = SourceF;
                DestB = pLast;
            }
            else
            {
                DestF = SourceF;
                DestB = pLast;
            }

            SourceF = pLast->pNext;
            if(!SourceF)
                SourceB = nullptr;

            pLast->pNext = nullptr;
        }
        template<typename T>
        __forceinline static void sFN_PushBackAll(T*& DestF, T*& DestB, T*& SourceF, T*& SourceB)
        {
            if(DestB)
            {
                DestB->pNext = SourceF;
                DestB = SourceB;
            }
            else
            {
                DestF = SourceF;
                DestB = SourceB;
            }
            SourceF = nullptr;
            SourceB = nullptr;
        }
    };
}
}



namespace UTIL{
namespace MEM{

#pragma region TMemoryObject
    struct TMemoryObject{
        TMemoryObject* pNext;
        __forceinline TMemoryObject* GetNext(){ return pNext; }
    };
    _MACRO_STATIC_ASSERT(sizeof(TMemoryObject) == sizeof(void*));
#pragma endregion

#pragma region TMemoryBasket and CACHE
#pragma pack(push, 8)
    struct TUnits{
        size_t         m_nUnit = 0;
        TMemoryObject* m_pUnit_F = nullptr;
        TMemoryObject* m_pUnit_B = nullptr;
    };
    struct _DEF_CACHE_ALIGN TMemoryBasket final : public CUnCopyAble{
        TMemoryBasket(size_t _index_CACHE);
        ~TMemoryBasket();

        inline void Lock(){ m_Lock.Lock__Busy(); }
        inline void UnLock(){ m_Lock.UnLock(); }

        __forceinline TMemoryObject* TMemoryBasket::mFN_Get_Object()
        {
            // Pop Front
            if(0 == m_Units.m_nUnit)
                return nullptr;

            m_Units.m_nUnit--;
        #if _DEF_USING_MEMORYPOOL_DEBUG
            if(m_cnt_Get < c_Debug_MaxCallCounting)
                m_cnt_Get++;
        #endif

            return MemoryPool_UTIL::sFN_PopFront(m_Units.m_pUnit_F, m_Units.m_pUnit_B);
        }
        __forceinline void TMemoryBasket::mFN_Return_Object(TMemoryObject* pObj)
        {
            // Push Front
            _Assert(pObj != nullptr);
            m_Units.m_nUnit++;

        #if _DEF_USING_MEMORYPOOL_DEBUG
            if(m_cnt_Ret < c_Debug_MaxCallCounting)
                m_cnt_Ret++;
        #endif

            MemoryPool_UTIL::sFN_PushBack(m_Units.m_pUnit_F, m_Units.m_pUnit_B, pObj);
        }

        // 64�� �����... 8B * 8
        size_t  m_index_CACHE;

        ::UTIL::LOCK::CSpinLockQ m_Lock;

        size_t m_nDemand;
        TUnits m_Units;

#if _DEF_USING_MEMORYPOOL_DEBUG
        static const UINT32 c_Debug_MaxCallCounting = UINT32_MAX;

        UINT32 m_cnt_Get;
        UINT32 m_cnt_Ret;
        UINT64 m_cnt_CacheMiss_from_Get_Basket;
#else
        UINT32 __FreeSlot1;
        UINT32 __FreeSlot2;
        UINT64 __FreeSlot3;
#endif
    };

    struct _DEF_CACHE_ALIGN TMemoryBasket_CACHE{
        TMemoryBasket_CACHE()
        {
            m_nMax_Keep = 0;

            m_Keep.m_nUnit = 0;
            m_Keep.m_pUnit_F = m_Keep.m_pUnit_B = nullptr;

            m_Share.m_nUnit = 0;
            m_Share.m_pUnit_F = m_Share.m_pUnit_B = nullptr;
        }
        inline void Lock(){ m_Lock.Lock__Busy(); }
        inline void UnLock(){ m_Lock.UnLock(); }

        // 64B
        ::UTIL::LOCK::CSpinLockQ m_Lock;
        size_t m_nMax_Keep;

        TUnits m_Keep;
        TUnits m_Share;
    };

    _MACRO_STATIC_ASSERT(64 == _DEF_CACHELINE_SIZE && sizeof(void*) == sizeof(size_t));
    _MACRO_STATIC_ASSERT(sizeof(::UTIL::LOCK::CSpinLock) == 8);
    _MACRO_STATIC_ASSERT(sizeof(TMemoryBasket) % _DEF_CACHELINE_SIZE == 0);
    _MACRO_STATIC_ASSERT(sizeof(TMemoryBasket_CACHE) % _DEF_CACHELINE_SIZE == 0);
#pragma pack(pop)
#pragma endregion

#pragma region DATA_BLOCK and Table
    struct _DEF_CACHE_ALIGN TDATA_BLOCK_HEADER{
        enum struct _TYPE_Units_SizeType : UINT32{
            // ������ �ٸ� �޸����ϰ� ��ġ�� �ʵ��� ����ũ �ϵ���
            E_Invalid    = 0, // ��ȿ
            E_SmallSize  = 0x012E98F5,
            E_NormalSize = 0x3739440C,
            E_OtherSize  = 0x3617DCF9,
        };
        // 32B ~ �ʼ� ������
        _TYPE_Units_SizeType    m_Type;
        struct{
            UINT16 high;    //(0 ~(64KB / sizeof(void*))
            UINT16 low;     //(0 ~(64KB / sizeof(void*))
        }m_Index;

        void* m_pUserValidPTR_S; //����� ���� ����
        void* m_pUserValidPTR_L; //����� ���� ������

        IMemoryPool* m_pPool;
        // �ʼ� ������ ~ 32B 
        
        

        // ������ ���� 32B ~
        TDATA_BLOCK_HEADER* m_pGroupFront; // �Ҵ������ �׷��� ù��° �ش�
        union{
            size_t __FreeSlot[3];
            struct{
            }ParamNormal;
            struct{
                size_t SizeThisUnit;
            }ParamBad;
        };
        __forceinline BOOL mFN_Query_ContainPTR(void* p) const
        {
            if(p < m_pUserValidPTR_S || m_pUserValidPTR_L < p)
                return FALSE;
            return TRUE;
        }
        __forceinline void* mFN_Get_AllocatedStartPTR() const
        {
            return (byte*)m_pUserValidPTR_S - sizeof(TDATA_BLOCK_HEADER);
        }
    };
    _MACRO_STATIC_ASSERT(sizeof(TDATA_BLOCK_HEADER) == _DEF_CACHELINE_SIZE);

    class _DEF_CACHE_ALIGN CTable_DataBlockHeaders{
    #pragma pack(push, 4)
        struct _DEF_CACHE_ALIGN _TYPE_Link_Recycle_Slots{
            struct TIndex{
                UINT16 h, l;
            };

            _TYPE_Link_Recycle_Slots* pPrevious;
            size_t cnt;

            static const size_t s_maxIndexes = (_DEF_CACHELINE_SIZE - sizeof(void*) - sizeof(size_t)) / sizeof(TIndex);
            TIndex m_Indexes[s_maxIndexes];
        };
        _MACRO_STATIC_ASSERT(sizeof(_TYPE_Link_Recycle_Slots) == _DEF_CACHELINE_SIZE);
    #pragma pack(pop)

        static const UINT32 s_MaxSlot_Table = 64 * 1024 / sizeof(void*);
        _MACRO_STATIC_ASSERT((UINT32)(s_MaxSlot_Table*s_MaxSlot_Table) <= UINT32_MAX);
    
    public:
        CTable_DataBlockHeaders();
        ~CTable_DataBlockHeaders();

    private:
        TDATA_BLOCK_HEADER** m_ppTable[s_MaxSlot_Table] = {0}; // 64KB

        // ���� 64B�� ���ĵ� �����ּ�...
        volatile UINT32 m_cnt_Index_High    = 0;
        volatile UINT32 m_cnt_TotalSlot     = 0;

        _TYPE_Link_Recycle_Slots* m_pRecycleSlots = nullptr;;

        UINT32 __FreeSlot[10];
        
        ::UTIL::LOCK::CSpinLockQ m_Lock;

    private:
        BOOL mFN_Insert_Recycle_Slots(UINT16 h, UINT16 l); // ��� ��ȣ�� �ʿ���
        void mFN_Delete_Last_LinkFreeSlots(); // ��� ��ȣ�� �ʿ���

    public:
        __forceinline TDATA_BLOCK_HEADER* mFN_Get_Link(UINT32 h, UINT32 l)
        {
            auto n = h * s_MaxSlot_Table + l;
            if(m_cnt_TotalSlot <= n)
                return nullptr;

            return m_ppTable[h][l];
        }
        __forceinline const TDATA_BLOCK_HEADER* mFN_Get_Link(UINT32 h, UINT32 l) const
        {
            auto n = h * s_MaxSlot_Table + l;
            if(m_cnt_TotalSlot <= n)
                return nullptr;

            return m_ppTable[h][l];
        }
        void mFN_Register(TDATA_BLOCK_HEADER* p);
        void mFN_UnRegister(TDATA_BLOCK_HEADER *p);
    };
#pragma endregion

#pragma region CMemoryPool_Allocator_Virtual
    class _DEF_CACHE_ALIGN CMemoryPool_Allocator_Virtual final : public CUnCopyAble{
        friend class CMemoryPool;
    public:
        CMemoryPool_Allocator_Virtual(CMemoryPool* pPool);
        ~CMemoryPool_Allocator_Virtual();

        /*----------------------------------------------------------------
        /   ���� ��밴ü ����
        ----------------------------------------------------------------*/
        struct _DEF_CACHE_ALIGN TDATA_VMEM_USED{
            static const size_t c_cntSlot_Max = (_DEF_CACHELINE_SIZE - sizeof(size_t) - sizeof(TDATA_VMEM_USED*)) / sizeof(void*);

            size_t              m_cntUsedSlot;
            TDATA_VMEM_USED*    m_pNext;

            void* m_Array_pUsedPTR[c_cntSlot_Max];

            _MACRO_STATIC_ASSERT(c_cntSlot_Max >= 6);
        };

    private:
        /*----------------------------------------------------------------
        /   ������
        ----------------------------------------------------------------*/
        CMemoryPool* m_pPool;
        BOOL    m_isSmallUnitSize;
        UINT32  m_nUnits_per_Page; // m_isSmallUnitSize ON �϶� �ŷ��Ҽ� �ִ� ��ġ

        size_t m_PageSize;      // GLOBAL::gc_minPageUnitSize �� ���
        size_t m_PagePerBlock;


        // Ȯ��� �ѹ��� �Ҵ�Ǵ� ��ϼ� ����
        // ���� �� ���� 0�̶�� �޸� �Ҵ��� ���� �Ұ����� �����̴�
        size_t m_nLimitBlocks_per_Expansion;

        size_t m_stats_cnt_Succeed_VirtualAlloc;
        size_t m_stats_size_TotalAllocated;         // �Ҵ�� �޸� �հ�
        
        TDATA_VMEM_USED*    m_pVMem_F;

    public:
        BOOL mFN_Set_ExpansionOption__Units_per_Block(size_t nUnits_per_Block, BOOL bWriteLog);
        BOOL mFN_Set_ExpansionOption__LimitBlocks_per_Expansion(size_t nLimitBlocks_per_Expansion, BOOL bWriteLog);
        size_t mFN_Query_ExpansionOption__MaxLimitBlocks_per_Expantion() const;

        size_t mFN_Add_ElementSize(size_t _Byte, BOOL bWriteLog);
        BOOL mFN_Expansion();

    private:
        BOOL mFN_Expansion_PRIVATE(size_t _cntPage);
        void mFN_Take_VirtualMem(void* _pVMem);

        /*----------------------------------------------------------------
        /   ���� const �Լ�������, ������� ���忡��
        /   �ٸ� �����尡 ������ �� ��ü�� ������ �����ϸ� ����� Ʋ������ ������
        /   ����� �ɰ� ����ؾ� �Ѵ�
        ----------------------------------------------------------------*/
        size_t mFN_Convert__PageCount_to_Size(size_t _Page) const;
        size_t mFN_Convert__Size_to_PageCount(size_t _Byte) const;
        size_t mFN_Convert__Units_to_PageCount(size_t _Units) const ;
        size_t mFN_Convert__Size_to_Units(size_t _Byte) const; // _Byte�� ������ ũ���� ���
        size_t mFN_Calculate__Size_per_Block() const;
        size_t mFN_Calculate__Units_per_Block() const;
    };
    _MACRO_STATIC_ASSERT(sizeof(CMemoryPool_Allocator_Virtual) == _DEF_CACHELINE_SIZE);
#pragma endregion


#pragma warning(pop)
}
}
#endif