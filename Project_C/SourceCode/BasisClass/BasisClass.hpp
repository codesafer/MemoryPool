/*----------------------------------------------------------------
/       ����Ʈ ������
----------------------------------------------------------------*/
template<typename T>
CSmartPointer<T>::CSmartPointer()
: m_bImLive(TRUE)
, m_bImDelete(FALSE)
, m_pPointer(NULL)
, m_RefCount(0)
{
}

template<typename T>
CSmartPointer<T>::CSmartPointer(T* pPointer)
: m_bImLive(TRUE)
, m_bImDelete(FALSE)
, m_pPointer(pPointer)
, m_RefCount(0)
{
}

template<typename T>
CSmartPointer<T>::~CSmartPointer()
{
    // �̰��� �̱����� ��츦 ���� ���ȴ�.
    m_bImLive = FALSE;

    // �ᱹ ���������� ���ο��� Ȯ���Ѵ�.
    Delete();
}

template<typename T>
void CSmartPointer<T>::operator = (T* pPointer)
{
    assert(m_bImLive && !m_pPointer && m_RefCount == 0);

    m_bImDelete = FALSE;
    m_pPointer = pPointer;
    m_RefCount = 0;
}

template<typename T>
BOOL CSmartPointer<T>::isEmpty() const
{
    return (m_pPointer)? FALSE : TRUE;
}

template<typename T>
BOOL CSmartPointer<T>::TestPointer(T* pPointer) const
{
    return (m_pPointer == pPointer);
}

template<typename T>
void CSmartPointer<T>::Add_Ref()
{
    assert(m_pPointer);
    ++m_RefCount;
}

template<typename T>
void CSmartPointer<T>::Release_Ref()
{
    assert(m_pPointer && 0 < m_RefCount);
    --m_RefCount;
    if(m_pPointer && 0 == m_RefCount)
        Delete();
}

template<typename T>
__forceinline void CSmartPointer<T>::Delete()
{
    if(!m_bImLive && !m_bImDelete && m_pPointer && 0 == m_RefCount)
    {
        m_bImDelete = TRUE;
        delete m_pPointer;
        m_pPointer = NULL;
    }
}

/*----------------------------------------------------------------
/       ����Ʈ ������ ���� ��ü
----------------------------------------------------------------*/
template<typename T>
CSmartPointerRef<T>::CSmartPointerRef()
: CSmartPointerRefBasis()
{
}

template<typename T>
template<typename TUnknown>
CSmartPointerRef<T>::CSmartPointerRef(CSmartPointer<TUnknown>* _pData)
: CSmartPointerRefBasis(_pData, (_pData)? _pData->m_pPointer : NULL)
{
    if(m_pSmartPointer)
        m_pSmartPointer->Add_Ref();
}

template<typename T>
template<typename TUnknown>
CSmartPointerRef<T>::CSmartPointerRef(const CSmartPointerRefBasis<TUnknown>& _DataRef)
: CSmartPointerRefBasis(_DataRef.m_pSmartPointer, _DataRef.m_pPointer)
{
    if(m_pSmartPointer)
        m_pSmartPointer->Add_Ref();
}

template<typename T>
CSmartPointerRef<T>::~CSmartPointerRef()
{
    Release();
}

template<typename T>
template<typename TUnknown>
CSmartPointerRef<T>& CSmartPointerRef<T>::operator = (CSmartPointer<TUnknown>* _pData)
{
    Release();

    m_pSmartPointer = _pData;
    m_pPointer      = (_pData)? _pData->m_pPointer : NULL;
    if(m_pSmartPointer)
        m_pSmartPointer->Add_Ref();
}

template<typename T>
template<typename TUnknown>
CSmartPointerRef<T>& CSmartPointerRef<T>::operator = (const CSmartPointerRefBasis<TUnknown>& _DataRef)
{
    //if(this == &_DataRef)
    //    return *this;
    //Unknown Ÿ���̱⶧���� �����ͷ� ���Ѵ�.
    if(m_pSmartPointer == _DataRef.m_pSmartPointer)
        return *this;

    Release();

    if(m_pSmartPointer)
        m_pSmartPointer->Release_Ref();

    m_pSmartPointer = _DataRef.m_pSmartPointer;
    m_pPointer      = _DataRef.m_pPointer;
    if(m_pSmartPointer)
        m_pSmartPointer->Add_Ref();
    return *this;
}

// ���� ����
template<typename T>
void CSmartPointerRef<T>::Release()
{
    if(m_pPointer)
        m_pPointer = NULL;
    if(m_pSmartPointer)
    {
        CSmartPointerBasis* pTemp = m_pSmartPointer;
        m_pSmartPointer           = NULL;
        pTemp->Release_Ref();
    }
}
template<typename T>
__forceinline BOOL CSmartPointerRef<T>::isEmpty() const
{
    return (m_pSmartPointer)? FALSE : TRUE;
}

// ������ ����
template<typename T>
__forceinline T* CSmartPointerRef<T>::operator * ()
{
    return m_pPointer;
}

// ������ ����
template<typename T>
__forceinline const T* CSmartPointerRef<T>::operator * () const
{
    return m_pPointer;
}


/*----------------------------------------------------------------
/       �̱���
----------------------------------------------------------------*/
#pragma warning(push)
#pragma warning(disable: 4127)
template<typename T, typename TInstanceFromGroup>
volatile LONG CSingleton<T, TInstanceFromGroup>::s_isCreated = 0;
template<typename T, typename TInstanceFromGroup>
T* CSingleton<T, TInstanceFromGroup>::s_pInstance = NULL;

template<typename T, typename TInstanceFromGroup>
T* CSingleton<T, TInstanceFromGroup>::Get_Instance()
{
    if(!s_pInstance)
        Create_Instance();
    return s_pInstance;
}
template<typename T, typename TInstanceFromGroup>
DECLSPEC_NOINLINE void CSingleton<T, TInstanceFromGroup>::Create_Instance()
{
    if(s_pInstance)
        return;

    if(0 == ::InterlockedExchange(&s_isCreated, 1))
    {
        if(0 == (sizeof(T) % _DEF_CACHELINE_SIZE))
            s_pInstance = __Internal__Create_Instance__AlignedCacheline();
        else
            s_pInstance = __Internal__Create_Instance__Default();

    #ifdef _ATOMIC_THREAD_FENCE
        std::_Atomic_thread_fence(std::memory_order::memory_order_release);
    #else
        _WriteBarrier();
    #endif
    }
    else
    {
        for(;;)
        {
        #ifdef _ATOMIC_THREAD_FENCE
            std::_Atomic_thread_fence(std::memory_order::memory_order_acquire);
        #else
            _ReadBarrier();
        #endif

            if(s_pInstance)
                break;

            ::UTIL::TUTIL::sFN_SwithToThread();
        }
    }
}
template<typename T, typename TInstanceFromGroup>
T* CSingleton<T, TInstanceFromGroup>::__Internal__Create_Instance__Default()
{
    static T _Instance;
    return &_Instance;
}
template<typename T, typename TInstanceFromGroup>
T* CSingleton<T, TInstanceFromGroup>::__Internal__Create_Instance__AlignedCacheline()
{
    _DEF_CACHE_ALIGN static T _Instance;
    return &_Instance;
}


// �������� ���� ������ �̱���

template<typename T, typename TInstanceFromGroup>
T* CSingleton_ManagedDeleteTiming<T, TInstanceFromGroup>::s_pInstance = nullptr;
template<typename T, typename TInstanceFromGroup>
volatile LONG CSingleton_ManagedDeleteTiming<T, TInstanceFromGroup>::s_isCreated = 0;
template<typename T, typename TInstanceFromGroup>
volatile LONG CSingleton_ManagedDeleteTiming<T, TInstanceFromGroup>::s_isClosed = 0;
template<typename T, typename TInstanceFromGroup>
volatile LONG CSingleton_ManagedDeleteTiming<T, TInstanceFromGroup>::s_cntAttach = 0;

template<typename T, typename TInstanceFromGroup>
T* CSingleton_ManagedDeleteTiming<T, TInstanceFromGroup>::Get_Instance()
{
    if(!s_pInstance)
        Create_Instance();
    return s_pInstance;
}
template<typename T, typename TInstanceFromGroup>
DECLSPEC_NOINLINE void CSingleton_ManagedDeleteTiming<T, TInstanceFromGroup>::Create_Instance()
{
    if(s_isClosed || s_pInstance)
        return;

    if(0 == ::InterlockedExchange(&s_isCreated, 1))
    {
        static TSensor__ProgramEnd<CSingleton_ManagedDeleteTiming<T, TInstanceFromGroup>> _Instance_Sensor;

        T* p;
        // �����Ϸ��� ���� ����ȭ �ɰ��̴�
        if(0 == (sizeof(T) % _DEF_CACHELINE_SIZE))
            p = (T*)::_aligned_malloc(sizeof(T), _DEF_CACHELINE_SIZE);
        else
            p = (T*)::malloc(sizeof(T));

        if(p)
        {
            new(p) T;
            s_pInstance = p;

        #ifdef _ATOMIC_THREAD_FENCE
            std::_Atomic_thread_fence(std::memory_order::memory_order_release);
        #else
            _WriteBarrier();
        #endif
        }
    }
    else
    {
        for(;;)
        {
        #ifdef _ATOMIC_THREAD_FENCE
            std::_Atomic_thread_fence(std::memory_order::memory_order_acquire);
        #else
            _ReadBarrier();
        #endif

            if(s_pInstance)
                break;

            ::UTIL::TUTIL::sFN_SwithToThread();
        }
    }
}
template<typename T, typename TInstanceFromGroup>
void CSingleton_ManagedDeleteTiming<T, TInstanceFromGroup>::Reference_Attach()
{
    auto valPrevious = InterlockedExchangeAdd(&s_cntAttach, 1);
    if(valPrevious == LONG_MAX)
        *((UINT32*)NULL) = 0x003BBA4A; // �ǵ��� ����
        
}
template<typename T, typename TInstanceFromGroup>
void CSingleton_ManagedDeleteTiming<T, TInstanceFromGroup>::Reference_Detach()
{
    auto valPrevious = InterlockedExchangeAdd(&s_cntAttach, -1);
    if(1 < valPrevious)
        return;

    if(1 == valPrevious)
    {
        if(s_isClosed)
            Destroy();
    }
    else // (0 == valPrevious)
    {
        *((UINT32*)NULL) = 0x003BBA4A; // �ǵ��� ����
    }
}
template<typename T, typename TInstanceFromGroup>
void CSingleton_ManagedDeleteTiming<T, TInstanceFromGroup>::Destroy()
{
    T* p = s_pInstance;
    s_pInstance = nullptr;
    if(!p)
        return;

    p->~T();

    // �����Ϸ��� ���� ����ȭ �ɰ��̴�
    if(0 == (sizeof(T) % _DEF_CACHELINE_SIZE))
        ::_aligned_free(p);
    else
        ::free(p);
}
#pragma warning(pop)
/*----------------------------------------------------------------
/       Tree��ȯ��ü( Tree������ �ڷ��� ���ȣ���� ��ü �Ѵ� )
----------------------------------------------------------------*/
template<typename TNode, typename TParam>
CNonRecursive<TNode, TParam>::CNonRecursive()
: m_FirstSize(32) // �⺻������� 32���� �ϰ���
, m_pStack(NULL)
, m_Size_Buffer(0)
, m_Size_Using(0)
{
}

template<typename TNode, typename TParam>
CNonRecursive<TNode, TParam>::CNonRecursive(size_t _FirstSize)
: m_FirstSize(_FirstSize)
, m_pStack(NULL)
, m_Size_Buffer(0)
, m_Size_Using(0)
{
}

template<typename TNode, typename TParam>
CNonRecursive<TNode, TParam>::~CNonRecursive()
{
    if(m_pStack)
        free(m_pStack);
}

template<typename TNode, typename TParam>
__inline void CNonRecursive<TNode, TParam>::Reserve(size_t _Count)
{
    if(m_Size_Buffer >= _Count)
        return;

    m_Size_Buffer = _Count;

    if(m_pStack)
    {
        m_pStack = reinterpret_cast<_tag_NodeUnit*>(
            ::realloc(m_pStack, sizeof(_tag_NodeUnit) * _Count)
            );
    }
    else
    {
        m_pStack = reinterpret_cast<_tag_NodeUnit*>(
            ::malloc(m_pStack, sizeof(_tag_NodeUnit) * _Count)
            );
    }
}

template<typename TNode, typename TParam>
__inline void CNonRecursive<TNode, TParam>::Push(TNode* _Node, TParam _Param)
{
    if(m_Size_Using == m_Size_Buffer)
    {
        if(m_pStack)
        {
            m_Size_Buffer *= 2;     // ���۰� �������� �ι�� Ȯ���Ѵ�.
            m_pStack = reinterpret_cast<_tag_NodeUnit*>(
                ::realloc(m_pStack, sizeof(_tag_NodeUnit) * m_Size_Buffer)
                );
        }
        else
        {
            m_Size_Buffer = m_FirstSize;
            m_pStack = reinterpret_cast<_tag_NodeUnit*>(
                ::malloc(sizeof(_tag_NodeUnit) * m_Size_Buffer)
                );
        }
    }

    m_pStack[m_Size_Using].m_pNode = _Node;
    m_pStack[m_Size_Using].m_Param = _Param;
    m_Size_Using++;
}

template<typename TNode, typename TParam>
__inline TNode* CNonRecursive<TNode, TParam>::Pop(TParam* _Param)
{
    if(m_Size_Using == 0)
        return NULL;

    m_Size_Using--;
    *_Param = m_pStack[m_Size_Using].m_Param;
    return m_pStack[m_Size_Using].m_pNode;
}