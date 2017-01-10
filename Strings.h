#pragma once

#include "std.h"
#include "Buffer.h"
#include "Heap.h"

namespace ngdp {

// A String is a view of UTF-8 bytes.  It does not own the memory it points to.
// A String's size is its length in bytes; there is no notion of UTF-8 codepoints
// in its length or its indexing.
struct String : public Slice<u8> {
	String() = default;
	String(const Slice<u8> slice) {
		m_data = slice.m_data;
		m_size = slice.m_size;
	}

	String(const char *cstr) {
		m_data = (u8 *)cstr;
		m_size = strlen(cstr);
	}

	bool operator==(const String &rhs) const {
		if (m_size != rhs.m_size) return false;
		if (m_data == rhs.m_data) return true;
		return 0 == memcmp(m_data, rhs.m_data, m_size);
	}

	bool operator==(const char *cstr) const {
		return *this == String(cstr);
	}

	String Substring(int start, int end) const {
		return Reslice(start, end);
	}

	String Substring(int start) const {
		return Reslice(start);
	}

	int Index(const String &sep) const {
		if (sep.m_size == 0) {
			return 0;
		} else if (sep.m_size == 1) {
			return IndexByte(sep[0]);
		} else if (sep.m_size == m_size) {
			if (*this == sep) {
				return 0;
			}
			return -1;
		} else if (sep.m_size > m_size) {
			return -1;
		}
		// TODO: rabin-karp
		int last = m_size - sep.m_size;
		for (int i = 0; i < last; i++) {
			if (Substring(i, i + sep.m_size) == sep) {
				return i;
			}
		}
		return -1;
	}

	int IndexByte(u8 b) const {
		for (int i = 0; i < m_size; i++) {
			if (m_data[i] == b) {
				return i;
			}
		}
		return -1;
	}

	int Count(const String &sep) const {
		int n = 0;
		if (sep.m_size == 0) {
			// UTF-8 rune count
			assert(!"nyi");
		} else if (sep.m_size == 1) {
			u8 c = sep[0];
			for (int i = 0; i < m_size; i++) {
				if (m_data[i] == c) {
					n++;
				}
			}
			return n;
		} else if (sep.m_size > m_size) {
			return 0;
		} else if (sep.m_size == m_size) {
			return *this == sep ? 1 : 0;
		}
		// TODO: rabin-karp
		int last = m_size - sep.m_size;
		for (int i = 0; i < last; i++) {
			if (Substring(i, i + sep.m_size) == sep) {
				n++;
			}
		}
		return n;
	}

	// Parse the string as a signed 32-bit integer.
	// For certain bases, this can skip a prefix like '0x' or '0b'.
	// If the number overflows s32, the parse will return the signed minimum
	// or maximum value of s32, according to the sign.
	// If the string contains a character outside the valid range, the parse
	// will stop at that character.
	int ParseInt(u8 base = 10) const {
		u32 uv = 0;
		bool neg = false;
		if (m_size > 1 && m_data[0] == '-') {
			neg = true;
			uv = Substring(1).ParseUint(base);
		} else if (m_size > 1 && m_data[0] == '+') {
			uv = Substring(1).ParseUint(base);
		} else {
			uv = ParseUint(base);
		}
		if (uv > 0x7fffffff) {
			if (neg) return (int)0x80000000;
			return 0x7fffffff;
		}
		if (neg) return -((int)uv);
		return uv;
	}

	// Parse the string as an unsigned 32-bit integer.
	// For certain bases, this can skip a prefix like '0x' or '0b'.
	// If the number overflows u32, the parse will return 2^32 - 1.
	// If the string contains a character outside the valid range, the parse
	// will stop at that character.
	u32 ParseUint(u8 base = 10) const {
		assert(base >= 2 && base <= 36);
		if (m_size == 0) {
			return 0;
		}
		u32 n = 0;
		int i = 0;
		if (m_size > i + 2) {
			if (base == 2 && m_data[0] == '0' && (m_data[1] == 'b' || m_data[1] == 'B')) {
				i += 2;
			}
			if (base == 16 && m_data[0] == '0' && (m_data[1] == 'x' || m_data[1] == 'X')) {
				i += 2;
			}
		}
		for (; i < m_size; i++) {
			u8 v = 0;
			u8 d = m_data[i];
			if (d >= '0' && d <= '9') {
				v = d - '0';
			} else if (d >= 'a' && d <= 'z') {
				v = d - 'a' + 10;
			} else if (d >= 'A' && d <= 'Z') {
				v = d - 'A' + 10;
			} else {
				break;
			}
			if (v >= base) {
				break;
			}
			u32 next = n * (u32)base + (u32)v;
			if (next < n) {
				return 0xffffffff;
			}
			n = next;
		}
		return n;
	}

	String Trim() const {
		String res = *this;
		while (res.m_size > 0 && isspace(res[0])) {
			res.m_data++;
			res.m_size--;
		}
		while (res.m_size > 0 && isspace(res[res.m_size - 1])) {
			res.m_size--;
		}
		return res;
	}

	// The returned buffer must be destroyed.
	Buffer<String> Split(Heap *h, const String &sep) const {
		if (sep.m_size == 0) {
			// Need a bit of UTF-8 stuff to do this.
			assert(!"nyi");
		}
		Buffer<String> res;
		// Actual number will be lower in case of overlapping sep instances;
		// count is just an upper bound
		int count = Count(sep) + 1;
		res.Init(h, count);
		Split(h, &res, sep);
		return res;
	}

	void Split(Heap *h, Buffer<String> *buf, const String &sep) const {
		int n = 0;
		int start = 0;
		u8 c = sep[0];
		for (int i = 0; i + sep.m_size <= m_size; i++) {
			if (m_data[i] == c && (sep.m_size == 1 || Substring(i, i + sep.m_size) == sep)) {
				buf->Push(h, Substring(start, i));
				n++;
				start = i + sep.m_size;
				i += sep.m_size - 1;
			}
		}
		buf->Push(h, Substring(start));
	}

	bool HasPrefix(const String &prefix) {
		return m_size >= prefix.m_size && Substring(0, prefix.m_size) == prefix;
	}
};

// StringBuffer is for writing incrementally.
struct StringBuffer {
	Buffer<u8> *m_buffer;

	void Init(Buffer<u8> *buf) {
		m_buffer = buf;
	}

	int Size() const {
		return m_buffer->m_size;
	}

	void AppendChar(Heap *h, char c) {
		m_buffer->Push(h, (u8)c);
	}

	void AppendString(Heap *h, const String &s) {
		u8 *dst = m_buffer->Alloc(h, s.m_size);
		memcpy(dst, s.m_data, s.m_size);
	}

	// Append a byte formatted as %02x
	void AppendHexByte(Heap *h,u8 b) {
		static const char hexChars[] = "0123456789abcdef";
		u8 hi = (b & 0xf0) >> 4;
		u8 lo = (b & 0xf);
		u8 *dst = m_buffer->Alloc(h, 2);
		dst[0] = hexChars[hi];
		dst[1] = hexChars[lo];
	}

	// Append the decimal representation of i
	void AppendInt(Heap *h,int i) {
		char buf[16];
		itoa(i, buf, 10);
		AppendString(h, buf);
	}

	void Start(BufferSegment *seg) const {
		seg->m_start = Size();
	}

	void End(BufferSegment *seg) const {
		seg->m_end = Size();
	}

	// Duplicates a string to the buffer and returns a segment with that string's
	// location in the buffer; useful for making a smaller string buffer from a
	// larger buffer.
	BufferSegment Duplicate(Heap *h, const String &s) {
		BufferSegment seg;
		Start(&seg);
		AppendString(h, s);
		End(&seg);
		AppendChar(h, '\0');
		return seg;
	}

	String MakeString(const BufferSegment &seg) {
		return m_buffer->MakeSlice(seg);
	}

	// Returns a pointer to a null-terminated string.  This is done without
	// copying by ensuring a null is at the end of the current Buffer.  This
	// means that any modification to the buffer will invalidate the result of
	// CString(); use something like strdup() to make the result last longer.
	const char *CString(Heap *h) {
		if (m_buffer->m_capacity > Size()) {
			m_buffer->m_storage[Size()] = 0;
		} else {
			m_buffer->Push(h, 0);
			m_buffer->Pop();
		}
		return (const char *)m_buffer->m_storage;
	}
};

}
