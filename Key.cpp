#include "Key.h"

namespace ngdp {

static u8 decodeHex(u8 c) {
	if (c >= '0' && c <= '9') {
		return c - '0';
	}
	if (c >= 'a' && c <= 'f') {
		return c - 'a' + 10;
	}
	if (c >= 'A' && c <= 'F') {
		return c - 'A' + 10;
	}
	return 0;
}

void Key::InitFromHexString(const String &s) {
	assert(s.m_size == 32);
	for (int i = 0; i < 16; i++) {
		u8 hi = s[2 * i];
		u8 lo = s[2 * i + 1];
		k[i] = (decodeHex(hi) << 4) | (decodeHex(lo));
	}
}

}

namespace natvis {
	struct x4lo { unsigned __int8 v : 4; unsigned __int8 _ : 4; };
	struct x4hi { unsigned __int8 _ : 4; unsigned __int8 v : 4; };
	struct x8 { unsigned __int8 _; };
	struct x32 { __int32 _; };
}
