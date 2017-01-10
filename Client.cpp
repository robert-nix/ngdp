#include "Client.h"
#include "Buffer.h"

#include <curl/curl.h>

namespace ngdp {

static thread_local Client *threadCurrentClient;

struct ScopedCurrentClient {
	ScopedCurrentClient(Client *c) {
		threadCurrentClient = c;
	}

	~ScopedCurrentClient() {
		threadCurrentClient = nullptr;
	}
};

struct DownloadContext {
	Heap *m_heap;
	Buffer<u8> m_buffer;
	bool m_bufferTooSmall;
};

static size_t DownloadUrlWriteCallback(char *buf, size_t size, size_t nmemb, void *ctx) {
	DownloadContext *d = (DownloadContext *)ctx;
	int downloadSize = (int)(size * nmemb);
	if (d->m_heap) {
		u8 *dst = d->m_buffer.Alloc(d->m_heap, downloadSize);
		memcpy(dst, buf, downloadSize);
	} else {
		if (d->m_buffer.m_size + downloadSize > d->m_buffer.m_capacity) {
			d->m_bufferTooSmall = true;
			downloadSize = d->m_buffer.m_capacity - d->m_buffer.m_size;
		}
		if (downloadSize > 0) {
			memcpy(d->m_buffer.m_storage + d->m_buffer.m_size, buf, downloadSize);
			d->m_buffer.m_size += downloadSize;
		}
	}
	return size * nmemb;
}

static int DownloadUrl(const char *url, int rangeStart, int rangeEnd, u8 **buffer, int *bufferSize) {
	DownloadContext ctx;
	ctx.m_heap = nullptr;
	ctx.m_buffer.Init();
	ctx.m_bufferTooSmall = false;

	Client *client = threadCurrentClient;
	client->Log("Downloading from %s [%d, %d)", url, rangeStart, rangeEnd);

	if (buffer) {
		if (*buffer) {
			ctx.m_buffer.m_storage = *buffer;
			ctx.m_buffer.m_capacity = *bufferSize;
		} else {
			ctx.m_heap = &client->m_heap;
		}
	}

	CURL *req = curl_easy_init();
	curl_easy_setopt(req, CURLOPT_URL, url);
	curl_easy_setopt(req, CURLOPT_WRITEFUNCTION, DownloadUrlWriteCallback);
	curl_easy_setopt(req, CURLOPT_WRITEDATA, &ctx);
	if (!buffer) {
		curl_easy_setopt(req, CURLOPT_NOBODY, 1);
	}
	if (rangeEnd > rangeStart && rangeEnd > 0) {
		char range[32];
		sprintf_s(range, 32, "%d-%d", rangeStart, rangeEnd - 1);
		curl_easy_setopt(req, CURLOPT_RANGE, range);
	}
	CURLcode res = curl_easy_perform(req);
	if (res != CURLE_OK) {
		// Usually a connection failure:
		curl_easy_cleanup(req);
		return NGDP_DOWNLOAD_SERVER_ERROR;
	}
	int status = 0;
	curl_easy_getinfo(req, CURLINFO_RESPONSE_CODE, &status);

	double contentLength;
	curl_easy_getinfo(req, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &contentLength);
	if (contentLength >= 0) {
		*bufferSize = (int)contentLength;
	} else if (ctx.m_buffer.m_size > 0) {
		*bufferSize = ctx.m_buffer.m_size;
	} else {
		*bufferSize = 0;
	}

	if (buffer && !*buffer) {
		*buffer = ctx.m_buffer.m_storage;
		*bufferSize = ctx.m_buffer.m_size;
	}

	curl_easy_cleanup(req);
	if (status >= 500) {
		return NGDP_DOWNLOAD_SERVER_ERROR;
	}
	if (status >= 400) {
		return NGDP_DOWNLOAD_400_ERROR;
	}
	if (ctx.m_bufferTooSmall) {
		return NGDP_DOWNLOAD_BUFFER_TOO_SMALL;
	}
	return 0;
}

void Client::Init(ngdpConfig *config) {
	ScopedCurrentClient _c(this);

	bool useFileCallbacks = config->fopenFn || config->fseekFn || config->freadFn || config->fwriteFn || config->fcloseFn;
	if (useFileCallbacks) {
		if (!(config->fopenFn && config->fseekFn && config->freadFn && config->fwriteFn && config->fcloseFn)) {
			config->errorDetail = "All file callbacks must be set if any are set.";
			config->error = NGDP_ERROR_INVALID_CONFIGURATION;
			return;
		}
		m_file.m_fopen = config->fopenFn;
		m_file.m_fseek = config->fseekFn;
		m_file.m_fread = config->freadFn;
		m_file.m_fwrite = config->fwriteFn;
		m_file.m_fclose = config->fcloseFn;
	} else {
		m_file.m_fopen = (ngdpFileOpenFn)fopen;
		m_file.m_fseek = (ngdpFileSeekFn)fseek;
		m_file.m_fread = (ngdpFileReadFn)fread;
		m_file.m_fwrite = (ngdpFileWriteFn)fwrite;
		m_file.m_fclose = (ngdpFileCloseFn)fclose;
	}
	m_log = config->logFn;
	if (m_log) {
		m_logBuffer = (char *)m_heap.Alloc(kDebugLogBufferSize);
	} else {
		m_logBuffer = nullptr;
	}
	m_stats = config->statsFn;
	m_download = config->downloadUrlFn;
	if (config->disableHTTPRequests) {
		m_download = nullptr;
	} else if (!m_download) {
		curl_global_init(CURL_GLOBAL_ALL);
		m_download = DownloadUrl;
	}

	m_remote.Init(this, config->httpRetryCount, config->ngdpUrl, config->gameUid, config->ngdpRegion);
}

void Client::Log(const char *fmt, ...) {
	if (!m_log) {
		return;
	}
	// TODO: mutex
	va_list args;
	va_start(args, fmt);
	int w = vsnprintf(m_logBuffer, kDebugLogBufferSize, fmt, args);
	va_end(args);
	if (w == -1 || w >= kDebugLogBufferSize) {
		w = kDebugLogBufferSize - 1;
	}
	m_logBuffer[w] = 0;
	m_log(m_logBuffer);
}

void Client::Report(int type, int arg0, int arg1, int arg2, const Key *key) {
	if (!m_stats) {
		return;
	}
	m_stats(type, arg0, arg1, arg2, key->k);
}

}

extern "C" ngdpClient *ngdpInit(ngdpConfig *config) {
	bool useMemoryCallbacks = config->mallocFn || config->freeFn || config->reallocFn;
	if (useMemoryCallbacks && !(config->mallocFn && config->freeFn && config->reallocFn)) {
		config->errorDetail = "All memory callbacks must be set if any are set.";
		config->error = NGDP_ERROR_INVALID_CONFIGURATION;
		return 0;
	}
	ngdp::Heap heap;
	if (useMemoryCallbacks) {
		heap.m_malloc = config->mallocFn;
		heap.m_free = config->freeFn;
		heap.m_realloc = config->reallocFn;
	} else {
		heap.m_malloc = malloc;
		heap.m_free = free;
		heap.m_realloc = realloc;
	}
	ngdp::Client *client = (ngdp::Client *)heap.Alloc(sizeof(ngdp::Client));
	memset(client, 0, sizeof(*client));
	client->m_heap = heap;
	config->error = 0;
	client->Init(config);
	if (config->error) {
		heap.Free(client);
		return 0;
	}
	return (ngdpClient *)client;
}
