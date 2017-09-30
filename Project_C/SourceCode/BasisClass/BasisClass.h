#pragma once


/*----------------------------------------------------------------
/       ���� ������ ���� ��ü ( ��ó: Effective C++ )
----------------------------------------------------------------*/
class CUnCopyAble{
public:
    CUnCopyAble()   {}
    ~CUnCopyAble()  {}

private:
    CUnCopyAble(const CUnCopyAble&);
    CUnCopyAble& operator = (const CUnCopyAble&);
};

/*----------------------------------------------------------------
/       ����Ʈ ������
/       [ CSmartPointer ]
/       CSmartPointerRef ��ü�� �Բ� ����մϴ�.
----------------------------------------------------------------*/
template<typename T>
class CSmartPointerRef;

class CSmartPointerBasis : private CUnCopyAble{
    template<typename>
    friend class CSmartPointerRef;
protected:
    virtual void Add_Ref() = 0;
    virtual void Release_Ref() = 0;
};

template<typename T>
class CSmartPointer : protected CSmartPointerBasis{
    template<typename>
    friend class CSmartPointerRef;
public:
    CSmartPointer();
    explicit CSmartPointer(T* pPointer);
    virtual ~CSmartPointer();

private:
    BOOL        m_bImLive;
    BOOL        m_bImDelete;

    T*          m_pPointer;
    size_t      m_RefCount;

public:
    void operator = (T* pPointer);
    BOOL isEmpty() const;
    BOOL TestPointer(T* pPointer) const;

private:
    virtual void Add_Ref() override;
    virtual void Release_Ref() override;
    __forceinline void Delete();
};

/*----------------------------------------------------------------
/       ����Ʈ ������ ���� ��ü
/       [ CSmartPointerRef ]
/       CSmartPointer��ü�� �Բ� ����մϴ�.
----------------------------------------------------------------*/
template<typename T>
class CSmartPointerRefBasis{
    template<typename>
    friend class CSmartPointerRef;
private:
    CSmartPointerRefBasis()
        : m_pSmartPointer(NULL)
        , m_pPointer(NULL)
    {
    }
    CSmartPointerRefBasis(CSmartPointerBasis* _pSmart, T* pData)
        : m_pSmartPointer(_pSmart)
        , m_pPointer(pData)
    {
    }

private:
    CSmartPointerBasis*     m_pSmartPointer;
    T*                      m_pPointer;
};


template<typename T>
class CSmartPointerRef : public CSmartPointerRefBasis<T>{
public:
    CSmartPointerRef();
    template<typename TUnknown>     CSmartPointerRef(CSmartPointer<TUnknown>* _pData);
    template<typename TUnknown>     CSmartPointerRef(const CSmartPointerRefBasis<TUnknown>& _DataRef);
    ~CSmartPointerRef();

public:
    template<typename TUnknown>
    CSmartPointerRef<T>& operator = (CSmartPointer<TUnknown>* _pData);

    template<typename TUnknown>
    CSmartPointerRef<T>& operator = (const CSmartPointerRefBasis<TUnknown>& _DataRef);

    void Release();                             // ���� ����
    __forceinline BOOL isEmpty() const;
    __forceinline T* operator * ();             // ������ ����
    __forceinline const T* operator * () const; // ������ ����
};

/*----------------------------------------------------------------
/       �̱���
/           CSingleton      ���� ������ ������ �̱���
/           ĳ�ö��δ���ũ���� ���ũ�⸦ ���� ��ü�� �ڵ����� ĳ�ö��ο� �����մϴ�
/----------------------------------------------------------------
/
/   ��1 :
/       class CTest1{
/           _DEF_FRIEND_SINGLETO        // �ʿ信 ���� �����Ͻʽÿ�(#1 ����)
/           ...
/       };
/
/       CSingleton<CTest1>::Get_Instance();
/
/   #1 ���� ��ü�� �����ڿ� �Ҹ��ڰ� ��������̶�� ���� ��ũ�θ� ��ü ���� ���ο� �����Ͻʽÿ�.
/       _DEF_FRIEND_SINGLETON
/
/   #2 �̱����� ������ ��ü�鰣�� ���� �켱������ �����ϰ� �ʹٸ�,
/      ������� ObjA�� ObjB���� ���� �����Ǿ�� �Ѵٸ�
/      ObjB�� �����ڿ��� CSingleton<ObjA>::Create_Instance() �� ȣ���մϴ�.
/
/   #3 ����, ��ü���� ����1���� ��ü�� �ƴ�,
/      � �ĺ��ڸ� �������� �ĺ��ڴ����� �̱��� ��ü�� ����ϰ� �ʹٸ�,
/      TInstanceFromGroup�� ����Ͻʽÿ�.
/
----------------------------------------------------------------*/
#define     _DEF_FRIEND_SINGLETON       template<typename, typename> friend class CSingleton; template<typename, typename> friend class CSingleton_ManagedDeleteTiming;

template<typename T, typename TInstanceFromGroup = T>
class CSingleton : protected CUnCopyAble{
public:
    static T* Get_Instance();
    static void Create_Instance();

private:
    static T* __Internal__Create_Instance__Default();
    static T* __Internal__Create_Instance__AlignedCacheline();
    static T* s_pInstance;
    static volatile LONG s_isCreated;
};

// CSingleton_ManagedDeleteTiming �� �⺻���� CSingleton �� �޸� ����Ÿ�̹��� �����մϴ�
//      �̰��� ���� ������ �����ϴµ� �ʿ��մϴ�
//      �Ʒ��� ���� ���� ��쿡 �ʿ��մϴ�
//          CA::CSingleton
//          CB::CSingleton
//
//          CA::CreateInstance
//          CA()
//              CB::CreateInstance  // CA ���� ������� ���Ͽ�
//              CB()
//              ~CB()
//          ~CA()   // CA �����ڿ��� ���� CB�� ����Ϸ��Ѵٸ� ����
//
//          ������ ���� �ذ�
//          CA() �ʱ⿡, CB::Reference_Attach()
//          ~CA() �������� CB::Reference_Detach()
// �̰��� �����Ҵ� �˴ϴ�
// T �� ũ�Ⱑ ĳ�ö���ũ���� ������ ĳ�ö��ο� ���ĵǵ��� �Ҵ�˴ϴ�
template<typename T, typename TInstanceFromGroup = T>
class CSingleton_ManagedDeleteTiming : protected CUnCopyAble{
public:
    static T* Get_Instance();
    static void Create_Instance();
    static void Reference_Attach();
    static void Reference_Detach();

private:
    static void Destroy();

private:
    static T* s_pInstance;
    static volatile LONG s_isCreated;
    static volatile LONG s_isClosed;
    static volatile LONG s_cntAttach;  // �ִ� ī���� ����

    template<typename TSingleton>
    struct TSensor__ProgramEnd{
        ~TSensor__ProgramEnd()
        {
            auto valPrevious = ::InterlockedExchange(&TSingleton::s_isClosed, 1);
            if(0 == valPrevious)
            {
                if(0 == TSingleton::s_cntAttach)
                    TSingleton::Destroy();
            }
            else
            {
                *((UINT32*)NULL) = 0x003BBA4A; // �ǵ��� ����
            }
        }
    };
};
/*----------------------------------------------------------------
/       �� ��� ��ü( Tree������ �ڷ��� ���ȣ���� ��ü �Ѵ� )
----------------------------------------------------------------*/
template<typename TNode, typename TParam>
class CNonRecursive : private CUnCopyAble{
    struct _tag_NodeUnit{
        TNode*  m_pNode;
        TParam  m_Param;
    };

public:
    CNonRecursive();
    CNonRecursive(size_t _FirstSize);
    ~CNonRecursive();

private:
    const size_t    m_FirstSize;    // ���� Ȯ�������

    _tag_NodeUnit*  m_pStack;       // ����
    size_t          m_Size_Buffer;  // ����ũ��
    size_t          m_Size_Using;   // ������� ũ��

public:
    __inline void Reserve(size_t _Count);
    __inline void Push(TNode* _Node, TParam _Param);
    __inline TNode* Pop(TParam* _Param);
};

#include "./BasisClass.hpp"