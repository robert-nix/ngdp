#pragma once

#include "std.h"
#include "ngdp.h"

namespace ngdp {

struct Heap {
	ngdpMallocFn m_malloc;
	ngdpFreeFn m_free;
	ngdpReallocFn m_realloc;

	void *Alloc(size_t size) {
		return m_malloc(size);
	}

	void Free(void *ptr) {
		m_free(ptr);
	}

	void *Realloc(void *ptr, size_t size) {
		return m_realloc(ptr, size);
	}
};

}
