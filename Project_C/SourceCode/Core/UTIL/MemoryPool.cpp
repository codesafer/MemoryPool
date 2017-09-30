#include "stdafx.h"
#if _DEF_MEMORYPOOL_MAJORVERSION == 1
#include "./MemoryPool.h"

#include "./ObjectPool.h"

#include "../Core.h"
/*----------------------------------------------------------------
/   �޸� ���� ���� ����(����/ť)
/       ���� ���� ����ϰԵ� �����޸� ���� ������ �������ڸ� ������ ����
/           T** ppT; for(;;) ppT[i] = new... for(;;) delete ppT[i]...
/       �̰��� �޸� �����ڿ��� [....] ���� pop front ����
/       �ݳ��޾� push front �ϰ� �Ǹ� 0 1 2 3 4 ... ���� 3 2 1 0 4... ������ ��������
/       �̰��� �ٽ� ����ڰ� ���������� �Ҵ��ϸ� ����ڰ� ���Ե� �޸𸮴� �������� ���������´�
/       ������ ����ڰ� �����ּ�~�����ּ� ������ ����� �����ϵ���, �ϱ� ����
/       pop front ���� push back�� ����Ѵ�
/-----------------------------------------------------------------
/   ������ ���� ����
/       CMemoryPool::mFN_Get_Memory_Process()
/           #7 [all] Basket�� ���� ����� Ȯ��
/           if(mFN_Fill_Basket_from_AllOtherBasketLocalStorage(mb))
/           mb1 mb2 �� ���� ���� ������ �����Ͽ�(������ ��� ���� ����)
/           ��ȣ���� ���縦 ���� ����� �õ��Ѵٸ� ������� ������ �ȴ�
/           SpinLock�� ��� ����Ƚ���� �Ķ���ͷ� �Ͽ� ���
/   �޸�Ǯ ���� ���� �ε����� ����ϴ� ���ũ�Ⱑ ������ ����
/       ���̺� ũ����� ����
/-----------------------------------------------------------------
/   m_map_Lend_MemoryUnits
/       ������ ���� ������ ó���� �����Ѱ�?
/       map -> unordered_map            20.4516 sec -> 18.0069 sec
/-----------------------------------------------------------------
/   ���ʿ����� ��� ����
/       _DEF_USING_MEMORYPOOLMANAGER_SAFEDELETE
/-----------------------------------------------------------------
/   ġ���� ���� ����
/       ��뷮�� �޸𸮰� Basket�� ������ ����
/       �� ������ 1������ �����϶� ������ ģȭ���� �������ϰ� OS�� n���� ���μ����� ����Ҷ� ��Ȥ �߻�
/       ���� ������ �ſ� ����� ������ �־���
/       Report ��� ���䰡 0�� Basket�� �뷮�� �޸𸮰� ������ ������ Ȯ��
/       mFN_Return_Memory_Process_Default ���� Basket���� cache�� �̵���ų�� ������ �̻� �̵���Ű�� ���� ������ �߸��Ǿ� ��뷮�� �޸𸮰� basket�� ���Ǵ� ����
/       if(mb.m_nDemand < GLOBAL::gc_LimitMinCounting_List) ���ּ��� �ƴ� ����� ���Ѱ� �߸�
/----------------------------------------------------------------*/
#define _DEF_USING_MEMORYPOOL_GETPROCESSORNUMBER_SIDT _DEF_USING_MEMORYPOOL_UPGRADE_MULTIPROCESSING__A

#define _DEF_INLINE_CHANGE_DEMAND __forceinline


namespace UTIL{
namespace MEM{
    namespace GLOBAL{
    #pragma region ����(����Ÿ�̹�)
        // ����Ʈ �ڷᱸ�� ī���� ����
        // �⺻128, Ŭ���� ���� CPU ������δ� ���� ���
        const size_t gc_LimitMinCounting_List = 4;
        const size_t gc_LimitMaxCounting_List = 128;
        // ���� ũ�� ��꿡 ����ϴ� ȯ�溯��
        // �� ���� ����� gc_Size_Table_MemoryPool , gFN_Get_MemoryPoolTableIndex ���� ������Ʈ�Ұ�
        const size_t gc_KB = 1024;
        const size_t gc_MB = gc_KB * 1024;
        const size_t gc_Array_Limit[]   = {256 , 512 , 1*gc_KB , 2*gc_KB , 4*gc_KB , 8*gc_KB , 16*gc_KB , 64*gc_KB , 512*gc_KB , gc_MB    , 10*gc_MB};
        const size_t gc_Array_MinUnit[] = {8   , 16  , 32      , 64      , 128     , 256     , 512      , 1*gc_KB  , 8*gc_KB   , 64*gc_KB , 512*gc_KB};
        const size_t gc_minSize_of_MemoryPoolUnitSize = 8;
        const size_t gc_maxSize_of_MemoryPoolUnitSize = gc_Array_Limit[_MACRO_ARRAY_COUNT(gc_Array_Limit) - 1];

        const size_t gc_SmallUnitSize_Limit = 2048;

        // ����ӵ��� �϶��� �����ϴ� �ʹ� ���� CPU ��(���� ���� ������)
        const size_t gc_Max_ThreadAccessKeys = 16;
        // ���� ����
        const size_t gc_AlignSize_LargeUnit = _DEF_CACHELINE_SIZE;
        // �ּ� ���������� ũ��� Windows: 64KB
        const size_t gc_minPageUnitSize = 64 * 1024;
        _MACRO_STATIC_ASSERT(_DEF_CACHELINE_SIZE == 64);
    #pragma endregion

    #pragma region ���� ������
        // �޸�Ǯ �ٷΰ��� ���̺�
        // 354 ���� x86(1.38KB) x64(2.76KB)
        const size_t gc_Size_Table_MemoryPool = gc_KB / 8
            + (gc_KB*4 - gc_KB) / 64
            + (gc_KB*16 - gc_KB*4) / 256
            + (gc_KB*64 - gc_KB*16) / gc_KB
            + (gc_KB*512 - gc_KB*64) / (gc_KB*8)
            + (gc_MB - gc_KB*512) / (gc_KB*64)
            + (gc_MB*10 - gc_MB) / (gc_KB*512);
        CMemoryPool* g_pTable_MemoryPool[gc_Size_Table_MemoryPool] = {nullptr};

        // ���Ἲ Ȯ�ο� ���̺�
        _DEF_CACHE_ALIGN CTable_DataBlockHeaders    g_Table_BlockHeaders_Normal;
        _DEF_CACHE_ALIGN CTable_DataBlockHeaders    g_Table_BlockHeaders_Big;
    #pragma endregion



    #pragma region ���� ����
        _DEF_CACHE_ALIGN byte g_Array_Key__CpuCore_to_CACHE[64] = {0};

    #if _DEF_USING_MEMORYPOOL_GETPROCESSORNUMBER_SIDT
        #if __X64
        size_t g_ArrayProcessorCode[64] = {0};
        #elif __X86
        size_t g_ArrayProcessorCode[32] = {0};
        #else
        size_t g_ArrayProcessorCode[?] = {0};
        #endif
    #endif
    #if _DEF_USING_MEMORYPOOL_GETPROCESSORNUMBER_SIDT_TLS_CACHE
        TMemoryPool_TLS_CACHE&(*gpFN_TlsCache_AccessFunction)(void) = nullptr;
    #endif

        size_t g_nProcessor = 1;
        size_t g_nBaskets   = 1;

        size_t g_nBasket__CACHE = 1;
        size_t g_nCore_per_Moudule = 1;  // (���� ����� �ƴ� �����̶�� 1�� �����Ѵ�)
        size_t g_iCACHE_AccessRate = 1;

        BOOL g_bDebug__Trace_MemoryLeak = FALSE;
        BOOL g_bDebug__Report_OutputDebug = FALSE;
    #pragma endregion


        size_t gFN_Get_MemoryPoolTableIndex(size_t _Size)
        {
            _Assert(_Size >= 1);

            _Size -= 1;

            const size_t l[] = {gc_KB,  gc_KB*4,    gc_KB*16,   gc_KB*64,   gc_KB*512,  gc_MB,      gc_MB*10};
            const size_t u[] = {8,      64,         256,        gc_KB,      gc_KB*8,    gc_KB*64,   gc_KB*512};
            const size_t s0 = 0;
            const size_t s1 = s0 + l[0] / u[0];
            const size_t s2 = s1 + (l[1]-l[0]) / u[1];
            const size_t s3 = s2 + (l[2]-l[1]) / u[2];
            const size_t s4 = s3 + (l[3]-l[2]) / u[3];
            const size_t s5 = s4 + (l[4]-l[3]) / u[4];
            const size_t s6 = s5 + (l[5]-l[4]) / u[5];


        #define __temp_macro_return_index(N) if(_Size < l[N]) return (_Size-l[N-1])/u[N] + s##N

            if(_Size < l[0])
                return _Size / u[0];
            __temp_macro_return_index(1);
            __temp_macro_return_index(2);
            __temp_macro_return_index(3);
            __temp_macro_return_index(4);
            __temp_macro_return_index(5);
            __temp_macro_return_index(6);

            return MAXSIZE_T;
        #undef __temp_macro_return_index
        }
        void gFN_Set_MemoryPoolTable(size_t _Index, CMemoryPool* p)
        {
            if(_Index >= gc_Size_Table_MemoryPool)
                return;

            g_pTable_MemoryPool[_Index] = p;
        }
        DECLSPEC_NOINLINE size_t gFN_Calculate_UnitSize(size_t _Size)
        {
            // �� �Լ��� ȣ���� ������ �ſ� ����
            // �� �Լ��� ������ �Լ����� ȣ���ϴ� ���� �����Ѵ� : CMemoryPool_Manager::mFN_Get_MemoryPool(size_t _Size)
            if(gc_maxSize_of_MemoryPoolUnitSize < _Size)
                return 0;
            if(!_Size)
                return _Size;

        #pragma message("gFN_Calculate_UnitSize ������ üũ ����: �Ҵ����� ������ ����� ĳ�ö����� ũ���� ��� �Ǵ� ���")

            // Ǯ�� �پ��ϰ� ������ ���� �����尡 �ܵ������� Ȯ���� �ö󰡰�����,
            // �ڵ��� ���� �Ҹ��Ѵ�

            // �� ĳ�ö��� ũ�⸦ �������� �ۼ��Ѵ�
            //    (8 �̸��� 8���� ������ ����Ѵ�)
            //  256 ���� 8�� ���
            //      8 16 .. 244 256                     [32]
            //  512 ���� 16�� ���
            //      272 288 .. 496 512                  [16]        �ִ� ���� 6.25% �̸�
            //  1KB ���� 32�� ���
            //      544 576 .. 992 1024                 [16]        �ִ� ���� 6.25% �̸�
            //  2KB ���� 64�� ���
            //      1088 1152 .. 1984 2048              [16]        �ִ� ���� 6.25% �̸�
            //  4KB ���� 128�� ���
            //      2176 2304 .. 3968 4096              [16]        �ִ� ���� 6.25% �̸�
            //  8KB ���� 256�� ���
            //      4352 4608 .. 7936 8192              [16]        �ִ� ���� 6.25% �̸�
            //  16KB ���� 512�� ���
            //      8704 9216 .. 12872 16384            [16]        �ִ� ���� 6.25% �̸�
            //----------------------------------------------------------------
            //  64KB ���� 1KB�� ���
            //      17KB 18KB .. 63KB 64KB              [48]        �ִ� ���� 6.25% �̸�
            //----------------------------------------------------------------
            //  512KB ���� 8KB�� ���
            //      72KB 80KB .. 504KB 512KB            [56]        �ִ� ���� 12.5% �̸�
            //  1MB ���� 64KB�� ���
            //      576KB 640KB .. 960KB 1024KB         [8]         �ִ� ���� 12.5% �̸�
            //  10MB ���� �ʰ� 512KB�� ���
            //      1.5MB 2MB .. 9.5MB 10MB             [18]        �ִ� ���� 50% �̸�
            //  10MB �ʰ� ����
        #if _DEF_CACHELINE_SIZE != 64
        #error _DEF_CACHELINE_SIZE != 64
        #endif


        #pragma warning(push)
        #pragma warning(disable: 4127)
            _Assert((_MACRO_ARRAY_COUNT(gc_Array_Limit) == _MACRO_ARRAY_COUNT(gc_Array_MinUnit)));
        #pragma warning(pop)

            for(int i=0; i<_MACRO_ARRAY_COUNT(gc_Array_Limit); i++)
            {
                if(_Size <= gc_Array_Limit[i])
                {
                    const auto m = gc_Array_MinUnit[i];
                    return (_Size + m - 1) / m * m;
                }
            }

            // 0 ũ��� ���ü� ����
            _DebugBreak("Return 0 : Calculate_UnitSize");
            return gc_minSize_of_MemoryPoolUnitSize;
        }

#if _DEF_USING_MEMORYPOOL_GETPROCESSORNUMBER_SIDT
        __forceinline size_t gFN_Get_SIDT()
        {
        #pragma pack(push, 1)
            struct idt_t
            {
                UINT16  limit;
                size_t  base;
            };
            // base size : 16bit(16) 32bit(32) 64bit(64)
        #pragma pack(pop)
            idt_t idt;
            __sidt(&idt);
            return idt.base;
        }
#endif


    #pragma region Baskets , Basket_CACHE �� ����Ÿ�ֿ̹� ���� ��������� �ش��ϴ� ������ƮǮ�� ��� ������ ���� �Լ�
        TMemoryBasket* gFN_Baskets_Alloc__Default()
        {
            return (TMemoryBasket*)::_aligned_malloc(sizeof(TMemoryBasket)*g_nProcessor, _DEF_CACHELINE_SIZE);
        }
        void gFN_Baskets_Free__Default(void* p)
        {
            ::_aligned_free(p);
        }
        template<size_t Size>
        TMemoryBasket* gFN_Baskets_Alloc()
        {
            CObjectPool_Handle_TSize<Size*sizeof(TMemoryBasket)>::Reference_Attach();
            return (TMemoryBasket*)CObjectPool_Handle_TSize<Size*sizeof(TMemoryBasket)>::Alloc();
        }
        template<size_t Size>
        void gFN_Baskets_Free(void* p)
        {
            CObjectPool_Handle_TSize<Size*sizeof(TMemoryBasket)>::Free(p);
            CObjectPool_Handle_TSize<Size*sizeof(TMemoryBasket)>::Reference_Detach();
        }

        template<size_t Size>
        TMemoryBasket_CACHE* gFN_BasketCACHE_Alloc()
        {
            CObjectPool_Handle_TSize<Size*sizeof(TMemoryBasket_CACHE)>::Reference_Attach();
            return (TMemoryBasket_CACHE*)CObjectPool_Handle_TSize<Size*sizeof(TMemoryBasket_CACHE)>::Alloc();
        }
        template<size_t Size>
        void gFN_BasketCACHE_Free(void* p)
        {
            CObjectPool_Handle_TSize<Size*sizeof(TMemoryBasket_CACHE)>::Free(p);
            CObjectPool_Handle_TSize<Size*sizeof(TMemoryBasket_CACHE)>::Reference_Detach();
        }
        TMemoryBasket_CACHE* gFN_BasketCACHE_Alloc__Default()
        {
            return (TMemoryBasket_CACHE*)::_aligned_malloc(sizeof(TMemoryBasket_CACHE)*g_nBasket__CACHE, _DEF_CACHELINE_SIZE);
        }
        void gFN_BasketCACHE_Free__Default(void* p)
        {
            ::_aligned_free(p);
        }

        TMemoryBasket* (*gpFN_Baskets_Alloc)(void) = nullptr;
        void (*gpFN_Baskets_Free)(void*) = nullptr;
        TMemoryBasket_CACHE* (*gpFN_BasketCACHE_Alloc)(void) = nullptr;
        void (*gpFN_BasketCACHE_Free)(void*) = nullptr;
    #pragma endregion

    }
    using namespace GLOBAL;
}
}


/*----------------------------------------------------------------
/   �޸�Ǯ
/---------------------------------------------------------------*/
namespace UTIL{
namespace MEM{



    CMemoryPool__BadSize::CMemoryPool__BadSize()
        : m_UsingSize(0)
        , m_UsingCounting(0)
        , m_bWriteStats_to_LogFile(TRUE)
        , m_stats_Maximum_AllocatedSize(0)
        , m_stats_Counting_Free_BadPTR(0)
    {
    }

    CMemoryPool__BadSize::~CMemoryPool__BadSize()
    {
        mFN_Debug_Report();
    }
    //----------------------------------------------------------------
    #if _DEF_USING_DEBUG_MEMORY_LEAK
    void * CMemoryPool__BadSize::mFN_Get_Memory(size_t _Size, const char * _FileName, int _Line)
    {
        const size_t _RealSize = mFN_Calculate_UnitSize(_Size) + sizeof(TDATA_BLOCK_HEADER);
        TDATA_BLOCK_HEADER* p = (TDATA_BLOCK_HEADER*)::_aligned_malloc_dbg(_RealSize,  _DEF_CACHELINE_SIZE, _FileName, _Line);
        if(!p)
            return nullptr;

        p->ParamBad.SizeThisUnit = _RealSize;


        InterlockedExchangeAdd(&m_UsingCounting, 1);
        const auto nUsingSizeNow = InterlockedExchangeAdd(&m_UsingSize, _RealSize) + _RealSize;
        for(;;)
        {
            const auto nMaxMemNow = m_stats_Maximum_AllocatedSize;
            if(nMaxMemNow >= nUsingSizeNow)
            {
                break;
            }
            else //if(nMaxMemNow < nUsingSizeNow)
            {
                if(nMaxMemNow == InterlockedCompareExchange(&m_stats_Maximum_AllocatedSize, nMaxMemNow, nUsingSizeNow))
                    break;
            }
        }

        mFN_Register(p, _RealSize);
        return p+1;
    }
    #else
    void * CMemoryPool__BadSize::mFN_Get_Memory(size_t _Size)
    {
        const size_t _RealSize = mFN_Calculate_UnitSize(_Size) + sizeof(TDATA_BLOCK_HEADER);
        TDATA_BLOCK_HEADER* p = (TDATA_BLOCK_HEADER*)::_aligned_malloc(_RealSize, _DEF_CACHELINE_SIZE);
        if(!p)
            return nullptr;

        p->ParamBad.SizeThisUnit = _RealSize;


        InterlockedExchangeAdd(&m_UsingCounting, 1);
        const auto nUsingSizeNow = InterlockedExchangeAdd(&m_UsingSize, _RealSize) + _RealSize;
        for(;;)
        {
            const auto nMaxMemNow = m_stats_Maximum_AllocatedSize;
            if(nMaxMemNow >= nUsingSizeNow)
            {
                break;
            }
            else //if(nMaxMemNow < nUsingSizeNow)
            {
                if(nMaxMemNow == InterlockedCompareExchange(&m_stats_Maximum_AllocatedSize, nMaxMemNow, nUsingSizeNow))
                    break;
            }
        }

        mFN_Register(p, _RealSize);
        return p+1;
    }
    #endif

    void CMemoryPool__BadSize::mFN_Return_Memory(void * pAddress)
    {
        if(!pAddress)
            return;

        TMemoryObject* pUnit = static_cast<TMemoryObject*>(pAddress);

        // ����ڿ� �Լ��̱� ������ ������ �Ѵ�
        if(!mFN_TestHeader(pUnit))
        {
            InterlockedExchangeAdd(&m_stats_Counting_Free_BadPTR, 1);
            _DebugBreak("�߸��� �ּ� ��ȯ �Ǵ� �ջ�� �ĺ���");
            return;
        }

        mFN_Return_Memory_Process(pUnit);
    }
    void CMemoryPool__BadSize::mFN_Return_MemoryQ(void* pAddress)
    {
        #ifdef _DEBUG
        CMemoryPool__BadSize::mFN_Return_Memory(pAddress);
        #else
        if(!pAddress)
            return;

        mFN_Return_Memory_Process(static_cast<TMemoryObject*>(pAddress));
        #endif
    }
    //----------------------------------------------------------------
#pragma region �� ����� ���� �ʴ� �޼ҵ�
    size_t CMemoryPool__BadSize::mFN_Query_This_UnitSize() const
    {
        return 0;
    }
    BOOL CMemoryPool__BadSize::mFN_Query_Stats(TMemoryPool_Stats* p)
    {
        if(!p)
            return FALSE;

        ::memset(p, 0, sizeof(TMemoryPool_Stats));
        // �ٻ� ��
        for(size_t i=0; i<0xffff; i++)
        {
            p->Base.nUnits_Using      = m_UsingCounting;
            p->Base.nCurrentSize_Pool = m_UsingSize;
            p->Base.nMaxSize_Pool     = m_stats_Maximum_AllocatedSize;

            if(p->Base.nUnits_Using != m_UsingCounting)
                continue;
            if(p->Base.nCurrentSize_Pool != m_UsingSize)
                continue;
            if(p->Base.nMaxSize_Pool != m_stats_Maximum_AllocatedSize)
                continue;

            return TRUE;
        }

        return FALSE;
    }
    size_t CMemoryPool__BadSize::mFN_Query_LimitBlocks_per_Expansion()
    {
        return 0;
    }
    size_t CMemoryPool__BadSize::mFN_Query_Units_per_Block()
    {
        return 0;
    }
    size_t CMemoryPool__BadSize::mFN_Query_MemorySize_per_Block()
    {
        return 0;
    }
    size_t CMemoryPool__BadSize::mFN_Query_PreCalculate_ReserveUnits_to_MemorySize(size_t)
    {
        return 0;
    }
    size_t CMemoryPool__BadSize::mFN_Query_PreCalculate_ReserveUnits_to_Units(size_t)
    {
        return 0;
    }
    size_t CMemoryPool__BadSize::mFN_Query_PreCalculate_ReserveMemorySize_to_MemorySize(size_t)
    {
        return 0;
    }
    size_t CMemoryPool__BadSize::mFN_Query_PreCalculate_ReserveMemorySize_to_Units(size_t)
    {
        return 0;
    }
    size_t CMemoryPool__BadSize::mFN_Reserve_Memory__nSize(size_t, BOOL)
    {
        return 0;
    }
    size_t CMemoryPool__BadSize::mFN_Reserve_Memory__nUnits(size_t, BOOL)
    {
        return 0;
    }
    size_t CMemoryPool__BadSize::mFN_Set_ExpansionOption__BlockSize(size_t, size_t, BOOL)
    {
        return 0;
    }
    size_t CMemoryPool__BadSize::mFN_Query_ExpansionOption__MaxLimitBlocks_per_Expantion() const
    {
        return 0;
    }
#pragma endregion
    //----------------------------------------------------------------
    void CMemoryPool__BadSize::mFN_Set_OnOff_WriteStats_to_LogFile(BOOL _On)
    {
        m_bWriteStats_to_LogFile = _On;
    }
    //----------------------------------------------------------------
    void CMemoryPool__BadSize::mFN_Return_Memory_Process(TMemoryObject * pAddress)
    {
        _Assert(pAddress != nullptr);

        // ��� �ջ� Ȯ��
    #ifdef _DEBUG
        BOOL bBroken = TRUE;
        // üũ�� Ȯ�ε� �߰��� ���ΰ�?
        const TDATA_BLOCK_HEADER* pH = reinterpret_cast<TDATA_BLOCK_HEADER*>(pAddress) - 1;
        if(pH->m_Type != TDATA_BLOCK_HEADER::_TYPE_Units_SizeType::E_OtherSize){}
        else if(pH->m_pPool != this){}
        else if(!pH->m_pGroupFront || pH->m_pGroupFront->m_pPool != this){}
        else if(!pH->mFN_Query_ContainPTR(pAddress)){}
        else if(pH->m_pUserValidPTR_S != pH->m_pUserValidPTR_L){}
        else { bBroken = FALSE; }
        _AssertMsg(FALSE == bBroken, "Broken Header Data");
    #endif

        TDATA_BLOCK_HEADER* p = (TDATA_BLOCK_HEADER*)pAddress;
        p--;

        const auto size = p->ParamBad.SizeThisUnit;
        auto prevSize = InterlockedExchangeSubtract(&m_UsingSize, size);
        auto prevCNT  = InterlockedExchangeSubtract(&m_UsingCounting, 1);
        if(prevSize < size || prevCNT == 0)
        {
        #ifdef _DEBUG
            UINT64 a = prevSize;
            UINT64 b = prevCNT;
            _MACRO_OUTPUT_DEBUG_STRING_ALWAYS("�޸� �ݳ� ��ȿ: �޸�Ǯ ������(UsingCounting %d, UsingSize %d Byte) �ݳ�ũ��(%d Byte)\n", b, a, size);
        #endif
            InterlockedExchangeAdd(&m_UsingSize, size);
            InterlockedExchangeAdd(&m_UsingCounting, 1);
            return;
        }

        mFN_UnRegister(p);
    #if _DEF_USING_DEBUG_MEMORY_LEAK
        ::_aligned_free_dbg(p);
    #else
        ::_aligned_free(p);
    #endif
    }
    BOOL CMemoryPool__BadSize::mFN_TestHeader(TMemoryObject * pAddress)
    {
        _CompileHint(nullptr != pAddress);

        const TDATA_BLOCK_HEADER* pH = reinterpret_cast<const TDATA_BLOCK_HEADER*>(pAddress) - 1;

        if(pH->m_Type != TDATA_BLOCK_HEADER::_TYPE_Units_SizeType::E_OtherSize)
            return FALSE;
        if(nullptr == pH->m_pGroupFront)
            return FALSE;

        const TDATA_BLOCK_HEADER* pHTrust = GLOBAL::g_Table_BlockHeaders_Big.mFN_Get_Link(pH->m_Index.high, pH->m_Index.low);
        if(pH->m_pGroupFront != pHTrust)
            return FALSE;
        if(!pHTrust->mFN_Query_ContainPTR(pAddress))
            return FALSE;
        // �׽�Ʈ ���� �ʾƵ� �ŷ��Ѵ�
        //if(pHTrust->m_pPool != this)
        //    return nullptr;
        return TRUE;
    }
    size_t CMemoryPool__BadSize::mFN_Calculate_UnitSize(size_t size) const
    {
        if(!size)
            size = 1;

        return (size - 1 + _DEF_CACHELINE_SIZE) / _DEF_CACHELINE_SIZE * _DEF_CACHELINE_SIZE;
    }
    void CMemoryPool__BadSize::mFN_Register(TDATA_BLOCK_HEADER * p, size_t /*size*/)
    {
        _Assert(p != nullptr);

        p->m_Type = TDATA_BLOCK_HEADER::_TYPE_Units_SizeType::E_OtherSize;

        p->m_pUserValidPTR_S = p+1;
        p->m_pUserValidPTR_L = p+1;

        p->m_pPool = this;
        p->m_pGroupFront = p;

        GLOBAL::g_Table_BlockHeaders_Big.mFN_Register(p);
    }

    void CMemoryPool__BadSize::mFN_UnRegister(TDATA_BLOCK_HEADER * p)
    {
        _Assert(p != nullptr);
        GLOBAL::g_Table_BlockHeaders_Big.mFN_UnRegister(p);
    }
    void CMemoryPool__BadSize::mFN_Debug_Report()
    {
        if(0 < m_UsingCounting || 0 < m_UsingSize)
        {
            UINT64 _64_UsingSize = (UINT64)m_UsingSize;
            UINT64 _64_UsingCounting = (UINT64)m_UsingCounting;
        #if _DEF_USING_DEBUG_MEMORY_LEAK
            _MACRO_OUTPUT_DEBUG_STRING_ALWAYS("================ MemoryPool : Detected memory leaks ================\n");
            _MACRO_OUTPUT_DEBUG_STRING_ALWAYS("MemoryPool(Not Managed Size)\n");
            _MACRO_OUTPUT_DEBUG_STRING_ALWAYS("UsingCounting[%llu] UsingSize[%llu]\n", _64_UsingCounting, _64_UsingSize);
        #endif
            if(m_bWriteStats_to_LogFile)
            {
                _LOG_DEBUG(_T("[Warning] Pool Size[Not Managed Size] : Memory Leak(UsingCounting: %llu , UsingSize: %lluKB)"), _64_UsingCounting, _64_UsingSize);
            }
        }
        
        if(0 < m_stats_Counting_Free_BadPTR)
        {
            // �ɰ��� ��Ȳ�̴� Release ���������� ����Ѵ�
            _LOG_DEBUG__WITH__OUTPUTDEBUGSTR_ALWAYS(FALSE, "================ Critical Error : Failed Count(%Iu) -> MemoryPool[OtherSize]:Free ================"
                , m_stats_Counting_Free_BadPTR);
        }
    }



    CMemoryPool::CMemoryPool(size_t _TypeSize)
        : m_UnitSize(_TypeSize)
        //, m_UnitSize_per_IncrementDemand(?)
        //, m_nLimit_Basket_KeepMax(?)
        //, m_nLimit_Cache_KeepMax(?)
        //, m_nLimit_CacheShare_KeepMax(?)
        , m_pBaskets(nullptr)
        , m_pBasket__CACHE(nullptr)
        // ���
        , m_nUnit(0), m_pUnit_F(nullptr), m_pUnit_B(nullptr)
        , m_stats_Units_Allocated(0)
        , m_stats_OrderCount__ReserveSize(0)
        , m_stats_OrderCount__ReserveUnits(0)
        , m_stats_OrderSum__ReserveSize(0)
        , m_stats_OrderSum__ReserveUnits(0)
        // ���
        , m_stats_OrderResult__Reserve_Size(0)
        , m_stats_OrderResult__Reserve_Units(0)
        , m_stats_OrderCount__ExpansionOption_UnitsPerBlock(0)
        , m_stats_OrderCount__ExpansionOption_LimitBlocksPerExpansion(0)
        , m_stats_Counting_Free_BadPTR(0)
        , m_bWriteStats_to_LogFile(TRUE)
        // ���
        , m_Allocator(this)
        // ���
        , m_Lock(1)
    #if _DEF_USING_DEBUG_MEMORY_LEAK
        , m_Debug_stats_UsingCounting(0)
    #endif
    {
        // ������ ����� �÷����� ���ܴ� �α׳� �ȳ������
        // _AssertRelease, _AssertReleaseMsg �� ������ ����Ѵ�
#if _DEF_USING_MEMORYPOOL_GETPROCESSORNUMBER_SIDT_TLS_CACHE
        _AssertReleaseMsg(nullptr != gpFN_TlsCache_AccessFunction, "�������� ���� �Լ� : mFN_Set_TlsCache_AccessFunction �� ����Ͻʽÿ�");
#endif

        _Assert(sizeof(void*) <= m_UnitSize);

        const UTIL::ISystem_Information* pSys = CORE::g_instance_CORE.mFN_Get_System_Information();
        const SYSTEM_INFO* pInfo = pSys->mFN_Get_SystemInfo();

        // ������ ���� ũ�Ⱑ �ʹ� ũ�ٸ� ����ó��
        // 128KB ����
        _AssertReleaseMsg(128*1024 >= pInfo->dwPageSize, "�� ��ǻ���� ������ ���� ũ�Ⱑ �ʹ� Ů�ϴ�");

        // ���μ��� ���� ������� �������� ������ ������ũ��(4KB) / ���ּ�
        // Val : 1 ~
        m_UnitSize_per_IncrementDemand = pInfo->dwPageSize / m_UnitSize;
        if(m_UnitSize_per_IncrementDemand == 0)
            m_UnitSize_per_IncrementDemand = 1;
        // ���μ��� ���� ������� �ִ� ����� VirtualAlloc ���� �ּҴ���(64KB)�� ������ ���Ѵ�
        // Val : 0 ~
        m_nLimit_Basket_KeepMax = pInfo->dwAllocationGranularity / (m_UnitSize * g_nBaskets);
        if(m_nLimit_Basket_KeepMax < GLOBAL::gc_LimitMinCounting_List)
            m_nLimit_Basket_KeepMax = 0; // ���� ������ ũ�ٸ� �޸� ���� ������ ���� Basket�� �ִ� ���並 0����

        // ������ũ��(���)�� �ý��� ������ũ�� ���� Ȯ��
        _AssertRelease(gc_minPageUnitSize >= pInfo->dwAllocationGranularity && (gc_minPageUnitSize % pInfo->dwAllocationGranularity == 0));

        // ĳ���� ���� �ִ� �������� ���� ū ����ũ�⸦ �������� �Ѵ�
        m_nLimit_Cache_KeepMax  = gc_maxSize_of_MemoryPoolUnitSize / m_UnitSize;
        if(m_nLimit_Cache_KeepMax < 1)
            m_nLimit_Cache_KeepMax = 1;
        m_nLimit_CacheShare_KeepMax = gc_maxSize_of_MemoryPoolUnitSize * 16 / m_UnitSize;
        _AssertReleaseMsg(1 <= m_nLimit_Cache_KeepMax && 1 <  m_nLimit_CacheShare_KeepMax, "��� �Ұ����� ���� ũ��");



        m_pBasket__CACHE = gpFN_BasketCACHE_Alloc();
        for(size_t i=0; i<g_nBasket__CACHE; i++)
        {
            _MACRO_CALL_CONSTRUCTOR(m_pBasket__CACHE+i, TMemoryBasket_CACHE);
        }

        m_pBaskets = gpFN_Baskets_Alloc();
        for(size_t i=0; i<g_nBaskets; i++)
        {
            _MACRO_CALL_CONSTRUCTOR(m_pBaskets+i, TMemoryBasket(g_Array_Key__CpuCore_to_CACHE[i]));
        }

#if _DEF_USING_MEMORYPOOL_DEBUG
        if(GLOBAL::g_bDebug__Report_OutputDebug)
            _MACRO_OUTPUT_DEBUG_STRING_ALWAYS(_T(" - MemoryPool Constructor [%u]\n"), m_UnitSize);
#endif
    }
    CMemoryPool::~CMemoryPool()
    {
        if(m_Lock.Query_isLocked())
        {
            _LOG_DEBUG(_T("[Critical Error] Pool Size[%u] : is Locked ... from Destructor"), m_UnitSize);
        }

        mFN_Debug_Report();
        
        for(size_t i=0; i<g_nBaskets; i++)
            _MACRO_CALL_DESTRUCTOR(m_pBaskets + i);
        gpFN_Baskets_Free(m_pBaskets);

        for(size_t i=0; i<g_nBasket__CACHE; i++)
            _MACRO_CALL_DESTRUCTOR(m_pBasket__CACHE + i);
        gpFN_BasketCACHE_Free(m_pBasket__CACHE);

#if _DEF_USING_MEMORYPOOL_DEBUG
        if(GLOBAL::g_bDebug__Report_OutputDebug)
            _MACRO_OUTPUT_DEBUG_STRING_ALWAYS(_T(" - MemoryPool Destructor [%u]\n"), m_UnitSize);
#endif
    }
    //----------------------------------------------------------------
    #if _DEF_USING_DEBUG_MEMORY_LEAK
    void* CMemoryPool::mFN_Get_Memory(size_t _Size, const char* _FileName, int _Line)
    {
        if(_Size > m_UnitSize)
        {
            _DebugBreak("_Size > m_UnitSize");
            return nullptr;
        }
        auto p = CMemoryPool::mFN_Get_Memory_Process();
        if(p)
        {
            InterlockedExchangeAdd(&m_Debug_stats_UsingCounting, 1);
            if(GLOBAL::g_bDebug__Trace_MemoryLeak)
            {
                m_Lock_Debug__Lend_MemoryUnits.Lock();
                m_map_Lend_MemoryUnits.insert(std::make_pair(p, TTrace_SourceCode(_FileName, _Line)));
                m_Lock_Debug__Lend_MemoryUnits.UnLock();
            }
        }
        return p;
    }
    #else
    void* CMemoryPool::mFN_Get_Memory(size_t _Size) 
    {
        if(_Size <= m_UnitSize)
            return CMemoryPool::mFN_Get_Memory_Process();

        return nullptr;
    }
    #endif
    void CMemoryPool::mFN_Return_Memory(void* pAddress)
    {
        if(!pAddress)
            return;

        TMemoryObject* pUnit = static_cast<TMemoryObject*>(pAddress);

        // ����ڿ� �Լ��̱� ������ ������ �Ѵ�
        if(!mFN_TestHeader(pUnit))
        {
            InterlockedExchangeAdd(&m_stats_Counting_Free_BadPTR, 1);
            _DebugBreak("�߸��� �ּ� ��ȯ �Ǵ� �ջ�� �ĺ���");
            return;
        }

        CMemoryPool::mFN_Return_Memory_Process(static_cast<TMemoryObject*>(pUnit));
    }
    void CMemoryPool::mFN_Return_MemoryQ(void* pAddress)
    {
        #ifdef _DEBUG
        CMemoryPool::mFN_Return_Memory(pAddress);
        return;
        #else
        if(!pAddress)
            return;

        TMemoryObject* pUnit = static_cast<TMemoryObject*>(pAddress);
        CMemoryPool::mFN_Return_Memory_Process(static_cast<TMemoryObject*>(pUnit));
        #endif
    }
    //----------------------------------------------------------------
    size_t CMemoryPool::mFN_Query_This_UnitSize() const
    {
        return m_UnitSize;
    }
    BOOL CMemoryPool::mFN_Query_Stats(TMemoryPool_Stats * pOUT)
    {
        if(!pOUT) return FALSE;
        ::memset(pOUT, 0, sizeof(TMemoryPool_Stats));
        auto& r = *pOUT;

        m_Lock.Begin_Write__INFINITE();
        mFN_all_Basket_Lock();
        mFN_all_BasketCACHE_Lock();
        {
            r.Base.nUnits_Created   = m_stats_Units_Allocated;
            r.Base.nUnits_Using     = m_stats_Units_Allocated - mFN_Counting_KeepingUnits();
            r.Base.nCount_Expansion     = m_Allocator.m_stats_cnt_Succeed_VirtualAlloc;
            r.Base.nTotalSize_Expansion = m_Allocator.m_stats_size_TotalAllocated;

            r.UserOrder.ExpansionOption_OrderCount_UnitsPerBlock            = m_stats_OrderCount__ExpansionOption_UnitsPerBlock;
            r.UserOrder.ExpansionOption_OrderCount_LimitBlocksPerExpansion  = m_stats_OrderCount__ExpansionOption_LimitBlocksPerExpansion;
            r.UserOrder.Reserve_OrderCount_Size         = m_stats_OrderCount__ReserveSize;
            r.UserOrder.Reserve_OrderCount_Units        = m_stats_OrderCount__ReserveUnits;
            r.UserOrder.Reserve_OrderSum_TotalSize      = m_stats_OrderSum__ReserveSize;
            r.UserOrder.Reserve_OrderSum_TotalUnits     = m_stats_OrderSum__ReserveUnits;
            r.UserOrder.Reserve_result_TotalSize        = m_stats_OrderResult__Reserve_Size;
            r.UserOrder.Reserve_result_TotalUnits       = m_stats_OrderResult__Reserve_Units;
        }
        mFN_all_BasketCACHE_UnLock();
        mFN_all_Basket_UnLock();
        m_Lock.End_Write();

        return TRUE;
    }
    size_t CMemoryPool::mFN_Query_LimitBlocks_per_Expansion()
    {
        m_Lock.Begin_Read();
        const size_t r = m_Allocator.m_nLimitBlocks_per_Expansion;
        m_Lock.End_Read();
        return r;
    }
    size_t CMemoryPool::mFN_Query_Units_per_Block()
    {
        m_Lock.Begin_Read();
        const size_t r = m_Allocator.mFN_Calculate__Units_per_Block();
        m_Lock.End_Read();
        return r;
    }
    size_t CMemoryPool::mFN_Query_MemorySize_per_Block()
    {
        m_Lock.Begin_Read();
        const size_t r = m_Allocator.mFN_Calculate__Size_per_Block();
        m_Lock.End_Read();
        return r;
    }
    //----------------------------------------------------------------
    size_t CMemoryPool::mFN_Query_PreCalculate_ReserveUnits_to_MemorySize(size_t nUnits)
    {
        const size_t nPage = m_Allocator.mFN_Convert__Units_to_PageCount(nUnits);
        const size_t r = m_Allocator.mFN_Convert__PageCount_to_Size(nPage);
        return max(r, CMemoryPool::mFN_Query_MemorySize_per_Block());
    }
    size_t CMemoryPool::mFN_Query_PreCalculate_ReserveUnits_to_Units(size_t nUnits)
    {
        const size_t nSize = CMemoryPool::mFN_Query_PreCalculate_ReserveUnits_to_MemorySize(nUnits);
        const size_t r = m_Allocator.mFN_Convert__Size_to_Units(nSize);
        return max(r, CMemoryPool::mFN_Query_Units_per_Block());
    }
    size_t CMemoryPool::mFN_Query_PreCalculate_ReserveMemorySize_to_MemorySize(size_t nByte)
    {
        const size_t nPage = m_Allocator.mFN_Convert__Size_to_PageCount(nByte);
        const size_t r = m_Allocator.mFN_Convert__PageCount_to_Size(nPage);
        return max(r, CMemoryPool::mFN_Query_MemorySize_per_Block());
    }
    size_t CMemoryPool::mFN_Query_PreCalculate_ReserveMemorySize_to_Units(size_t nByte)
    {
        const size_t nSize = CMemoryPool::mFN_Query_PreCalculate_ReserveMemorySize_to_MemorySize(nByte);
        const size_t r =  m_Allocator.mFN_Convert__Size_to_Units(nSize);
        return max(r, CMemoryPool::mFN_Query_Units_per_Block());
    }


    size_t CMemoryPool::mFN_Reserve_Memory__nSize(size_t nByte, BOOL bWriteLog)
    {
        size_t r, n;
        m_Lock.Begin_Write__INFINITE();
        {
            const size_t minimal = m_Allocator.mFN_Calculate__Size_per_Block();
            const size_t nRealByte = max(minimal, nByte);
            r = m_Allocator.mFN_Add_ElementSize(nRealByte, bWriteLog);
            n = m_Allocator.mFN_Convert__Size_to_Units(r);

            m_stats_OrderCount__ReserveSize++;
            m_stats_OrderSum__ReserveSize += nByte;
            m_stats_OrderResult__Reserve_Size += r; 
            m_stats_OrderResult__Reserve_Units += n;
        }
        m_Lock.End_Write();

        return r;
    }

    size_t CMemoryPool::mFN_Reserve_Memory__nUnits(size_t nUnits, BOOL bWriteLog)
    {
        size_t r, n;
        m_Lock.Begin_Write__INFINITE();
        {
            const size_t minimal = m_Allocator.mFN_Calculate__Units_per_Block();
            const size_t nRealUnits = max(minimal, nUnits);
            r = m_Allocator.mFN_Add_ElementSize(nRealUnits * m_UnitSize, bWriteLog);
            n = m_Allocator.mFN_Convert__Size_to_Units(r);

            m_stats_OrderCount__ReserveUnits++;
            m_stats_OrderSum__ReserveUnits += nUnits;
            m_stats_OrderResult__Reserve_Size += r; 
            m_stats_OrderResult__Reserve_Units += n;
        }
        m_Lock.End_Write();

        return n;
    }

    size_t CMemoryPool::mFN_Set_ExpansionOption__BlockSize(size_t nUnits_per_Block, size_t nLimitBlocks_per_Expansion, BOOL bWriteLog)
    {
        if(!nUnits_per_Block && !nLimitBlocks_per_Expansion)
            return 0;

        size_t r;
        m_Lock.Begin_Write__INFINITE();
        {
            BOOL bSucceed = TRUE;
            if(0 < nUnits_per_Block)
            {
                bSucceed &= m_Allocator.mFN_Set_ExpansionOption__Units_per_Block(nUnits_per_Block, bWriteLog);
                m_stats_OrderCount__ExpansionOption_UnitsPerBlock++;
            }
            if(0 < nLimitBlocks_per_Expansion)
            {
                bSucceed &= m_Allocator.mFN_Set_ExpansionOption__LimitBlocks_per_Expansion(nLimitBlocks_per_Expansion, bWriteLog);
                m_stats_OrderCount__ExpansionOption_LimitBlocksPerExpansion++;
            }

            r = (bSucceed)? m_Allocator.mFN_Calculate__Units_per_Block() : 0;
        }
        m_Lock.End_Write();

        return r;
    }
    size_t CMemoryPool::mFN_Query_ExpansionOption__MaxLimitBlocks_per_Expantion() const
    {
        return m_Allocator.mFN_Query_ExpansionOption__MaxLimitBlocks_per_Expantion();
    }
    //----------------------------------------------------------------
    void CMemoryPool::mFN_Set_OnOff_WriteStats_to_LogFile(BOOL _On)
    {
        m_bWriteStats_to_LogFile = _On;
    }
    //----------------------------------------------------------------
    DECLSPEC_NOINLINE void CMemoryPool::mFN_KeepingUnit_from_AllocatorS(byte* pStart, size_t size)
    {
        _Assert(size % gc_minPageUnitSize == 0 && size >= gc_minPageUnitSize);
        const size_t nPage = size / gc_minPageUnitSize;
        const size_t nUnitsPerPage  = (gc_minPageUnitSize - sizeof(TDATA_BLOCK_HEADER)) / m_UnitSize;
        const size_t SizeValidUnit_Page = nUnitsPerPage * m_UnitSize;
        const size_t SizeValid_Page = sizeof(TDATA_BLOCK_HEADER) + SizeValidUnit_Page;
        const size_t cntNewUnit = nUnitsPerPage * nPage;
        if(!cntNewUnit)
            return;

        byte* pUserS = pStart + sizeof(TDATA_BLOCK_HEADER);
        byte* pUserL = pStart + (((nPage-1)*gc_minPageUnitSize) + SizeValid_Page - m_UnitSize);

        TMemoryObject* pPrevious = nullptr;
        union{
            TMemoryObject* pOBJ = nullptr;
            byte* pobj;
        };

        byte* pCulPage = pStart;
        for(size_t iP=0; iP<nPage; iP++)
        {
            TDATA_BLOCK_HEADER* pHF = (TDATA_BLOCK_HEADER*)pCulPage;

            pHF->m_Type  = TDATA_BLOCK_HEADER::_TYPE_Units_SizeType::E_SmallSize;
            pHF->m_pPool = this;
            pHF->m_pUserValidPTR_S = pUserS;
            pHF->m_pUserValidPTR_L = pUserL;
            pHF->m_pGroupFront = (TDATA_BLOCK_HEADER*)pStart;
            // ���� �������� ������ ���� �ִ´�
            // pHF->m_Index

            pobj = pCulPage + sizeof(TDATA_BLOCK_HEADER);
            const byte* pE = pobj + SizeValidUnit_Page;
            if(pPrevious)
                pPrevious->pNext = pOBJ;

            for(;;)
            {
            #if _DEF_USING_MEMORYPOOL_DEBUG__CHECK_OVERFLOW
                mFN_Debug_Overflow_Set(pobj+sizeof(TMemoryObject), m_UnitSize-sizeof(TMemoryObject), 0xDD);
            #endif

                byte* pNext = pobj + m_UnitSize;
                if(pNext >= pE)
                    break;

                pOBJ->pNext = (TMemoryObject*)pNext;
                pobj = pNext;
            }
            pPrevious = pOBJ;

            pCulPage += gc_minPageUnitSize;
        }
        pOBJ->pNext = nullptr;

        TMemoryObject* pUnitF = (TMemoryObject*)(pStart + sizeof(TDATA_BLOCK_HEADER));
        TMemoryObject* pUnitL = pOBJ;

        MemoryPool_UTIL::sFN_PushBackAll(m_pUnit_F, m_pUnit_B, pUnitF, pUnitL);
        m_nUnit                 += cntNewUnit;
        m_stats_Units_Allocated += cntNewUnit;
    }
    DECLSPEC_NOINLINE void CMemoryPool::mFN_KeepingUnit_from_AllocatorS_With_Register(byte * pStart, size_t size)
    {
        _Assert(size % gc_minPageUnitSize == 0 && size >= gc_minPageUnitSize);
        const size_t nPage = size / gc_minPageUnitSize;
        const size_t nUnitsPerPage  = (gc_minPageUnitSize - sizeof(TDATA_BLOCK_HEADER)) / m_UnitSize;
        const size_t SizeValidUnit_Page = nUnitsPerPage * m_UnitSize;
        const size_t SizeValid_Page = sizeof(TDATA_BLOCK_HEADER) + SizeValidUnit_Page;
        const size_t cntNewUnit = nUnitsPerPage * nPage;
        if(!cntNewUnit)
            return;

        byte* pUserS = pStart + sizeof(TDATA_BLOCK_HEADER);
        byte* pUserL = pStart + (((nPage-1)*gc_minPageUnitSize) + SizeValid_Page - m_UnitSize);

        TMemoryObject* pPrevious = nullptr;
        union{
            TMemoryObject* pOBJ = nullptr;
            byte* pobj;
        };

        byte* pCulPage = pStart;
        for(size_t iP=0; iP<nPage; iP++)
        {
            // ��� �ּҴ� ��ü
            TDATA_BLOCK_HEADER* pHF = (TDATA_BLOCK_HEADER*)pCulPage;
            if(iP == 0)
            {
                pHF->m_Type  = TDATA_BLOCK_HEADER::_TYPE_Units_SizeType::E_SmallSize;
                pHF->m_pUserValidPTR_S = pUserS;
                pHF->m_pUserValidPTR_L = pUserL;
                pHF->m_pPool = this;
                pHF->m_pGroupFront = pHF;

                g_Table_BlockHeaders_Normal.mFN_Register(pHF);
            }
            else
            {
                TDATA_BLOCK_HEADER* pFirstHead = (TDATA_BLOCK_HEADER*)pStart;
                *pHF = *pFirstHead;
            }

            pobj = pCulPage + sizeof(TDATA_BLOCK_HEADER);
            const byte* pE = pobj + SizeValidUnit_Page;
            if(pPrevious)
                pPrevious->pNext = pOBJ;

            for(;;)
            {
            #if _DEF_USING_MEMORYPOOL_DEBUG__CHECK_OVERFLOW
                mFN_Debug_Overflow_Set(pobj+sizeof(TMemoryObject), m_UnitSize-sizeof(TMemoryObject), 0xDD);
            #endif

                byte* pNext = pobj + m_UnitSize;
                if(pNext >= pE)
                    break;

                pOBJ->pNext = (TMemoryObject*)pNext;
                pobj = pNext;
            }
            pPrevious = pOBJ;

            pCulPage += gc_minPageUnitSize;
        }
        pOBJ->pNext = nullptr;

        TMemoryObject* pUnitF = (TMemoryObject*)(pStart + sizeof(TDATA_BLOCK_HEADER));
        TMemoryObject* pUnitL = pOBJ;

        MemoryPool_UTIL::sFN_PushBackAll(m_pUnit_F, m_pUnit_B, pUnitF, pUnitL);
        m_nUnit                 += cntNewUnit;
        m_stats_Units_Allocated += cntNewUnit;
    }
    DECLSPEC_NOINLINE void CMemoryPool::mFN_KeepingUnit_from_AllocatorN_With_Register(byte * pStart, size_t size)
    {
        _Assert(size % gc_minPageUnitSize == 0 && size >= gc_minPageUnitSize);

        const size_t nRealUnitSize = m_UnitSize + sizeof(TDATA_BLOCK_HEADER);
        const size_t cntNewUnit = size / nRealUnitSize;
        const size_t TotalRealSize = cntNewUnit * nRealUnitSize;

        if(!cntNewUnit)
            return;

        byte* pUserS = pStart + sizeof(TDATA_BLOCK_HEADER);
        byte* pUserL = pStart + (TotalRealSize - m_UnitSize);

        TDATA_BLOCK_HEADER* pFirstHeader = (TDATA_BLOCK_HEADER*)pStart;
        TMemoryObject* pFirstOBJ = (TMemoryObject*)(pFirstHeader+1);
        {
            pFirstHeader->m_Type = TDATA_BLOCK_HEADER::_TYPE_Units_SizeType::E_NormalSize;
            pFirstHeader->m_pUserValidPTR_S = pUserS;
            pFirstHeader->m_pUserValidPTR_L = pUserL;
            pFirstHeader->m_pPool = this;
            pFirstHeader->m_pGroupFront = pFirstHeader;

            g_Table_BlockHeaders_Normal.mFN_Register(pFirstHeader);
        #if _DEF_USING_MEMORYPOOL_DEBUG__CHECK_OVERFLOW
            mFN_Debug_Overflow_Set(pFirstOBJ+1, m_UnitSize-sizeof(TMemoryObject), 0xDD);
        #endif
            pFirstOBJ->pNext = (TMemoryObject*)((byte*)pFirstOBJ + nRealUnitSize);
        }

        union{
            TDATA_BLOCK_HEADER* pHead;
            TMemoryObject* pOBJ;
            byte* pobj;
        };
        const byte* pE = pStart + TotalRealSize;
        for(pobj = (byte*)pFirstOBJ+m_UnitSize; pobj < pE;)
        {
            *pHead = *pFirstHeader;
            pHead++;
        #if _DEF_USING_MEMORYPOOL_DEBUG__CHECK_OVERFLOW
            mFN_Debug_Overflow_Set(pOBJ+1, m_UnitSize-sizeof(TMemoryObject), 0xDD);
        #endif
            pOBJ->pNext = (TMemoryObject*)(pobj + nRealUnitSize);
            pobj += m_UnitSize;
        }

        TMemoryObject* pUnitF = pFirstOBJ;
        TMemoryObject* pUnitL = (TMemoryObject*)(pobj - m_UnitSize);
        pUnitL->pNext = nullptr;

        MemoryPool_UTIL::sFN_PushBackAll(m_pUnit_F, m_pUnit_B, pUnitF, pUnitL);
        m_nUnit                 += cntNewUnit;
        m_stats_Units_Allocated += cntNewUnit;
    }


    // GetCurrentProcessorNumberXP �� XP���� ������ �����ϴ�
    //  ������ DX11 �̻��� ����ϹǷ� XP�� �����Ѵ�
    //DWORD GetCurrentProcessorNumberXP(void)
    //{
    //    _asm{
    //        mov eax, 1;
    //        cpuid;
    //        shr ebx, 24;
    //        mov eax, ebx;
    //    }
    //}
    DWORD GetCurrentProcessorNumberVerX86()
    {
        int CPUInfo[4];
        __cpuid(CPUInfo, 1);
        // CPUInfo[1] is EBX, bits 24-31 are APIC ID
        if((CPUInfo[3] & (1 << 9)) == 0) return 0;  // no APIC on chip
        return (unsigned)CPUInfo[1] >> 24;
    }
    __forceinline TMemoryBasket& CMemoryPool::mFN_Get_Basket()
    {
#if _DEF_USING_MEMORYPOOL_GETPROCESSORNUMBER_SIDT_TLS_CACHE
        // TLS ĳ�� ��� ����
        auto code = gFN_Get_SIDT();
        size_t Key;
        TMemoryPool_TLS_CACHE& ref_This_Thread__CACHE = gpFN_TlsCache_AccessFunction();
        if(ref_This_Thread__CACHE.m_Code == code)
        {
            // ���������� �����ϴ� �������� ���� ���μ����� �ٲ�� �󵵴� ���� �ʴ�
            Key = ref_This_Thread__CACHE.m_ID;
        }
        else
        {
            // ĳ�� �̽�
            Key = 0;
            do{
                if(g_ArrayProcessorCode[Key] == code)
                    break;
            } while(++Key < g_nProcessor);
            ref_This_Thread__CACHE.m_Code = code;
            ref_This_Thread__CACHE.m_ID = Key;

    #if _DEF_USING_MEMORYPOOL_DEBUG
            // ������ ��� ����
            m_pBaskets[Key].m_cnt_CacheMiss_from_Get_Basket++;
    #endif
        }
        #if _DEF_USING_MEMORYPOOL_OPTIMIZE__CACHELINE_PREFETCH_AND_FLUSH
        // ���� �����尡 �� ���μ������� �ٽ� ����� Ȯ���� �ſ� ����
        // �����͸� �ٷ� ������ �ֵ���...
        _mm_prefetch((const char*)&ref_This_Thread__CACHE, _MM_HINT_NTA);
        #endif

#elif _DEF_USING_MEMORYPOOL_GETPROCESSORNUMBER_SIDT
        // TLS ĳ�� �̻�� ����
        auto code = gFN_Get_SIDT();
        size_t Key = 0;
        do{
            if(g_ArrayProcessorCode[Key] == code)
                break;
        }while(++Key < g_nProcessor);

    #if _DEF_USING_MEMORYPOOL_DEBUG
            // ������ ��� ����
            // _DEF_USING_MEMORYPOOL_GETPROCESSORNUMBER_SIDT_TLS_CACHE �� ������� �ʾ������� �� �ϱ����� ����
            m_pBaskets[Key].m_cnt_CacheMiss_from_Get_Basket++;
    #endif
#elif __X64
        size_t Key = GetCurrentProcessorNumber();
#elif __X86
        size_t Key = GetCurrentProcessorNumberVerX86();
#else
#endif

        _Assert(Key < g_nBaskets);
        return m_pBaskets[Key];
    }
    __forceinline TMemoryObject* CMemoryPool::mFN_Get_Memory_Process()
    {
        TMemoryObject* p;
        // Basket�� ������ ������ �������� �б�
        if(m_nLimit_Basket_KeepMax)
            p = mFN_Get_Memory_Process_Default();
        else
            p = mFN_Get_Memory_Process_DirectCACHE();

    #if _DEF_USING_MEMORYPOOL_DEBUG__CHECK_OVERFLOW
        if(p)
        {
            mFN_Debug_Overflow_Check(p+1, m_UnitSize-sizeof(TMemoryObject), 0xDD);
            mFN_Debug_Overflow_Set(p, m_UnitSize, 0xCD);
        }
    #endif

        return p;
    }
    namespace{
        __forceinline void gFN_Prefetch_NextMemory(void* pFrontUnit)
        {
            #if _DEF_USING_MEMORYPOOL_OPTIMIZE__CACHELINE_PREFETCH_AND_FLUSH
            if(!pFrontUnit)
                return;
            // ���� �Ҵ��� ����Ͽ� �̸� ĳ�ÿ� �ε��صд�
            //      ���� Front ������ ���Ҷ� �ᱹ �޸𸮸� �о�� �ϱ� ������
            // �̶�, pFrontUnit�� ���� �����忡�� ���� ���ɼ��� ���� ���� ��
            // �ɼ�
            // _MM_HINT_NTA ?
            // _MM_HINT_T0  L1
            // _MM_HINT_T1  L2
            // _MM_HINT_T2  L3  (���� ĳ��)
            
            _mm_prefetch((const CHAR*)pFrontUnit, _MM_HINT_T1);
            #else
            // �ƹ��ϵ� ���� �ʴ´�
            #endif
        }
    }
    DECLSPEC_NOINLINE TMemoryObject* CMemoryPool::mFN_Get_Memory_Process_Default()
    {
        TMemoryObject* p;
        TMemoryBasket& mb = mFN_Get_Basket();
        mb.Lock();
        for(;;)
        {
            // #1 [p]Basket Ȯ��
            p = mb.mFN_Get_Object();
            if(p)
            {
                gFN_Prefetch_NextMemory(mb.m_Units.m_pUnit_F);
                break;
            }
            else
            {
                mFN_BasketDemand_Expansion(mb);
            }

            // Basket�� ���� ������ ���� �ϴٸ�...
            // #2 [p]Basket::CACHE Ȯ��
            p = mFN_Fill_Basket_from_ThisCache_andRET1(mb);
            if(p)
                break;

            // #3 [pair]Bakset::CACHE Ȯ��
            //      g_nCore_per_Moudule �� �̿��� ¦�� ã�� �� �ִ�
            //  ���� �� ����� �ʿ����� �ʴ�

            // #4 Pool Ȯ��(������ Ȯ��)
            if(0 < m_nUnit)
            {
                m_Lock.Begin_Read();// ���� �켱���� ���(�б� �������� 1)
                p = mFN_Fill_Basket_from_Pool_andRET1(mb);
                m_Lock.End_Read();

                if(p)
                    break;
            }

            // #5 [all] Basket::CACHE Ȯ��
            p = mFN_Fill_Basket_from_AllOtherBasketCache_andRET1(mb);
            if(p)
                break;

            // #6 Alloc
            m_Lock.Begin_Write__INFINITE();// ���� �켱���� ���
            {
                if(0 <m_nUnit)
                    p = mFN_Fill_Basket_from_Pool_andRET1(mb);
                else if(m_Allocator.mFN_Expansion())
                    p = mFN_Fill_Basket_from_Pool_andRET1(mb);
                else
                    p = nullptr;
            }
            m_Lock.End_Write();
            if(p)
                break;

            // #7 [#6] ���н�, [all] Basket�� ���� ����� Ȯ��
            p = mFN_Fill_Basket_from_AllOtherBasketLocalStorage_andRET1(mb);
            if(p)
                break;

            // #END ���н� ���� ���
            ::RaiseException((DWORD)E_OUTOFMEMORY, EXCEPTION_NONCONTINUABLE, 0, NULL);
            mb.UnLock();
            return nullptr;
        }
        mb.UnLock();

        return p;
    }
    DECLSPEC_NOINLINE TMemoryObject* CMemoryPool::mFN_Get_Memory_Process_DirectCACHE()
    {
        TMemoryObject* p;
        TMemoryBasket& mb = mFN_Get_Basket();
        TMemoryBasket_CACHE& thisCACHE = m_pBasket__CACHE[mb.m_index_CACHE];

        thisCACHE.Lock();
        for(;;)
        {
            // #1 [p]CACHE Ȯ��
            if(0 < thisCACHE.m_Keep.m_nUnit)
            {
                thisCACHE.m_Keep.m_nUnit--;
                p = MemoryPool_UTIL::sFN_PopFront(thisCACHE.m_Keep.m_pUnit_F, thisCACHE.m_Keep.m_pUnit_B);
                gFN_Prefetch_NextMemory(thisCACHE.m_Keep.m_pUnit_F);
                break;
            }
            else if(0 < thisCACHE.m_Share.m_nUnit)
            {
                _Assert(0 == thisCACHE.m_Keep.m_nUnit);

                thisCACHE.m_Share.m_nUnit--;
                p = MemoryPool_UTIL::sFN_PopFront(thisCACHE.m_Share.m_pUnit_F, thisCACHE.m_Share.m_pUnit_B);
                if(0 == thisCACHE.m_Share.m_nUnit)
                    break;

                // ĳ�� ���� ����
                mFN_BasektCacheDemand_Expansion(thisCACHE);

                const size_t demand = min(thisCACHE.m_nMax_Keep, GLOBAL::gc_LimitMaxCounting_List);
                if(demand < thisCACHE.m_Share.m_nUnit)
                {
                    thisCACHE.m_Keep.m_nUnit  = demand;
                    thisCACHE.m_Share.m_nUnit -= demand;
                    MemoryPool_UTIL::sFN_PushBackN(thisCACHE.m_Keep.m_pUnit_F, thisCACHE.m_Keep.m_pUnit_B
                        , thisCACHE.m_Share.m_pUnit_F, thisCACHE.m_Share.m_pUnit_B, demand);
                }
                else if(0 < thisCACHE.m_Share.m_nUnit)
                {
                    thisCACHE.m_Keep.m_nUnit  = thisCACHE.m_Share.m_nUnit;
                    thisCACHE.m_Share.m_nUnit = 0;
                    MemoryPool_UTIL::sFN_PushBackAll(thisCACHE.m_Keep.m_pUnit_F, thisCACHE.m_Keep.m_pUnit_B
                        , thisCACHE.m_Share.m_pUnit_F, thisCACHE.m_Share.m_pUnit_B);
                }
                break;
            }

            // ĳ�� ���� ����
            mFN_BasektCacheDemand_Expansion(thisCACHE);

            // #2 Pool Ȯ��(������ Ȯ��)
            if(0 < m_nUnit)
            {
                m_Lock.Begin_Read();// ���� �켱���� ���(�б� �������� 1)
                p = mFN_Fill_CACHE_from_Pool_andRET1(thisCACHE);
                m_Lock.End_Read();
                if(p)
                    break;
            }

            // #3 [all] Basket::CACHE Ȯ��
            p = mFN_Fill_CACHE_from_AllOtherBasketCache_andRET1(thisCACHE);
            if(p)
                break;

            // #6 Alloc
            m_Lock.Begin_Write__INFINITE();// ���� �켱���� ���
            {
                if(0 <m_nUnit)
                    p = mFN_Fill_CACHE_from_Pool_andRET1(thisCACHE);
                else if(m_Allocator.mFN_Expansion())
                    p = mFN_Fill_CACHE_from_Pool_andRET1(thisCACHE);
                else
                    p = nullptr;
            }
            m_Lock.End_Write();
            if(p)
                break;

            // #END ���н� ���� ���
            ::RaiseException((DWORD)E_OUTOFMEMORY, EXCEPTION_NONCONTINUABLE, 0, NULL);
            thisCACHE.UnLock();
            return nullptr;
        }
        thisCACHE.UnLock();

        return p;
    }

    namespace{
        BOOL gFN_Query_isBadPTR(IMemoryPool* pThisPool, TMemoryObject* pAddress, size_t UnitSize, BOOL isSmallSize)
        {
            // üũ�� Ȯ�ε� �߰��� ���ΰ�?
            if(isSmallSize)
            {
                const TDATA_BLOCK_HEADER* pH = _MACRO_POINTER_GET_ALIGNED((TDATA_BLOCK_HEADER*)pAddress, GLOBAL::gc_minPageUnitSize);
                const byte* pS = (byte*)(pH+1);
                const size_t offset = (size_t)((byte*)pAddress - pS);

                if(pH->m_Type != TDATA_BLOCK_HEADER::_TYPE_Units_SizeType::E_SmallSize){}
                else if(pH->m_pPool != pThisPool){}
                else if(!pH->m_pGroupFront || pH->m_pGroupFront->m_pPool != pThisPool){}
                else if(!pH->mFN_Query_ContainPTR(pAddress)){}
                else if(offset % UnitSize != 0){}
                else { return FALSE; }
            }
            else
            {
                const TDATA_BLOCK_HEADER* pH = reinterpret_cast<TDATA_BLOCK_HEADER*>(pAddress) - 1;
                if(pH->m_Type != TDATA_BLOCK_HEADER::_TYPE_Units_SizeType::E_NormalSize){}
                else if(pH->m_pPool != pThisPool){}
                else if(!pH->m_pGroupFront || pH->m_pGroupFront->m_pPool != pThisPool){}
                else if(!pH->mFN_Query_ContainPTR(pAddress)){}
                else
                {
                    // �̻��� Ȯ���� ����� ���� Ȯ�������� ����� ��ġ�� Ȯ���� �� ����
                    // ������ ������� �Ǽ��� �ƴϸ� �Ͼ�� �ʴ� ���̴�
                    // ���������� �ּ� �������� Ȯ���Ѵ�
                    auto pHTrust = GLOBAL::g_Table_BlockHeaders_Normal.mFN_Get_Link(pH->m_Index.high, pH->m_Index.low);
                    if(!pHTrust) return TRUE;
                    const byte* pS = (byte*)(pHTrust->m_pGroupFront+1);
                    const size_t offset = (size_t)((byte*)pAddress - pS);
                    const size_t RealUnitSize = UnitSize + sizeof(TDATA_BLOCK_HEADER);
                    if(offset % RealUnitSize != 0)
                        return TRUE;

                    return FALSE; 
                }
            }
            return TRUE;
        }
    }
    __forceinline void CMemoryPool::mFN_Return_Memory_Process(TMemoryObject* pAddress)
    {
        _Assert(nullptr != pAddress);

    #ifdef _DEBUG
        // ��� �ջ� Ȯ��
        _AssertMsg(FALSE == gFN_Query_isBadPTR(this, pAddress, m_UnitSize, m_Allocator.m_isSmallUnitSize), "Broken Header Data");
    #endif

    #if _DEF_USING_DEBUG_MEMORY_LEAK
        InterlockedExchangeSubtract(&m_Debug_stats_UsingCounting, 1); // ����÷ο�� Ȯ������ �ʴ´�
        if(GLOBAL::g_bDebug__Trace_MemoryLeak)
        {
            m_Lock_Debug__Lend_MemoryUnits.Lock();
            auto iter = m_map_Lend_MemoryUnits.find(pAddress);
            _AssertReleaseMsg(iter != m_map_Lend_MemoryUnits.end(), "������ ���� �޸��� �ݳ�");
            m_map_Lend_MemoryUnits.erase(iter);
            m_Lock_Debug__Lend_MemoryUnits.UnLock();
        }
    #endif
    #if _DEF_USING_MEMORYPOOL_DEBUG__CHECK_OVERFLOW
        mFN_Debug_Overflow_Set(pAddress+1, m_UnitSize-sizeof(TMemoryObject), 0xDD);
    #endif
        
        if(m_nLimit_Basket_KeepMax)
            mFN_Return_Memory_Process_Default(pAddress);
        else
            mFN_Return_Memory_Process_DirectCACHE(pAddress);
    }
    DECLSPEC_NOINLINE void CMemoryPool::mFN_Return_Memory_Process_Default(TMemoryObject* pAddress)
    {
        TMemoryBasket& mb = mFN_Get_Basket();
        mb.Lock();
        {
            mb.mFN_Return_Object(pAddress);

            if(mb.m_Units.m_nUnit > mb.m_nDemand)
            {
                //if(mb.m_nDemand < GLOBAL::gc_LimitMinCounting_List) �̰��� ġ���� �����̴�. Basket�� ��뷮�� �޸𸮰� ������ ������ �����Ͼ��
                if(mb.m_Units.m_nUnit < GLOBAL::gc_LimitMinCounting_List)
                    goto Label_END;

                TMemoryBasket_CACHE* p = m_pBasket__CACHE + mb.m_index_CACHE;

                // ����� �����ϰ� �̸� ���
                //      cache�� ��ݽð��� �ּ�ȭ �ϱ� ����
                //      cache�� �ټ��� mb�� ���� ������ �� �ִ�(1:1 ���谡 �ƴ� �� �ִ�)
                size_t nMoveK = p->m_nMax_Keep - p->m_Keep.m_nUnit;
                TMemoryObject* pF_to_Keep;
                TMemoryObject* pB_to_Keep;
                size_t nMoveS;
                if(nMoveK >= mb.m_Units.m_nUnit)
                {
                    nMoveK = mb.m_Units.m_nUnit;
                    pF_to_Keep = mb.m_Units.m_pUnit_F;
                    pB_to_Keep = mb.m_Units.m_pUnit_B;
                    nMoveS = 0;
                }
                else if(nMoveK == 0)
                {
                    nMoveS = mb.m_Units.m_nUnit;
                }
                else // nMoveK < mb.m_Units.m_nUnit
                {
                    //nMoveK = min(nMoveK, mb.m_Units.m_nUnit);
                    nMoveK = min(nMoveK, GLOBAL::gc_LimitMaxCounting_List);
                    nMoveS = mb.m_Units.m_nUnit - nMoveK;
                    pF_to_Keep = pB_to_Keep = nullptr;
                    MemoryPool_UTIL::sFN_PushBackN(pF_to_Keep, pB_to_Keep, mb.m_Units.m_pUnit_F, mb.m_Units.m_pUnit_B, nMoveK);
                }

                p->Lock();
                if(nMoveK)
                {
                    p->m_Keep.m_nUnit += nMoveK;
                    MemoryPool_UTIL::sFN_PushBackAll(p->m_Keep.m_pUnit_F, p->m_Keep.m_pUnit_B, pF_to_Keep, pB_to_Keep);
                }
                else
                {
                    // ���� ĳ�ð� ���� á�ٸ� Basket ���� ����
                    //mFN_BasketDemand_Reduction(mb);
                }
                if(nMoveS)
                {
                    p->m_Share.m_nUnit += nMoveS;
                    MemoryPool_UTIL::sFN_PushBackAll(p->m_Share.m_pUnit_F, p->m_Share.m_pUnit_B, mb.m_Units.m_pUnit_F, mb.m_Units.m_pUnit_B);
                }
                mb.m_Units.m_nUnit = 0;
                mb.m_Units.m_pUnit_F = mb.m_Units.m_pUnit_B = nullptr;
                p->UnLock();
            }
        }

    Label_END:
        mb.UnLock();
    }
    DECLSPEC_NOINLINE void CMemoryPool::mFN_Return_Memory_Process_DirectCACHE(TMemoryObject* pAddress)
    {
        TMemoryBasket& mb = mFN_Get_Basket();
        TMemoryBasket_CACHE* p = m_pBasket__CACHE + mb.m_index_CACHE;

        p->Lock();
        if(p->m_Keep.m_nUnit < p->m_nMax_Keep)
        {
            p->m_Keep.m_nUnit++;
            MemoryPool_UTIL::sFN_PushBack(p->m_Keep.m_pUnit_F, p->m_Keep.m_pUnit_B, pAddress);
        }
        else
        {
            p->m_Share.m_nUnit++;
            MemoryPool_UTIL::sFN_PushBack(p->m_Share.m_pUnit_F, p->m_Share.m_pUnit_B, pAddress);

            // ������ ĳ�ð� ��������, ��� �޸�Ǯ�� �ű��
            if(m_nLimit_CacheShare_KeepMax < p->m_Share.m_nUnit)
                mFN_Fill_Pool_from_Other(&p->m_Share);
        }
        p->UnLock();
    }
    __forceinline BOOL CMemoryPool::mFN_TestHeader(TMemoryObject * pAddress)
    {
        _CompileHint(this && nullptr != pAddress);
        if(m_UnitSize <= GLOBAL::gc_SmallUnitSize_Limit)
        {
            if(this == CMemoryPool_Manager::sFN_Get_MemoryPool_SmallObjectSize(pAddress))
                return TRUE;
        }
        else if(m_UnitSize <= GLOBAL::gc_maxSize_of_MemoryPoolUnitSize)
        {
            if(this == CMemoryPool_Manager::sFN_Get_MemoryPool_NormalObjectSize(pAddress))
                return TRUE;
        }

        return FALSE;
    }

    TMemoryObject* CMemoryPool::mFN_Fill_Basket_andRET1(TMemoryBasket& mb, TMemoryObject*& F, TMemoryObject*& B, size_t& nSource)
    {
        _Assert(F && B && 0<nSource && 0<mb.m_nDemand);

        if(nSource <= mb.m_nDemand)
        {
            mb.m_Units.m_nUnit += nSource;
            nSource            = 0;
            MemoryPool_UTIL::sFN_PushBackAll(mb.m_Units.m_pUnit_F, mb.m_Units.m_pUnit_B, F, B);
        }
        else//if(nSource > mb.m_nDemand)
        {
            // �ʹ� ���� ���� ī���� ���� �ʵ��� �����Ѵ�
            const size_t nMove = min(mb.m_nDemand, gc_LimitMaxCounting_List);

            mb.m_Units.m_nUnit += nMove;
            nSource            -= nMove;
            MemoryPool_UTIL::sFN_PushBackN(mb.m_Units.m_pUnit_F, mb.m_Units.m_pUnit_B, F, B, nMove);
        }

        mb.m_Units.m_nUnit--;
        return MemoryPool_UTIL::sFN_PopFront(mb.m_Units.m_pUnit_F, mb.m_Units.m_pUnit_B);
    }
    TMemoryObject* CMemoryPool::mFN_Fill_CACHE_andRET1(TMemoryBasket_CACHE& cache, TMemoryObject*& F, TMemoryObject*& B, size_t& nSource)
    {
        _Assert(F && B && 0<nSource && cache.m_nMax_Keep);

        if(nSource <= cache.m_nMax_Keep)
        {
            cache.m_Keep.m_nUnit += nSource;;
            nSource              = 0;
            MemoryPool_UTIL::sFN_PushBackAll(cache.m_Keep.m_pUnit_F, cache.m_Keep.m_pUnit_B, F, B);
        }
        else//if(nSource > cache.m_nMax_Keep)
        {
            // �ʹ� ���� ���� ī���� ���� �ʵ��� �����Ѵ�
            const size_t nMove = min(cache.m_nMax_Keep, gc_LimitMaxCounting_List);

            cache.m_Keep.m_nUnit += nMove;
            nSource              -= nMove;
            MemoryPool_UTIL::sFN_PushBackN(cache.m_Keep.m_pUnit_F, cache.m_Keep.m_pUnit_B, F, B, nMove);
        }

        cache.m_Keep.m_nUnit--;
        return MemoryPool_UTIL::sFN_PopFront(cache.m_Keep.m_pUnit_F, cache.m_Keep.m_pUnit_B);
    }
    // �� �żҵ�� ȣ�� Ȯ���� ����
    __forceinline TMemoryObject* CMemoryPool::mFN_Fill_Basket_from_ThisCache_andRET1(TMemoryBasket& mb)
    {
        TMemoryObject* pRET;

        auto pThisCACHE = m_pBasket__CACHE + mb.m_index_CACHE;
        pThisCACHE->Lock();
        if(0 < pThisCACHE->m_Keep.m_nUnit)
        {
            pRET =  mFN_Fill_Basket_andRET1(mb, pThisCACHE->m_Keep.m_pUnit_F, pThisCACHE->m_Keep.m_pUnit_B, pThisCACHE->m_Keep.m_nUnit);
        }
        else if(0 < pThisCACHE->m_Share.m_nUnit)
        {
            pRET = mFN_Fill_Basket_andRET1(mb, pThisCACHE->m_Share.m_pUnit_F, pThisCACHE->m_Share.m_pUnit_B, pThisCACHE->m_Share.m_nUnit);

            // ĳ�� ���� ����
            _Assert(0 == pThisCACHE->m_Keep.m_nUnit);
            mFN_BasektCacheDemand_Expansion(*pThisCACHE);
        }
        else
        {
            pRET = nullptr;
        }
        pThisCACHE->UnLock();
        return pRET;
    }

    __forceinline TMemoryObject* CMemoryPool::mFN_Fill_Basket_from_Pool_andRET1(TMemoryBasket& mb)
    {
        if(0 == m_nUnit)
            return nullptr;

        _Assert(m_pUnit_F && m_pUnit_B && 0<m_nUnit);
        return mFN_Fill_Basket_andRET1(mb, m_pUnit_F, m_pUnit_B, m_nUnit);
    }
    __forceinline TMemoryObject* CMemoryPool::mFN_Fill_CACHE_from_Pool_andRET1(TMemoryBasket_CACHE& cache)
    {
        if(0 == m_nUnit)
            return nullptr;

        _Assert(m_pUnit_F && m_pUnit_B && 0<m_nUnit);
        return mFN_Fill_CACHE_andRET1(cache, m_pUnit_F, m_pUnit_B, m_nUnit);
    }
    DECLSPEC_NOINLINE TMemoryObject* CMemoryPool::mFN_Fill_Basket_from_AllOtherBasketCache_andRET1(TMemoryBasket& mb)
    {
        const auto iThisCACHE = mb.m_index_CACHE;
        for(size_t i=0; i<g_nBasket__CACHE; i++)
        {
            if(i == iThisCACHE)
                continue;

            auto p = m_pBasket__CACHE + i;

            //LoadFence();

            // ������ Ȯ��
            if(0 == p->m_Share.m_nUnit && 0 == p->m_Keep.m_nUnit)
                continue;
            p->Lock();

            if(0 < p->m_Share.m_nUnit)
            {
                auto pRET = mFN_Fill_Basket_andRET1(mb, p->m_Share.m_pUnit_F, p->m_Share.m_pUnit_B, p->m_Share.m_nUnit);

                // ��� �ٱ����� ĳ�ÿ� ���� �͵��� �޸�Ǯ�� �����´�
                if(GLOBAL::gc_LimitMinCounting_List < p->m_Share.m_nUnit)
                    mFN_Fill_Pool_from_Other(&p->m_Share);

                p->UnLock();
                return pRET;
            }
            else if(0 < p->m_Keep.m_nUnit)
            {
                auto pRET = mFN_Fill_Basket_andRET1(mb, p->m_Keep.m_pUnit_F, p->m_Keep.m_pUnit_B, p->m_Keep.m_nUnit);

                // �ٸ� ���μ����� ĳ�ø� ��� ���� �Դٸ�, �ش� ���μ����� ĳ�ÿ� ���Ͽ�,
                // ĳ�� ���� ����
                if(0 == p->m_Keep.m_nUnit)
                    mFN_BasektCacheDemand_Reduction(*p);

                p->UnLock();
                return pRET;
            }
            else
            {
                p->UnLock();
            }
        }

        return nullptr;
    }
    DECLSPEC_NOINLINE TMemoryObject * CMemoryPool::mFN_Fill_CACHE_from_AllOtherBasketCache_andRET1(TMemoryBasket_CACHE& cache)
    {
        // ����� ���� üũ
        for(size_t i=0; i<g_nBasket__CACHE; i++)
        {
            auto& OtherCache = m_pBasket__CACHE[i];
            if(&OtherCache == &cache)
                continue;

            // ������ Ȯ��
            if(0 == OtherCache.m_Share.m_nUnit && 0 == OtherCache.m_Keep.m_nUnit)
                continue;

            OtherCache.Lock();
            if(0 < OtherCache.m_Share.m_nUnit)
            {
                auto pRET = mFN_Fill_CACHE_andRET1(cache, OtherCache.m_Share.m_pUnit_F, OtherCache.m_Share.m_pUnit_B, OtherCache.m_Share.m_nUnit);

                // ��� �ٱ����� ĳ�ÿ� ���� �͵��� �޸�Ǯ�� �����´�
                if(GLOBAL::gc_LimitMinCounting_List < OtherCache.m_Share.m_nUnit)
                    mFN_Fill_Pool_from_Other(&OtherCache.m_Share);

                OtherCache.UnLock();
                return pRET;
            }
            else if(0 < OtherCache.m_Keep.m_nUnit)
            {
                auto pRET = mFN_Fill_CACHE_andRET1(cache, OtherCache.m_Keep.m_pUnit_F, OtherCache.m_Keep.m_pUnit_B, OtherCache.m_Keep.m_nUnit);

                // �ٸ� ���μ����� ĳ�ø� ��� ���� �Դٸ�, �ش� ���μ����� ĳ�ÿ� ���Ͽ�,
                // ĳ�� ���� ����
                if(0 == OtherCache.m_Keep.m_nUnit)
                    mFN_BasektCacheDemand_Reduction(OtherCache);

                OtherCache.UnLock();
                return pRET;
            }
            else
            {
                OtherCache.UnLock();
            }
        }

        return nullptr;
    }
    DECLSPEC_NOINLINE TMemoryObject* CMemoryPool::mFN_Fill_Basket_from_AllOtherBasketLocalStorage_andRET1(TMemoryBasket& mb)
    {
        for(size_t i=0; i<g_nBaskets; i++)
        {
            TMemoryBasket* p = &m_pBaskets[i];
            if(p == &mb)
                continue;

            // p->Lock();
            // �� �ڵ�� ���� ��� ��Ȳ,
            // mb1 �� mb2��, ��� mb2�� mb1�� ��ٸ��� ������� ������ �־� ���ѵ� ����� ����ؾ� �Ѵ�.
            // ����� �����ϸ� ����� skip
            if(!p->m_Lock.Lock__NoInfinite(0x0000FFFF))
                continue;

            if(0 < p->m_Units.m_nUnit)
            {
                auto pRET = mFN_Fill_Basket_andRET1(mb, p->m_Units.m_pUnit_F, p->m_Units.m_pUnit_B, p->m_Units.m_nUnit);
                p->UnLock();
                return pRET;
            }
            else
            {
                p->UnLock();
            }
        }

        return nullptr;
    }

    void CMemoryPool::mFN_Fill_Pool_from_Other(TUnits* pUnits)
    {
        m_Lock.Begin_Write__INFINITE();
        m_nUnit += pUnits->m_nUnit;
        pUnits->m_nUnit = 0;
        MemoryPool_UTIL::sFN_PushBackAll(m_pUnit_F, m_pUnit_B, pUnits->m_pUnit_F, pUnits->m_pUnit_B);
        m_Lock.End_Write();
    }
    _DEF_INLINE_CHANGE_DEMAND void CMemoryPool::mFN_BasketDemand_Expansion(TMemoryBasket& mb)
    {
        if(mb.m_nDemand < m_nLimit_Basket_KeepMax)
        {
            mb.m_nDemand += m_UnitSize_per_IncrementDemand;
            if(mb.m_nDemand > m_nLimit_Basket_KeepMax)
                mb.m_nDemand = m_nLimit_Basket_KeepMax;
        }
    }

    _DEF_INLINE_CHANGE_DEMAND void CMemoryPool::mFN_BasketDemand_Reduction(TMemoryBasket & mb)
    {
        if(mb.m_nDemand > m_UnitSize_per_IncrementDemand)
            mb.m_nDemand -= m_UnitSize_per_IncrementDemand;
        else
            mb.m_nDemand = 0;
    }

    _DEF_INLINE_CHANGE_DEMAND void CMemoryPool::mFN_BasektCacheDemand_Expansion(TMemoryBasket_CACHE & cache)
    {
        if(cache.m_nMax_Keep < m_nLimit_Cache_KeepMax)
        {
            cache.m_nMax_Keep += m_UnitSize_per_IncrementDemand; // (1 ~ )
            if(cache.m_nMax_Keep > m_nLimit_Cache_KeepMax)
                cache.m_nMax_Keep = m_nLimit_Cache_KeepMax;
        }
    }

    _DEF_INLINE_CHANGE_DEMAND DECLSPEC_NOINLINE void CMemoryPool::mFN_BasektCacheDemand_Reduction(TMemoryBasket_CACHE & cache)
    {
        if(cache.m_nMax_Keep > m_UnitSize_per_IncrementDemand)
            cache.m_nMax_Keep -= m_UnitSize_per_IncrementDemand;
        else
            cache.m_nMax_Keep = 0;
    }

    void CMemoryPool::mFN_Debug_Overflow_Set(void* p, size_t size, BYTE code)
    {
        if(!size)
            return;
        ::memset(p, code, size);
    }
    namespace{
        BOOL gFN_Test_Memory(const void* pPTR, size_t size, BYTE code)
        {
            const BYTE* pC = (const BYTE*)pPTR;
            if(size < 16) {
                for(size_t i=0; i<size; i++)
                    if(*(pC+i) != code)
                        return FALSE;
                return TRUE;
            }

            while((size_t)pC % sizeof(size_t) != 0)
            {
                if(*pC != code)
                    return FALSE;

                pC++;
                size--;
            }
            size_t wCode = 0;
            for(size_t i=0; i<sizeof(size_t); i++)
                wCode = (wCode << 8) | (size_t)(byte)code;

            size_t nWord = size / sizeof(size_t);
            size_t nChar = size % sizeof(size_t);

            size_t* pWord = (size_t*)pC;
            size_t* pWordEnd = pWord + nWord;
            do{
                if(*pWord != wCode)
                    return FALSE;
                pWord++;
            } while(pWord < pWordEnd);

            pC = (const BYTE*)pWord;
            const BYTE* pCEnd = pC + nChar;
            for(; pC < pCEnd; pC++)
            {
                if(*pC != code)
                    return FALSE;
            }
            return TRUE;
        }
    }
    DECLSPEC_NOINLINE void CMemoryPool::mFN_Debug_Overflow_Check(const void* p, size_t size, BYTE code) const
    {
        if(!gFN_Test_Memory(p, size, code))
            _DebugBreak("Broken Memory");
    }
    void CMemoryPool::mFN_Debug_Report()
    {
        size_t _TotalFree = mFN_Counting_KeepingUnits();
#if _DEF_USING_DEBUG_MEMORY_LEAK
        // �ݳ����� ���� �޸� ����
        m_Lock_Debug__Lend_MemoryUnits.Lock();
        //if(!m_map_Lend_MemoryUnits.empty())
        if(0 < m_Debug_stats_UsingCounting)
        {
            // print caption (������ �ѹ��� ȣ�� �ǵ���)
            static BOOL bRequire_PrintCaption = TRUE;
            if(bRequire_PrintCaption)
            {
                _MACRO_OUTPUT_DEBUG_STRING_ALWAYS("================ MemoryPool : Detected memory leaks ================\n");
                bRequire_PrintCaption = FALSE;
            }

            //_MACRO_OUTPUT_DEBUG_STRING_ALWAYS("MemoryPool[%u] : Total: [%0.2f]KB\n"
            //    , (UINT32)m_UnitSize
            //    , static_cast<double>(m_UnitSize) * m_map_Lend_MemoryUnits.size() / 1024.);
            _MACRO_OUTPUT_DEBUG_STRING_ALWAYS("MemoryPool[%u] : Total: [%0.2f]KB\n"
                , (UINT32)m_UnitSize
                , static_cast<double>(m_UnitSize) * m_Debug_stats_UsingCounting / 1024.);

            if(GLOBAL::g_bDebug__Trace_MemoryLeak)
            {
                for(auto iter : m_map_Lend_MemoryUnits)
                {
                    const void* p = iter.first;
                    const TTrace_SourceCode& tag = iter.second;
                    _MACRO_OUTPUT_DEBUG_STRING_ALWAYS("%s(%d): 0x%p\n", tag.m_File, tag.m_Line, p);
                }
            }
            else if(!m_map_Lend_MemoryUnits.empty())
            {
                m_map_Lend_MemoryUnits.clear();
            }
        }
        m_Lock_Debug__Lend_MemoryUnits.UnLock();
#endif


    #if _DEF_USING_MEMORYPOOL_DEBUG
        if(GLOBAL::g_bDebug__Report_OutputDebug)
        {
            _LOG_DEBUG__WITH__OUTPUTDEBUGSTR_ALWAYS(FALSE, _T("MemoryPool[%llu] Shared Storage n[%10llu] / n[%10llu], Expansion Count[%10llu]\n")
                , (UINT64)m_UnitSize
                , (UINT64)m_nUnit
                , (UINT64)m_stats_Units_Allocated
                , (UINT64)m_Allocator.m_stats_cnt_Succeed_VirtualAlloc
                );

            for(size_t i=0; i<GLOBAL::g_nBaskets; i++)
            {
                _LOG_DEBUG__WITH__OUTPUTDEBUGSTR_ALWAYS(FALSE, _T("\tCACHE Level1[%d] = n[%10llu] nDemand[%10llu] Get[%10llu] Ret[%10llu] CacheMiss[%10llu]\n"), i
                    , (UINT64)m_pBaskets[i].m_Units.m_nUnit
                    , (UINT64)m_pBaskets[i].m_nDemand
                    , (UINT64)m_pBaskets[i].m_cnt_Get
                    , (UINT64)m_pBaskets[i].m_cnt_Ret
                    , (UINT64)m_pBaskets[i].m_cnt_CacheMiss_from_Get_Basket
                    );
            }
            for(size_t i=0; i<GLOBAL::g_nBasket__CACHE; i++)
                _LOG_DEBUG__WITH__OUTPUTDEBUGSTR_ALWAYS(FALSE, _T("\tCACHE Level2[%d] = n[%10llu]\n"), i, (UINT64)m_pBasket__CACHE[i].m_Keep.m_nUnit);
            for(size_t i=0; i<GLOBAL::g_nBasket__CACHE; i++)
                _LOG_DEBUG__WITH__OUTPUTDEBUGSTR_ALWAYS(FALSE, _T("\tCACHE Level3[%d] = n[%10llu]\n"), i, (UINT64)m_pBasket__CACHE[i].m_Share.m_nUnit);

            _LOG_DEBUG__WITH__OUTPUTDEBUGSTR_ALWAYS(FALSE, _T("\tTotal : [%10llu]/[%10llu]\n"), (UINT64)_TotalFree, (UINT64)m_stats_Units_Allocated);
        }
    #endif

        if(m_bWriteStats_to_LogFile)
        {
            if(_TotalFree == m_stats_Units_Allocated)
            {
                // �̻� ����
            }
            else if(_TotalFree < m_stats_Units_Allocated)
            {
                const size_t leak_cnt   = m_stats_Units_Allocated - _TotalFree;
                const size_t leak_size  = leak_cnt * m_UnitSize;
                _LOG_DEBUG(_T("[Warning] Pool Size[%llu] : Memory Leak(Count:%llu , TotalSize:%lluKB)")
                    , (UINT64)m_UnitSize
                    , (UINT64)leak_cnt
                    , (UINT64)leak_size/1024);
            }
            else if(_TotalFree > m_stats_Units_Allocated)
            {
                _LOG_DEBUG(_T("[Critical Error] Pool Size[%llu] : Total Allocated < Total Free"), (UINT64)m_UnitSize);
            }

            if(0 == m_Allocator.m_nLimitBlocks_per_Expansion)
            {
                _LOG_DEBUG(_T("[Warning] Pool Size[%llu] : Failed Alloc"), (UINT64)m_UnitSize);
            }
        }

        if(0 < m_stats_Counting_Free_BadPTR)
        {
            // �ɰ��� ��Ȳ�̴� Release ���������� ����Ѵ�
            _LOG_DEBUG__WITH__OUTPUTDEBUGSTR_ALWAYS(FALSE, "================ Critical Error : Failed Count(%Iu) -> MemoryPool[%Iu]:Free ================"
                , m_stats_Counting_Free_BadPTR
                , m_UnitSize);
        }
    }
    size_t CMemoryPool::mFN_Counting_KeepingUnits()
    {
        size_t _TotalFree = m_nUnit;
        for(size_t i=0; i<g_nBaskets; i++)
        {
            _TotalFree += m_pBaskets[i].m_Units.m_nUnit;
        }
        for(size_t i=0; i<g_nBasket__CACHE; i++)
        {
            _TotalFree += m_pBasket__CACHE[i].m_Keep.m_nUnit;
            _TotalFree += m_pBasket__CACHE[i].m_Share.m_nUnit;
        }
        return _TotalFree;
    }

    DECLSPEC_NOINLINE void CMemoryPool::mFN_all_Basket_Lock()
    {
        for(size_t i=0; i<g_nBaskets; i++)
            m_pBaskets[i].Lock();
    }
    DECLSPEC_NOINLINE void CMemoryPool::mFN_all_Basket_UnLock()
    {
        for(size_t i=0; i<g_nBaskets; i++)
            m_pBaskets[i].UnLock();
    }
    DECLSPEC_NOINLINE void CMemoryPool::mFN_all_BasketCACHE_Lock()
    {
        for(size_t i=0; i<g_nBasket__CACHE; i++)
            m_pBasket__CACHE[i].Lock();
    }
    DECLSPEC_NOINLINE void CMemoryPool::mFN_all_BasketCACHE_UnLock()
    {
        for(size_t i=0; i<g_nBasket__CACHE; i++)
            m_pBasket__CACHE[i].UnLock();
    }

}
}


/*----------------------------------------------------------------
/   �޸�Ǯ ������
/---------------------------------------------------------------*/
namespace UTIL{
namespace MEM{
    CMemoryPool_Manager::CMemoryPool_Manager()
        : m_bWriteStats_to_LogFile(TRUE)
        , m_stats_Counting_Free_BadPTR(0)
    {
        CObjectPool_Handle<CMemoryPool>::Reference_Attach();

        _LOG_DEBUG__WITH__TRACE(TRUE, _T("--- Create CMemoryPool_Manager ---"));

        mFN_Test_Environment_Variable();
        mFN_Initialize_MemoryPool_Shared_Environment_Variable1();
        mFN_Initialize_MemoryPool_Shared_Environment_Variable2();

        ::UTIL::g_pMem = this;
        mFN_Report_Environment_Variable();
    }
    CMemoryPool_Manager::~CMemoryPool_Manager()
    {
        ::UTIL::g_pMem = nullptr;

        m_Lock__set_Pool.Begin_Write__INFINITE();

        for(auto iter : m_set_Pool)
        {
            //_SAFE_DELETE_ALIGNED_CACHELINE(iter.m_pPool);
            CObjectPool_Handle<CMemoryPool>::Free_and_CallDestructor(iter.m_pPool);
        }
        m_set_Pool.clear();


        mFN_Destroy_MemoryPool_Shared_Environment_Variable();
        mFN_Debug_Report();
        _LOG_DEBUG__WITH__TRACE(TRUE, _T("--- Destroy CMemoryPool_Manager ---"));
        CObjectPool_Handle<CMemoryPool>::Reference_Detach();
    }


    DECLSPEC_NOINLINE void CMemoryPool_Manager::mFN_Test_Environment_Variable()
    {
        // ��Ʈ�� 1���� on �̾�� �ϴ� ��� üũ
        _AssertRelease(1 == _MACRO_BIT_COUNT(gc_Max_ThreadAccessKeys));
        _AssertRelease(1== _MACRO_BIT_COUNT(gc_AlignSize_LargeUnit));
        _AssertRelease(1== _MACRO_BIT_COUNT(gc_minPageUnitSize));
        
        

        // gFN_Calculate_UnitSize ����
        {
        #pragma warning(push)
        #pragma warning(disable: 4127)
            _AssertRelease(0 < _MACRO_ARRAY_COUNT(gc_Array_Limit) && _MACRO_ARRAY_COUNT(gc_Array_Limit) == _MACRO_ARRAY_COUNT(gc_Array_MinUnit));
        #pragma warning(pop)

            _AssertRelease(gc_minSize_of_MemoryPoolUnitSize == gc_Array_MinUnit[0]);

            BOOL bDetected_SmallUnitSize = FALSE;
            for(auto i=0; i<_MACRO_ARRAY_COUNT(gc_Array_Limit); i++)
            {
                if(gc_SmallUnitSize_Limit == gc_Array_Limit[i])
                {
                    bDetected_SmallUnitSize = TRUE;
                    break;
                }
            }
            _AssertReleaseMsg(TRUE == bDetected_SmallUnitSize, "���� ���� ũ������� �߸� �Ǿ����ϴ�");

            size_t tempPrevious = 0;
            for(auto i=0; i<_MACRO_ARRAY_COUNT(gc_Array_Limit); i++)
            {
                _AssertReleaseMsg(0 == gc_Array_Limit[i] %  _DEF_CACHELINE_SIZE, "ĳ�ö���ũ���� ������� �մϴ�");
                _AssertRelease(tempPrevious < gc_Array_Limit[i]); // ���� �ε������� ū ���̿��� ��
                tempPrevious = gc_Array_Limit[i];

                const auto& m = gc_Array_MinUnit[i];
                _AssertRelease(0 < m);
                // ĳ�ö��� ũ���� ��� �Ǵ� ������� ��
                if(m > _DEF_CACHELINE_SIZE)
                {
                    _AssertRelease(0 == m % _DEF_CACHELINE_SIZE);
                }
                else
                {
                    _AssertRelease(0 == _DEF_CACHELINE_SIZE % m);
                }
            }
        }
    }

    void CMemoryPool_Manager::mFN_Initialize_MemoryPool_Shared_Environment_Variable1()
    {
        g_nProcessor = CORE::g_instance_CORE.mFN_Get_System_Information()->mFN_Get_NumProcessor_Logical();
        g_nBaskets = g_nProcessor;

        if(gc_Max_ThreadAccessKeys < g_nProcessor)
            _LOG_SYSTEM__WITH__TRACE(TRUE, "too many CPU Core : %d", g_nProcessor);

    #if _DEF_USING_MEMORYPOOL_GETPROCESSORNUMBER_SIDT
        _MACRO_STATIC_ASSERT(gc_Max_ThreadAccessKeys <= 64); // SetThreadAffinityMask �Լ� ����

        // SIDT �� �̿���, ���μ����� �ڵ�����, ĳ�÷ν� �ۼ�
        auto hThisThread  = GetCurrentThread();
        auto Mask_Before  = ::SetThreadAffinityMask(hThisThread, 1);
        _AssertRelease(0 != Mask_Before);

        for(DWORD_PTR i=0; i<g_nProcessor; i++)
        {
            ::SetThreadAffinityMask(hThisThread, (DWORD_PTR)1 << i);
            g_ArrayProcessorCode[i] = gFN_Get_SIDT();
        }
        ::SetThreadAffinityMask(hThisThread, Mask_Before);
    #endif
    }

    void CMemoryPool_Manager::mFN_Initialize_MemoryPool_Shared_Environment_Variable2()
    {
    #define __temp_macro_make4case(s)   \
            case s+0: gpFN_Baskets_Alloc = gFN_Baskets_Alloc<s+0>; gpFN_Baskets_Free = gFN_Baskets_Free<s+0>; break;    \
            case s+1: gpFN_Baskets_Alloc = gFN_Baskets_Alloc<s+1>; gpFN_Baskets_Free = gFN_Baskets_Free<s+1>; break;    \
            case s+2: gpFN_Baskets_Alloc = gFN_Baskets_Alloc<s+2>; gpFN_Baskets_Free = gFN_Baskets_Free<s+2>; break;    \
            case s+3: gpFN_Baskets_Alloc = gFN_Baskets_Alloc<s+3>; gpFN_Baskets_Free = gFN_Baskets_Free<s+3>; break;
        switch(g_nBaskets)
        {
            __temp_macro_make4case(0+1);
            __temp_macro_make4case(4+1);
            __temp_macro_make4case(8+1);
            __temp_macro_make4case(12+1);
            __temp_macro_make4case(16+1);
            __temp_macro_make4case(20+1);
            __temp_macro_make4case(24+1);
            __temp_macro_make4case(28+1);
        default:
            gpFN_Baskets_Alloc = gFN_Baskets_Alloc__Default;
            gpFN_Baskets_Free  = gFN_Baskets_Free__Default;
        }
    #undef __temp_macro_make4case


        // CPU ������ ���� ���� ������ ������ �����ϴ� ���� ����(1M2C �� ���� ���)
        //      g_Array_Key__CpuCore_to_CACHE �� ���� �ε���
        // ĳ�ô� �ʹ� ���Ƽ��� �ȵȴ�
        _AssertReleaseMsg(g_nProcessor <= _MACRO_ARRAY_COUNT(g_Array_Key__CpuCore_to_CACHE), "�ʹ� ���� CPU");
        size_t nPhysical_CORE = CORE::g_instance_CORE.mFN_Get_System_Information()->mFN_Get_NumProcessor_PhysicalCore();

        // ���� �ھ�� ����
        for(size_t i=1;; i++)
        {
            const auto temp = nPhysical_CORE * i;
            if(temp == g_nProcessor)
            {
                g_nCore_per_Moudule = i;
                break;
            }
            if(temp > g_nProcessor)
            {
                // �ھ�/��� �� ��Ȯ�� ���� �������� �ʴ´�
                g_nCore_per_Moudule = 1;
                break;
            }
        }

        g_iCACHE_AccessRate = g_nCore_per_Moudule;
        g_nBasket__CACHE = g_nProcessor / g_iCACHE_AccessRate;

        // ĳ�ð� �ʹ� ���ٸ� 1���� ĳ�ô� ���μ��� ������ ��� ���δ�
        while(8 < g_nBasket__CACHE)
        {
            g_iCACHE_AccessRate *= 2;
            g_nBasket__CACHE = g_nProcessor / g_iCACHE_AccessRate;
            if(g_nProcessor % g_iCACHE_AccessRate)
                g_nBasket__CACHE++;
        }

        for(size_t i=0; i<g_nProcessor; i++)
        {
            auto t = i / g_iCACHE_AccessRate;
            _AssertReleaseMsg(i < 256, "CPU �ھ �ʹ� �����ϴ�");
            g_Array_Key__CpuCore_to_CACHE[i] = static_cast<byte>(t);
        }

    #define __temp_macro_make4case(s)   \
            case s+0: gpFN_BasketCACHE_Alloc = gFN_BasketCACHE_Alloc<s+0>; gpFN_BasketCACHE_Free = gFN_BasketCACHE_Free<s+0>; break;    \
            case s+1: gpFN_BasketCACHE_Alloc = gFN_BasketCACHE_Alloc<s+1>; gpFN_BasketCACHE_Free = gFN_BasketCACHE_Free<s+1>; break;    \
            case s+2: gpFN_BasketCACHE_Alloc = gFN_BasketCACHE_Alloc<s+2>; gpFN_BasketCACHE_Free = gFN_BasketCACHE_Free<s+2>; break;    \
            case s+3: gpFN_BasketCACHE_Alloc = gFN_BasketCACHE_Alloc<s+3>; gpFN_BasketCACHE_Free = gFN_BasketCACHE_Free<s+3>; break;
        switch(g_nBasket__CACHE)
        {
            __temp_macro_make4case(0+1);
            __temp_macro_make4case(4+1);
            __temp_macro_make4case(8+1);
            __temp_macro_make4case(12+1);
            __temp_macro_make4case(16+1);
            __temp_macro_make4case(20+1);
            __temp_macro_make4case(24+1);
            __temp_macro_make4case(28+1);
        default:
            gpFN_BasketCACHE_Alloc = gFN_BasketCACHE_Alloc__Default;
            gpFN_BasketCACHE_Free  = gFN_BasketCACHE_Free__Default;
        }
    #undef __temp_macro_make4case
    }

    void CMemoryPool_Manager::mFN_Destroy_MemoryPool_Shared_Environment_Variable()
    {

    }

#if _DEF_USING_DEBUG_MEMORY_LEAK
    void* CMemoryPool_Manager::mFN_Get_Memory(size_t _Size, const char* _FileName, int _Line)
    {
        auto pPool = CMemoryPool_Manager::mFN_Get_MemoryPool(_Size);
        return pPool->mFN_Get_Memory(_Size, _FileName, _Line);
    }
#else
    void* CMemoryPool_Manager::mFN_Get_Memory(size_t _Size)
    {
        auto pPool = CMemoryPool_Manager::mFN_Get_MemoryPool(_Size);
        return pPool->mFN_Get_Memory(_Size);
    }
#endif

    void CMemoryPool_Manager::mFN_Return_Memory(void* pAddress)
    {
        if(!pAddress)
            return;

        TDATA_BLOCK_HEADER::_TYPE_Units_SizeType sizeType;
        auto pPool = sFN_Get_MemoryPool_fromPointer(pAddress, sizeType);
        switch(sizeType)
        {
        case TDATA_BLOCK_HEADER::_TYPE_Units_SizeType::E_SmallSize:
        case TDATA_BLOCK_HEADER::_TYPE_Units_SizeType::E_NormalSize:
        {
            CMemoryPool& refPool = *(CMemoryPool*)pPool;
            refPool.mFN_Return_Memory_Process(static_cast<TMemoryObject*>(pAddress));
            return;
        }
        case TDATA_BLOCK_HEADER::_TYPE_Units_SizeType::E_OtherSize:
        {
            CMemoryPool__BadSize& refPool = *(CMemoryPool__BadSize*)pPool;
            refPool.mFN_Return_Memory_Process(static_cast<TMemoryObject*>(pAddress));
            return;
        }
        default: // TDATA_BLOCK_HEADER::_TYPE_Units_SizeType::E_Invalid:
        {
            InterlockedExchangeAdd(&m_stats_Counting_Free_BadPTR, 1);
            _DebugBreak("�߸��� �ּ� ��ȯ �Ǵ� �ջ�� �ĺ���");
        }
        }
    }
    void CMemoryPool_Manager::mFN_Return_MemoryQ(void* pAddress)
    {
        if(!pAddress)
            return;

        TDATA_BLOCK_HEADER::_TYPE_Units_SizeType sizeType;
        #ifdef _DEBUG
        auto pPool = sFN_Get_MemoryPool_fromPointer(pAddress, sizeType);
        #else
        auto pPool = sFN_Get_MemoryPool_fromPointerQ(pAddress, sizeType);
        #endif
        switch(sizeType)
        {
        case TDATA_BLOCK_HEADER::_TYPE_Units_SizeType::E_SmallSize:
        case TDATA_BLOCK_HEADER::_TYPE_Units_SizeType::E_NormalSize:
        {
            CMemoryPool& refPool = *(CMemoryPool*)pPool;
            refPool.mFN_Return_Memory_Process(static_cast<TMemoryObject*>(pAddress));
            return;
        }
        case TDATA_BLOCK_HEADER::_TYPE_Units_SizeType::E_OtherSize:
        {
            CMemoryPool__BadSize& refPool = *(CMemoryPool__BadSize*)pPool;
            refPool.mFN_Return_Memory_Process(static_cast<TMemoryObject*>(pAddress));
            return;
        }
        default: // TDATA_BLOCK_HEADER::_TYPE_Units_SizeType::E_Invalid:
        {
            InterlockedExchangeAdd(&m_stats_Counting_Free_BadPTR, 1);
            _DebugBreak("�߸��� �ּ� ��ȯ �Ǵ� �ջ�� �ĺ���");
        }
        }
    }

#if _DEF_USING_DEBUG_MEMORY_LEAK
    void* CMemoryPool_Manager::mFN_Get_Memory__AlignedCacheSize(size_t _Size, const char* _FileName, int _Line)
    {
        if(!_Size )
        {
            _Size = _DEF_CACHELINE_SIZE;
        }
        else
        {
            const size_t mask = _DEF_CACHELINE_SIZE - 1;
            if(_Size & mask)
                _Size = _DEF_CACHELINE_SIZE + _Size & (~mask);
        }
        
        auto pPool = CMemoryPool_Manager::mFN_Get_MemoryPool(_Size);
        return pPool->mFN_Get_Memory(_Size, _FileName, _Line);
    }
#else
    void* CMemoryPool_Manager::mFN_Get_Memory__AlignedCacheSize(size_t _Size)
    {
        if(!_Size )
        {
            _Size = _DEF_CACHELINE_SIZE;
        }
        else
        {
            const size_t mask = _DEF_CACHELINE_SIZE - 1;
            if(_Size & mask)
                _Size = _DEF_CACHELINE_SIZE + _Size & (~mask);
        }

        auto pPool = CMemoryPool_Manager::mFN_Get_MemoryPool(_Size);
        return pPool->mFN_Get_Memory(_Size);
    }
#endif
    void CMemoryPool_Manager::mFN_Return_Memory__AlignedCacheSize(void* pAddress)
    {
        CMemoryPool_Manager::mFN_Return_Memory(pAddress);
    }

    IMemoryPool* CMemoryPool_Manager::mFN_Get_MemoryPool(size_t _Size)
    {
        if(!_Size)
            _Size = 1;

        const size_t iTable = gFN_Get_MemoryPoolTableIndex(_Size);
        if(iTable < gc_Size_Table_MemoryPool)
        {
            if(g_pTable_MemoryPool[iTable])
                return g_pTable_MemoryPool[iTable];
        }
        else if(_Size > GLOBAL::gc_maxSize_of_MemoryPoolUnitSize)
        {
            return &m_SlowPool;
        }

        const size_t UnitSize = gFN_Calculate_UnitSize(_Size);
        return CMemoryPool_Manager::_mFN_Prohibit__CreatePool_and_SetMemoryPoolTable(iTable, UnitSize);
    }

#if _DEF_USING_MEMORYPOOL_GETPROCESSORNUMBER_SIDT_TLS_CACHE
    void CMemoryPool_Manager::mFN_Set_TlsCache_AccessFunction(TMemoryPool_TLS_CACHE&(*pFN)(void))
    {
        if(!pFN)
            return;
        gpFN_TlsCache_AccessFunction = pFN;
    }
#endif

    void CMemoryPool_Manager::mFN_Set_OnOff_WriteStats_to_LogFile(BOOL _On)
    {
        m_bWriteStats_to_LogFile = _On;

        m_Lock__set_Pool.Begin_Read();
        {
            for(auto iter : m_set_Pool)
            {
                if(iter.m_pPool)
                    iter.m_pPool->mFN_Set_OnOff_WriteStats_to_LogFile(_On);
            }
        }
        m_Lock__set_Pool.End_Read();
    }
    void CMemoryPool_Manager::mFN_Set_OnOff_Trace_MemoryLeak(BOOL _On)
    {
        GLOBAL::g_bDebug__Trace_MemoryLeak = _On;
    }
    void CMemoryPool_Manager::mFN_Set_OnOff_ReportOutputDebugString(BOOL _On)
    {
        GLOBAL::g_bDebug__Report_OutputDebug = _On;
    }


    DECLSPEC_NOINLINE CMemoryPool* CMemoryPool_Manager::_mFN_Prohibit__CreatePool_and_SetMemoryPoolTable(size_t iTable, size_t UnitSize)
    {
        // �޸�Ǯ�� ã�ų� �����ϰ�, ���� ĳ�ÿ� ����

        TLinkPool temp ={UnitSize, nullptr};
        CMemoryPool* pReturn = nullptr;
        // --Ž��--
        {
            m_Lock__set_Pool.Begin_Read();
            {
                auto iter = m_set_Pool.find(temp);
                if(iter != m_set_Pool.end())
                    pReturn = iter->m_pPool;
            }
            m_Lock__set_Pool.End_Read();
            if(pReturn)
            {
                gFN_Set_MemoryPoolTable(iTable, pReturn);
                return pReturn;
            }
        }

        // --��� �õ�--
        m_Lock__set_Pool.Begin_Write__INFINITE();
        {
            {
                // �ٸ� �����尡 ������, �ش�Ǵ� Ǯ�� ��������� �ѹ��� Ȯ��
                auto iter = m_set_Pool.find(temp);
                if(iter != m_set_Pool.end())
                    pReturn = iter->m_pPool;
            }
            if(pReturn)
            {
                m_Lock__set_Pool.End_Write();
                gFN_Set_MemoryPoolTable(iTable, pReturn);
                return pReturn;
            }

            // ���� ���
            //_SAFE_NEW_ALIGNED_CACHELINE(pReturn, CMemoryPool(UnitSize));
            pReturn = (CMemoryPool*)CObjectPool_Handle<CMemoryPool>::Alloc();
            _MACRO_CALL_CONSTRUCTOR(pReturn, CMemoryPool(UnitSize));
            pReturn->mFN_Set_OnOff_WriteStats_to_LogFile(m_bWriteStats_to_LogFile);
            if(pReturn)
            {
                temp.m_pPool = pReturn;
                m_set_Pool.insert(temp);
                gFN_Set_MemoryPoolTable(iTable, pReturn);
            }
        }
        m_Lock__set_Pool.End_Write();

        return pReturn;
    }


    __forceinline IMemoryPool* CMemoryPool_Manager::sFN_Get_MemoryPool_SmallObjectSize(void * p)
    {
        _CompileHint(nullptr != p);
        TDATA_BLOCK_HEADER* pH = _MACRO_POINTER_GET_ALIGNED((TDATA_BLOCK_HEADER*)p, GLOBAL::gc_minPageUnitSize);

    #if _DEF_USING_MEMORYPOOL_TEST_SMALLUNIT__FROM__DBH_TABLE
        // �ùٸ� ��ó�� ���� ��� + �߸��� �ּҷ� ���� ������ �����ϴ� ���
        const auto pHTrust = GLOBAL::g_Table_BlockHeaders_Normal.mFN_Get_Link(pH->m_Index.high, pH->m_Index.low);
        if(pHTrust->m_Type != TDATA_BLOCK_HEADER::_TYPE_Units_SizeType::E_SmallSize)
            return nullptr;
        if(!pHTrust->mFN_Query_ContainPTR(p))
            return nullptr;

        return pHTrust->m_pPool;
    #else
        // �ٸ� ��� ����ũ�⸦ Ȯ���Ŀ� ����ؾ� �Ѵ�
        if(pH->m_Type != TDATA_BLOCK_HEADER::_TYPE_Units_SizeType::E_SmallSize)
            return nullptr;
        if(!pH->mFN_Query_ContainPTR(p))
            return nullptr;

        return pH->m_pPool;
    #endif
    }
    __forceinline IMemoryPool* CMemoryPool_Manager::sFN_Get_MemoryPool_NormalObjectSize(void * p)
    {
        _CompileHint(nullptr != p);
        if(!_MACRO_POINTER_IS_ALIGNED(p, GLOBAL::gc_AlignSize_LargeUnit))
            return nullptr;

        const TDATA_BLOCK_HEADER* pH = static_cast<TDATA_BLOCK_HEADER*>(p) - 1;

        if(pH->m_Type != TDATA_BLOCK_HEADER::_TYPE_Units_SizeType::E_NormalSize)
            return nullptr;
        //if(nullptr == pH->m_pGroupFront)
        //    return nullptr;

        const TDATA_BLOCK_HEADER* pHTrust = GLOBAL::g_Table_BlockHeaders_Normal.mFN_Get_Link(pH->m_Index.high, pH->m_Index.low);
        //if(pH->m_pGroupFront != pHTrust)
        //    return nullptr;

        // �� �ּ�ó���� if(nullptr == pH->m_pGroupFront) if(pH->m_pGroupFront != pHTrust) �� �ӵ��� ���� �����Ѵ�
        // pHTrust �� �ŷ��Ѵ�
        // ��, pHTrust �� �ջ���� �ʾҴٴ� �����Ͽ�. pH �� �ջ󿩺δ� üũ���� ���Ѵ�
        // �̴� ������忡 ���Ͽ� �޸�Ǯ �޸� �ݳ� ���μ��� ���� Ȯ���Ѵ�

        if(!pHTrust->mFN_Query_ContainPTR(p))
            return nullptr;

        return pHTrust->m_pPool;
    }

    __forceinline IMemoryPool* CMemoryPool_Manager::sFN_Get_MemoryPool_fromPointer(void * p, TDATA_BLOCK_HEADER::_TYPE_Units_SizeType& _out_Type)
    {
        _CompileHint(nullptr != p);

        // ���� ����ũ����� ĳ��ũ�⿡ ���ĵ� ������ ���� �� ������ ���ǹ� �ϴ�
        //if(!_MACRO_POINTER_IS_ALIGNED(p, GLOBAL::gc_AlignSize_LargeUnit))
        //    goto Label_S;

        // p�� �� 64����ư ������ �����Ѵ�
        const TDATA_BLOCK_HEADER& header = *(static_cast<TDATA_BLOCK_HEADER*>(p) - 1);
        // �ּ�ó����: ����� �Ϻΰ� �ջ�Ǵ��� pHTrust�� �ŷ��Ѵ�
        //if(nullptr == header.m_pGroupFront)
        //    goto Label_S;

        const CTable_DataBlockHeaders* pTable;
        const TDATA_BLOCK_HEADER* pHTrust;
        switch(header.m_Type)
        {
        case TDATA_BLOCK_HEADER::_TYPE_Units_SizeType::E_NormalSize:
            pTable = &GLOBAL::g_Table_BlockHeaders_Normal;
            pHTrust = pTable->mFN_Get_Link(header.m_Index.high, header.m_Index.low);
            _out_Type = TDATA_BLOCK_HEADER::_TYPE_Units_SizeType::E_NormalSize;
            break;

        case TDATA_BLOCK_HEADER::_TYPE_Units_SizeType::E_OtherSize:
            pTable = &GLOBAL::g_Table_BlockHeaders_Big;
            pHTrust = pTable->mFN_Get_Link(header.m_Index.high, header.m_Index.low);
            _out_Type = TDATA_BLOCK_HEADER::_TYPE_Units_SizeType::E_OtherSize;
            break;

        default:
            goto Label_S;
        }
        // �ּ�ó����: ����� �Ϻΰ� �ջ�Ǵ��� pHTrust�� �ŷ��Ѵ�
        //if(header.m_pGroupFront != pHTrust)
        //    goto Label_S;
        if(!pHTrust->mFN_Query_ContainPTR(p))
            goto Label_S;

        return pHTrust->m_pPool;

    Label_S:

        IMemoryPool* pSmallPool = sFN_Get_MemoryPool_SmallObjectSize(p);
        if(pSmallPool)
            _out_Type = TDATA_BLOCK_HEADER::_TYPE_Units_SizeType::E_SmallSize;
        else
            _out_Type = TDATA_BLOCK_HEADER::_TYPE_Units_SizeType::E_Invalid;
        return pSmallPool;
    }
    __forceinline IMemoryPool* CMemoryPool_Manager::sFN_Get_MemoryPool_fromPointerQ(void * p, TDATA_BLOCK_HEADER::_TYPE_Units_SizeType& _out_Type)
    {
        _CompileHint(nullptr != p);

        // p�� �� 64����ư ������ �����Ѵ�
        const TDATA_BLOCK_HEADER& header = *(static_cast<TDATA_BLOCK_HEADER*>(p) - 1);

        const CTable_DataBlockHeaders* pTable;
        const TDATA_BLOCK_HEADER* pHTrust;
        // ���� ������ �պκ��� �쿬�� ������ ��ġ�� �� �ֱ� ������ ���̺��� Ȯ���ؾ� �Ѵ�
        switch(header.m_Type)
        {
        case TDATA_BLOCK_HEADER::_TYPE_Units_SizeType::E_NormalSize:
            pTable = &GLOBAL::g_Table_BlockHeaders_Normal;
            pHTrust = pTable->mFN_Get_Link(header.m_Index.high, header.m_Index.low);
            _out_Type = TDATA_BLOCK_HEADER::_TYPE_Units_SizeType::E_NormalSize;
            break;

        case TDATA_BLOCK_HEADER::_TYPE_Units_SizeType::E_OtherSize:
            pTable = &GLOBAL::g_Table_BlockHeaders_Big;
            pHTrust = pTable->mFN_Get_Link(header.m_Index.high, header.m_Index.low);
            _out_Type = TDATA_BLOCK_HEADER::_TYPE_Units_SizeType::E_OtherSize;
            break;

        default:
            goto Label_S;
        }
        // �ݵ�� üũ�ؾ� �Ѵ�
        if(!pHTrust->mFN_Query_ContainPTR(p))
            goto Label_S;

        return pHTrust->m_pPool;

    Label_S:
        #ifdef _DEBUG
        IMemoryPool* pSmallPool = sFN_Get_MemoryPool_SmallObjectSize(p);
        #else
        TDATA_BLOCK_HEADER* pHS = _MACRO_POINTER_GET_ALIGNED((TDATA_BLOCK_HEADER*)p, GLOBAL::gc_minPageUnitSize);
        IMemoryPool* pSmallPool = ((pHS->m_Type == TDATA_BLOCK_HEADER::_TYPE_Units_SizeType::E_SmallSize)? pHS->m_pPool : nullptr);
        #endif
        if(pSmallPool)
            _out_Type = TDATA_BLOCK_HEADER::_TYPE_Units_SizeType::E_SmallSize;
        else
            _out_Type = TDATA_BLOCK_HEADER::_TYPE_Units_SizeType::E_Invalid;
        return pSmallPool;
    }

    void CMemoryPool_Manager::mFN_Report_Environment_Variable()
    {
        auto sysinfo = CORE::g_instance_CORE.mFN_Get_System_Information();
        _LOG_DEBUG__WITH__TRACE(FALSE, "CPU Core(%d) Thrad(%d) CACHE(%d) per POOL"
            , sysinfo->mFN_Get_NumProcessor_PhysicalCore()
            , sysinfo->mFN_Get_NumProcessor_Logical()
            , g_nBasket__CACHE);
    }
    void CMemoryPool_Manager::mFN_Debug_Report()
    {
        if(0 < m_stats_Counting_Free_BadPTR)
        {
            // �ɰ��� ��Ȳ�̴� Release ���������� ����Ѵ�
            _LOG_DEBUG__WITH__OUTPUTDEBUGSTR_ALWAYS(FALSE, "================ Critical Error : Failed Count(%Iu) -> MemoryPoolManager:Free ================"
                , m_stats_Counting_Free_BadPTR);
        }
    }

}
}
#endif