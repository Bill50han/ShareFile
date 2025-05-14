#pragma once
#include "wKutilities.h"
#include <fltKernel.h>
#include <ntifs.h>
#include <ntddk.h>
#include <ntstrsafe.h>
#include <dontuse.h>

//Լ��������Ŀ������size��λ�����ֽڣ�������size���ַ����Ĵ�С����ô������ĩβ��0������UNICODE_STRING�Ķ�����ȫ��ͬ����������ָ����̬���ַ���ĩβ��ӦΪ0��

//����һ�رȽϺ����ļ��ϣ�ʵ��������ֻ��������һ����ʵ��ʹ�á�
bool CmpTrivial(wchar_t*, size_t, const wchar_t*, size_t);	//ʹ���Դ���RtlCompareUnicodeString������ΪPathList�������ռ�
bool CmpPrefix(wchar_t*, size_t, const wchar_t*, size_t);	//RtlCompareUnicodeString�����Ƚϳ��Ⱦ�Ϊ�����ַ������ȣ���˿�������ͨ��ǰ׺ƥ��
bool CmpAvx1Prefix(wchar_t* a, size_t asize, const wchar_t* b, size_t bsize);	//��ʽavx1�Ƚϣ���ͨ��ǰ׺ƥ��
bool CmpAnotherAvx1Prefix(wchar_t* a, size_t asize, const wchar_t* b, size_t bsize);	//һ���ռ任ʱ��������㷨����ҪCmpMemorySize=32��ͨ��ǰ׺ƥ��

class Database
{
private:
	Database() = default;		//ʹ�õ���ģʽ�������캯����Ϊ˽�У���ֹ�ⲿ��������µ�ʵ����
	static Database instance;

#pragma pack(push)
#pragma pack(1)		//ǿ����PathList��1�ֽڶ��룬������������ڴ��С������ʹ��c++�Դ���alignas����Ϊ1С�ڽṹ��������Ԫ�ش�С8��
	struct PathList
	{
		size_t size;
		PathList* pre;
		PathList* next;

		size_t hash;
		wchar_t path[1];	//msvc����ȫ֧��gcc�������չ����˴�С��Ϊ1����ֹ�ں˻����������� ��warning C4200: ʹ���˷Ǳ�׼��չ: �ṹ/�����е����С���顱 ��Ϊ������ֹ���롣
	};
#pragma pack(pop)

	PathList head = { sizeof(PathList),nullptr,nullptr,0,L'\0' };
	KSPIN_LOCK lHead = NULL;

	using CompareFunction = bool(*)(wchar_t*, size_t, const wchar_t*, size_t);
	CompareFunction cmp = CmpTrivial;
	size_t CmpMemorySize = 0;	//��һЩ�ȽϺ�����Ҫ����Ŀռ䣬�������¼

	//һ�������ڵ��ַ���hash�㷨�����ڸ���Lock������������ת��
	constexpr static size_t ConstexprHash(const char* str, size_t hash = 2166136261)
	{
		return *str ? ConstexprHash(str + 1, (hash ^ (size_t)(*str)) * 16777619) : hash;
	}

public:
	static Database& GetInstance()
	{
		return instance;
	}
	// ��ֹ�����͸�ֵ
	Database(const Database&) = delete;
	Database& operator=(const Database&) = delete;

	sfresult add(const wchar_t* path, size_t size, size_t hash);
	sfresult check(const wchar_t* path, size_t size);
	sfresult del(size_t hash);
	sfresult isIn(size_t hash);

	void Init();

	//���ģ�庯��Ϊ�����ĸ��������������������������Ϊ����KComm����߳�ʹ�á�
	//���в�����Ϊ��������ɣ���ֹӰ������ʱЧ�ʡ�
	//��Ϊ�����Ĳ������ǻ������ͣ����ں˻�����֧��std::forward�����ֱ�ӵ��ö���ʹ��forward������ת����
	template <const char* Name, typename... Args>
	constexpr inline sfresult Lock(Args... args)
	{
		KIRQL irql;
		constexpr size_t hash = ConstexprHash(Name);
		KeAcquireSpinLock(&lHead, &irql);

		sfresult r;
		if constexpr (hash == ConstexprHash("add"))
		{
			r = add(args...);
		}
		else if constexpr (hash == ConstexprHash("check"))
		{
			r = check(args...);
		}
		else if constexpr (hash == ConstexprHash("del"))
		{
			r = del(args...);
		}
		else if constexpr (hash == ConstexprHash("isIn"))
		{
			r = isIn(args...);
		}
		else
		{
			r = R_ERROR_FUNC;
		}

		KeReleaseSpinLock(&lHead, irql);

		return r;
	}

	

};

//�����Ϸ���ģ�壨c++������ģ������Ͳ���ֱ��д�ַ�����������ֻ�ܶ��ⶨ��һ�£�
constexpr const char add_l[] = "add";
constexpr const char check_l[] = "check";
constexpr const char del_l[] = "del";
constexpr const char isIn_l[] = "isIn";
