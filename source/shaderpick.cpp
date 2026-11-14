#include "shaderpick.h"
#include "player.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include <algorithm>
#include <random>
#include <set>

namespace shaderpick {
namespace {

const char* const FAV_FILE = "sdmc:/switch/shaderfun/favourites.txt";
const size_t HISTORY_MAX = 64;

std::set<std::string> g_favs;          // keys: the path inside the shaders folder, so they survive a docked / handheld change
Order g_order = ORDER_LIST;
Scope g_scope = SCOPE_ALL;
std::vector<std::string> g_history;    // shaders shown before this one (for "previous" in random mode)
std::set<std::string> g_played;        // random mode: shaders already shown in this round
std::string g_poolKey;                 // identifies the set of shaders being chosen from; a change starts a new round
std::string g_planned;                 // random mode: the shader "next" will show, chosen early by peekIndex
bool g_plannedNewRound = false;        // ...and it starts a new round
bool g_fallback = false, g_fallbackNotified = false;
std::mt19937 g_rng;

// "sdmc:/switch/shaderfun/shaders/Docked/x.frag" -> "Docked/x.frag"
std::string keyOf(const std::string& path) {
	static const char* const prefixes[] = { "sdmc:/switch/shaderfun/shaders/", "sdmc:/switch/shaderfun/test/", "romfs:/shaders/" };
	for (const char* p : prefixes) {
		const size_t n = strlen(p);
		if (path.compare(0, n, p) == 0) return path.substr(n);
	}
	return path;
}

void saveFavourites() {
	mkdir("sdmc:/switch", 0777);
	mkdir("sdmc:/switch/shaderfun", 0777);
	FILE* f = fopen(FAV_FILE, "w");
	if (!f) return;
	fputs("# ShaderFun favourite shaders, one per line (path inside the shaders folder)\n", f);
	for (const std::string& k : g_favs) fprintf(f, "%s\n", k.c_str());
	fclose(f);
}

// the shaders that may be chosen: favourites (when asked for and there are some) or everything
std::vector<int> poolOf(const std::vector<std::string>& list) {
	std::vector<int> pool;
	if (g_scope == SCOPE_FAVOURITES) {
		for (size_t i = 0; i < list.size(); i++) if (isFavourite(list[i])) pool.push_back((int)i);
		if (pool.empty() && !list.empty() && !g_fallbackNotified) { g_fallback = true; g_fallbackNotified = true; }
	}
	if (pool.empty()) for (size_t i = 0; i < list.size(); i++) pool.push_back((int)i);
	return pool;
}

bool inPool(const std::vector<int>& pool, int i) { return std::find(pool.begin(), pool.end(), i) != pool.end(); }

// the planned next shader, if it is still a valid choice (in the pool and not the current one)
int plannedIndex(const std::vector<std::string>& list, const std::vector<int>& pool, int current) {
	if (g_planned.empty()) return -1;
	for (int i : pool) if (i != current && list[i] == g_planned) return i;
	g_planned.clear();
	return -1;
}

void pushHistory(const std::string& path) {
	if (path.empty() || (!g_history.empty() && g_history.back() == path)) return;
	g_history.push_back(path);
	if (g_history.size() > HISTORY_MAX) g_history.erase(g_history.begin());
}

// next / previous member of the pool in list order (wraps round)
int stepInList(const std::vector<std::string>& list, const std::vector<int>& pool, int current, int dir) {
	const int n = (int)list.size();
	for (int step = 1; step <= n; step++) {
		const int i = (((current + dir * step) % n) + n) % n;
		if (inPool(pool, i)) return i;
	}
	return current;
}

} // namespace

void load() {
	g_rng.seed((unsigned)time(nullptr));
	g_order = player::configInt("shader_order", 0) == 1 ? ORDER_RANDOM : ORDER_LIST;
	g_scope = player::configInt("shader_scope", 0) == 1 ? SCOPE_FAVOURITES : SCOPE_ALL;
	g_favs.clear();
	FILE* f = fopen(FAV_FILE, "r");
	if (!f) return;
	char line[512];
	while (fgets(line, sizeof(line), f)) {
		std::string s = line;
		while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' ' || s.back() == '\t')) s.pop_back();
		if (s.empty() || s[0] == '#') continue;
		g_favs.insert(s);
	}
	fclose(f);
}

Order order() { return g_order; }
Scope scope() { return g_scope; }

void setOrder(Order o) {
	g_order = o;
	player::setConfigInt("shader_order", o == ORDER_RANDOM ? 1 : 0);
	g_played.clear();
	g_planned.clear();
}

void setScope(Scope s) {
	g_scope = s;
	player::setConfigInt("shader_scope", s == SCOPE_FAVOURITES ? 1 : 0);
	g_played.clear();
	g_planned.clear();
	g_fallbackNotified = false;
}

bool isFavourite(const std::string& shaderPath) { return g_favs.count(keyOf(shaderPath)) != 0; }

bool toggleFavourite(const std::string& shaderPath) {
	const std::string k = keyOf(shaderPath);
	bool now;
	if (g_favs.count(k)) { g_favs.erase(k); now = false; }
	else { g_favs.insert(k); now = true; }
	saveFavourites();
	g_fallbackNotified = false;
	return now;
}

int favouriteCount() { return (int)g_favs.size(); }

int pickIndex(const std::vector<std::string>& list, int current, int dir) {
	if (list.empty()) return 0;
	if (current < 0 || current >= (int)list.size()) current = 0;
	const std::vector<int> pool = poolOf(list);

	// a different set of shaders (docked / handheld change, favourites changed...) starts a new random round
	std::string key;
	for (int i : pool) { key += list[i]; key += '\n'; }
	if (key != g_poolKey) { g_poolKey = key; g_played.clear(); g_planned.clear(); }

	const std::string curPath = list[current];

	if (g_order == ORDER_LIST) return stepInList(list, pool, current, dir);

	// ---- random
	if (dir < 0) {
		while (!g_history.empty()) { // back to what was shown before
			const std::string p = g_history.back();
			g_history.pop_back();
			if (p == curPath) continue;
			for (int i : pool) if (list[i] == p) return i;
		}
		return stepInList(list, pool, current, -1);
	}

	pushHistory(curPath);
	const int planned = plannedIndex(list, pool, current); // chosen earlier by peekIndex
	if (planned >= 0) {
		if (g_plannedNewRound) g_played.clear();
		g_played.insert(curPath);
		g_played.insert(list[planned]);
		g_planned.clear();
		g_plannedNewRound = false;
		return planned;
	}
	g_played.insert(curPath);
	std::vector<int> bag; // not shown yet in this round
	for (int i : pool) if (i != current && !g_played.count(list[i])) bag.push_back(i);
	if (bag.empty()) { // everything has been shown: a new round
		g_played.clear();
		g_played.insert(curPath);
		for (int i : pool) if (i != current) bag.push_back(i);
	}
	if (bag.empty()) return current; // only one shader to choose from
	const int chosen = bag[g_rng() % bag.size()];
	g_played.insert(list[chosen]);
	return chosen;
}

int peekIndex(const std::vector<std::string>& list, int current, int dir) {
	if (list.empty()) return 0;
	if (current < 0 || current >= (int)list.size()) current = 0;
	const bool savedFallback = g_fallback, savedNotified = g_fallbackNotified; // (peeking must not use up the "no favourites" notice)
	const std::vector<int> pool = poolOf(list);
	g_fallback = savedFallback;
	g_fallbackNotified = savedNotified;

	std::string key;
	for (int i : pool) { key += list[i]; key += '\n'; }
	if (key != g_poolKey) { g_poolKey = key; g_played.clear(); g_planned.clear(); }

	if (g_order == ORDER_LIST) return stepInList(list, pool, current, dir);

	const std::string curPath = list[current];
	if (dir < 0) { // what pickIndex(-1) would go back to, without removing it from the history
		for (auto it = g_history.rbegin(); it != g_history.rend(); ++it) {
			if (*it == curPath) continue;
			for (int i : pool) if (list[i] == *it) return i;
		}
		return stepInList(list, pool, current, -1);
	}

	const int planned = plannedIndex(list, pool, current);
	if (planned >= 0) return planned;
	std::vector<int> bag;
	for (int i : pool) if (i != current && !g_played.count(list[i])) bag.push_back(i);
	bool newRound = false;
	if (bag.empty()) { // everything has been shown: pickIndex would start a new round
		for (int i : pool) if (i != current) bag.push_back(i);
		newRound = true;
	}
	if (bag.empty()) return current; // only one shader to choose from
	const int chosen = bag[g_rng() % bag.size()];
	g_planned = list[chosen]; // remembered, so the next pickIndex(+1) agrees
	g_plannedNewRound = newRound;
	return chosen;
}

void noteJump(const std::string& leavingPath) {
	pushHistory(leavingPath);
	g_played.insert(leavingPath);
}

bool consumeFallbackNotice() {
	const bool f = g_fallback;
	g_fallback = false;
	return f;
}

} // namespace shaderpick
