#pragma once

#include "std.h"
#include "Heap.h"

namespace ngdp {

// A Slice is a view of memory.  A Slice's pointer is not owned by the Slice.
template <typename T>
struct Slice {
	T *m_data;
	int m_size;

	T *begin() {
		return m_data;
	}
	T *end() {
		return m_data + m_size;
	}
	const T *begin() const {
		return m_data;
	}
	const T *end() const {
		return m_data + m_size;
	}

	T &operator[](int index) {
		return m_data[index];
	}
	const T &operator[](int index) const {
		return m_data[index];
	}

	Slice Reslice(int start) const {
		return Reslice(start, m_size);
	}

	Slice Reslice(int start, int end) const {
		assert(start <= m_size);
		assert(end <= m_size);
		assert(end >= start);
		return Slice{m_data + start, end - start};
	}
};

// BufferSegment is used to make a future slice from a buffer, after the buffer
// stops relocating
struct BufferSegment {
	int m_start;
	int m_end;
};

// A Buffer is a dynamic array.
template <typename T>
struct Buffer {
	T *m_storage;
	int m_size;
	int m_capacity;

	T *begin() {
		return m_storage;
	}
	T *end() {
		return m_storage + m_size;
	}
	const T *begin() const {
		return m_storage;
	}
	const T *end() const {
		return m_storage + m_size;
	}

	T &operator[](int index) {
		return m_storage[index];
	}
	const T &operator[](int index) const {
		return m_storage[index];
	}

	void Init() {
		m_storage = 0;
		m_size = 0;
		m_capacity = 0;
	}

	void Init(Heap *h, int capacity) {
		m_storage = (T *)h->Alloc(capacity * sizeof(T));
		m_size = 0;
		m_capacity = capacity;
	}

	void Destroy(Heap *h) {
		if (m_capacity > 0) {
			h->Free(m_storage);
		}
	}

	T *Alloc(Heap *h, int count) {
		T *storage = m_storage;
		int oldSize = m_size;
		int capacity = m_capacity;
		bool wasStackStorage = capacity < 0;
		if (wasStackStorage) {
			capacity = -capacity;
		}
		if (oldSize + count > capacity) {
			int newCapacity = capacity ? ((capacity * 3) >> 1) : 8;
			if (newCapacity < oldSize + count) {
				newCapacity = oldSize + count;
			}
			if (wasStackStorage) {
				storage = (T *)h->Alloc(newCapacity * sizeof(T));
				memcpy(storage, m_storage, capacity * sizeof(T));
			} else {
				storage = (T *)h->Realloc(storage, newCapacity * sizeof(T));
			}
			m_storage = storage;
			m_capacity = newCapacity;
		}
		m_size = oldSize + count;
		return storage + oldSize;
	}

	T *AllocZero(Heap *h, int count) {
		T *result = Alloc(h, count);
		memset(result, 0, count * sizeof(T));
		return result;
	}

	void Append(Heap *h, const T *elems, int count) {
		T *dst = Alloc(h, count);
		memcpy(dst, elems, count * sizeof(T));
	}

	void Push(Heap *h, T elem) {
		T *dst = Alloc(h, 1);
		memcpy(dst, &elem, sizeof(T));
	}

	T *Pop() {
		int size = m_size;
		assert(size > 0);
		m_size = size - 1;
		return &m_storage[m_size];
	}

	void RemoveAt(int index) {
		int size = m_size;
		assert(size > index);
		m_size = size - 1;
		m_storage[index] = m_storage[m_size];
	}

	Slice<T> MakeSlice() const {
		return MakeSlice(0, m_size);
	}

	Slice<T> MakeSlice(int start) const {
		return MakeSlice(start, m_size);
	}

	Slice<T> MakeSlice(int start, int end) const {
		assert(start <= m_size);
		assert(end <= m_size);
		assert(end >= start);
		return Slice<T>{m_storage + start, end - start};
	}

	Slice<T> MakeSlice(const BufferSegment &seg) const {
		return MakeSlice(seg.m_start, seg.m_end);
	}
};

template <typename T, int StackSize>
struct StackBuffer : public Buffer<T> {
	T m_stackStorage[StackSize];

	void Init() {
		m_storage = m_stackStorage;
		m_size = 0;
		m_capacity = -StackSize;
	}
};

}
