#pragma once

#include "std.h"
#include "ngdp.h"
#include "Heap.h"
#include "FileIO.h"
#include "Remote.h"

namespace ngdp {

struct Client {
	Heap m_heap;
	FileIO m_file;
	ngdpDownloadUrlFn m_download;

	ngdpDebugLogFn m_log;
	char *m_logBuffer;
	static const int kDebugLogBufferSize = 64 * 1024;

	ngdpStatisticsFn m_stats;

	Remote m_remote;

	void Init(ngdpConfig *config);

	void Log(const char *fmt, ...);
	void Report(int type, int arg0, int arg1, int arg2, const Key *key);
};

}
