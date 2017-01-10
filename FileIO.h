#pragma once

#include "std.h"
#include "ngdp.h"

namespace ngdp {

struct FileIO {
	ngdpFileOpenFn m_fopen;
	ngdpFileSeekFn m_fseek;
	ngdpFileReadFn m_fread;
	ngdpFileWriteFn m_fwrite;
	ngdpFileCloseFn m_fclose;

	void *Open(const char *filename, const char *mode) {
		return m_fopen(filename, mode);
	}

	int Seek(void *stream, long offset, int origin) {
		return m_fseek(stream, offset, origin);
	}

	size_t Read(void *buffer, size_t size, size_t count, void *stream) {
		return m_fread(buffer, size, count, stream);
	}

	size_t Write(void *buffer, size_t size, size_t count, void *stream) {
		return m_fwrite(buffer, size, count, stream);
	}

	int Close(void *stream) {
		return m_fclose(stream);
	}
};

}
