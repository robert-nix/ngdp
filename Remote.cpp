#include "Remote.h"
#include "Key.h"
#include "Client.h"

#include <functional>
#include <chrono>

#include "Strings.h"

#define _heap &m_client->m_heap

namespace ngdp {

void Remote::Init(Client *c, int retryLimit, const char *url, const char *uid, const char *region) {
	memset(this, 0, sizeof(*this));
	m_url = url;
	m_uid = uid;
	m_region = region;
	m_retryLimit = retryLimit;
	m_client = c;
	if (m_retryLimit <= 0) {
		m_retryLimit = 5;
	}

	if (!c->m_download) {
		return;
	}

	StackBuffer<u8, 128> buf;
	buf.Init();
	StringBuffer sb;
	sb.Init(&buf);

	sb.AppendString(_heap, m_url);
	sb.AppendChar(_heap, '/');
	sb.AppendString(_heap, m_uid);
	int pathStart = buf.m_size;
	sb.AppendString(_heap, "/cdns");

	int res = DownloadAlloc(&m_cdnsResponse, sb.CString(_heap));
	if (!res) {
		ParseCDNs(m_cdnsResponse.MakeSlice());
	}

	m_cdnHostCount = 8;
	for (int i = 0; i < 8; i++) {
		if (!m_cdnHosts[i].m_data) {
			m_cdnHostCount = i;
			break;
		}
	}
	
	buf.m_size = pathStart;
	sb.AppendString(_heap, "/versions");

	res = DownloadAlloc(&m_versionsResponse, sb.CString(_heap));
	if (!res) {
		ParseVersions(m_versionsResponse.MakeSlice());
	}

	buf.Destroy(_heap);
}

void Remote::Destroy() {
	m_cdnsResponse.Destroy(_heap);
	m_versionsResponse.Destroy(_heap);
}

void Remote::ParsePSV(const String &s, std::function<void (const String &key, const String &value)> onData) {
	StackBuffer<String, 10> lines, keys, values;
	lines.Init();
	s.Split(_heap, &lines, "\n");
	bool haveKeys = false;
	keys.Init();
	for (auto &line : lines) {
		if (line.m_size == 0 || line[0] == '#') {
			continue;
		}
		if (!haveKeys) {
			line.Split(_heap, &keys, "|");
			for (int i = 0; i < keys.m_size; i++) {
				int nameStop = keys[i].IndexByte('!');
				keys[i] = keys[i].Substring(0, nameStop);
			}
			haveKeys = true;
		} else {
			if (line.HasPrefix(m_region)) {
				values.Init();
				line.Split(_heap, &values, "|");
				for (int i = 0; i < keys.m_size; i++) {
					if (values.m_size <= i) {
						break;
					}
					onData(keys[i], values[i]);
				}
				values.Destroy(_heap);
			}
		}
	}
	keys.Destroy(_heap);
	lines.Destroy(_heap);
}

void Remote::ParseCDNs(const String &s) {
	ParsePSV(s, [this](const String &key, const String &value) {
		if (key == "Path") {
			m_cdnPath = value;
		} else if (key == "Hosts") {
			StackBuffer<String, 8> cdnHosts;
			cdnHosts.Init();
			value.Split(_heap, &cdnHosts, " ");
			for (int i = 0; i < cdnHosts.m_size; i++) {
				if (i >= 8) {
					break;
				}
				m_cdnHosts[i] = cdnHosts[i];
			}
			cdnHosts.Destroy(_heap);
		}
	});
}

void Remote::ParseVersions(const String &s) {
	ParsePSV(s, [this](const String &key, const String &value) {
		if (key == "BuildConfig") {
			m_buildConfig.InitFromHexString(value);
		} else if (key == "CDNConfig") {
			m_cdnConfig.InitFromHexString(value);
		} else if (key == "VersionsName") {
			m_versionsName = value;
		}
	});
}

const char *Remote::_MakeURL(Buffer<u8> *buf, CDNResourceType type, bool isIndex, const Key &key) {
	StringBuffer sb;
	sb.Init(buf);
	int ofs = buf->m_size;

	int maxTransfer = 0;
	int bestIdx = m_nextCdnHostIndex;
	for (int i = 0; i < m_cdnHostIndex; i++) {
		if (m_cdnTransferRates[i] > maxTransfer) {
			bestIdx = i;
			maxTransfer = m_cdnTransferRates[i];
		}
	}
	m_cdnHostIndex = bestIdx;
	if (maxTransfer > 10) {
		m_cdnTransferRates[m_cdnHostIndex] = maxTransfer / 2;
	}

	sb.AppendString(_heap, "http://");
	sb.AppendString(_heap, m_cdnHosts[m_cdnHostIndex]);
	sb.AppendChar(_heap, '/');
	sb.AppendString(_heap, m_cdnPath);
	switch (type) {
	case CDNResourceType::Data:
		sb.AppendString(_heap, "/data/");
		break;
	case CDNResourceType::Config:
		sb.AppendString(_heap, "/config/");
		break;
	case CDNResourceType::Patch:
		sb.AppendString(_heap, "/patch/");
		break;
	default:
		sb.AppendChar(_heap, '/');
		break;
	}
	key.WriteURLFragment(_heap, sb);
	if (isIndex) {
		sb.AppendString(_heap, ".index");
	}
	sb.AppendChar(_heap, '\0');
	return (const char *)buf->m_storage + ofs;
}

// TODO: much of the below stuff is semi-braindead.  want to refactor without making unnecessary types

int Remote::DownloadAlloc(Buffer<u8> *buffer, const char *url) {
	assert(buffer && !buffer->m_storage);

	int res = NGDP_DOWNLOAD_SERVER_ERROR;
	int resSize = 0;
	auto overall_start = std::chrono::system_clock::now();
	for (int i = 0; i < m_retryLimit; i++) {
		if (i == 0) {
			m_client->Report(NGDP_STATISTIC_DOWNLOAD_STARTED, -1, 0, 0, 0);
		} else {
			int elapsed_us = (int)(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now() - overall_start).count());
			m_client->Report(NGDP_STATISTIC_DOWNLOAD_RETRY, -1, elapsed_us, i + 1, 0);
		}
		res = m_client->m_download(url, 0, 0, &buffer->m_storage, &buffer->m_size);
		resSize = buffer->m_size;
		if (res != NGDP_DOWNLOAD_SERVER_ERROR) {
			break;
		}
	}

	{
		int elapsed_us = (int)(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now() - overall_start).count());
		m_client->Report(NGDP_STATISTIC_DOWNLOAD_FINISHED, -1, resSize, elapsed_us, 0);
	}
	return res;
}

int Remote::DownloadAlloc(Buffer<u8> *buffer, CDNResourceType type, bool isIndex, const Key &key) {
	assert(buffer && !buffer->m_storage);

	StackBuffer<u8, 128> buf;
	int res = NGDP_DOWNLOAD_SERVER_ERROR;
	int resSize = 0;
	int idx = -1;
	auto overall_start = std::chrono::system_clock::now();
	for (int i = 0; i < m_retryLimit; i++) {
		buf.Init();
		// TODO: lb mutex
		const char *url = _MakeURL(&buf, type, isIndex, key);
		idx = m_cdnHostIndex;
		auto start_time = std::chrono::system_clock::now();
		if (i == 0) {
			m_client->Report(NGDP_STATISTIC_DOWNLOAD_STARTED, idx, 0, 0, 0);
		} else {
			int elapsed_us = (int)(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now() - overall_start).count());
			m_client->Report(NGDP_STATISTIC_DOWNLOAD_RETRY, idx, elapsed_us, i + 1, 0);
		}

		res = m_client->m_download(url, 0, 0, &buffer->m_storage, &buffer->m_size);
		buf.Destroy(_heap);

		auto end_time = std::chrono::system_clock::now();
		double dur_sec = std::chrono::duration<double>(end_time - start_time).count();
		resSize = buffer->m_size;
		if (res != NGDP_DOWNLOAD_SUCCESS) {
			resSize = 0;
		}
		int rate = (int)(resSize / dur_sec);
		m_cdnTransferRates[idx] = rate;
		m_nextCdnHostIndex = (idx + 1) % m_cdnHostCount;
		if (res != NGDP_DOWNLOAD_SERVER_ERROR) {
			break;
		}
	}

	{
		int elapsed_us = (int)(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now() - overall_start).count());
		m_client->Report(NGDP_STATISTIC_DOWNLOAD_FINISHED, idx, resSize, elapsed_us, 0);
	}
	return res;
}

int Remote::Download(Slice<u8> *slice, const char *url, int rangeStart, int rangeEnd) {
	assert(slice && slice->m_data && slice->m_size >= rangeEnd - rangeStart);

	int res = NGDP_DOWNLOAD_SERVER_ERROR;
	int resSize = 0;
	auto overall_start = std::chrono::system_clock::now();
	for (int i = 0; i < m_retryLimit; i++) {
		if (i == 0) {
			m_client->Report(NGDP_STATISTIC_DOWNLOAD_STARTED, -1, slice->m_size, 0, 0);
		} else {
			int elapsed_us = (int)(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now() - overall_start).count());
			m_client->Report(NGDP_STATISTIC_DOWNLOAD_RETRY, -1, elapsed_us, i + 1, 0);
		}

		int size = slice->m_size;
		res = m_client->m_download(url, rangeStart, rangeEnd, &slice->m_data, &size);
		if (size > slice->m_size) {
			res = NGDP_DOWNLOAD_BUFFER_TOO_SMALL;
		}
		if (res != NGDP_DOWNLOAD_SERVER_ERROR) {
			break;
		}
	}

	{
		int elapsed_us = (int)(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now() - overall_start).count());
		m_client->Report(NGDP_STATISTIC_DOWNLOAD_FINISHED, -1, resSize, elapsed_us, 0);
	}
	return res;
}

int Remote::Download(Slice<u8> *slice, CDNResourceType type, bool isIndex, const Key &key, int rangeStart, int rangeEnd) {
	assert(slice && slice->m_data && slice->m_size >= rangeEnd - rangeStart);

	StackBuffer<u8, 128> buf;
	int res = NGDP_DOWNLOAD_SERVER_ERROR;
	int resSize = 0;
	int idx = -1;
	auto overall_start = std::chrono::system_clock::now();
	for (int i = 0; i < m_retryLimit; i++) {
		buf.Init();
		// TODO: lb mutex
		const char *url = _MakeURL(&buf, type, isIndex, key);
		idx = m_cdnHostIndex;
		auto start_time = std::chrono::system_clock::now();
		if (i == 0) {
			m_client->Report(NGDP_STATISTIC_DOWNLOAD_STARTED, idx, slice->m_size, 0, 0);
		} else {
			int elapsed_us = (int)(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now() - overall_start).count());
			m_client->Report(NGDP_STATISTIC_DOWNLOAD_RETRY, idx, elapsed_us, i + 1, 0);
		}

		resSize = slice->m_size;
		res = m_client->m_download(url, rangeStart, rangeEnd, &slice->m_data, &resSize);
		buf.Destroy(_heap);

		auto end_time = std::chrono::system_clock::now();
		double dur_sec = std::chrono::duration<double>(end_time - start_time).count();
		int size = resSize;
		if (res != NGDP_DOWNLOAD_SUCCESS) {
			size = 0;
		} else if (!slice->m_size) {
			// bogus size for a potential HEAD request
			size = 512;
		}
		int rate = (int)(size / dur_sec);
		m_cdnTransferRates[idx] = rate;
		m_nextCdnHostIndex = (idx + 1) % m_cdnHostCount;
		if (resSize > slice->m_size) {
			res = NGDP_DOWNLOAD_BUFFER_TOO_SMALL;
		}
		if (res != NGDP_DOWNLOAD_SERVER_ERROR) {
			break;
		}
	}

	{
		int elapsed_us = (int)(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now() - overall_start).count());
		m_client->Report(NGDP_STATISTIC_DOWNLOAD_FINISHED, idx, resSize, elapsed_us, 0);
	}
	return res;
}

}
