/**
 * @file bridge_log.cpp
 * @brief The USB bridge's traffic log. @see bridge_log.h
 */

#include "bridge_log.h"

#if USB_BRIDGE_LOG

#include "plat.h"
#include "tusb.h"

#include <stdio.h>
#include <string.h>

static uint8_t  s_ring[USB_BRIDGE_LOG_BYTES];
static uint16_t s_head = 0;       /**< Where the next byte goes. */
static uint16_t s_tail = 0;       /**< The oldest record. */
static uint16_t s_used = 0;       /**< Bytes held. */
static uint16_t s_count = 0;      /**< Records held. */
static bool     s_overflow = false;
static uint32_t s_t0 = 0;         /**< millis() at the reset. */

/** @brief The dump: where it is. */
enum class Dump : uint8_t { Off, Title, Heads, Records, End, Done };
static Dump     s_dump = Dump::Off;
static uint16_t s_rec = 0;        /**< The next record to print. */
static uint16_t s_off = 0;        /**< Its offset from the tail. */
static uint32_t s_prevMs = 0;     /**< The time of the record printed last. */
static char     s_line[BRIDGE_LOG_MAX_PAYLOAD * 3 + 64];
static uint16_t s_lineLen = 0;    /**< Characters in s_line. */
static uint16_t s_lineSent = 0;   /**< Of them, written. */
static uint32_t s_progressMs = 0; /**< millis() when the dump last wrote a byte. */

static void put(uint8_t b) {
	s_ring[s_head] = b;
	s_head = (uint16_t)((s_head + 1u) % sizeof(s_ring));
	s_used++;
}

static uint8_t at(uint16_t off) { return s_ring[(s_tail + off) % sizeof(s_ring)]; }

/** @brief Drop the oldest record. */
static void dropOldest() {
	uint16_t total = (uint16_t)(BRIDGE_LOG_HDR + at(5));
	s_tail = (uint16_t)((s_tail + total) % sizeof(s_ring));
	s_used = (uint16_t)(s_used - total);
	s_count--;
	s_overflow = true;
}

void bridgeLogReset() {
	s_head = s_tail = s_used = s_count = 0;
	s_overflow = false;
	s_t0 = millis();
	s_dump = Dump::Off;
}

void bridgeLogAdd(BridgeLogDir dir, const uint8_t *data, uint16_t len) {
	uint16_t kept = len > BRIDGE_LOG_MAX_PAYLOAD ? BRIDGE_LOG_MAX_PAYLOAD : len;
	uint16_t need = (uint16_t)(BRIDGE_LOG_HDR + kept);
	while (s_count && sizeof(s_ring) - s_used < need) dropOldest();
	uint32_t ms = millis() - s_t0;
	put((uint8_t)ms);
	put((uint8_t)(ms >> 8));
	put((uint8_t)(ms >> 16));
	put((uint8_t)(ms >> 24));
	put((uint8_t)dir);
	put((uint8_t)kept);
	put((uint8_t)(len > 255 ? 255 : len));
	for (uint16_t i = 0; i < kept; i++) put(data[i]);
	s_count++;
}

void bridgeLogNote(const char *text) {
	bridgeLogAdd(BLOG_NOTE, (const uint8_t *)text, (uint16_t)strlen(text));
}

uint16_t bridgeLogCount() { return s_count; }
bool bridgeLogOverflowed() { return s_overflow; }

void bridgeLogDumpBegin() {
	s_dump = Dump::Title;
	s_rec = 0;
	s_off = 0;
	s_prevMs = 0;
	s_lineLen = s_lineSent = 0;
	s_progressMs = millis();
}

/** @brief Record @p r's line into s_line, from s_off; s_off moves past it. */
static void recordLine(uint16_t r) {
	uint32_t ms = (uint32_t)at(s_off) | ((uint32_t)at((uint16_t)(s_off + 1)) << 8) |
	              ((uint32_t)at((uint16_t)(s_off + 2)) << 16) |
	              ((uint32_t)at((uint16_t)(s_off + 3)) << 24);
	uint8_t dir = at((uint16_t)(s_off + 4));
	uint8_t kept = at((uint16_t)(s_off + 5));
	uint8_t total = at((uint16_t)(s_off + 6));
	uint16_t d = (uint16_t)(s_off + BRIDGE_LOG_HDR);
	const int room = (int)sizeof(s_line);
	int p;
	if (dir == BLOG_NOTE) {
		p = snprintf(s_line, room, "%6lu  ----     -  ", (unsigned long)ms);
		for (uint8_t i = 0; i < kept && p < room - 2; i++) s_line[p++] = (char)at((uint16_t)(d + i));
		s_line[p] = '\0';
	} else {
		// The gap since the record before is the number that matters: the
		// echo and the answer have to reach the PC within one of its reads.
		p = snprintf(s_line, room, "%6lu  %s %3u  ", (unsigned long)ms,
		             dir == BLOG_HOST_TO_ESC ? "H>E " : "E>H ", (unsigned)total);
		for (uint8_t i = 0; i < kept && p < room - 4; i++)
			p += snprintf(s_line + p, room - p, "%02X ", at((uint16_t)(d + i)));
		if (total > kept && p < room - 24)
			p += snprintf(s_line + p, room - p, "...(%u of %u shown)", (unsigned)kept, (unsigned)total);
		if (r && p < room - 16)
			p += snprintf(s_line + p, room - p, "  (+%lums)", (unsigned long)(ms - s_prevMs));
	}
	snprintf(s_line + p, room - p, "\n");
	s_prevMs = ms;
	s_off = (uint16_t)(d + kept);
}

/** @brief Compose the dump's next line; false once there is none. */
static bool nextLine() {
	switch (s_dump) {
		case Dump::Title:
			snprintf(s_line, sizeof(s_line), "\n--- USB BRIDGE LOG: %u record(s)%s ---\n",
			         (unsigned)s_count, s_overflow ? ", OLDEST DROPPED" : "");
			s_dump = Dump::Heads;
			break;
		case Dump::Heads:
			snprintf(s_line, sizeof(s_line), "  ms  dir      n  bytes\n");
			s_dump = s_count ? Dump::Records : Dump::End;
			break;
		case Dump::Records:
			recordLine(s_rec);
			if (++s_rec >= s_count) s_dump = Dump::End;
			break;
		case Dump::End:
			snprintf(s_line, sizeof(s_line), "--- END ---\n\n");
			s_dump = Dump::Done;
			break;
		case Dump::Off:
		case Dump::Done:
			return false;
	}
	s_lineLen = (uint16_t)strlen(s_line);
	s_lineSent = 0;
	return true;
}

bool bridgeLogDumpStep() {
	if (s_dump == Dump::Off) return true;
	for (;;) {
		if (s_lineSent >= s_lineLen && !nextLine()) {
			s_dump = Dump::Off;
			return true;
		}
		uint32_t room = tud_cdc_write_available();
		if (room == 0) break;
		uint32_t n = (uint32_t)(s_lineLen - s_lineSent);
		if (n > room) n = room;
		uint32_t wrote = tud_cdc_write(s_line + s_lineSent, n);
		if (wrote == 0) break;
		s_lineSent = (uint16_t)(s_lineSent + wrote);
		s_progressMs = millis();
	}
	tud_cdc_write_flush();
	if (millis() - s_progressMs >= BRIDGE_LOG_STALL_MS) {
		s_dump = Dump::Off;
		return true;
	}
	return false;
}

#endif
