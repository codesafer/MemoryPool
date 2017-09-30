#pragma once
// �޸𸮴��� �����
#if !defined(_DEF_USING_DEBUG_MEMORY_LEAK)
#if defined(_DEBUG)
#define _DEF_USING_DEBUG_MEMORY_LEAK    1
#else
#define _DEF_USING_DEBUG_MEMORY_LEAK    0
#endif
#endif

#if _DEF_USING_DEBUG_MEMORY_LEAK
#define _DEF_FILE_LINE  , __FILE__, __LINE__
#else
#define _DEF_FILE_LINE
#endif

#if _DEF_USING_DEBUG_MEMORY_LEAK
#define debug_new new(__FILE__, __LINE__)
#else
#define debug_new new
#endif
#define NEW debug_new

#if _DEF_USING_DEBUG_MEMORY_LEAK
#include <crtdbg.h>
/*----------------------------------------------------------------
/   new �����Ǹ� �����ϸ鼭, #define new�� ���� ���尡 �Ǿ����� �ʴ� ������
/   ���Ͽ� �߰��մϴ�.
----------------------------------------------------------------*/
#include <xmemory>
#include <xtree>


// MFC �޸� ������� Ȱ��ȭ ���� ���� ��쿡�� ���˴ϴ�.
// ���� ȣ���� ��ȣ�����ϴ�.
#if !defined(__AFX_H__) || defined(_AFX_NO_DEBUG_CRT)
static class CDebugMemoryLeak{
public:
    CDebugMemoryLeak()
    {
        _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
    }
} __CDebugMemoryLeak;
#endif

// __autoclassinit ȣ�� ������ ���� ������ ���ܹ߻�����(nothrow)�� �ƴ� size�ܿ� �ٸ� �Ķ������ �߰��� �����̴�
// �׷��Ƿ� ����� ������ ��� __FILE__ __LINE__ ���� ����ϵ��� �Ѵ�
// size �̿� ������� �ʴ� �Ķ���ʹ� �����Ϸ��� ���°�ó�� ����ȭ ���ش�
//#define NEW new(std::nothrow, __FILE__, __LINE__)

#ifdef delete
#pragma message("delete�� macro�� ���Ǿ���")
#endif

#endif


//namespace UTIL{
#if _DEF_USING_DEBUG_MEMORY_LEAK
    inline void* operator new(size_t _Size, const char* _FileName, int _Line)
    {
        return ::operator new(_Size, _NORMAL_BLOCK, _FileName, _Line);
    }
    inline void* operator new[](size_t _Size, const char* _FileName, int _Line)
    {
        return ::operator new[](_Size, _NORMAL_BLOCK, _FileName, _Line);
    }
    inline void operator delete(void* _pData, const char* /*_FileName*/, int /*_Line*/)
    {
        ::operator delete(_pData);
    }
    inline void operator delete[](void* _pData, const char* /*_FileName*/, int /*_Line*/)
    {
        ::operator delete[](_pData);
    }
#else
    inline void* operator new(size_t _Size, const char* /*_FileName*/, int /*_Line*/)
    {
        return ::operator new(_Size);
    }
    inline void* operator new[](size_t _Size, const char* /*_FileName*/, int /*_Line*/)
    {
        return ::operator new[](_Size);
    }
        inline void operator delete(void* _pData, const char* /*_FileName*/, int /*_Line*/)
    {
        ::operator delete(_pData);
    }
    inline void operator delete[](void* _pData, const char* /*_FileName*/, int /*_Line*/)
    {
        ::operator delete[](_pData);
    }
#endif
//}
//// �ܺη� �����Ѵ�.
//using UTIL::operator new;
//using UTIL::operator delete;
//using UTIL::operator new[];
//using UTIL::operator delete[];