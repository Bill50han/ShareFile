#pragma once

#include <ntifs.h>

void* operator new(size_t size);

void operator delete(void* p);


enum sfresult : ULONG
{
	R_OK,
	R_FIND_PATH = R_OK,

	R_UNSUCCESS,

	R_BAD_ALLOC,
	R_NOT_FIND_PATH,
	R_ERROR_FUNC,

	R_INVALID_LENGTH,

	R_NO_CONNECTION
};
using sferror = sfresult;
