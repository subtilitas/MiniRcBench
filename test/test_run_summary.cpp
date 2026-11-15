/**
 * @file test_run_summary.cpp
 * @brief The run summary: held and raw extremes, sources, the minimum
 *        voltage while armed, the charge, the armed time, the `.TXT` text
 *        and SD LOG's run rows. @see run_summary.h
 *
 * Every sample goes through escMerge() as the logger's would, with
 * KISS_STALE_MS and EDT_STALE_MS, at the logger's 500 Hz unless a test says
 * otherwise.
 */

#include "check.h"
#include "config.h"
#include "esc_merge.h"
#include "esc_task.h"
#include "gfx.h"
#include "run_summary.h"
#include "sd_log.h"
#include "version.h"

#include <string.h>

namespace {

/** @brief A log in progress: the telemetry block, the clocks, the arm state. */
struct Sim {
	RunSum       s;
	EscTelemetry t;
	uint32_t     ms;
	uint64_t     us;
	bool         armed;
};

void simBegin(Sim *m, bool armed, uint32_t ms0 = 1000) {
	memset(&m->t, 0, sizeof(m->t));
	m->ms = ms0;
	m->us = (uint64_t)ms0 * 1000u;
	m->armed = armed;
	runSumBegin(&m->s, m->us, armed);
}

/** @brief Move both clocks on. */
void simStep(Sim *m, uint32_t dms = 2) {
	m->ms += dms;
	m->us += (uint64_t)dms * 1000u;
}

/** @brief One logged frame from the block as it stands. */
void simFeed(Sim *m) {
	EscReading r;
	escMerge(&m->t, m->ms, KISS_STALE_MS, EDT_STALE_MS, &r);
	RunSumSample x = { &m->t, &r, m->ms, m->us, m->armed };
	runSumAdd(&m->s, &x);
}

/** @brief Step, then feed. */
void simTick(Sim *m, uint32_t dms = 2) {
	simStep(m, dms);
	simFeed(m);
}

void kissFrame(Sim *m, float volts, float amps, int temp, uint16_t mah,
               uint32_t erpm = 0) {
	m->t.kissVolts = volts;
	m->t.kissAmps = amps;
	m->t.kissTempC = (int16_t)temp;
	m->t.kissMah = mah;
	m->t.kissErpm = erpm;
	m->t.kissLastMs = m->ms;
	m->t.haveKiss = true;
}

void edtVolts(Sim *m, float v) { m->t.volts = v;            m->t.edtVoltsMs = m->ms; }
void edtAmps(Sim *m, int a)    { m->t.amps = (float)a;      m->t.edtAmpsMs = m->ms; }
void edtTemp(Sim *m, int c)    { m->t.tempC = (int16_t)c;   m->t.edtTempMs = m->ms; }
void dshot(Sim *m, uint32_t e) { m->t.erpm = e;             m->t.lastRpmMs = m->ms; }

/** @brief The summary's text with the counters of @p st, or zeros. */
void text(const RunSum *s, char *buf, size_t n, uint8_t poles = 14,
          const SdLogStatus *st = nullptr) {
	SdLogStatus z = {};
	z.fileNumber = 12;
	runSumText(s, st ? st : &z, poles, buf, n);
}

/** @brief The line of @p t that starts with @p prefix, without its line feed. */
const char *lineOf(const char *t, const char *prefix, char *buf, size_t n) {
	size_t pl = strlen(prefix);
	for (const char *p = t; *p; ) {
		const char *e = strchr(p, '\n');
		size_t len = e ? (size_t)(e - p) : strlen(p);
		if (len >= pl && strncmp(p, prefix, pl) == 0) {
			if (len >= n) len = n - 1;
			memcpy(buf, p, len);
			buf[len] = '\0';
			return buf;
		}
		if (!e) break;
		p = e + 1;
	}
	buf[0] = '\0';
	return buf;
}

/** @brief Check one line of an ended summary's text. */
void checkLine(const char *what, const RunSum *s, const char *prefix,
               const char *want, uint8_t poles = 14) {
	char t[1200], l[200];
	text(s, t, sizeof(t), poles);
	lineOf(t, prefix, l, sizeof(l));
	checkStr(what, l, want);
}

void checkRow(const char *what, const RunSum *s, RunSumRow row,
              const char *value, const char *tag, uint8_t poles = 14,
              uint32_t fileNumber = 12) {
	RunSumRowText o;
	runSumRowText(s, row, poles, fileNumber, &o);
	char w[64];
	snprintf(w, sizeof(w), "%s value", what);
	checkStr(w, o.value, value);
	snprintf(w, sizeof(w), "%s tag", what);
	checkStr(w, o.tag ? o.tag : "(none)", tag ? tag : "(none)");
}

void end(Sim *m, RunSumEnd how = RunSumEnd::Stop) {
	simStep(m);
	runSumEnd(&m->s, m->us, how);
}

} // namespace

/** @brief Three new values make a held maximum; one or two outliers do not. */
static void testHeld() {
	section("Run summary: held maximum over three new values");
	Sim m;
	simBegin(&m, true);
	const uint32_t seq[] = { 1000, 1000, 1000 };
	for (uint32_t v : seq) { simStep(&m); dshot(&m, v); simFeed(&m); }
	RunSumValue v = runSumMaxErpm(&m.s);
	checkTrue("three values: held", v.heldOk);
	checkInt("three values: held eRPM", v.held, 1000);

	// One damaged frame between good ones.
	simStep(&m); dshot(&m, 60000000); simFeed(&m);
	simStep(&m); dshot(&m, 900); simFeed(&m);
	simStep(&m); dshot(&m, 900); simFeed(&m);
	v = runSumMaxErpm(&m.s);
	checkInt("one outlier: held", v.held, 1000);
	checkInt("one outlier: raw", v.raw, 60000000);
	for (int i = 0; i < 2; i++) { simStep(&m); dshot(&m, 1100); simFeed(&m); }
	checkInt("two at 1100: held", runSumMaxErpm(&m.s).held, 1000);
	simStep(&m); dshot(&m, 1100); simFeed(&m);
	checkInt("three at 1100: held", runSumMaxErpm(&m.s).held, 1100);

	// Two outliers in a row still set nothing; a third does.
	simStep(&m); dshot(&m, 50000); simFeed(&m);
	simStep(&m); dshot(&m, 50000); simFeed(&m);
	simStep(&m); dshot(&m, 1100); simFeed(&m);
	checkInt("two outliers: held", runSumMaxErpm(&m.s).held, 1100);
	simStep(&m); dshot(&m, 50000); simFeed(&m);
	simStep(&m); dshot(&m, 50000); simFeed(&m);
	simStep(&m); dshot(&m, 50000); simFeed(&m);
	checkInt("three outliers: held", runSumMaxErpm(&m.s).held, 50000);
	checkInt("raw keeps the highest", runSumMaxErpm(&m.s).raw, 60000000);

	// The held value is the lowest of the three: 900, 2000, 1500 hold 900.
	Sim n;
	simBegin(&n, true);
	const uint32_t seq2[] = { 900, 2000, 1500 };
	for (uint32_t x : seq2) { simStep(&n); dshot(&n, x); simFeed(&n); }
	checkInt("held is the lowest of three", runSumMaxErpm(&n.s).held, 900);
	simStep(&n); dshot(&n, 1800); simFeed(&n);
	checkInt("next window 2000 1500 1800", runSumMaxErpm(&n.s).held, 1500);
}

/** @brief A value repeated without a new arrival time counts once. */
static void testRepeat() {
	section("Run summary: a value without a new arrival counts once");
	Sim m;
	simBegin(&m, true);
	simStep(&m); dshot(&m, 5000);
	for (int i = 0; i < 5; i++) { simFeed(&m); simStep(&m, 1); }
	RunSumValue v = runSumMaxErpm(&m.s);
	checkTrue("five samples of one frame: no held value", !v.heldOk);
	checkInt("raw", v.raw, 5000);
	checkInt("one value in the window", m.s.erpm.run, 1);

	// KISS: one frame sampled 20 times at 500 Hz is one value.
	Sim k;
	simBegin(&k, true);
	simStep(&k); kissFrame(&k, 12.0f, 50.0f, 40, 0);
	for (int i = 0; i < 20; i++) simTick(&k);
	checkInt("KISS frame sampled 20 times: run", k.s.amps.run, 1);
	checkTrue("no held current", !runSumMaxAmps(&k.s).heldOk);
}

/** @brief Stale fields are not counted. */
static void testStale() {
	section("Run summary: stale fields not counted");
	Sim m;
	simBegin(&m, true);
	// eRPM 500 ms old, an EDT current 1000 ms old, a KISS frame 500 ms old.
	simStep(&m); dshot(&m, 7000); edtAmps(&m, 40); kissFrame(&m, 15.0f, 0.0f, 0, 0, 4000);
	simStep(&m, 500);
	m.t.edtAmpsMs = m.ms - 1000;
	simFeed(&m);
	checkTrue("eRPM 500 ms old: none", !m.s.erpm.rawOk);
	checkTrue("KISS eRPM 500 ms old: none", !m.s.kissErpm.rawOk);
	checkTrue("EDT current 1000 ms old: none", !m.s.ampsEdt.rawOk);
	checkTrue("per-frame current: none", !m.s.amps.rawOk);
	checkTrue("KISS 500 ms old: not seen", !m.s.kissSeen);

	// One millisecond younger, each counts.
	Sim n;
	simBegin(&n, true);
	simStep(&n); dshot(&n, 7000); kissFrame(&n, 15.0f, 0.0f, 0, 0, 4000);
	simStep(&n, 499);
	edtAmps(&n, 40);
	n.t.edtAmpsMs = n.ms - 999;
	simFeed(&n);
	checkInt("eRPM 499 ms old: counted", n.s.erpm.raw, 7000);
	checkInt("KISS eRPM 499 ms old: counted", n.s.kissErpm.raw, 4000);
	checkInt("EDT current 999 ms old: counted", n.s.ampsEdt.raw, 4000);
	checkTrue("KISS 499 ms old: seen", n.s.kissSeen);
}

/** @brief A KISS gap over the peak: the peak from EDT, tagged MIXED. */
static void testMixed() {
	section("Run summary: a KISS gap filled by EDT, MIXED");
	Sim m;
	simBegin(&m, true);
	// KISS at 50 Hz: 10.00 A to 23.41 A.
	const float ka[] = { 10.0f, 20.0f, 23.41f, 23.41f, 23.41f, 22.0f };
	for (float a : ka) {
		simStep(&m); kissFrame(&m, 15.0f, a, 40, 100); simFeed(&m);
		for (int i = 0; i < 9; i++) simTick(&m);
	}
	RunSumValue v = runSumMaxAmps(&m.s);
	checkTrue("KISS only: tag KISS", v.from == RunSumFrom::Kiss);
	checkInt("KISS only: held 0.01 A", v.held, 2341);

	// KISS stops for 600 ms; EDT carries 31, 30, 32, 31 A at 100 ms.
	simStep(&m, 500);
	const int ea[] = { 31, 30, 32, 31 };
	for (int a : ea) { simStep(&m, 100); edtAmps(&m, a); simFeed(&m); }
	simStep(&m); kissFrame(&m, 15.0f, 21.0f, 40, 110); simFeed(&m);
	end(&m);
	v = runSumMaxAmps(&m.s);
	checkTrue("tag MIXED", v.from == RunSumFrom::Mixed);
	checkInt("held from EDT", v.held, 3000);
	checkTrue("held came from EDT", !v.heldKiss);
	checkInt("raw from EDT", v.raw, 3200);
	checkLine("text", &m.s, "Max current:",
	          "Max current: 30 A (KISS and EDT, 0.01 A and 1 A steps, peak from EDT; raw 32 A)");
	checkRow("row", &m.s, RunSumRow::MaxCurrent, "30A", "MIXED");

	// The reverse: EDT first, then KISS sets the peak; drawn as KISS draws it.
	Sim n;
	simBegin(&n, true);
	for (int i = 0; i < 3; i++) { simStep(&n, 10); edtAmps(&n, 5); simFeed(&n); }
	for (int i = 0; i < 3; i++) { simStep(&n, 20); kissFrame(&n, 15.0f, 12.34f, 40, 0); simFeed(&n); }
	end(&n);
	checkLine("KISS peak text", &n.s, "Max current:",
	          "Max current: 12.34 A (KISS and EDT, 0.01 A and 1 A steps, peak from KISS; raw 12.34 A)");
	checkRow("KISS peak row", &n.s, RunSumRow::MaxCurrent, "12.34A", "MIXED");

	// The held value's source is that of the value it holds, not the newest.
	Sim h;
	simBegin(&h, true);
	for (int i = 0; i < 2; i++) { simStep(&h, 10); edtAmps(&h, 30); simFeed(&h); }
	simStep(&h, 20); kissFrame(&h, 15.0f, 40.0f, 40, 0); simFeed(&h);
	end(&h);
	checkLine("held from the older EDT value", &h.s, "Max current:",
	          "Max current: 30 A (KISS and EDT, 0.01 A and 1 A steps, peak from EDT; raw 40.00 A)");

	// Temperature: one step for both sources.
	Sim t;
	simBegin(&t, true);
	for (int i = 0; i < 3; i++) { simStep(&t, 20); kissFrame(&t, 15.0f, 1.0f, 50, 0); simFeed(&t); }
	simStep(&t, 600);
	for (int i = 0; i < 3; i++) { simStep(&t, 10); edtTemp(&t, 95); simFeed(&t); }
	end(&t);
	checkLine("temperature text", &t.s, "Max temperature:",
	          "Max temperature: 95 C (KISS and EDT, 1 C steps, peak from EDT; raw 95 C)");
	RunSumRowText o;
	runSumRowText(&t.s, RunSumRow::MaxTemp, 14, 12, &o);
	checkStr("temperature row", o.value, "95`C");
	checkTrue("95 C: alert", o.alert);
}

/** @brief KISS at 0 in every fresh frame: the EDT-only figure. */
static void testKissZero() {
	section("Run summary: KISS sends 0, EDT's figure reported");
	Sim m;
	simBegin(&m, true);
	const int ea[] = { 23, 23, 23, 25, 22 };
	for (int a : ea) {
		simStep(&m, 20);
		kissFrame(&m, 0.0f, 0.0f, 0, 0);
		edtAmps(&m, a); edtTemp(&m, 60); edtVolts(&m, 14.75f);
		simFeed(&m);
	}
	end(&m);
	RunSumValue v = runSumMaxAmps(&m.s);
	checkTrue("current: EDT", v.from == RunSumFrom::Edt);
	checkTrue("current: KISS sent 0", v.kissSentZero);
	checkLine("current text", &m.s, "Max current:",
	          "Max current: 23 A (EDT, 1 A steps, KISS sent 0; raw 25 A)");
	checkLine("temperature text", &m.s, "Max temperature:",
	          "Max temperature: 60 C (EDT, 1 C steps, KISS sent 0; raw 60 C)");
	checkLine("voltage text", &m.s, "Min voltage:",
	          "Min voltage: 14.75 V (EDT, 0.25 V steps, KISS sent 0, while armed; raw 14.75 V)");
	checkRow("current row", &m.s, RunSumRow::MaxCurrent, "23A", "EDT");
	checkRow("voltage row", &m.s, RunSumRow::MinVoltage, "14.75V", "EDT");

	// One fresh KISS frame with a current: the per-frame figure again.
	Sim n;
	simBegin(&n, true);
	for (int i = 0; i < 4; i++) {
		simStep(&n, 20);
		kissFrame(&n, 15.0f, i == 3 ? 0.5f : 0.0f, 30, 0);
		edtAmps(&n, 20);
		simFeed(&n);
	}
	end(&n);
	v = runSumMaxAmps(&n.s);
	checkTrue("one KISS current: per frame", !v.kissSentZero && v.from == RunSumFrom::Kiss);
	checkInt("held from KISS", v.held, 0);

	// No KISS frame at all: EDT per frame, no "KISS sent 0".
	Sim e;
	simBegin(&e, true);
	for (int i = 0; i < 3; i++) { simStep(&e, 20); edtAmps(&e, 7); simFeed(&e); }
	end(&e);
	checkLine("EDT only", &e.s, "Max current:", "Max current: 7 A (EDT, 1 A steps; raw 7 A)");

	// KISS at 0 A and no EDT current: KISS's own zeros are the figure.
	Sim k;
	simBegin(&k, true);
	for (int i = 0; i < 3; i++) { simStep(&k, 20); kissFrame(&k, 15.0f, 0.0f, 0, 0); simFeed(&k); }
	// An EDT temperature of 99 C, 1000 ms old by the next sample.
	simStep(&k, 20); edtTemp(&k, 99);
	simStep(&k, 1000); simFeed(&k);
	end(&k);
	checkLine("KISS 0 A, no EDT", &k.s, "Max current:",
	          "Max current: 0.00 A (KISS, 0.01 A steps; raw 0.00 A)");
	checkLine("KISS 0 A, no EDT: charge", &k.s, "Used, from current:",
	          "Used, from current: 0 mAh (KISS, current known for 0.5 s of 1.1 s)");
	checkTrue("a stale EDT temperature not counted", !k.s.tempEdt.rawOk);
}

/** @brief The minimum voltage counts only while armed; 0.00 V is no reading. */
static void testVoltsArmed() {
	section("Run summary: minimum voltage while armed");
	Sim m;
	simBegin(&m, false);
	// Disarmed: the pack plugged in, the ESC powering up at 3 V.
	for (int i = 0; i < 5; i++) { simStep(&m, 20); kissFrame(&m, 3.0f, 0.0f, 25, 0); simFeed(&m); }
	m.armed = true;
	const float kv[] = { 15.10f, 14.82f, 14.79f, 14.82f, 14.80f, 14.90f };
	for (float x : kv) { simStep(&m, 20); kissFrame(&m, x, 10.0f, 30, 0); simFeed(&m); }
	// A frame of 0.00 V while armed is no reading.
	simStep(&m, 20); kissFrame(&m, 0.0f, 10.0f, 30, 0); simFeed(&m);
	m.armed = false;
	for (int i = 0; i < 5; i++) { simStep(&m, 20); kissFrame(&m, 2.0f, 0.0f, 25, 0); simFeed(&m); }
	end(&m);
	RunSumValue v = runSumMinVolts(&m.s);
	checkInt("held: the highest of the lowest three", v.held, 1482);
	checkInt("raw while armed", v.raw, 1479);
	checkLine("text", &m.s, "Min voltage:",
	          "Min voltage: 14.82 V (KISS, 0.01 V steps, while armed; raw 14.79 V)");

	// Never armed.
	Sim n;
	simBegin(&n, false);
	for (int i = 0; i < 5; i++) { simStep(&n, 20); kissFrame(&n, 15.0f, 1.0f, 25, 0); simFeed(&n); }
	end(&n);
	checkLine("never armed", &n.s, "Min voltage:", "Min voltage: unknown (never armed)");
	checkRow("never armed row", &n.s, RunSumRow::MinVoltage, "--", nullptr);

	// Armed with no voltage, and armed with only 0.00 V and EDT stale.
	Sim z;
	simBegin(&z, true);
	for (int i = 0; i < 5; i++) { simStep(&z, 20); dshot(&z, 100); simFeed(&z); }
	end(&z);
	checkLine("no voltage", &z.s, "Min voltage:", "Min voltage: unknown (no voltage from KISS or EDT)");

	// Three armed values split by a disarm hold nothing.
	Sim w;
	simBegin(&w, true);
	for (int i = 0; i < 2; i++) { simStep(&w, 20); edtVolts(&w, 14.0f); simFeed(&w); }
	w.armed = false;
	simStep(&w, 20); edtVolts(&w, 14.0f); simFeed(&w);
	w.armed = true;
	simStep(&w, 20); edtVolts(&w, 14.0f); simFeed(&w);
	end(&w);
	checkTrue("two, disarm, one: no held minimum", !runSumMinVolts(&w.s).heldOk);
	checkLine("text", &w.s, "Min voltage:",
	          "Min voltage: unknown (EDT, 0.25 V steps, fewer than 3 new values in a row, while armed; raw 14.00 V)");
	checkRow("its row", &w.s, RunSumRow::MinVoltage, "--", nullptr);

	// EDT at 0.00 V is no reading, per frame and in the EDT-only figure.
	Sim e;
	simBegin(&e, true);
	for (int i = 0; i < 3; i++) { simStep(&e, 20); edtVolts(&e, 0.0f); simFeed(&e); }
	end(&e);
	checkTrue("EDT 0.00 V: no per-frame value", !e.s.volts.rawOk);
	checkTrue("EDT 0.00 V: no EDT-only value", !e.s.voltsEdt.rawOk);

	// The EDT-only figure: a stale EDT voltage does not count, and a disarm
	// starts its three values again.
	Sim k;
	simBegin(&k, true);
	for (int i = 0; i < 2; i++) {
		simStep(&k, 20); kissFrame(&k, 0.0f, 0.0f, 0, 0); edtVolts(&k, 14.5f); simFeed(&k);
	}
	k.armed = false;
	simStep(&k, 20); kissFrame(&k, 0.0f, 0.0f, 0, 0); simFeed(&k);
	k.armed = true;
	simStep(&k, 20); kissFrame(&k, 0.0f, 0.0f, 0, 0); edtVolts(&k, 14.5f); simFeed(&k);
	// An EDT voltage of 10.00 V, 1000 ms old by the next sample.
	simStep(&k, 20); edtVolts(&k, 10.0f);
	simStep(&k, 1000); kissFrame(&k, 0.0f, 0.0f, 0, 0); simFeed(&k);
	end(&k);
	RunSumValue v2 = runSumMinVolts(&k.s);
	checkTrue("EDT-only: two, disarm, one: no held minimum", v2.kissSentZero && !v2.heldOk);
	checkInt("EDT-only: a stale 10.00 V not counted", v2.raw, 1450);
}

/** @brief Decision 3: the maxima cover the whole log, armed or not. */
static void testWholeLog() {
	section("Run summary: maxima over the whole log");
	Sim m;
	simBegin(&m, false);
	for (int i = 0; i < 3; i++) { simStep(&m, 20); kissFrame(&m, 15.0f, 40.0f, 70, 0, 9000); dshot(&m, 9900); simFeed(&m); }
	m.armed = true;
	for (int i = 0; i < 3; i++) { simStep(&m, 20); kissFrame(&m, 15.0f, 20.0f, 50, 0, 5000); dshot(&m, 5500); simFeed(&m); }
	m.armed = false;
	end(&m);
	checkInt("current before arming counts", runSumMaxAmps(&m.s).held, 4000);
	checkInt("temperature before arming counts", runSumMaxTemp(&m.s).held, 70);
	checkInt("eRPM before arming counts", runSumMaxErpm(&m.s).held, 9900);
}

/** @brief KISS eRPM only without DShot eRPM; RPM divided at the stop. */
static void testRpm() {
	section("Run summary: RPM sources");
	Sim m;
	simBegin(&m, true);
	const uint32_t ke[] = { 128800, 128800, 128800, 128900 };
	for (uint32_t e : ke) { simStep(&m, 20); kissFrame(&m, 15.0f, 1.0f, 30, 0, e); simFeed(&m); }
	end(&m);
	checkTrue("KISS eRPM without DShot", runSumMaxErpm(&m.s).from == RunSumFrom::Kiss);
	checkLine("KISS text", &m.s, "Max RPM:",
	          "Max RPM: 18400 RPM (KISS, eRPM 128800 in 100 eRPM steps, 14 poles; raw 18414 RPM)");
	checkRow("KISS row", &m.s, RunSumRow::MaxRpm, "18400", "KISS");
	checkLine("12 poles at the stop", &m.s, "Max RPM:",
	          "Max RPM: 21466 RPM (KISS, eRPM 128800 in 100 eRPM steps, 12 poles; raw 21483 RPM)", 12);

	// One DShot frame anywhere in the log makes DShot the source.
	Sim n;
	simBegin(&n, true);
	for (uint32_t e : ke) { simStep(&n, 20); kissFrame(&n, 15.0f, 1.0f, 30, 0, e); simFeed(&n); }
	simStep(&n); dshot(&n, 129220); simFeed(&n);
	end(&n);
	checkTrue("one DShot frame: DShot", runSumMaxErpm(&n.s).from == RunSumFrom::DShot);
	checkLine("DShot, fewer than three", &n.s, "Max RPM:",
	          "Max RPM: unknown (DShot, fewer than 3 new values in a row, 14 poles; raw 18460 RPM)");
	checkRow("DShot row unknown", &n.s, RunSumRow::MaxRpm, "--", nullptr);

	Sim d;
	simBegin(&d, true);
	const uint32_t de[] = { 129150, 129220, 129150, 129200 };
	for (uint32_t e : de) { simStep(&d); dshot(&d, e); simFeed(&d); }
	end(&d);
	checkLine("DShot text", &d.s, "Max RPM:",
	          "Max RPM: 18450 RPM (DShot, eRPM 129150, 14 poles; raw 18460 RPM)");
	checkRow("DShot row", &d.s, RunSumRow::MaxRpm, "18450", nullptr);

	Sim z;
	simBegin(&z, true);
	end(&z);
	checkLine("no eRPM", &z.s, "Max RPM:", "Max RPM: unknown (no eRPM from DShot or KISS)");
	checkInt("rpm below 2 poles", (long)runSumRpm(1000, 1), 1000);
	checkInt("rpm at 2 poles", (long)runSumRpm(1000, 2), 1000);
	checkInt("rpm at 14 poles", (long)runSumRpm(1400, 14), 200);
}

/** @brief The KISS counter's rise, an ESC restart, a counter that stays 0. */
static void testCounter() {
	section("Run summary: the KISS counter");
	Sim m;
	simBegin(&m, true);
	const uint16_t mah[] = { 100, 120, 150, 150, 0, 10, 30 };
	for (uint16_t x : mah) { simStep(&m, 20); kissFrame(&m, 15.0f, 5.0f, 30, x); simFeed(&m); }
	// A gap: the next fresh reading still counts its rise.
	simStep(&m, 2000); kissFrame(&m, 15.0f, 5.0f, 30, 45); simFeed(&m);
	end(&m);
	RunSumCharge c = runSumUsedCounter(&m.s);
	checkInt("rise across a restart and a gap", c.mah, 50 + 30 + 15);
	checkLine("text", &m.s, "Used, ESC counter:", "Used, ESC counter: 95 mAh (KISS, 1 mAh steps)");
	checkRow("row USED", &m.s, RunSumRow::Used, "95mAh", "KISS");
	RunSumRowText o;
	runSumRowText(&m.s, RunSumRow::Used, 14, 12, &o);
	checkStr("label", o.label, "USED");

	Sim z;
	simBegin(&z, true);
	for (int i = 0; i < 5; i++) { simStep(&z, 20); kissFrame(&z, 15.0f, 36.0f, 30, 0); simFeed(&z); }
	end(&z);
	checkLine("stayed 0", &z.s, "Used, ESC counter:", "Used, ESC counter: unknown (KISS counter stayed 0)");
	runSumRowText(&z.s, RunSumRow::Used, 14, 12, &o);
	checkStr("stayed 0: label from current", o.label, "USED FROM CURRENT");

	Sim n;
	simBegin(&n, true);
	end(&n);
	checkLine("no KISS", &n.s, "Used, ESC counter:", "Used, ESC counter: unknown (no KISS frame)");
	checkLine("no current", &n.s, "Used, from current:", "Used, from current: unknown (no current from KISS or EDT)");
	checkRow("no charge row", &n.s, RunSumRow::Used, "--", nullptr);

	// A KISS frame from before the log, stale at its first sample, is no
	// starting value: the rise counts from the first fresh one.
	Sim b;
	simBegin(&b, true);
	kissFrame(&b, 15.0f, 5.0f, 30, 100);
	simTick(&b, 600);
	simStep(&b, 20); kissFrame(&b, 15.0f, 5.0f, 30, 150); simFeed(&b);
	simStep(&b, 20); kissFrame(&b, 15.0f, 5.0f, 30, 160); simFeed(&b);
	end(&b);
	checkInt("a stale frame before the log: not counted", runSumUsedCounter(&b.s).mah, 10);

	// A counter that does not rise: 0 mAh, and SD LOG shows the current's.
	Sim f;
	simBegin(&f, true);
	for (int i = 0; i < 5; i++) { simStep(&f, 20); kissFrame(&f, 15.0f, 36.0f, 30, 312); simFeed(&f); }
	end(&f);
	checkLine("flat counter", &f.s, "Used, ESC counter:", "Used, ESC counter: 0 mAh (KISS, 1 mAh steps)");
	runSumRowText(&f.s, RunSumRow::Used, 14, 12, &o);
	checkStr("flat counter: label", o.label, "USED FROM CURRENT");
}

/** @brief The current's integral against a hand sum, with gaps. */
static void testIntegral() {
	section("Run summary: charge from current");
	Sim m;
	simBegin(&m, true, 1000);
	uint64_t want = 0, known = 0;
	// 1 s at 500 Hz of KISS 36.00 A, a frame every 20 ms.
	for (int i = 0; i < 500; i++) {
		simStep(&m);
		if (i % 10 == 0) kissFrame(&m, 15.0f, 36.0f, 30, 0);
		simFeed(&m);
		if (i > 0) { want += 3600ull * 2000u; known += 2000u; }
	}
	// A 600 ms stall on KISS: 500 ms of it counts.
	simStep(&m, 600);
	kissFrame(&m, 15.0f, 36.0f, 30, 0);
	simFeed(&m);
	want += 3600ull * 500000u; known += 500000u;
	// KISS stops; EDT current 20 A. The KISS frame 20 ms back still holds
	// that sample.
	simStep(&m, 20); edtAmps(&m, 20); simFeed(&m);
	want += 3600ull * 20000u; known += 20000u;
	// 500 ms on: the KISS current holds the interval, capped at 500 ms; the
	// KISS frame is 520 ms old by now, so this sample takes EDT's 20 A.
	simStep(&m, 500); edtAmps(&m, 20); simFeed(&m);
	want += 3600ull * 500000u; known += 500000u;
	// A 1200 ms stall on EDT: 1000 ms of it counts.
	simStep(&m, 1200); edtAmps(&m, 20); simFeed(&m);
	want += 2000ull * 1000000u; known += 1000000u;
	// The stop 2 ms on.
	end(&m);
	want += 2000ull * 2000u; known += 2000u;
	checkTrue("sum against the hand sum", m.s.ampUs == want);
	checkTrue("known time against the hand sum", m.s.ampKnownUs == known);
	RunSumCharge c = runSumUsedCurrent(&m.s);
	checkTrue("tag MIXED", c.from == RunSumFrom::Mixed);
	checkInt("mAh", c.mah, (long)((want + 180000000u) / 360000000u));
	char l[200];
	snprintf(l, sizeof(l),
	         "Used, from current: %lu mAh (KISS and EDT, current known for 3.0 s of 3.3 s)",
	         (unsigned long)c.mah);
	checkLine("text", &m.s, "Used, from current:", l);
	checkRow("row", &m.s, RunSumRow::Used, "26mAh", "MIXED");

	// mAh rounds to the nearest: 1 mAh is 3.6e8 units.
	Sim r;
	simBegin(&r, true, 1000);
	simStep(&r); edtAmps(&r, 1); simFeed(&r);
	end(&r);
	r.s.ampUs = 179999999u;
	checkInt("0.49999 mAh", runSumUsedCurrent(&r.s).mah, 0);
	r.s.ampUs = 180000000u;
	checkInt("0.5 mAh", runSumUsedCurrent(&r.s).mah, 1);

	// KISS current 0 throughout: the EDT sum, with its own known time.
	Sim z;
	simBegin(&z, true, 1000);
	for (int i = 0; i < 50; i++) {
		simStep(&z, 20);
		kissFrame(&z, 15.0f, 0.0f, 30, 0);
		edtAmps(&z, 180);
		simFeed(&z);
	}
	end(&z);
	c = runSumUsedCurrent(&z.s);
	checkTrue("KISS sent 0: EDT", c.from == RunSumFrom::Edt && c.kissSentZero);
	// 49 intervals of 20 ms and the 2 ms to the stop at 180 A.
	checkInt("EDT sum, mAh", c.mah, (long)((18000ull * 982000u + 180000000u) / 360000000u));
	checkLine("EDT text", &z.s, "Used, from current:",
	          "Used, from current: 49 mAh (EDT, KISS sent 0, current known for 1.0 s of 1.0 s)");
	checkRow("EDT row", &z.s, RunSumRow::Used, "49mAh", "EDT");

	// Current that stops: once no source is fresh, frames add no current.
	// KISS at 36.00 A for 100 ms, then 1 s of frames with nothing fresh.
	Sim g;
	simBegin(&g, true, 1000);
	for (int i = 0; i < 50; i++) {
		simStep(&g);
		if (i % 10 == 0) kissFrame(&g, 15.0f, 36.0f, 30, 0);
		simFeed(&g);
	}
	for (int i = 0; i < 500; i++) simTick(&g);
	end(&g);
	// 49 intervals of 2 ms, then the frames from 100 ms until the last
	// frame (at 82 ms) is 500 ms old, at 582 ms: 482 ms more.
	checkTrue("KISS stops: known time", g.s.ampKnownUs == 98000u + 482000u);
	checkTrue("KISS stops: the sum", g.s.ampUs == 3600ull * (98000u + 482000u));

	// The EDT-only sum: a 1200 ms stall counts 1000 ms, and EDT going
	// stale stops it.
	Sim q;
	simBegin(&q, true, 1000);
	for (int i = 0; i < 3; i++) {
		simStep(&q, 20); kissFrame(&q, 15.0f, 0.0f, 30, 0); edtAmps(&q, 10); simFeed(&q);
	}
	simStep(&q, 1200); kissFrame(&q, 15.0f, 0.0f, 30, 0); edtAmps(&q, 10); simFeed(&q);
	for (int i = 0; i < 30; i++) { simStep(&q, 100); kissFrame(&q, 15.0f, 0.0f, 30, 0); simFeed(&q); }
	end(&q);
	// 2 × 20 ms, 1000 ms of the stall, then 1000 ms until EDT's last frame
	// goes stale.
	checkTrue("EDT-only: known time", q.s.ampEdtKnownUs == 40000u + 1000000u + 1000000u);
	checkTrue("EDT-only: the sum", q.s.ampEdtUs == 1000ull * 2040000u);

	// A source that held no time is not in the tag; a sample whose time is
	// before the last adds nothing.
	Sim w;
	simBegin(&w, true, 1000);
	for (int i = 0; i < 3; i++) { simStep(&w, 20); edtAmps(&w, 10); simFeed(&w); }
	kissFrame(&w, 15.0f, 20.0f, 30, 0);
	w.us -= 5000u;
	simFeed(&w);
	runSumEnd(&w.s, w.s.lastUs, RunSumEnd::Stop);
	checkTrue("KISS for no time: tag EDT", runSumUsedCurrent(&w.s).from == RunSumFrom::Edt);
	checkTrue("a sample back in time: 40 ms known", w.s.ampKnownUs == 40000u);

	// The next sample counts from the last forward one, and a stop stamped
	// before the last sample ends at that sample.
	Sim bk;
	simBegin(&bk, true, 1000);
	for (int i = 0; i < 3; i++) { simStep(&bk, 20); edtAmps(&bk, 10); simFeed(&bk); }
	uint64_t last = bk.us;
	bk.us = last - 5000u;  simFeed(&bk);
	bk.us = last + 20000u; simFeed(&bk);
	checkTrue("after a sample back in time: 60 ms known", bk.s.ampKnownUs == 60000u);
	runSumEnd(&bk.s, last, RunSumEnd::Stop);
	checkTrue("a stop before the last sample ends at it", bk.s.stopUs == last + 20000u);
	checkTrue("armed to the last sample: 80 ms", bk.s.armedUs == 80000u);

	// EDT alone: no "KISS sent 0".
	Sim e;
	simBegin(&e, true, 1000);
	for (int i = 0; i < 10; i++) { simStep(&e, 100); edtAmps(&e, 36); simFeed(&e); }
	end(&e);
	checkLine("EDT alone", &e.s, "Used, from current:",
	          "Used, from current: 9 mAh (EDT, current known for 0.9 s of 1.0 s)");
}

/** @brief Armed time from the arm state at each frame. */
static void testArmed() {
	section("Run summary: armed time");
	Sim m;
	simBegin(&m, false);
	for (int i = 0; i < 10; i++) simTick(&m, 25);   // 250 ms disarmed
	m.armed = true;
	simTick(&m, 25);                                // the edge's frame
	for (int i = 0; i < 40; i++) simTick(&m, 25);   // 1000 ms armed
	m.armed = false;
	simTick(&m, 25);
	for (int i = 0; i < 4; i++) simTick(&m, 25);
	end(&m);
	checkTrue("armed: 41 intervals of 25 ms", m.s.armedUs == 41u * 25000u);
	checkLine("armed text", &m.s, "Armed:", "Armed: 1.0 s");
	checkLine("duration text", &m.s, "Duration:", "Duration: 1.4 s");

	// A log arming started and disarming stopped: armed for all of it.
	Sim a;
	simBegin(&a, true);
	for (int i = 0; i < 100; i++) simTick(&a);
	end(&a, RunSumEnd::Disarm);
	checkTrue("auto log: armed equals duration", a.s.armedUs == a.s.stopUs - a.s.startUs);
	checkLine("ended by", &a.s, "Ended by:", "Ended by: DISARM");
	checkRow("ARMED row", &a.s, RunSumRow::Armed, "0:00:00", nullptr);

	// Each interval counts by the sample before it: the 50 ms before the
	// arming frame are not armed, the 100 ms before the disarming frame are.
	Sim ed;
	simBegin(&ed, false);
	simTick(&ed, 50);
	ed.armed = true;
	simTick(&ed, 50);
	simTick(&ed, 2);
	ed.armed = false;
	simTick(&ed, 100);
	end(&ed);
	checkTrue("edges of unequal intervals: 102 ms armed", ed.s.armedUs == 102000u);
}

/** @brief Sums and times past the 32-bit micros() wrap, 71.6 minutes. */
static void testLong() {
	section("Run summary: 75 minutes, past micros() and millis() wraps");
	Sim m;
	// millis() wraps 1000 s into the log; no EDT stamp falls on 0.
	simBegin(&m, true, 0xFFFF0000u);
	m.us = 4000000000ull;
	runSumBegin(&m.s, m.us, true);
	for (int i = 0; i < 4500; i++) {
		simStep(&m, 1000);
		edtAmps(&m, 36);
		edtVolts(&m, 15.0f);
		simFeed(&m);
	}
	end(&m);
	checkTrue("armed 4500.002 s", m.s.armedUs == 4500002000ull);
	checkLine("duration", &m.s, "Duration:", "Duration: 4500.0 s");
	// 36 A for 4499 s and 2 ms: 44,990 mAh.
	checkInt("mAh", runSumUsedCurrent(&m.s).mah, 44990);
	checkRow("ARMED row", &m.s, RunSumRow::Armed, "1:15:00", nullptr);
	checkInt("min voltage held", runSumMinVolts(&m.s).held, 1500);
}

/** @brief How the log ended, and what became of the file. */
static void testEnds() {
	section("Run summary: the end and the file");
	Sim m;
	simBegin(&m, true);
	for (int i = 0; i < 3; i++) { simStep(&m); dshot(&m, 1000); simFeed(&m); }
	end(&m, RunSumEnd::WriteFailed);
	uint64_t stop = m.s.stopUs;
	// Nothing after the end counts, and a second end changes nothing.
	simStep(&m); dshot(&m, 90000); simFeed(&m);
	runSumEnd(&m.s, m.us + 1000000u, RunSumEnd::Stop);
	checkInt("after a failed write: held", runSumMaxErpm(&m.s).held, 1000);
	checkTrue("stop time kept", m.s.stopUs == stop);
	checkTrue("ended by WriteFailed", m.s.end == RunSumEnd::WriteFailed);
	checkTrue("file not saved", m.s.file == RunSumFile::WriteFailed);
	RunSumRowText o;
	runSumRowText(&m.s, RunSumRow::Summary, 14, 12, &o);
	checkStr("SUMMARY", o.value, "NOT SAVED: WRITE FAILED");
	checkTrue("SUMMARY alert", o.alert);
	checkRow("RPM up to the failure", &m.s, RunSumRow::MaxRpm, "142", nullptr);

	Sim f;
	simBegin(&f, true);
	end(&f, RunSumEnd::CardFull);
	checkTrue("card full: not saved", f.s.file == RunSumFile::CardFull);
	checkRow("card full row", &f.s, RunSumRow::Summary, "NOT SAVED: CARD FULL", nullptr);
	checkLine("card full ended by", &f.s, "Ended by:", "Ended by: CARD FULL");

	Sim s;
	simBegin(&s, true);
	checkRow("running: no file yet", &s.s, RunSumRow::Summary, "--", nullptr);
	simStep(&s, 100);
	runSumEnd(&s.s, s.us, RunSumEnd::Running);
	checkTrue("Running is no end", s.s.end == RunSumEnd::Running);
	checkTrue("Running: no interval counted", s.s.armedUs == 0 && s.s.lastUs == s.s.startUs);
	end(&s, RunSumEnd::Stop);
	checkTrue("STOP leaves the file to the logger", s.s.file == RunSumFile::NotWritten);
	runSumFileResult(&s.s, RunSumFile::Saved);
	checkRow("saved", &s.s, RunSumRow::Summary, "LOG00012.TXT", nullptr);
	runSumRowText(&s.s, RunSumRow::Summary, 14, 12, &o);
	checkTrue("saved: no alert", !o.alert && !o.unknown);
	runSumFileResult(&s.s, RunSumFile::WriteFailed);
	checkRow("its write failed", &s.s, RunSumRow::Summary, "NOT SAVED: WRITE FAILED", nullptr);

	const struct { RunSumEnd e; const char *t; } ends[] = {
		{ RunSumEnd::Stop, "Ended by: STOP" },
		{ RunSumEnd::Disarm, "Ended by: DISARM" },
		{ RunSumEnd::ByKst, "Ended by: KST SERVO" },
		{ RunSumEnd::ByHitec, "Ended by: HITEC SERVO" },
		{ RunSumEnd::ByAm32, "Ended by: AM32 CONFIG" },
		{ RunSumEnd::WriteFailed, "Ended by: WRITE FAILED" },
	};
	for (const auto &c : ends) {
		Sim x;
		simBegin(&x, false);
		end(&x, c.e);
		checkLine(c.t, &x.s, "Ended by:", c.t);
	}
}

/** @brief The `.TXT` name, and that it does not count as a log's name. */
static void testName() {
	section("Run summary: the file's name");
	char b[RUN_SUM_NAME_BYTES];
	runSumTxtName(12, b);
	checkStr("12", b, "LOG00012.TXT");
	runSumTxtName(99999, b);
	checkStr("99999", b, "LOG99999.TXT");
	runSumTxtName(1, b);
	checkStr("1", b, "LOG00001.TXT");
	uint32_t n = 0, hi = 7;
	checkTrue("not a log's name", !sdLogNameNumber("LOG00012.TXT", &n));
	sdLogNoteName(&hi, "LOG00012.TXT");
	checkInt("the next log's number unmoved", hi, 7);
}

/** @brief The note's example, byte for byte. */
static void testExample() {
	section("Run summary: the note's example text");
	RunSum s;
	runSumBegin(&s, 1000000u, false);
	s.end = RunSumEnd::Stop;
	s.stopUs = s.startUs + 144200000u;
	s.everArmed = true;
	s.armedUs = 102500000u;
	s.erpm.heldOk = s.erpm.rawOk = s.erpm.anyLine = true;
	s.erpm.held = 129150; s.erpm.raw = 129220;
	s.amps.heldOk = s.amps.rawOk = s.amps.anyKiss = true;
	s.amps.heldKiss = s.amps.rawKiss = true;
	s.amps.held = 2341; s.amps.raw = 2355;
	s.temp = s.amps;
	s.temp.held = s.temp.raw = 51;
	s.volts = s.amps;
	s.volts.held = 1482; s.volts.raw = 1479;
	s.kissSeen = s.kissVoltsSet = s.kissAmpsSet = s.kissTempSet = s.kissMahSet = true;
	s.mahRise = 312;
	s.ampSumKiss = true;
	s.ampKnownUs = 144000000u;
	s.ampUs = 305ull * 360000000u;
	SdLogStatus st = {};
	st.fileNumber = 12;
	st.framesLogged = 72100;
	st.bytesWritten = 1043u * 1024u;
	st.peakBuffer = 2048;
	st.worstFlushMs = 12;
	char t[1200];
	size_t len = runSumText(&s, &st, 14, t, sizeof(t));
	const char *want =
		"MiniRcBench run summary, format 1\n"
		"Log: LOG00012.BFL\n"
		"Firmware: " FW_VERSION_STRING "\n"
		"Clock at start: not set\n"
		"Clock at stop: not set\n"
		"Ended by: STOP\n"
		"Duration: 144.2 s\n"
		"Armed: 102.5 s\n"
		"Max RPM: 18450 RPM (DShot, eRPM 129150, 14 poles; raw 18460 RPM)\n"
		"Max current: 23.41 A (KISS, 0.01 A steps; raw 23.55 A)\n"
		"Max temperature: 51 C (KISS, 1 C steps; raw 51 C)\n"
		"Min voltage: 14.82 V (KISS, 0.01 V steps, while armed; raw 14.79 V)\n"
		"Used, ESC counter: 312 mAh (KISS, 1 mAh steps)\n"
		"Used, from current: 305 mAh (KISS, current known for 144.0 s of 144.2 s)\n"
		"Frames: 72100\n"
		"Dropped frames: 0 (0 bytes)\n"
		"Written: 1043 kB\n"
		"Buffer peak: 2048 of 8192 bytes\n"
		"Worst flush: 12 ms\n";
	bool same = strcmp(t, want) == 0;
	checkTrue("the example, byte for byte", same);
	if (!same) printf("%s", t);
	checkInt("length", (long)len, (long)strlen(want));

	// Rows of the same summary, as SD LOG shows them.
	runSumFileResult(&s, RunSumFile::Saved);
	checkRow("MAX RPM", &s, RunSumRow::MaxRpm, "18450", nullptr);
	checkRow("MAX CURRENT", &s, RunSumRow::MaxCurrent, "23.41A", "KISS");
	checkRow("MAX TEMP", &s, RunSumRow::MaxTemp, "51`C", "KISS");
	checkRow("MIN VOLTAGE", &s, RunSumRow::MinVoltage, "14.82V", "KISS");
	checkRow("USED", &s, RunSumRow::Used, "312mAh", "KISS");
	checkRow("ARMED", &s, RunSumRow::Armed, "0:01:42", nullptr);
	checkRow("SUMMARY", &s, RunSumRow::Summary, "LOG00012.TXT", nullptr);
	RunSumRowText o;
	runSumRowText(&s, RunSumRow::MaxTemp, 14, 12, &o);
	checkTrue("51 C: no alert", !o.alert);
	s.temp.held = 89;
	runSumRowText(&s, RunSumRow::MaxTemp, 14, 12, &o);
	checkTrue("89 C: no alert", !o.alert);
	s.temp.held = 90;
	runSumRowText(&s, RunSumRow::MaxTemp, 14, 12, &o);
	checkTrue("90 C: alert", o.alert);

	// A buffer too short: cut, terminated, the full length returned.
	char small[40];
	memset(small, 'x', sizeof(small));
	size_t l2 = runSumText(&s, &st, 14, small, sizeof(small));
	checkInt("cut: full length returned", (long)l2, (long)len);
	checkInt("cut: terminated", (long)strlen(small), (long)sizeof(small) - 1);
	checkTrue("cut: the start kept", strncmp(small, want, sizeof(small) - 1) == 0);
}

/** @brief The longest text fits in 1000 bytes. */
static void testLongest() {
	section("Run summary: the longest text");
	RunSum s;
	runSumBegin(&s, 0, true);
	s.end = RunSumEnd::WriteFailed;
	s.stopUs = 4294967295ull * 100000u;
	s.armedUs = s.stopUs;
	s.erpm.rawOk = s.erpm.anyLine = true;
	s.erpm.raw = 60000000;
	RunSumExtreme x = {};
	x.rawOk = x.anyKiss = x.anyLine = true;
	x.raw = 65535;
	s.amps = s.volts = x;
	s.temp = x;
	s.temp.raw = 255;
	s.kissSeen = s.kissAmpsSet = s.kissTempSet = s.kissVoltsSet = s.kissMahSet = true;
	s.mahRise = 4294967295u;
	s.ampSumKiss = s.ampSumEdt = true;
	s.ampKnownUs = s.stopUs;
	s.ampUs = 4294967295ull * 360000000u;
	SdLogStatus st = {};
	st.fileNumber = 99999;
	st.framesLogged = st.dropEvents = st.bytesDropped = st.bytesWritten = 4294967295u;
	st.peakBuffer = st.worstFlushMs = 4294967295u;
	char t[1200];
	size_t len = runSumText(&s, &st, 255, t, sizeof(t));
	checkTrue("under 1000 bytes", len < 1000);
	printf("  longest text: %u bytes\n", (unsigned)len);
	// The minimum's mixed wording, with a held value.
	s.volts.heldOk = true; s.volts.held = 1475; s.volts.heldKiss = false;
	s.volts.rawKiss = false; s.volts.raw = 1450;
	text(&s, t, sizeof(t));
	char l[200];
	lineOf(t, "Min voltage:", l, sizeof(l));
	checkStr("mixed minimum", l,
	         "Min voltage: 14.75 V (KISS and EDT, 0.01 V and 0.25 V steps, minimum from EDT, while armed; raw 14.50 V)");
}

/**
 * @brief Each row's widest value fits beside its label, and each tag in its
 *        place, at scale 1: the label from x 8, a value's right edge x 197
 *        with a tag and x 232 without, a tag from x 203 to at most x 231.
 */
static void testFits() {
	section("Run summary: SD LOG rows fit");
	// The widest values, through the formatter.
	Sim m;
	simBegin(&m, true);
	for (int i = 0; i < 3; i++) {
		simStep(&m, 20);
		dshot(&m, 60000000);
		kissFrame(&m, 655.35f, 655.35f, 255, 0);
		simFeed(&m);
	}
	end(&m);
	// Charge above 99,999 mAh from current, ARMED at 999:59:59.
	m.s.ampUs = 100000ull * 360000000u;
	m.s.armedUs = 3599999000000ull;
	runSumFileResult(&m.s, RunSumFile::WriteFailed);

	const struct { RunSumRow row; const char *value; bool tagged; int gap; } rows[] = {
		{ RunSumRow::MaxRpm,     "60000000",                true,  101 },
		{ RunSumRow::MaxCurrent, "655.35A",                 true,  83 },
		{ RunSumRow::MaxTemp,    "255`C",                   true,  113 },
		{ RunSumRow::MinVoltage, "655.35V",                 true,  83 },
		{ RunSumRow::Used,       ">99999mAh",               true,  35 },
		{ RunSumRow::Armed,      "999:59:59",               false, 142 },
		{ RunSumRow::Summary,    "NOT SAVED: WRITE FAILED", false, 46 },
	};
	for (const auto &r : rows) {
		RunSumRowText o;
		runSumRowText(&m.s, r.row, 2, 12, &o);
		checkStr(o.label, o.value, r.value);
		int labelLast = 8 + gfxTextW(o.label, 1) - 1;
		int valueFirst = (r.tagged ? 197 : 232) - gfxTextW(o.value, 1);
		char w[64];
		snprintf(w, sizeof(w), "%s: gap px", o.label);
		checkInt(w, valueFirst - labelLast - 1, r.gap);
	}
	// 99,999 mAh is written in full.
	m.s.ampUs = 99999ull * 360000000u;
	checkRow("99,999 mAh", &m.s, RunSumRow::Used, "99999mAh", "KISS");
	// EDT's widest current.
	Sim e;
	simBegin(&e, true);
	for (int i = 0; i < 3; i++) { simStep(&e, 20); edtAmps(&e, 255); simFeed(&e); }
	end(&e);
	checkRow("EDT's widest", &e.s, RunSumRow::MaxCurrent, "255A", "EDT");
	const char *tags[] = { "KISS", "EDT", "MIXED" };
	for (const char *tg : tags) {
		char w[32];
		snprintf(w, sizeof(w), "tag %s last px", tg);
		checkTrue(w, 203 + gfxTextW(tg, 1) - 1 <= 231);
	}
	checkInt("MIXED's last pixel", 203 + gfxTextW("MIXED", 1) - 1, 231);
	// The labels are the ones the note gives.
	RunSumRowText o;
	const char *labels[] = { "MAX RPM", "MAX CURRENT", "MAX TEMP", "MIN VOLTAGE",
	                         "USED FROM CURRENT", "ARMED", "SUMMARY" };
	for (int i = 0; i < (int)RunSumRow::Count; i++) {
		runSumRowText(&m.s, (RunSumRow)i, 2, 12, &o);
		checkStr("label", o.label, labels[i]);
	}
	runSumRowText(&m.s, RunSumRow::Count, 2, 12, &o);
	checkTrue("past the rows: unknown", o.unknown);
}

/** @brief The summary's RAM. */
static void testSize() {
	section("Run summary: size");
	printf("  sizeof(RunSum) on the host: %u bytes\n", (unsigned)sizeof(RunSum));
	checkTrue("under 512 bytes", sizeof(RunSum) < 512);
}

void runRunSummaryTests() {
	testHeld();
	testRepeat();
	testStale();
	testMixed();
	testKissZero();
	testVoltsArmed();
	testWholeLog();
	testRpm();
	testCounter();
	testIntegral();
	testArmed();
	testLong();
	testEnds();
	testName();
	testExample();
	testLongest();
	testFits();
	testSize();
}
