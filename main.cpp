#include "std.h"

#include "ngdp.h"

static void debugLog(const char *message) {
	fprintf(stderr, "[ngdp] %s\n", message);
}

static void reportStat(int type, int arg0, int arg1, int arg2, const uint8_t *key) {
	fprintf(stderr, "[stat] %d %d %d %d\n", type, arg0, arg1, arg2);
}

int main() {
	ngdpConfig config;
	memset(&config, 0, sizeof(config));
	config.ngdpUrl = "http://us.patch.battle.net";
	config.ngdpRegion = "us";
	config.gameUid = "wow";
	config.logFn = debugLog;
	config.statsFn = reportStat;
	ngdpClient *c = ngdpInit(&config);
	// TODO:
	// - Create multi-archive index and load & decode encoding
	// - Write decoder frame stuff for encoding
	// - Load root
	// - Query interface for filedataid, filename -> ckey; ckey -> ekey -> buffer
	fflush(stderr);
}
