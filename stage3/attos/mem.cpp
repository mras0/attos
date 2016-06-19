#include "mem.h"

void* operator new(size_t, void* address) {
	return address;
}

void operator delete(void*, size_t) {
}
