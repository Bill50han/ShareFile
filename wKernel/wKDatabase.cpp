#include "wKutilities.h"
#include "wKDatabase.h"
#include <immintrin.h>

Database Database::instance;

void Database::Init()
{
	DbgPrint("Database head: %p\n", &head);
	//KeInitializeSpinLock(&lHead);
}

sfresult Database::add(const wchar_t* path, size_t size, size_t hash)
{
	PathList* last = &head;
	while (last->next)
	{
		last = last->next;
		if (last->hash == hash)
		{
			return R_OK;
		}
	}

	PathList* now = (PathList*)ExAllocatePoolUninitialized(NonPagedPool, size + sizeof(L'\0') + sizeof(PathList) + CmpMemorySize, 'PL');
	if (now == NULL)
	{
		return R_BAD_ALLOC;
	}
	last->next = now;
	now->pre = last;
	now->next = nullptr;
	now->size = size + sizeof(L'\0') + sizeof(PathList);
	now->hash = hash;

	wcscpy(now->path, path);

	return R_OK;
}


sfresult Database::check(const wchar_t* path, size_t size)
{
	PathList* now = head.next;
	while (now)
	{
		if (cmp(now->path, now->size - sizeof(PathList) - sizeof(L'\0'), path, size))
		{
			return R_FIND_PATH;
		}
		now = now->next;
	}

	return R_NOT_FIND_PATH;
}

sfresult Database::del(size_t hash)
{
	PathList* now = head.next;
	while (now)
	{
		if (now->hash == hash)
		{
			now->pre->next = now->next;
			now->next->pre = now->pre;
			ExFreePoolWithTag(now, 'PL');
			return R_OK;
		}
		now = now->next;
	}

	return R_NOT_FIND_PATH;
}

sfresult Database::isIn(size_t hash)
{
	PathList* now = head.next;
	while (now)
	{
		if (now->hash == hash)
		{
			return R_FIND_PATH;
		}
		now = now->next;
	}

	return R_NOT_FIND_PATH;
}

bool CmpTrivial(wchar_t* a, size_t asize, const wchar_t* b, size_t bsize)
{
	UNICODE_STRING ua;
	ua.Buffer = a;
	ua.Length = (USHORT)asize;
	ua.MaximumLength = (USHORT)(asize + sizeof(wchar_t));

	UNICODE_STRING ub;
	ub.Buffer = (PWCH)b;
	ub.Length = (USHORT)bsize;
	ub.MaximumLength = (USHORT)(bsize + sizeof(wchar_t));

	return !RtlCompareUnicodeString(&ua, &ub, TRUE);
}

bool CmpPrefix(wchar_t* a, size_t asize, const wchar_t* b, size_t bsize)
{
	if (bsize < asize) return false;

	UNICODE_STRING ua;
	ua.Buffer = a;
	ua.Length = (USHORT)asize;
	ua.MaximumLength = (USHORT)(asize + sizeof(wchar_t));

	UNICODE_STRING ub;
	ub.Buffer = (PWCH)b;
	ub.Length = (USHORT)asize;
	ub.MaximumLength = (USHORT)(bsize + sizeof(wchar_t));

	return !RtlCompareUnicodeString(&ua, &ub, TRUE);
}

bool CmpAvx1Prefix(wchar_t* a, size_t asize, const wchar_t* b, size_t bsize)
{
	if (bsize < asize) return false;
	if (bsize == 0) return false;

	const size_t stride = sizeof(__m256i) / sizeof(wchar_t);
	const size_t avx_chunk = stride;
	size_t i = 0;

	for (; i + avx_chunk <= asize; i += avx_chunk)
	{
		__m256i vec1 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(a + i));
		__m256i vec2 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(b + i));

		__m256i cmp = _mm256_cmpeq_epi16(vec1, vec2);

		int mask = _mm256_movemask_epi8(cmp);

		if (mask != 0xFFFFFFFF)
		{
			return false;
		}
	}

	for (; i < asize; i++)
	{
		if (a[i] != b[i])
		{
			return false;
		}
	}

	return true;
}

bool CmpAnotherAvx1Prefix(wchar_t* a, size_t asize, const wchar_t* b, size_t bsize)
{
	if (bsize < asize) return false;
	if (bsize == 0) return false;

	if (bsize < asize + 32) return CmpAvx1Prefix(a, asize, b, bsize);

	__m256i data = _mm256_loadu_si256(reinterpret_cast<const __m256i*>((char*)b+asize));
	_mm256_storeu_si256(reinterpret_cast<__m256i*>(a), data);

	const size_t stride = sizeof(__m256i) / sizeof(wchar_t);
	const size_t avx_chunk = stride;
	size_t i = 0;

	for (; i + avx_chunk <= asize + 32; i += avx_chunk)
	{
		__m256i vec1 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(a + i));
		__m256i vec2 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(b + i));

		__m256i cmp = _mm256_cmpeq_epi16(vec1, vec2);

		int mask = _mm256_movemask_epi8(cmp);

		if (mask != 0xFFFFFFFF)
		{
			return false;
		}
	}

	a[asize >> 1] = L'0';

	return true;
}
