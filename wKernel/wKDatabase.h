#pragma once
#include "wKutilities.h"
#include <fltKernel.h>
#include <ntifs.h>
#include <ntddk.h>
#include <ntstrsafe.h>
#include <dontuse.h>

//约定：本项目内所有size单位都是字节；如果这个size是字符串的大小，那么不包含末尾的0（即与UNICODE_STRING的定义完全相同）。所有裸指针形态的字符串末尾均应为0。

//这是一簇比较函数的集合，实际运行中只会有其中一个被实际使用。
bool CmpTrivial(wchar_t*, size_t, const wchar_t*, size_t);	//使用自带的RtlCompareUnicodeString，无需为PathList分配额外空间
bool CmpPrefix(wchar_t*, size_t, const wchar_t*, size_t);	//RtlCompareUnicodeString，但比较长度均为库中字符串长度，因此可以做到通用前缀匹配
bool CmpAvx1Prefix(wchar_t* a, size_t asize, const wchar_t* b, size_t bsize);	//显式avx1比较，且通用前缀匹配
bool CmpAnotherAvx1Prefix(wchar_t* a, size_t asize, const wchar_t* b, size_t bsize);	//一个空间换时间的特殊算法，需要CmpMemorySize=32，通用前缀匹配

class Database
{
private:
	Database() = default;		//使用单例模式，将构造函数设为私有，禁止外部建立类的新的实例。
	static Database instance;

#pragma pack(push)
#pragma pack(1)		//强制让PathList以1字节对齐，方便计算分配的内存大小。不能使用c++自带的alignas，因为1小于结构体中最大的元素大小8。
	struct PathList
	{
		size_t size;
		PathList* pre;
		PathList* next;

		size_t hash;
		wchar_t path[1];	//msvc不完全支持gcc的这个扩展，因此大小填为1，防止内核环境编译器将 “warning C4200: 使用了非标准扩展: 结构/联合中的零大小数组” 视为错误阻止编译。
	};
#pragma pack(pop)

	PathList head = { sizeof(PathList),nullptr,nullptr,0,L'\0' };
	KSPIN_LOCK lHead = NULL;

	using CompareFunction = bool(*)(wchar_t*, size_t, const wchar_t*, size_t);
	CompareFunction cmp = CmpTrivial;
	size_t CmpMemorySize = 0;	//有一些比较函数需要额外的空间，在这里记录

	//一个编译期的字符串hash算法，用于辅助Lock函数做编译期转发
	constexpr static size_t ConstexprHash(const char* str, size_t hash = 2166136261)
	{
		return *str ? ConstexprHash(str + 1, (hash ^ (size_t)(*str)) * 16777619) : hash;
	}

public:
	static Database& GetInstance()
	{
		return instance;
	}
	// 禁止拷贝和赋值
	Database(const Database&) = delete;
	Database& operator=(const Database&) = delete;

	sfresult add(const wchar_t* path, size_t size, size_t hash);
	sfresult check(const wchar_t* path, size_t size);
	sfresult del(size_t hash);
	sfresult isIn(size_t hash);

	void Init();

	//这个模板函数为上面四个操作函数添加了自旋锁保护，为了在KComm里多线程使用。
	//所有操作均为编译期完成，防止影响运行时效率。
	//因为函数的参数都是基本类型，且内核环境不支持std::forward，因此直接调用而不使用forward做完美转发。
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

//辅助上方的模板（c++不允许模板非类型参数直接写字符串字面量，只能额外定义一下）
constexpr const char add_l[] = "add";
constexpr const char check_l[] = "check";
constexpr const char del_l[] = "del";
constexpr const char isIn_l[] = "isIn";
