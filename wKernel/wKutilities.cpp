#include "wKutilities.h"

void* operator new(size_t size) {
	return ExAllocatePoolUninitialized(NonPagedPool, size, 'SCPP');
}

void operator delete(void* p) {
	ExFreePoolWithTag(p, 'SCPP');
}
