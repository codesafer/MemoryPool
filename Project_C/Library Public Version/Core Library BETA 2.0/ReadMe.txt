���̺귯�� ���۱���
	lastpenguin83@gmail.com

��� �䱸����
	visual studio 2015 C++

���� �䱸����
	visual studio 2015 C++ �Ǵ�,
	Visual Studio 2015�� Visual C++ ����� ���� ��Ű��
		https://www.microsoft.com/ko-kr/download/details.aspx?id=48145
//----------------------------------------------------------------
����
	Core_x86.dll	Windows XP �̻�
	Core_x64.dll	Windows Vista �̻�
	�� ���� ���� : �����ڵ�
//----------------------------------------------------------------
��� ���
	�ʱ�ȭ�� ó���� cpp ���Ͽ��� ���� ������ ����(include)
		./User/Core_import.h
	�ʱ�ȭ
		::CORE::Connect_CORE(...)
	����(����� ���� ������ �ʿ��Ҷ��� ���)
		::CORE::Disconnect_CORE

	����� ���ؼ��� ���� ������ ����(include)
		./User/Core_include.h
//----------------------------------------------------------------
�������̽��� ���� ���ϵ��� ����
	Core_Interface.h
	MemoryPool_Interface.h
	System_Information_Interface.h

������ �α� �ʿ�� ���� ������ �����Ͽ� ��ü�� �����Ͽ�, �ʱ�ȭ�� �ν��Ͻ��� �����ؾ� ��
	LogWriter_Interface.h

	pLogSystem	�߿��� �������� ���
	pLogDebug	�Ϲ� ����� ����