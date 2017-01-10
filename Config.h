#pragma once

#include "Strings.h"
#include "Key.h"

namespace ngdp {

struct CDNConfig {
	Slice<Key> m_archives;
	Key m_archiveGroup;
	Slice<Key> m_patchArchives;
	Key m_patchArchiveGroup;
	// Rest in peace
	Slice<Key> m_builds;

	Buffer<Key> m_allKeys;

	void Init(Heap *h, const String &configFile);
	void Destroy(Heap *h);

	void ParseKeyList(Heap *h, BufferSegment *seg, const String &keys);
};

struct BuildConfig {
	Key m_root;
	Key m_install;
	Key m_download;
	Key m_partialPriority;
	Key m_patch;
	Key m_patchConfig;
	// [0] = ckey, [1] = ekey
	Key m_encoding[2];
	int m_encodingSize[2];
	int m_installSize;
	int m_downloadSize;
	int m_partialPrioritySize;
	int m_patchSize;
	String m_buildName;
	String m_buildPlaybuildInstaller;
	String m_buildProduct;
	String m_buildUid;

	Buffer<u8> m_strings;

	void Init(Heap *h, const String &configFile);
	void Destroy(Heap *h);
};

}
