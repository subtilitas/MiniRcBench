/**
 * @file test_kv_est.cpp
 * @brief Host tests for `src/kv_est.h`, the tester's RPM/V and kV estimate:
 *        slice 3 of `docs/design/esc-commands.md`.
 *
 * Every rule at its boundary and one past it: the rounding, the clamp, the
 * 0.5 V floor, the pole count, the 75 % minimum and the 2000 maximum, the
 * 1000 ms settle and the millis() wrap, the 3D note, and the texts with
 * their widths. The tester's wiring is in test_ui.cpp, testTesterKvEst().
 */

#include "check.h"

#include "../src/kv_est.h"
#include "../src/gfx.h"

#include <stdio.h>
#include <string.h>

/** @brief Inputs that show both figures: 22,400 RPM, 16.00 V, 75 %, settled. */
static KvEstIn kvBase() {
	KvEstIn in;
	in.armed = true;
	in.alive = true;
	in.erpm = 156800;        // 14 poles: 22,400 RPM
	in.poles = 14;
	in.voltsFresh = true;
	in.volts100 = 1600;
	in.t = 1500;
	in.settled = true;
	in.esc3d = false;
	return in;
}

static bool isUnknown(const KvEstView &v) {
	return v.rpv == KvEstShow::Unknown && v.kv == KvEstShow::Unknown;
}

static void testFormulas() {
	section("kV estimate: the two formulas");

	checkInt("RPM/V, the note's example: 14000 RPM at 16.00 V", kvEstRpmPerVolt(14000, 1600), 875);
	checkInt("KV EST, the note's example: at 50 %", kvEstKv(14000, 1600, 1000), 1750);
	// Half up: 1 RPM at 2.00 V is 0.5; at 2.01 V 0.4975.
	checkInt("RPM/V 0.5 rounds up to 1", kvEstRpmPerVolt(1, 200), 1);
	checkInt("RPM/V 0.4975 rounds down to 0", kvEstRpmPerVolt(1, 201), 0);
	// 1 RPM at 1.00 V: 2000 / t, 2.5 at t 800, 2.497 at t 801.
	checkInt("KV EST 2.5 rounds up to 3", kvEstKv(1, 100, 800), 3);
	checkInt("KV EST 2.497 rounds down to 2", kvEstKv(1, 100, 801), 2);
	// The clamp, at the field's width.
	checkInt("RPM/V 99999 stands", kvEstRpmPerVolt(99999, 100), 99999);
	checkInt("RPM/V 100000 clamps to 99999", kvEstRpmPerVolt(100000, 100), 99999);
	checkInt("KV EST 99999 stands", kvEstKv(99999, 100, 2000), 99999);
	checkInt("KV EST 100000 clamps to 99999", kvEstKv(100000, 100, 2000), 99999);
	// 60,000,000 RPM, the decoder's largest eRPM at 2 poles: 32 bits would
	// overflow in both numerators.
	checkInt("RPM/V of 60,000,000 RPM at 1000 V: 60000, no overflow",
	         kvEstRpmPerVolt(60000000u, 100000), 60000);
	checkInt("KV EST of 60,000,000 RPM at 1000 V, 100 %: 60000, no overflow",
	         kvEstKv(60000000u, 100000, 2000), 60000);
	checkInt("RPM/V at 0 V: 0, no division", kvEstRpmPerVolt(1000, 0), 0);
	checkInt("KV EST at t 0: 0, no division", kvEstKv(1000, 1600, 0), 0);
	checkInt("KV EST at 0 V: 0, no division", kvEstKv(1000, 0, 1500), 0);
}

static void testSettle() {
	section("kV estimate: the throttle's 1000 ms settle");

	KvSettle s;
	memset(&s, 0, sizeof(s));
	checkTrue("a new throttle: not settled", !kvSettleStep(&s, 1500, 5000));
	checkTrue("999 ms on: not settled", !kvSettleStep(&s, 1500, 5999));
	checkTrue("1000 ms on: settled", kvSettleStep(&s, 1500, 6000));
	checkTrue("and stays settled", kvSettleStep(&s, 1500, 9000));
	checkTrue("a step of 1: not settled", !kvSettleStep(&s, 1501, 9025));
	checkTrue("  999 ms after it: not settled", !kvSettleStep(&s, 1501, 10024));
	checkTrue("  1000 ms after it: settled", kvSettleStep(&s, 1501, 10025));
	checkTrue("back to 1500: not settled", !kvSettleStep(&s, 1500, 10050));

	// Across the millis() wrap.
	memset(&s, 0, sizeof(s));
	const uint32_t at = 0xFFFFFE00u;
	checkTrue("stamped 512 ms before the wrap: not settled", !kvSettleStep(&s, 1600, at));
	checkTrue("999 ms on, across the wrap: not settled", !kvSettleStep(&s, 1600, at + 999u));
	checkTrue("1000 ms on, across the wrap: settled", kvSettleStep(&s, 1600, at + 1000u));
	checkTrue("held 2^32 ms more: still settled", kvSettleStep(&s, 1600, at + 10u));
}

static void testView() {
	section("kV estimate: when each figure shows");

	KvEstIn in = kvBase();
	KvEstView v = kvEstView(&in);
	checkTrue("all five conditions: both figures",
	          v.rpv == KvEstShow::Value && v.kv == KvEstShow::Value);
	checkInt("  RPM/V 22400 / 16.00", v.rpvValue, 1400);
	checkInt("  KV EST 22400 x 2000 / (16.00 x 1500)", v.kvValue, 1867);

	// 1. Armed, the link, the pole count, RPM above 0.
	in = kvBase(); in.armed = false;
	checkTrue("disarmed: neither", isUnknown(kvEstView(&in)));
	in = kvBase(); in.alive = false;
	checkTrue("the link stale: neither", isUnknown(kvEstView(&in)));
	in = kvBase(); in.poles = 1;
	checkTrue("1 pole: neither", isUnknown(kvEstView(&in)));
	in = kvBase(); in.poles = 0;
	checkTrue("0 poles: neither, no division", isUnknown(kvEstView(&in)));
	in = kvBase(); in.poles = 2; in.erpm = 22400;
	v = kvEstView(&in);
	checkTrue("2 poles: RPM is eRPM, both figures",
	          v.rpv == KvEstShow::Value && v.rpvValue == 1400 && v.kvValue == 1867);
	in = kvBase(); in.erpm = 0;
	checkTrue("eRPM 0: neither", isUnknown(kvEstView(&in)));
	in = kvBase(); in.erpm = 6;
	checkTrue("eRPM 6 on 14 poles, 0 RPM: neither", isUnknown(kvEstView(&in)));
	in = kvBase(); in.erpm = 7; in.volts100 = 100;
	v = kvEstView(&in);
	checkTrue("eRPM 7 on 14 poles, 1 RPM: both", v.rpv == KvEstShow::Value &&
	          v.rpvValue == 1 && v.kv == KvEstShow::Value);

	// 2. A fresh voltage above 0.5 V.
	in = kvBase(); in.voltsFresh = false;
	checkTrue("no fresh voltage: neither", isUnknown(kvEstView(&in)));
	in = kvBase(); in.volts100 = 50;
	checkTrue("0.50 V: neither", isUnknown(kvEstView(&in)));
	in = kvBase(); in.volts100 = 51;
	v = kvEstView(&in);
	checkTrue("0.51 V: both", v.rpv == KvEstShow::Value && v.kv == KvEstShow::Value);
	checkInt("  RPM/V at 0.51 V", v.rpvValue, 43922);
	in = kvBase(); in.volts100 = 0;
	checkTrue("0 V: neither", isUnknown(kvEstView(&in)));
	in = kvBase(); in.volts100 = -100;
	checkTrue("a negative voltage: neither", isUnknown(kvEstView(&in)));

	// 3. The throttle: 75 % to 100 %.
	in = kvBase(); in.t = KV_EST_MIN_THROTTLE - 1;
	v = kvEstView(&in);
	checkTrue("t 1499: RPM/V, and the estimate asks for 75 %",
	          v.rpv == KvEstShow::Value && v.kv == KvEstShow::Low);
	in = kvBase(); in.t = KV_EST_MIN_THROTTLE;
	checkTrue("t 1500: the estimate", kvEstView(&in).kv == KvEstShow::Value);
	in = kvBase(); in.t = 2000;
	v = kvEstView(&in);
	checkTrue("t 2000: the estimate", v.kv == KvEstShow::Value && v.kvValue == 1400);
	in = kvBase(); in.t = 2001;
	v = kvEstView(&in);
	checkTrue("t 2001, off the scale: RPM/V, no estimate",
	          v.rpv == KvEstShow::Value && v.kv == KvEstShow::Unknown);
	in = kvBase(); in.t = 0;
	checkTrue("t 0: the estimate asks for 75 %", kvEstView(&in).kv == KvEstShow::Low);

	// 4. Settled.
	in = kvBase(); in.settled = false;
	v = kvEstView(&in);
	checkTrue("not settled: RPM/V, no estimate",
	          v.rpv == KvEstShow::Value && v.kv == KvEstShow::Unknown);
	in = kvBase(); in.settled = false; in.t = 1499;
	checkTrue("not settled below 75 %: it asks for 75 %", kvEstView(&in).kv == KvEstShow::Low);

	// 5. The 3D note.
	in = kvBase(); in.esc3d = true;
	v = kvEstView(&in);
	checkTrue("the 3D note: RPM/V, no estimate",
	          v.rpv == KvEstShow::Value && v.kv == KvEstShow::Unknown);
	in = kvBase(); in.esc3d = true; in.t = 1000;
	checkTrue("the 3D note below 75 %: no estimate, not 75%+",
	          kvEstView(&in).kv == KvEstShow::Unknown);

	// Data before throttle: no data reads --, not 75%+.
	in = kvBase(); in.t = 1000; in.voltsFresh = false;
	checkTrue("no voltage below 75 %: --, not 75%+", isUnknown(kvEstView(&in)));
	in = kvBase(); in.t = 1000; in.alive = false;
	checkTrue("no link below 75 %: --, not 75%+", isUnknown(kvEstView(&in)));
}

static void testTexts() {
	section("kV estimate: the texts and their widths");

	char buf[40];
	KvEstIn in = kvBase();
	KvEstView v = kvEstView(&in);

	kvEstErpmLine(buf, sizeof(buf), 156800, false, &v);
	checkStr("disarmed", buf, "ERPM 156800");
	kvEstErpmLine(buf, sizeof(buf), 156800, true, &v);
	checkStr("armed", buf, "ERPM 156800  RPM/V 1400");
	in.voltsFresh = false;
	v = kvEstView(&in);
	kvEstErpmLine(buf, sizeof(buf), 156800, true, &v);
	checkStr("armed, no voltage", buf, "ERPM 156800  RPM/V --");

	in = kvBase();
	v = kvEstView(&in);
	kvEstText(buf, sizeof(buf), &v);
	checkStr("the estimate", buf, "KV EST 1867");
	in.t = 1499;
	v = kvEstView(&in);
	kvEstText(buf, sizeof(buf), &v);
	checkStr("below 75 %", buf, "KV EST 75%+");
	in.settled = false; in.t = 1500;
	v = kvEstView(&in);
	kvEstText(buf, sizeof(buf), &v);
	checkStr("no figure", buf, KV_EST_TEXT_UNKNOWN);
	checkStr("  which reads", KV_EST_TEXT_UNKNOWN, "KV EST --");

	// The widest of each, at the clamp and at the decoder's largest eRPM.
	v.rpv = KvEstShow::Value; v.rpvValue = KV_EST_MAX;
	v.kv = KvEstShow::Value; v.kvValue = KV_EST_MAX;
	kvEstErpmLine(buf, sizeof(buf), 60000000u, true, &v);
	checkStr("the widest ERPM line", buf, "ERPM 60000000  RPM/V 99999");
	checkInt("  its characters", (long)strlen(buf), KV_EST_ERPM_LINE_WIDEST);
	checkInt("  155 px, x 10 to 164", gfxTextW(buf, 1), 155);
	kvEstText(buf, sizeof(buf), &v);
	checkStr("the widest estimate", buf, "KV EST 99999");
	checkInt("  its characters", (long)strlen(buf), KV_EST_TEXT_WIDEST);
	checkInt("  71 px, x 10 to 80", gfxTextW(buf, 1), 71);
	checkInt("KV EST 75%+ is 65 px", gfxTextW("KV EST 75%+", 1), 65);
	checkInt("KV EST -- is 53 px", gfxTextW("KV EST --", 1), 53);
	checkInt("ERPM 39200  RPM/V 875 is 125 px", gfxTextW("ERPM 39200  RPM/V 875", 1), 125);
}

void runKvEstTests() {
	testFormulas();
	testSettle();
	testView();
	testTexts();
}
