#include "Config.h"
#include <functional>

namespace ngdp {

// Parse an ini-like config file formatted as `# comment\nkey = value\n...`
static void ParseConfig(Heap *h, const String &s, std::function<void(const String &key, const String &value)> onData) {
	Buffer<String> lines = s.Split(h, "\n");
	for (auto &line : lines) {
		if (line.m_size == 0 || line[0] == '#') {
			continue;
		}
		int firstEquals = line.Index("=");
		String key = line.Substring(0, firstEquals);
		key = key.Trim();
		String value = line.Substring(firstEquals + 1);
		value = value.Trim();
		onData(key, value);
	}
	lines.Destroy(h);
}

void CDNConfig::Init(Heap *h, const String &configFile) {
	m_allKeys.Init(h, 256);
	BufferSegment archives = {0};
	BufferSegment patchArchives = {0};
	BufferSegment builds = {0};
	ParseConfig(h, configFile, [&](const String &key, const String &value) {
		if (key == "archives") {
			ParseKeyList(h, &archives, value);
		} else if (key == "archive-group") {
			m_archiveGroup.InitFromHexString(value);
		} else if (key == "patch-archives") {
			ParseKeyList(h, &patchArchives, value);
		} else if (key == "patch-archive-group") {
			m_patchArchiveGroup.InitFromHexString(value);
		} else if (key == "builds") {
			ParseKeyList(h, &builds, value);
		}
	});
	m_archives = m_allKeys.MakeSlice(archives);
	m_patchArchives = m_allKeys.MakeSlice(patchArchives);
	m_builds = m_allKeys.MakeSlice(builds);
}

void CDNConfig::Destroy(Heap *h) {
	m_allKeys.Destroy(h);
}

void CDNConfig::ParseKeyList(Heap *h, BufferSegment *seg, const String &keys) {
	seg->m_start = m_allKeys.m_size;
	Buffer<String> keyStrings = keys.Split(h, " ");
	for (auto &keyString : keyStrings) {
		keyString = keyString.Trim();
		if (keyString.m_size > 0) {
			Key *k = m_allKeys.Alloc(h, 1);
			k->InitFromHexString(keyString);
		}
	}
	seg->m_end = m_allKeys.m_size;
}

void BuildConfig::Init(Heap *h, const String &configFile) {
	BufferSegment buildName = {0};
	BufferSegment buildPlaybuildInstaller = {0};
	BufferSegment buildProduct = {0};
	BufferSegment buildUid = {0};
	m_strings.Init(h, 64);
	StringBuffer sb;
	sb.m_buffer = &m_strings;
	ParseConfig(h, configFile, [&](const String &key, const String &value) {
		if (key == "root") {
			m_root.InitFromHexString(value);
		} else if (key == "install") {
			m_install.InitFromHexString(value);
		} else if (key == "download") {
			m_download.InitFromHexString(value);
		} else if (key == "partial-priority") {
			m_partialPriority.InitFromHexString(value);
		} else if (key == "patch") {
			m_patch.InitFromHexString(value);
		} else if (key == "patch-config") {
			m_patchConfig.InitFromHexString(value);
		} else if (key == "encoding") {
			Buffer<String> parts = value.Split(h, " ");
			assert(parts.m_size == 2);
			m_encoding[0].InitFromHexString(parts[0]);
			m_encoding[1].InitFromHexString(parts[1]);
			parts.Destroy(h);
		} else if (key == "encoding-size") {
			Buffer<String> parts = value.Split(h, " ");
			assert(parts.m_size == 2);
			m_encodingSize[0] = parts[0].ParseInt();
			m_encodingSize[1] = parts[1].ParseInt();
			parts.Destroy(h);
		} else if (key == "install-size") {
			m_installSize = value.ParseInt();
		} else if (key == "download-size") {
			m_downloadSize = value.ParseInt();
		} else if (key == "partial-priority-size") {
			m_partialPrioritySize = value.ParseInt();
		} else if (key == "patch-size") {
			m_patchSize = value.ParseInt();
		} else if (key == "build-name") {
			buildName = sb.Duplicate(h, value);
		} else if (key == "build-playbuild-installer") {
			buildPlaybuildInstaller = sb.Duplicate(h, value);
		} else if (key == "build-product") {
			buildProduct = sb.Duplicate(h, value);
		} else if (key == "build-uid") {
			buildUid = sb.Duplicate(h, value);
		}
	});
	m_buildName = sb.MakeString(buildName);
	m_buildPlaybuildInstaller = sb.MakeString(buildPlaybuildInstaller);
	m_buildProduct = sb.MakeString(buildProduct);
	m_buildUid = sb.MakeString(buildUid);
}

void BuildConfig::Destroy(Heap *h) {
	m_strings.Destroy(h);
}

}
