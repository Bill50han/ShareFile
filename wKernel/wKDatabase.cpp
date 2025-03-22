#include "wKutilities.h"
#include "wKDatabase.h"

Database Database::instance;

void Database::Init()
{
	KeInitializeSpinLock(&lHead);
}

sfresult Database::add(const wchar_t* path, size_t size, size_t hash)
{
	PathList* last = &head;
	while (last->next)
	{
		last = last->next;
	}

	PathList* now = (PathList*)ExAllocatePoolUninitialized(NonPagedPool, size + sizeof(PathList) + CmpMemorySize, 'PL');
	if (now == NULL)
	{
		return R_BAD_ALLOC;
	}
	last->next = now;
	now->pre = last;
	now->next = nullptr;
	now->size = size + sizeof(PathList);
	now->hash = hash;
	
	wcscpy(now->path, path);

	return R_OK;
}


sfresult Database::check(const wchar_t* path, size_t size)
{
	PathList* now = head.next;
	while (now)
	{
		if (cmp(now->path, now->size - sizeof(PathList), path, size))
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
