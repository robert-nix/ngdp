#pragma once

#include "std.h"
#include "Strings.h"

namespace ngdp {

struct Key {
	uint8_t k[16];

	// Assigns key by decoding a 32-character hex-encoded String
	void InitFromHexString(const String &s);

	// Writes 00/00/00000000000000000000000000000000
	void WriteURLFragment(Heap *h, StringBuffer &sb) const {
		sb.AppendHexByte(h, k[0]);
		sb.AppendChar(h, '/');
		sb.AppendHexByte(h, k[1]);
		sb.AppendChar(h, '/');
		for (int i = 0; i < 16; i++) {
			sb.AppendHexByte(h, k[i]);
		}
	}
};

}
