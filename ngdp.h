#ifndef NGDP_H
#define NGDP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* malloc, free, realloc */
typedef void *(*ngdpMallocFn)(size_t size);
typedef void (*ngdpFreeFn)(void *ptr);
typedef void *(*ngdpReallocFn)(void *ptr, size_t size);

/* fopen, fseek, fread, fwrite, fclose */
typedef void *(*ngdpFileOpenFn)(const char *filename, const char *mode);
typedef int (*ngdpFileSeekFn)(void *stream, long offset, int origin);
typedef size_t (*ngdpFileReadFn)(void *buffer, size_t size, size_t count, void *stream);
typedef size_t (*ngdpFileWriteFn)(void *buffer, size_t size, size_t count, void *stream);
typedef int (*ngdpFileCloseFn)(void *stream);

/* Downloads data from url.  If 0 <= rangeStart < rangeEnd, will perform a range
 * request.  Downloaded data will be read in to buffer.  If buffer is null, will
 * perform a HEAD request.  The response's Content-Length will be written to
 * bufferSize.  If buffer is not null, but the pointer at buffer is null, memory
 * will be allocated from the heap and stored at buffer.  This memory should be
 * freed with the configured freeFn (or stdlib free if that was null).
 *
 * When a range request is made, the range in the HTTP header will use rangeEnd
 * minus one as the end of the range, since HTTP ranges are inclusive.
 *
 * Returns zero on success, non-zero on failure.  Timeouts and 5xx
 * status codes return 1, indicating the download may be retried; 4xx status
 * codes should not return 1.
 */
typedef int (*ngdpDownloadUrlFn)(const char *url, int rangeStart, int rangeEnd, uint8_t **buffer, int *bufferSize);

#define NGDP_DOWNLOAD_SUCCESS (0)
#define NGDP_DOWNLOAD_SERVER_ERROR (1)
#define NGDP_DOWNLOAD_400_ERROR (2)
#define NGDP_DOWNLOAD_BUFFER_TOO_SMALL (3)

typedef void (*ngdpDebugLogFn)(const char *message);

/* arg0 = CDN host index (-1 if it's the patch server)
 * arg1 = expected body size
 */
#define NGDP_STATISTIC_DOWNLOAD_STARTED (1)

/* arg0 = CDN host index
 * arg1 = bytes downloaded (HTTP body size)
 * arg2 = download time in microseconds (including retries)
 */
#define NGDP_STATISTIC_DOWNLOAD_FINISHED (2)

/* arg0 = CDN host index
 * arg1 = current download time elapsed in microseconds
 * arg2 = number of requests so far
 */
#define NGDP_STATISTIC_DOWNLOAD_RETRY (3)

/* arg0 = patch size in bytes
 * arg1 = target size in bytes
 */
#define NGDP_STATISTIC_PATCHING (4)

/* arg0 = CASC archive index
 * arg1 = file archive offset
 * arg2 = read size
 */
#define NGDP_STATISTIC_CASC_READ_STARTED (5)

/* arg0 = CASC archive index
 * arg1 = bytes read
 * arg2 = read time in microseconds
 */
#define NGDP_STATISTIC_CASC_READ_FINISHED (6)

/* Reports a statistic event.  Useful for showing the user progress.
 *   type: one of the NGDP_STATISTIC constants
 *   args: depends on the type
 *   key: content key associated with the operation, if applicable
 */
typedef void (*ngdpStatisticsFn)(int type, int arg0, int arg1, int arg2, const uint8_t *key);

/* This struct should be zero-initialized. */
typedef struct ngdpConfig {
	const char *ngdpUrl;
	const char *ngdpRegion;
	const char *gameUid;
	const char *cascPath;

	/* callbacks -- if they are null, the stdlib/curl versions will be used
	 * instead.  All memory functions must be set if any are to be set, and all
	 * file functions must be set if any are to be set.
	 * If memory callbacks are specified but downloadUrlFn is null, note that
	 * the default implementation (curl) will still use stdlib malloc/free.
	 */
	ngdpMallocFn mallocFn;
	ngdpFreeFn freeFn;
	ngdpReallocFn reallocFn;
	ngdpFileOpenFn fopenFn;
	ngdpFileSeekFn fseekFn;
	ngdpFileReadFn freadFn;
	ngdpFileWriteFn fwriteFn;
	ngdpFileCloseFn fcloseFn;
	ngdpDownloadUrlFn downloadUrlFn;
	/* if null, no debug logging will occur */
	ngdpDebugLogFn logFn;
	/* if null, no statistics will be reported */
	ngdpStatisticsFn statsFn;

	uint8_t disableHTTPRequests;
	uint8_t overrideBuildConfig;
	uint8_t overrideCDNConfig;
	uint8_t overrideCDNs;
	/* Used if disableHTTPRequests or overrideBuildConfig is set: */
	uint8_t buildConfigKey[16];
	/* Used if disableHTTPRequests or overrideCDNConfig is set: */
	uint8_t cdnConfigKey[16];
	/* Used if overrideCDNs is set: */
	const char *cdnPath;
	const char *cdnHosts;
	const char *cdnConfigPath;

	/* If the server returns a 5xx or times out, retry this number of times */
	int httpRetryCount;

	/* An error message to supplement the error code */
	const char *errorDetail;

	/* One of the NGDP_ERROR constants */
	int error;
} ngdpConfig;

typedef void ngdpClient;

#define NGDP_ERROR_SUCCESS (0)
#define NGDP_ERROR_INVALID_CONFIGURATION (1)
#define NGDP_ERROR_FILE_NOT_FOUND (2)
#define NGDP_ERROR_WORKING_BUFFER_TOO_SMALL (3)
#define NGDP_ERROR_HTTP_TIMEOUT (4)
#define NGDP_ERROR_HTTP_SERVER_ERROR (5)

/* Allocates and initializes a new ngdp client according to config.  If an error
 * occurs during initialization, this will return null and set config->error.
 */
ngdpClient *ngdpInit(ngdpConfig *config);

/* This struct is used for all ngdp operations; it should be zero-initialized.
 */
typedef struct ngdpOperation {
	uint8_t contentKey[16];

	/* Read/write data */
	uint8_t *buffer;
	int bufferSize;
	int fileOffset;

	/* Used as a temporary buffer for decoding and streaming. */
	uint8_t *workingBuffer;
	int workingBufferSize;

	/* Total file size */
	int fileSize;

	/* How the file should be encoded */
	const char *encodingSpec;

	/* Required working buffer size maximum */
	int workingBufferRequiredSize;

	/* Required working buffer size for an operation without saved state,
	 * i.e. when bufferSize >= fileSize
	 */
	int workingBufferRequiredSizeWithoutState;

	/* Used to keep track of what state the operation is currently in -- this
	 * should be set to 0 for new operations; it should also be reset to 0 if
	 * the contents of workingBuffer are modified.
	 */
	int state;

	/* Error code for the last operation */
	int error;

	/* Used by ngdpSave for patch creation when creating new files
	 * size = patchSourceContentKeyCount * 16
	 */
	uint8_t *patchSourceContentKeys;
	int patchSourceContentKeyCount;

	/* dataIsLocal is non-zero if the data exists in local archives.
	 * localArchiveIndex is the index of the archive (i.e. data.001 = 1) if the
	 * data is local.
	 */
	uint8_t dataIsLocal;
	uint8_t localArchiveIndex;

	/* disableFileWrites avoids writing to the local CASC installation when new
	 * data is downloaded.
	 */
	uint8_t disableFileWrites;

	uint8_t encodedKeyIsValid;
	uint8_t encodedKey[16];
	int encodedSize;

	/* localArchiveFileOffset is the offset of the file data in the archive.
	 * This offset is after the 30-byte file record header, i.e. it points
	 * directly to (encoded) file data.
	 */
	int localArchiveFileOffset;
} ngdpOperation;

/* FileInfo looks up contentKey in encoding and sets fields in op accordingly:
 *   fileSize, workingBufferRequiredSize, workingBufferRequiredSizeWithoutState,
 *   encodedKeyIsValid, encodedKey, encodingSpec, encodedSize
 */
int ngdpFileInfo(ngdpClient *c, ngdpOperation *op);

/* IsLocal looks up the file by encodedKey in the local CASC index and sets
 * dataIsLocal, localArchiveIndex, and localArchiveFileOffset.  If the file does
 * not exist in the CASC index, dataIsLocal will be 0.
 */
int ngdpIsLocal(ngdpClient *c, ngdpOperation *op);

/* Read reads data from the file to op->buffer.
 */
int ngdpRead(ngdpClient *c, ngdpOperation *op);

/* Create creates a new file which can be written to.  When creating a new file,
 * the required size of workingBuffer depends on the encodingSpec; if the
 * workingBuffer is too small, Create will return an error.  The workingBuffer
 * must be retained for future writing and saving of this file.
 */
int ngdpCreate(ngdpClient *c, ngdpOperation *op);
int ngdpWrite(ngdpClient *c, ngdpOperation *op);
int ngdpSave(ngdpClient *c, ngdpOperation *op);

#ifdef __cplusplus
}
#endif

#endif
