/**
 * @file fake_bridge_line.cpp
 * @brief The host suite's USB bridge line. @see fake_bridge_line.h
 */

#include "fake_bridge_line.h"
#include "bridge_line_port.h"
#include "bridge_wire.h"

#include <pico/time.h>

#include <algorithm>
#include <deque>
#include <vector>

namespace {

/** @brief A byte on its way into the receive FIFO. */
struct RxByte {
	uint64_t start;  /**< Its start bit's cycle. */
	uint64_t push;   /**< When the receiver pushes it. */
	uint8_t  b;
	bool     lost;   /**< Its echo never comes. */
};

/** @brief A word in the transmit FIFO. */
struct TxWord {
	uint16_t word;
	uint64_t arrival;
};

/** @brief A byte the bridge sent. */
struct WireByte {
	uint8_t  b;
	uint64_t start;
	bool     keep;
};

bool                 g_open = false;
uint8_t              g_refuse = 0;
FakeEscReply         g_responder = nullptr;
int                  g_opens = 0, g_closes = 0, g_cuts = 0, g_misuse = 0;
int                  g_releases = 0;
uint8_t              g_pin = 0xFF;
bool                 g_stall = false;
std::deque<TxWord>   g_txq;
uint64_t             g_txFree = 0;     // the transmitter waits on `pull` from here
uint64_t             g_drivenFrom = 0; // the frame's first start bit
bool                 g_driven = false;
std::vector<uint8_t> g_frame;          // the frame under way
std::vector<RxByte>  g_rxPending;      // sorted by push
std::deque<uint8_t>  g_rxFifo;
bool                 g_overflow = false;
std::vector<WireByte> g_wire;
int                  g_sentSinceOpen = 0;
std::vector<std::pair<int, uint8_t>> g_corrupt;
std::vector<int>     g_drop;
int                  g_conflicts = 0;
std::vector<std::pair<uint64_t, uint64_t>> g_escBusy;  // the ESC's bytes
uint64_t             g_lastLetGo = 0;  // the last frame's let-go
bool                 g_hadFrame = false;
uint64_t             g_gapCy = UINT64_MAX;  // idle line before the frame being answered

const uint64_t BYTE_PUSH = 78;   // start bit to the receive FIFO
const uint64_t BYTE_LET_GO = 80; // start bit to the pad let go
const uint64_t BYTE_NEXT = 81;   // start bit to `pull`
const uint64_t ESC_BYTE = 80;    // the ESC's bytes, back to back
const unsigned FIFO = 8;

uint64_t now() { return (uint64_t)time_us_32() * 1536u / 10000u; }

void pushRx(const RxByte &r) {
	auto at = std::upper_bound(g_rxPending.begin(), g_rxPending.end(), r,
	                           [](const RxByte &a, const RxByte &b) { return a.push < b.push; });
	g_rxPending.insert(at, r);
}

/** @brief Overlap, in cycles, of the bridge driving and an ESC byte. */
void conflictCheck(uint64_t from, uint64_t to) {
	for (auto &e : g_escBusy) {
		uint64_t a = std::max(from, e.first), b = std::min(to, e.second);
		if (a < b) g_conflicts += (int)(b - a);
	}
}

/**
 * @brief The pad let go at @p at: the frame is done, and the ESC answers it
 *        when @p answer.
 */
void letGo(uint64_t at, bool answer) {
	g_driven = false;
	g_releases++;
	conflictCheck(g_drivenFrom, at);
	g_gapCy = g_hadFrame ? g_drivenFrom - g_lastLetGo : UINT64_MAX;
	g_hadFrame = true;
	g_lastLetGo = at;
	if (!g_responder || !answer) { g_frame.clear(); return; }
	static uint8_t reply[1100];
	uint32_t delayUs = 0;
	int n = g_responder(g_frame.data(), (int)g_frame.size(), reply, &delayUs);
	g_frame.clear();
	uint64_t s = at + (uint64_t)delayUs * 1536u / 10000u;
	for (int i = 0; i < n; i++, s += ESC_BYTE) {
		pushRx({s, s + BYTE_PUSH, reply[i], false});
		g_escBusy.push_back({s, s + ESC_BYTE});
	}
}

/** @brief The transmitter takes a word at @p at. */
void take(uint64_t at) {
	TxWord w = g_txq.front();
	g_txq.pop_front();
	const uint64_t s = at + 1;
	const uint8_t b = (uint8_t)w.word;
	const bool keep = (w.word & BRIDGE_LINE_KEEP) != 0;
	if (!g_driven) { g_driven = true; g_drivenFrom = s; }
	g_wire.push_back({b, s, keep});
	g_frame.push_back(b);
	const int n = g_sentSinceOpen++;
	uint8_t echo = b;
	for (auto &c : g_corrupt) if (c.first == n) echo = c.second;
	bool lost = false;
	for (int d : g_drop) if (d == n || d == -1) lost = true;
	pushRx({s, s + BYTE_PUSH, echo, lost});
	g_txFree = s + BYTE_NEXT;
	if (!keep) letGo(s + BYTE_LET_GO, true);
}

/** @brief Run the wire up to @p t. */
void advance(uint64_t t) {
	for (;;) {
		uint64_t tx = UINT64_MAX;
		// A waiting `pull` takes a word the cycle after it arrives.
		if (!g_stall && !g_txq.empty()) tx = std::max(g_txFree, g_txq.front().arrival + 1);
		uint64_t rx = g_rxPending.empty() ? UINT64_MAX : g_rxPending.front().push;
		if (tx > t && rx > t) return;
		if (tx <= rx) {
			take(tx);
		} else {
			RxByte r = g_rxPending.front();
			g_rxPending.erase(g_rxPending.begin());
			if (r.lost) continue;
			if (g_rxFifo.size() >= FIFO) g_overflow = true;
			else g_rxFifo.push_back(r.b);
		}
	}
}

}  // namespace

void fakeBridgeLineReset() {
	g_open = false;
	g_refuse = 0;
	g_responder = nullptr;
	g_opens = g_closes = g_cuts = g_misuse = g_releases = 0;
	g_pin = 0xFF;
	g_stall = false;
	g_txq.clear();
	g_rxPending.clear();
	g_rxFifo.clear();
	g_overflow = false;
	g_wire.clear();
	g_frame.clear();
	g_corrupt.clear();
	g_drop.clear();
	g_conflicts = 0;
	g_escBusy.clear();
	g_driven = false;
	g_sentSinceOpen = 0;
	g_hadFrame = false;
	g_gapCy = UINT64_MAX;
}

uint32_t fakeBridgeLineGapUs() {
	return g_gapCy == UINT64_MAX ? UINT32_MAX : (uint32_t)(g_gapCy * 10000u / 1536u);
}

void fakeBridgeLineRefuse(uint8_t err) { g_refuse = err; }
void fakeBridgeLineResponder(FakeEscReply r) { g_responder = r; }
void fakeBridgeLineCorruptEcho(int n, uint8_t value) { g_corrupt.push_back({n, value}); }
void fakeBridgeLineDropEcho(int n) { g_drop.push_back(n); }
void fakeBridgeLineStall(bool on) { advance(now()); g_stall = on; }
int fakeBridgeLineOpens() { return g_opens; }
int fakeBridgeLineCloses() { return g_closes; }
uint8_t fakeBridgeLinePin() { return g_pin; }
int fakeBridgeLineCuts() { return g_cuts; }
int fakeBridgeLineConflicts() { advance(now()); return g_conflicts; }
int fakeBridgeLineMisuse() { return g_misuse; }
int fakeBridgeLineWireCount() { advance(now()); return (int)g_wire.size(); }
int fakeBridgeLineReleases() { advance(now()); return g_releases; }
uint64_t fakeBridgeLineCycle() { return now(); }

void fakeBridgeLineInject(uint8_t b) {
	if (!g_open) return;
	const uint64_t s = now();
	advance(s);
	pushRx({s, s + BYTE_PUSH, b, false});
	g_escBusy.push_back({s, s + ESC_BYTE});
}

void fakeBridgeLineWire(int i, uint8_t *b, uint64_t *startCy, bool *keep) {
	advance(now());
	if (i < 0 || (size_t)i >= g_wire.size()) {
		// Past the wire: nothing, so a test that asks too far fails, not crashes.
		*b = 0;
		*startCy = 0;
		*keep = false;
		return;
	}
	*b = g_wire[(size_t)i].b;
	*startCy = g_wire[(size_t)i].start;
	*keep = g_wire[(size_t)i].keep;
}

uint8_t bridgeLineOpen(uint8_t gpio) {
	if (g_refuse) return g_refuse;
	g_open = true;
	g_opens++;
	g_pin = gpio;
	g_txq.clear();
	g_rxPending.clear();
	g_rxFifo.clear();
	g_overflow = false;
	g_txFree = now();
	g_driven = false;
	g_frame.clear();
	g_sentSinceOpen = 0;
	return 0;
}

void bridgeLineClose() {
	if (!g_open) return;
	const uint64_t t = now();
	advance(t);
	// A byte between its start bit and its release, or words never sent.
	bool cut = !g_txq.empty();
	if (!g_wire.empty() && g_wire.back().start <= t && t < g_wire.back().start + BYTE_LET_GO)
		cut = true;
	if (cut) g_cuts++;
	if (g_driven) letGo(t, false);  // a cut frame gets no answer
	g_open = false;
	g_closes++;
	g_txq.clear();
	g_rxPending.clear();
	g_rxFifo.clear();
}

bool bridgeLineIsOpen() { return g_open; }

bool bridgeLinePut(uint16_t word) {
	if (!g_open) { g_misuse++; return false; }
	const uint64_t t = now();
	advance(t);
	if (g_txq.size() >= FIFO) return false;
	g_txq.push_back({word, t});
	return true;
}

bool bridgeLineGet(uint8_t *out) {
	advance(now());
	if (g_rxFifo.empty()) return false;
	*out = g_rxFifo.front();
	g_rxFifo.pop_front();
	return true;
}

bool bridgeLineIdle() {
	const uint64_t t = now();
	advance(t);
	if (!g_txq.empty() || t < g_txFree) return false;
	for (auto &r : g_rxPending)
		if (r.start <= t && t < r.push) return false;
	return true;
}

bool bridgeLineTakeOverflow() {
	advance(now());
	bool f = g_overflow;
	g_overflow = false;
	return f;
}
