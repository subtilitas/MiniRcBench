/**
 * @file sbus_out_view.cpp
 * @brief The S.BUS OUTPUT screen's chip, notes and row texts. @see sbus_out_view.h
 */

#include "sbus_out_view.h"
#include "config.h"
#include "ui_widgets.h"
#include "gfx.h"

#include <stdio.h>

SbusOutChip sbusOutChip(const SbusOutStatus *st) {
	if (st->running) return SbusOutChip::Running;
	if (st->tripped) return SbusOutChip::Stalled;
	return SbusOutChip::Safe;
}

const char *sbusOutChipText(SbusOutChip c) {
	switch (c) {
		case SbusOutChip::Running: return SBUS_OUT_VIEW_RUNNING;
		case SbusOutChip::Stalled: return SBUS_OUT_VIEW_STALLED;
		case SbusOutChip::Safe:    break;
	}
	return SBUS_OUT_VIEW_SAFE;
}

uint16_t sbusOutChipFill(SbusOutChip c) {
	switch (c) {
		case SbusOutChip::Running: return UI_COL_ON;
		case SbusOutChip::Stalled: return UI_COL_ALERT;
		case SbusOutChip::Safe:    break;
	}
	return UI_COL_SAFE;
}

/** @brief `ON` or `OFF`. */
static const char *onOff(bool on) { return on ? "ON" : "OFF"; }

void sbusOutNotes(const SbusOutStatus *st, SbusOutStop stop, uint8_t thrCh, uint16_t ceilRaw,
                  uint16_t maxThrottle, char *n1, char *n2, size_t n, uint16_t *c1, uint16_t *c2) {
	if (st->running) {
		if (maxThrottle > MAX_THROTTLE_CEILING) maxThrottle = MAX_THROTTLE_CEILING;
		snprintf(n1, n, SBUS_OUT_VIEW_CEIL_FMT, (unsigned)thrCh + 1u, (unsigned)sbus_to_us(ceilRaw),
		         (unsigned)(maxThrottle * 100u / MAX_THROTTLE_CEILING));
		const sbus_frame_t &f = st->sent;
		snprintf(n2, n, "CH17 %s  CH18 %s  FS %s  LOST %s", onOff(f.ch17), onOff(f.ch18),
		         onOff(f.failsafe), onOff(f.frame_lost));
		*c1 = C_TEXT;
		*c2 = (f.ch17 || f.ch18 || f.failsafe || f.frame_lost) ? UI_COL_WARN : C_TEXT;
		return;
	}
	const char *why = nullptr;
	*c1 = UI_COL_WARN;
	if (st->tripped) {
		why = SBUS_OUT_VIEW_STALL;
		*c1 = UI_COL_ALERT;
	} else if (st->refused) {
		why = SBUS_OUT_VIEW_REFUSED;
	} else if (stop == SbusOutStop::Idle) {
		why = SBUS_OUT_VIEW_IDLE;
	}
	if (why) {
		snprintf(n1, n, "%s", why);
		snprintf(n2, n, "%s", SBUS_OUT_VIEW_AGAIN);
		*c2 = C_DIM;
		return;
	}
	snprintf(n1, n, "%s", SBUS_OUT_VIEW_WARN_1);
	snprintf(n2, n, "%s", SBUS_OUT_VIEW_WARN_2);
	*c2 = UI_COL_WARN;
}

bool sbusOutLabel(char *buf, size_t n, unsigned ch, uint8_t thrCh) {
	bool thr = ch == thrCh;
	snprintf(buf, n, thr ? "CH%u THR" : "CH%u", ch + 1u);
	return thr;
}

uint16_t sbusOutRawOf(const SbusOutStatus *st, unsigned ch, uint8_t thrCh) {
	if (ch >= SBUS_CHANNELS) return SBUS_OUT_RAW_LO;
	if (st->frames) return st->sent.channel[ch];
	sbus_frame_t f;
	sbusOutStartFrame(&f, thrCh);
	return f.channel[ch];
}

void sbusOutValueText(char *buf, size_t n, const SbusOutStatus *st, unsigned ch, uint8_t thrCh) {
	snprintf(buf, n, "%u", (unsigned)sbus_to_us(sbusOutRawOf(st, ch, thrCh)));
}

uint16_t sbusOutStepUs(float value, uint16_t topUs) {
	float steps = (value - (float)SBUS_OUT_VIEW_US_LO) / (float)SBUS_OUT_VIEW_STEP_US + 0.5f;
	int k = steps < 0.0f ? 0 : (int)steps;
	unsigned us = SBUS_OUT_VIEW_US_LO + (unsigned)k * SBUS_OUT_VIEW_STEP_US;
	if (us > SBUS_OUT_VIEW_US_HI) us = SBUS_OUT_VIEW_US_HI;
	if (us > topUs) us = topUs;
	return (uint16_t)us;
}

uint16_t sbusOutRawFor(uint16_t us, uint16_t topRaw) {
	uint16_t raw = sbus_from_us(us);
	if (raw < SBUS_OUT_RAW_LO) raw = SBUS_OUT_RAW_LO;
	if (topRaw > SBUS_OUT_RAW_HI) topRaw = SBUS_OUT_RAW_HI;
	if (raw > topRaw) raw = topRaw;
	return raw;
}

int sbusOutUsX(uint16_t us, int x0, int w) {
	if (us < SBUS_OUT_VIEW_US_LO) us = SBUS_OUT_VIEW_US_LO;
	if (us > SBUS_OUT_VIEW_US_HI) us = SBUS_OUT_VIEW_US_HI;
	return x0 + (int)((us - SBUS_OUT_VIEW_US_LO) * (unsigned)(w - 1) /
	                  (SBUS_OUT_VIEW_US_HI - SBUS_OUT_VIEW_US_LO));
}

bool sbusOutOffStart(const sbus_frame_t *f, uint8_t thrCh) {
	if (thrCh < 4u && f->channel[thrCh] > SBUS_OUT_RAW_LO) return true;
	for (unsigned i = 4; i < SBUS_CHANNELS; i++)
		if (f->channel[i] != SBUS_OUT_RAW_LO) return true;
	return f->ch17 || f->ch18;
}

bool sbusOutIdleDue(bool running, const sbus_frame_t *f, uint8_t thrCh, uint32_t nowMs,
                    uint32_t touchMs) {
	return running && sbusOutOffStart(f, thrCh) && (uint32_t)(nowMs - touchMs) > IDLE_DISARM_MS;
}
