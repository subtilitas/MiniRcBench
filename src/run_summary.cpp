#include "run_summary.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "config.h"
#include "diag.h"
#include "esc_task.h"
#include "version.h"

/**
 * @file run_summary.cpp
 * @brief The run summary's update, its figures and its texts. Pure.
 * @see run_summary.h, docs/design/run-summary.md
 */

/** @brief A float reading in 0.01 units, rounded, as the logger logs it. */
static int32_t centi(float v) {
	return v > 0.0f ? (int32_t)(v * 100.0f + 0.5f) : 0;
}

/** @brief Whether @p a beats @p b for a maximum (@p isMax) or a minimum. */
static bool beats(bool isMax, int32_t a, int32_t b) {
	return isMax ? a > b : a < b;
}

/**
 * @brief Count one fresh value in an extreme, if it is new.
 * @param e     The extreme.
 * @param isMax A maximum, else a minimum.
 * @param v     The value.
 * @param kiss  It came from KISS, else from the DShot line.
 * @param stamp Its arrival time.
 */
static void extAdd(RunSumExtreme *e, bool isMax, int32_t v, bool kiss,
                   uint32_t stamp) {
	if (kiss) {
		if (e->anyKiss && e->kissMs == stamp) return;
		e->anyKiss = true;
		e->kissMs = stamp;
	} else {
		if (e->anyLine && e->lineMs == stamp) return;
		e->anyLine = true;
		e->lineMs = stamp;
	}

	if (!e->rawOk || beats(isMax, v, e->raw)) {
		e->raw = v;
		e->rawKiss = kiss;
		e->rawOk = true;
	}

	e->win[0] = e->win[1];         e->win[1] = e->win[2];         e->win[2] = v;
	e->winKiss[0] = e->winKiss[1]; e->winKiss[1] = e->winKiss[2]; e->winKiss[2] = kiss;
	if (e->run < 3) e->run++;
	if (e->run < 3) return;

	// The value all three reach: the lowest of them for a maximum, the
	// highest for a minimum.
	int k = 0;
	for (int i = 1; i < 3; i++)
		if (beats(!isMax, e->win[i], e->win[k])) k = i;
	if (!e->heldOk || beats(isMax, e->win[k], e->held)) {
		e->held = e->win[k];
		e->heldKiss = e->winKiss[k];
		e->heldOk = true;
	}
}

/** @brief The previous sample's state over @p dt µs: armed time and charge. */
static void addInterval(RunSum *s, uint64_t dt) {
	if (s->prevArmed) s->armedUs += dt;
	if (s->curFrame >= 0) {
		uint64_t cap = (uint64_t)(s->curFrameKiss ? KISS_STALE_MS : EDT_STALE_MS) * 1000u;
		uint64_t d = dt < cap ? dt : cap;
		s->ampUs += (uint64_t)s->curFrame * d;
		s->ampKnownUs += d;
		if (d) {
			if (s->curFrameKiss) s->ampSumKiss = true;
			else                 s->ampSumEdt = true;
		}
	}
	if (s->curEdt >= 0) {
		uint64_t cap = (uint64_t)EDT_STALE_MS * 1000u;
		uint64_t d = dt < cap ? dt : cap;
		s->ampEdtUs += (uint64_t)s->curEdt * d;
		s->ampEdtKnownUs += d;
	}
}

void runSumBegin(RunSum *s, uint64_t startUs, bool armed) {
	memset(s, 0, sizeof(*s));
	s->end = RunSumEnd::Running;
	s->startUs = s->lastUs = s->stopUs = startUs;
	s->prevArmed = armed;
	s->everArmed = armed;
	s->curFrame = -1;
	s->curEdt = -1;
}

void runSumAdd(RunSum *s, const RunSumSample *x) {
	if (s->end != RunSumEnd::Running) return;
	const EscTelemetry *t = x->tel;
	const EscReading *r = x->reading;
	uint32_t now = x->nowMs;

	addInterval(s, x->nowUs > s->lastUs ? x->nowUs - s->lastUs : 0);
	s->lastUs = x->nowUs;
	s->prevArmed = x->armed;
	if (x->armed) s->everArmed = true;

	bool kiss = r->kissFresh;
	int32_t kV = centi(t->kissVolts), kA = centi(t->kissAmps);
	int32_t eV = centi(t->volts), eA = centi(t->amps);
	bool eVf = escFieldFresh(t->edtVoltsMs, now, EDT_STALE_MS);
	bool eAf = escFieldFresh(t->edtAmpsMs, now, EDT_STALE_MS);
	bool eTf = escFieldFresh(t->edtTempMs, now, EDT_STALE_MS);

	if (kiss) {
		s->kissSeen = true;
		if (kV > 0) s->kissVoltsSet = true;
		if (kA > 0) s->kissAmpsSet = true;
		if (t->kissTempC != 0) s->kissTempSet = true;
		if (t->kissMah != 0) s->kissMahSet = true;
	}

	// eRPM: DShot's and KISS's apart; the stop chooses.
	if (escFieldFresh(t->lastRpmMs, now, ESC_LINK_STALE_MS))
		extAdd(&s->erpm, true, (int32_t)t->erpm, false, t->lastRpmMs);
	if (kiss && r->haveKissErpm)
		extAdd(&s->kissErpm, true, (int32_t)r->kissErpm, true, t->kissLastMs);

	// Current and temperature: the per-frame figure follows the merge, the
	// EDT-only figure EDT's freshness.
	if (r->ampsFrom == EscSource::Kiss)     extAdd(&s->amps, true, kA, true, t->kissLastMs);
	else if (r->ampsFrom == EscSource::Edt) extAdd(&s->amps, true, eA, false, t->edtAmpsMs);
	if (eAf) extAdd(&s->ampsEdt, true, eA, false, t->edtAmpsMs);

	if (r->tempFrom == EscSource::Kiss)     extAdd(&s->temp, true, t->kissTempC, true, t->kissLastMs);
	else if (r->tempFrom == EscSource::Edt) extAdd(&s->temp, true, t->tempC, false, t->edtTempMs);
	if (eTf) extAdd(&s->tempEdt, true, t->tempC, false, t->edtTempMs);

	// Voltage: while armed only, 0.00 V no reading. Disarmed, the window
	// starts again, so the three values of a held minimum are armed ones.
	if (x->armed) {
		if (r->voltsFrom == EscSource::Kiss && kV > 0)
			extAdd(&s->volts, false, kV, true, t->kissLastMs);
		else if (r->voltsFrom == EscSource::Edt && eV > 0)
			extAdd(&s->volts, false, eV, false, t->edtVoltsMs);
		if (eVf && eV > 0) extAdd(&s->voltsEdt, false, eV, false, t->edtVoltsMs);
	} else {
		s->volts.run = 0;
		s->voltsEdt.run = 0;
	}

	// The KISS counter: the sum of its increases between new readings. A
	// decrease is an ESC that restarted; counting goes on from there.
	if (kiss && (!s->mahSeen || s->mahMs != t->kissLastMs)) {
		if (s->mahSeen && t->kissMah > s->mahLast) s->mahRise += (uint32_t)(t->kissMah - s->mahLast);
		s->mahLast = t->kissMah;
		s->mahMs = t->kissLastMs;
		s->mahSeen = true;
	}

	// The current that holds until the next sample.
	if (r->ampsFrom == EscSource::Kiss)     { s->curFrame = kA; s->curFrameKiss = true; }
	else if (r->ampsFrom == EscSource::Edt) { s->curFrame = eA; s->curFrameKiss = false; }
	else                                      s->curFrame = -1;
	s->curEdt = eAf ? eA : -1;
}

void runSumEnd(RunSum *s, uint64_t stopUs, RunSumEnd how) {
	if (s->end != RunSumEnd::Running || how == RunSumEnd::Running) return;
	addInterval(s, stopUs > s->lastUs ? stopUs - s->lastUs : 0);
	s->lastUs = s->stopUs = stopUs > s->lastUs ? stopUs : s->lastUs;
	s->end = how;
	if (how == RunSumEnd::CardFull)    s->file = RunSumFile::CardFull;
	if (how == RunSumEnd::WriteFailed) s->file = RunSumFile::WriteFailed;
}

void runSumFileResult(RunSum *s, RunSumFile f) { s->file = f; }

/**
 * @brief A figure's value, tagged by the sources it counted.
 * @param e           The figure's extreme.
 * @param lineIsDshot The figure's DShot line source is DShot eRPM, not EDT.
 */
static RunSumValue fromExtreme(const RunSumExtreme *e, bool lineIsDshot) {
	RunSumValue v = {};
	v.heldOk = e->heldOk;   v.held = e->held;   v.heldKiss = e->heldKiss;
	v.rawOk = e->rawOk;     v.raw = e->raw;     v.rawKiss = e->rawKiss;
	if (e->anyKiss && e->anyLine) v.from = RunSumFrom::Mixed;
	else if (e->anyKiss)          v.from = RunSumFrom::Kiss;
	else if (e->anyLine)          v.from = lineIsDshot ? RunSumFrom::DShot : RunSumFrom::Edt;
	return v;
}

/**
 * @brief The per-frame figure, or the EDT-only one when every fresh KISS
 *        frame carried 0 in the field and EDT gave a value.
 */
static RunSumValue pick(const RunSum *s, const RunSumExtreme *frame,
                        const RunSumExtreme *edt, bool kissSet) {
	if (s->kissSeen && !kissSet && edt->rawOk) {
		RunSumValue v = fromExtreme(edt, false);
		v.kissSentZero = true;
		return v;
	}
	return fromExtreme(frame, false);
}

RunSumValue runSumMaxErpm(const RunSum *s) {
	if (s->erpm.rawOk) return fromExtreme(&s->erpm, true);
	return fromExtreme(&s->kissErpm, true);
}

RunSumValue runSumMaxAmps(const RunSum *s) {
	return pick(s, &s->amps, &s->ampsEdt, s->kissAmpsSet);
}

RunSumValue runSumMaxTemp(const RunSum *s) {
	return pick(s, &s->temp, &s->tempEdt, s->kissTempSet);
}

RunSumValue runSumMinVolts(const RunSum *s) {
	return pick(s, &s->volts, &s->voltsEdt, s->kissVoltsSet);
}

/** @brief 0.01 A × µs to mAh, rounded: 1 mAh is 3.6 × 10^8 of them. */
static uint32_t toMah(uint64_t centiAmpUs) {
	return (uint32_t)((centiAmpUs + 180000000u) / 360000000u);
}

RunSumCharge runSumUsedCounter(const RunSum *s) {
	RunSumCharge c = {};
	if (!s->kissSeen) return c;
	if (!s->kissMahSet) { c.counterZero = true; return c; }
	c.from = RunSumFrom::Kiss;
	c.mah = s->mahRise;
	return c;
}

RunSumCharge runSumUsedCurrent(const RunSum *s) {
	RunSumCharge c = {};
	if (s->kissSeen && !s->kissAmpsSet && s->ampEdtKnownUs > 0) {
		c.from = RunSumFrom::Edt;
		c.mah = toMah(s->ampEdtUs);
		c.knownUs = s->ampEdtKnownUs;
		c.kissSentZero = true;
		return c;
	}
	if (s->ampKnownUs == 0) return c;
	c.from = s->ampSumKiss && s->ampSumEdt ? RunSumFrom::Mixed
	       : s->ampSumKiss                 ? RunSumFrom::Kiss : RunSumFrom::Edt;
	c.mah = toMah(s->ampUs);
	c.knownUs = s->ampKnownUs;
	return c;
}

void runSumTxtName(uint32_t n, char *out) {
	snprintf(out, RUN_SUM_NAME_BYTES, "LOG%05lu.TXT", (unsigned long)(n % 100000u));
}

/* ---- Texts ---------------------------------------------------------- */

/** @brief The three quantities with a decimal value. */
enum class Qty : uint8_t { Amps, Volts, Temp };

/**
 * @brief A value as the file or SD LOG writes it.
 * @param q    The quantity.
 * @param v    The value, 0.01 A, 0.01 V or °C.
 * @param kiss It came from KISS: current in hundredths, EDT current whole.
 * @param tile SD LOG's form, no space and a backtick for the degree.
 */
static void fmtQty(char *b, size_t n, Qty q, int32_t v, bool kiss, bool tile) {
	const char *sp = tile ? "" : " ";
	switch (q) {
		case Qty::Amps:
			if (kiss) snprintf(b, n, "%ld.%02ld%sA", (long)(v / 100), (long)(v % 100), sp);
			else      snprintf(b, n, "%ld%sA", (long)(v / 100), sp);
			break;
		case Qty::Volts:
			snprintf(b, n, "%ld.%02ld%sV", (long)(v / 100), (long)(v % 100), sp);
			break;
		case Qty::Temp:
			snprintf(b, n, "%ld%s", (long)v, tile ? "`C" : " C");
			break;
	}
}

/** @brief The step a source's value comes in. */
static const char *stepText(Qty q, bool kiss) {
	switch (q) {
		case Qty::Amps:  return kiss ? "0.01 A" : "1 A";
		case Qty::Volts: return kiss ? "0.01 V" : "0.25 V";
		case Qty::Temp:  break;
	}
	return "1 C";
}

/** @brief Tenths of a second, rounded, from µs. */
static unsigned long tenths(uint64_t us) {
	return (unsigned long)((us + 50000u) / 100000u);
}

/** @brief An appending writer that never overruns and counts the whole length. */
struct Out {
	char *p;      /**< The buffer. */
	size_t n;     /**< Its size. */
	size_t len;   /**< Characters the text needs, beyond @ref n when cut. */
};

/** @brief Append printf-style to @p o. */
static void put(Out *o, const char *fmt, ...) __attribute__((format(printf, 2, 3)));

static void put(Out *o, const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	char *dst = o->len < o->n ? o->p + o->len : nullptr;
	size_t room = o->len < o->n ? o->n - o->len : 0;
	int k = vsnprintf(dst, room, fmt, ap);
	va_end(ap);
	if (k > 0) o->len += (size_t)k;
}

/** @brief Source names in the file. */
static const char *fromText(RunSumFrom f) {
	switch (f) {
		case RunSumFrom::DShot: return "DShot";
		case RunSumFrom::Kiss:  return "KISS";
		case RunSumFrom::Edt:   return "EDT";
		case RunSumFrom::Mixed: return "KISS and EDT";
		case RunSumFrom::None:  break;
	}
	return "none";
}

/**
 * @brief One line of current, temperature or voltage.
 * @param o     The text.
 * @param name  The line's name.
 * @param q     The quantity.
 * @param v     The value.
 * @param isMax A maximum, else the minimum voltage.
 * @param none  The text for no value.
 */
static void putQtyLine(Out *o, const char *name, Qty q, const RunSumValue &v,
                       bool isMax, const char *none) {
	put(o, "%s: ", name);
	if (v.from == RunSumFrom::None) { put(o, "unknown (%s)\n", none); return; }
	char b[24];
	if (v.heldOk) { fmtQty(b, sizeof(b), q, v.held, v.heldKiss, false); put(o, "%s (", b); }
	else          put(o, "unknown (");
	put(o, "%s, ", fromText(v.from));
	if (v.from == RunSumFrom::Mixed) {
		const char *ks = stepText(q, true), *es = stepText(q, false);
		if (strcmp(ks, es) == 0) put(o, "%s steps", ks);
		else                     put(o, "%s and %s steps", ks, es);
	} else {
		put(o, "%s steps", stepText(q, v.from == RunSumFrom::Kiss));
	}
	if (v.kissSentZero) put(o, ", KISS sent 0");
	if (v.from == RunSumFrom::Mixed && v.heldOk)
		put(o, ", %s from %s", isMax ? "peak" : "minimum", v.heldKiss ? "KISS" : "EDT");
	if (!v.heldOk) put(o, ", fewer than 3 new values in a row");
	if (!isMax) put(o, ", while armed");
	fmtQty(b, sizeof(b), q, v.raw, v.rawKiss, false);
	put(o, "; raw %s)\n", b);
}

/** @brief How a log ended, in the file. */
static const char *endText(RunSumEnd e) {
	switch (e) {
		case RunSumEnd::Stop:        return "STOP";
		case RunSumEnd::Disarm:      return "DISARM";
		case RunSumEnd::ByKst:       return "KST SERVO";
		case RunSumEnd::ByHitec:     return "HITEC SERVO";
		case RunSumEnd::ByAm32:      return "AM32 CONFIG";
		case RunSumEnd::CardFull:    return "CARD FULL";
		case RunSumEnd::WriteFailed: return "WRITE FAILED";
		case RunSumEnd::Running:     break;
	}
	return "RUNNING";
}

size_t runSumText(const RunSum *s, const SdLogStatus *st, uint8_t poles,
                  char *out, size_t n) {
	Out o = { out, n, 0 };
	if (n) out[0] = '\0';
	uint64_t dur = s->stopUs - s->startUs;

	put(&o, "MiniRcBench run summary, format %d\n", RUN_SUM_FORMAT);
	put(&o, "Log: LOG%05lu.BFL\n", (unsigned long)st->fileNumber);
	put(&o, "Firmware: %s\n", FW_VERSION_STRING);
	put(&o, "Clock at start: not set\n");
	put(&o, "Clock at stop: not set\n");
	put(&o, "Ended by: %s\n", endText(s->end));
	put(&o, "Duration: %lu.%lu s\n", tenths(dur) / 10u, tenths(dur) % 10u);
	put(&o, "Armed: %lu.%lu s\n", tenths(s->armedUs) / 10u, tenths(s->armedUs) % 10u);

	RunSumValue e = runSumMaxErpm(s);
	put(&o, "Max RPM: ");
	if (e.from == RunSumFrom::None) {
		put(&o, "unknown (no eRPM from DShot or KISS)\n");
	} else {
		bool kiss = e.from == RunSumFrom::Kiss;
		if (e.heldOk) put(&o, "%lu RPM (%s, eRPM %lu%s, %u poles; ",
		                  (unsigned long)runSumRpm((uint32_t)e.held, poles), fromText(e.from),
		                  (unsigned long)e.held, kiss ? " in 100 eRPM steps" : "", (unsigned)poles);
		else          put(&o, "unknown (%s, fewer than 3 new values in a row, %u poles; ",
		                  fromText(e.from), (unsigned)poles);
		put(&o, "raw %lu RPM)\n", (unsigned long)runSumRpm((uint32_t)e.raw, poles));
	}

	putQtyLine(&o, "Max current", Qty::Amps, runSumMaxAmps(s), true,
	           "no current from KISS or EDT");
	putQtyLine(&o, "Max temperature", Qty::Temp, runSumMaxTemp(s), true,
	           "no temperature from KISS or EDT");
	if (!s->everArmed) put(&o, "Min voltage: unknown (never armed)\n");
	else putQtyLine(&o, "Min voltage", Qty::Volts, runSumMinVolts(s), false,
	                "no voltage from KISS or EDT");

	RunSumCharge c = runSumUsedCounter(s);
	if (c.from != RunSumFrom::None)
		put(&o, "Used, ESC counter: %lu mAh (KISS, 1 mAh steps)\n", (unsigned long)c.mah);
	else
		put(&o, "Used, ESC counter: unknown (%s)\n",
		    c.counterZero ? "KISS counter stayed 0" : "no KISS frame");

	c = runSumUsedCurrent(s);
	if (c.from != RunSumFrom::None)
		put(&o, "Used, from current: %lu mAh (%s%s, current known for %lu.%lu s of %lu.%lu s)\n",
		    (unsigned long)c.mah, fromText(c.from), c.kissSentZero ? ", KISS sent 0" : "",
		    tenths(c.knownUs) / 10u, tenths(c.knownUs) % 10u,
		    tenths(dur) / 10u, tenths(dur) % 10u);
	else
		put(&o, "Used, from current: unknown (no current from KISS or EDT)\n");

	put(&o, "Frames: %lu\n", (unsigned long)st->framesLogged);
	put(&o, "Dropped frames: %lu (%lu bytes)\n", (unsigned long)st->dropEvents,
	    (unsigned long)st->bytesDropped);
	put(&o, "Written: %lu kB\n", (unsigned long)(st->bytesWritten / 1024u));
	put(&o, "Buffer peak: %lu of %u bytes\n", (unsigned long)st->peakBuffer,
	    (unsigned)SD_LOG_BUFFER_BYTES);
	put(&o, "Worst flush: %lu ms\n", (unsigned long)st->worstFlushMs);
	return o.len;
}

/** @brief SD LOG's tag for a source. */
static const char *tagText(RunSumFrom f) {
	switch (f) {
		case RunSumFrom::Kiss:  return "KISS";
		case RunSumFrom::Edt:   return "EDT";
		case RunSumFrom::Mixed: return "MIXED";
		case RunSumFrom::DShot:
		case RunSumFrom::None:  break;
	}
	return nullptr;
}

/** @brief A row's value from a held figure, `--` without one. */
static void rowQty(RunSumRowText *o, Qty q, const RunSumValue &v) {
	if (!v.heldOk) return;
	fmtQty(o->value, sizeof(o->value), q, v.held, v.heldKiss, true);
	o->tag = tagText(v.from);
	o->unknown = false;
}

/** @brief The largest charge SD LOG writes in full. */
#define RUN_SUM_ROW_MAH_MAX 99999u

void runSumRowText(const RunSum *s, RunSumRow row, uint8_t poles,
                   uint32_t fileNumber, RunSumRowText *o) {
	o->label = "";
	o->tag = nullptr;
	o->unknown = true;
	o->alert = false;
	snprintf(o->value, sizeof(o->value), "--");

	switch (row) {
		case RunSumRow::MaxRpm: {
			o->label = "MAX RPM";
			RunSumValue v = runSumMaxErpm(s);
			if (!v.heldOk) break;
			snprintf(o->value, sizeof(o->value), "%lu",
			         (unsigned long)runSumRpm((uint32_t)v.held, poles));
			o->tag = tagText(v.from);
			o->unknown = false;
			break;
		}
		case RunSumRow::MaxCurrent:
			o->label = "MAX CURRENT";
			rowQty(o, Qty::Amps, runSumMaxAmps(s));
			break;
		case RunSumRow::MaxTemp: {
			o->label = "MAX TEMP";
			RunSumValue v = runSumMaxTemp(s);
			rowQty(o, Qty::Temp, v);
			o->alert = v.heldOk && v.held >= RUN_SUM_TEMP_HOT_C;
			break;
		}
		case RunSumRow::MinVoltage:
			o->label = "MIN VOLTAGE";
			if (s->everArmed) rowQty(o, Qty::Volts, runSumMinVolts(s));
			break;
		case RunSumRow::Used: {
			o->label = "USED";
			RunSumCharge c = runSumUsedCounter(s);
			if (c.from == RunSumFrom::None || c.mah == 0) {
				c = runSumUsedCurrent(s);
				if (c.from == RunSumFrom::None) break;
				o->label = "USED FROM CURRENT";
			}
			if (c.mah > RUN_SUM_ROW_MAH_MAX)
				snprintf(o->value, sizeof(o->value), ">%lumAh", (unsigned long)RUN_SUM_ROW_MAH_MAX);
			else
				snprintf(o->value, sizeof(o->value), "%lumAh", (unsigned long)c.mah);
			o->tag = tagText(c.from);
			o->unknown = false;
			break;
		}
		case RunSumRow::Armed:
			o->label = "ARMED";
			diagUptimeText(o->value, sizeof(o->value), (uint32_t)(s->armedUs / 1000000u));
			o->unknown = false;
			break;
		case RunSumRow::Summary:
			o->label = "SUMMARY";
			switch (s->file) {
				case RunSumFile::Saved:
					runSumTxtName(fileNumber, o->value);
					o->unknown = false;
					break;
				case RunSumFile::CardFull:
					snprintf(o->value, sizeof(o->value), "NOT SAVED: CARD FULL");
					o->unknown = false;
					o->alert = true;
					break;
				case RunSumFile::WriteFailed:
					snprintf(o->value, sizeof(o->value), "NOT SAVED: WRITE FAILED");
					o->unknown = false;
					o->alert = true;
					break;
				case RunSumFile::NotWritten:
					break;
			}
			break;
		case RunSumRow::Count:
			break;
	}
}
