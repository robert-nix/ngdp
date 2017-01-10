#pragma once

#include "std.h"
#include "ngdp.h"
#include "Buffer.h"
#include "Key.h"
#include <functional>

void CASInit();

namespace ngdp {

enum class CDNResourceType {
	Data,
	Config,
	Patch
};

struct Client;

struct Remote {
	String m_url;
	String m_uid;
	String m_region;

	String m_cdnPath;
	String m_cdnHosts[8];
	Key m_buildConfig;
	Key m_cdnConfig;
	String m_versionsName;

	Buffer<u8> m_cdnsResponse;
	Buffer<u8> m_versionsResponse;

	Client *m_client;
	int m_retryLimit;
	int m_cdnHostIndex;
	int m_nextCdnHostIndex;
	int m_cdnHostCount;
	int m_cdnTransferRates[8];

	void Init(Client *c, int retryLimit, const char *url, const char *uid, const char *region);
	void Destroy();

	// Parse pipe-separated value, assuming region is the first column, and
	// calling onData for all values in a row with region matching m_region
	void ParsePSV(const String &s, std::function<void (const String &key, const String &value)> onData);
	void ParseCDNs(const String &s);
	void ParseVersions(const String &s);

	int Download(Slice<u8> *slice, const char *url, int rangeStart = 0, int rangeEnd = 0);
	int Download(Slice<u8> *slice, CDNResourceType type, bool isIndex, const Key &key, int rangeStart = 0, int rangeEnd = 0);
	int DownloadAlloc(Buffer<u8> *buffer, const char *url);
	int DownloadAlloc(Buffer<u8> *buffer, CDNResourceType type, bool isIndex, const Key &key);

	const char *_MakeURL(Buffer<u8> *buf, CDNResourceType type, bool isIndex, const Key &key);
};

}

