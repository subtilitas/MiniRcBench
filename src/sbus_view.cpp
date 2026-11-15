/**
 * @file sbus_view.cpp
 * @brief The S.BUS MONITOR screen's link state and texts. @see sbus_view.h
 */

#include "sbus_view.h"
#include "ui_widgets.h"
#include "gfx.h"

#include <stdio.h>

void sbusLostHoldStep(SbusLostHold *h, const SbusStatus *st, uint32_t nowMs) {
	if (st->lostFrames > h->lostFrames) {
		h->held = true;
		h->atMs = nowMs;
	}
	h->lostFrames = st->lostFrames;
	// Cleared once over, so the stamp never reads as recent again when the
	// clock comes round.
	if (h->held && nowMs - h->atMs >= SBUS_VIEW_LOST_HOLD_MS) h->held = false;
}

/** @brief Whether @p count is above 0 and @p stampMs is less than SBUS_LOST_MS old. */
static bool recent(uint32_t count, uint32_t stampMs, uint32_t nowMs) {
	return count != 0 && nowMs - stampMs < SBUS_LOST_MS;
}

SbusLink sbusLink(const SbusStatus *st, const SbusLostHold *h, uint32_t nowMs) {
	bool frame = recent(st->frames, st->lastFrameMs, nowMs);
	bool byte = recent(st->bytes, st->lastByteMs, nowMs);
	if (!frame && !byte) return SbusLink::NoSignal;
	if (!frame) return SbusLink::NoFrames;
	if (st->failsafe) return SbusLink::Failsafe;
	if (h->held && nowMs - h->atMs < SBUS_VIEW_LOST_HOLD_MS) return SbusLink::FrameLost;
	return SbusLink::Ok;
}

const char *sbusLinkText(SbusLink l) {
	switch (l) {
		case SbusLink::NoSignal:  return SBUS_VIEW_NO_SIGNAL;
		case SbusLink::NoFrames:  return SBUS_VIEW_NO_FRAMES;
		case SbusLink::Failsafe:  return SBUS_VIEW_FAILSAFE;
		case SbusLink::FrameLost: return SBUS_VIEW_FRAME_LOST;
		case SbusLink::Ok:        break;
	}
	return SBUS_VIEW_OK;
}

uint16_t sbusLinkFill(SbusLink l) {
	switch (l) {
		case SbusLink::Failsafe: return UI_COL_ON;
		case SbusLink::Ok:       return UI_COL_SAFE;
		default:                 return UI_COL_WARN;
	}
}

SbusInk sbusInk(const SbusStatus *st, SbusLink l) {
	if (st->frames == 0) return SbusInk::Empty;
	if (l == SbusLink::NoSignal || l == SbusLink::NoFrames) return SbusInk::Stale;
	if (l == SbusLink::Failsafe) return SbusInk::Failsafe;
	return SbusInk::Live;
}

int sbusBarFill(uint16_t raw, int widthPx) {
	uint32_t r = raw > SBUS_RAW_MAX ? SBUS_RAW_MAX : raw;
	return (int)(r * (uint32_t)widthPx / SBUS_RAW_MAX);
}

/** @brief @p v, or @p max when it is more. */
static unsigned long capped(uint32_t v, uint32_t max) {
	return (unsigned long)(v > max ? max : v);
}

void sbusValueText(char *buf, size_t n, const SbusStatus *st, unsigned ch) {
	if (st->frames == 0 || ch >= SBUS_CHANNELS) {
		snprintf(buf, n, "----");
		return;
	}
	snprintf(buf, n, "%u", (unsigned)sbus_to_us(st->channel[ch]));
}

void sbusDigitalText(char *buf, size_t n, const SbusStatus *st) {
	if (st->frames == 0) {
		snprintf(buf, n, "CH17 --  CH18 --");
		return;
	}
	snprintf(buf, n, "CH17 %s  CH18 %s", st->ch17 ? "ON" : "OFF", st->ch18 ? "ON" : "OFF");
}

void sbusRateText(char *buf, size_t n, const SbusStatus *st) {
	if (st->intervalUs == 0) {
		snprintf(buf, n, "-- MS  -- HZ");
		return;
	}
	// The interval is under 100,000 us (sbus_job.cpp), so at most 100.0 ms.
	unsigned long ms10 = (st->intervalUs + 50u) / 100u;
	unsigned long hz10 = capped(sbusRateDeciHz(st->intervalUs), SBUS_VIEW_RATE_MAX);
	snprintf(buf, n, "%lu.%lu MS  %lu.%lu HZ", ms10 / 10u, ms10 % 10u, hz10 / 10u, hz10 % 10u);
}

void sbusFramesText(char *buf, size_t n, const SbusStatus *st) {
	snprintf(buf, n, "FRAMES %lu", capped(st->frames, SBUS_VIEW_FRAMES_MAX));
}

void sbusLostText(char *buf, size_t n, const SbusStatus *st) {
	snprintf(buf, n, "LOST %lu  FS %lu", capped(st->lostFrames, SBUS_VIEW_COUNT_MAX),
	         capped(st->failsafeFrames, SBUS_VIEW_COUNT_MAX));
}

void sbusErrorText(char *buf, size_t n, const SbusStatus *st) {
	snprintf(buf, n, "PAR %lu FRM %lu BRK %lu OVR %lu", capped(st->parityErr, SBUS_VIEW_COUNT_MAX),
	         capped(st->framingErr, SBUS_VIEW_COUNT_MAX), capped(st->breakErr, SBUS_VIEW_COUNT_MAX),
	         capped(st->overrun, SBUS_VIEW_COUNT_MAX));
}

void sbusDecoderText(char *buf, size_t n, const SbusStatus *st) {
	snprintf(buf, n, "RESYNC %lu FOOTER %lu", capped(st->resyncs, SBUS_VIEW_COUNT_MAX),
	         capped(st->badFooter, SBUS_VIEW_COUNT_MAX));
}
