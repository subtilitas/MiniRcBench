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
	in.erpmRaw = 156800;
	in.poles = 14;
	in.voltsFresh = true;
	in.volts100 = 1600;
	in.t = 1500;
	in.settled = true;
	in.esc3d = false;
	in.erpmHeld = false;
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
	// A denominator past 32 bits: 30,000 V at 100 %, 6 x 10^9.
	checkInt("KV EST with V x t past 32 bits: 2000, no overflow",
	         kvEstKv(60000000u, 3000000u, 2000), 2000);
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
	checkTrue("a step of 21: not settled", !kvSettleStep(&s, 1521, 9025));
	checkTrue("  999 ms after it: not settled", !kvSettleStep(&s, 1521, 10024));
	checkTrue("  1000 ms after it: settled", kvSettleStep(&s, 1521, 10025));
	checkTrue("a step of -21 from 1521: not settled", !kvSettleStep(&s, 1500, 10050));
	checkTrue("  1000 ms after it: settled", kvSettleStep(&s, 1500, 11050));
	checkTrue("a step of 20 from the start: still settled", kvSettleStep(&s, 1520, 11075));
	checkTrue("a step of -20 from the start: still settled", kvSettleStep(&s, 1480, 11100));
	checkTrue("1521, 21 from the start: not settled", !kvSettleStep(&s, 1521, 11125));

	// Touch jitter: a resting finger moves the throttle by about 11 units a
	// frame at MAX 100%. Within 20 of the start, the settle runs on.
	memset(&s, 0, sizeof(s));
	bool early = false;
	uint32_t now = 20000;
	kvSettleStep(&s, 1600, now);
	for (int i = 1; i < 40; i++) {
		now += 25;
		early = early || kvSettleStep(&s, (uint16_t)(i % 2 ? 1611 : 1589), now);
	}
	checkTrue("jitter of +-11 a frame: not settled at 975 ms", !early);
	checkTrue("  settled at 1000 ms", kvSettleStep(&s, 1611, now + 25));
	// The start does not follow the jitter: drift 1 a frame and the settle
	// restarts once the throttle is 21 from where it started.
	memset(&s, 0, sizeof(s));
	kvSettleStep(&s, 1600, 30000);
	bool restarted = false;
	for (int i = 1; i <= 21; i++) kvSettleStep(&s, (uint16_t)(1600 + i), 30000 + 25 * i);
	restarted = s.t == 1621 && s.sinceMs == 30000 + 25 * 21;
	checkTrue("a drift of 1 a frame restarts it at 21", restarted);

	// Across the millis() wrap.
	memset(&s, 0, sizeof(s));
	const uint32_t at = 0xFFFFFE00u;
	checkTrue("stamped 512 ms before the wrap: not settled", !kvSettleStep(&s, 1600, at));
	checkTrue("999 ms on, across the wrap: not settled", !kvSettleStep(&s, 1600, at + 999u));
	checkTrue("1000 ms on, across the wrap: settled", kvSettleStep(&s, 1600, at + 1000u));
	checkTrue("held 2^32 ms more: still settled", kvSettleStep(&s, 1600, at + 10u));
	memset(&s, 0, sizeof(s));
	checkTrue("stamped 16 ms before the wrap: not settled", !kvSettleStep(&s, 1600, 0xFFFFFFF0u));
	checkTrue("5 ms on: not settled", !kvSettleStep(&s, 1600, 0xFFFFFFF5u));
	memset(&s, 0, sizeof(s));
	checkTrue("a new throttle at 0 ms", !kvSettleStep(&s, 1600, 0));
	checkTrue("next seen 2^31 ms on: settled", kvSettleStep(&s, 1600, 0x80000000u));
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
	in = kvBase(); in.poles = 2; in.erpm = 22400; in.erpmRaw = 22400;
	v = kvEstView(&in);
	checkTrue("2 poles: RPM is eRPM, both figures",
	          v.rpv == KvEstShow::Value && v.rpvValue == 1400 && v.kvValue == 1867);
	in = kvBase(); in.erpm = 0;
	checkTrue("eRPM 0: neither", isUnknown(kvEstView(&in)));
	in = kvBase(); in.erpm = 6;
	checkTrue("eRPM 6 on 14 poles, 0 RPM: neither", isUnknown(kvEstView(&in)));
	in = kvBase(); in.erpm = 7; in.erpmRaw = 7; in.volts100 = 100;
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

	// 4. The filtered eRPM within 0.5 % of the last reply.
	in = kvBase(); in.erpm = 201000; in.erpmRaw = 200000;
	checkTrue("filtered 0.5 % above the reply: the estimate",
	          kvEstView(&in).kv == KvEstShow::Value);
	in.erpm = 201001;
	v = kvEstView(&in);
	checkTrue("filtered 1 eRPM more: RPM/V, no estimate",
	          v.rpv == KvEstShow::Value && v.kv == KvEstShow::Unknown);
	in.erpm = 199000;
	checkTrue("filtered 0.5 % below the reply: the estimate",
	          kvEstView(&in).kv == KvEstShow::Value);
	in.erpm = 198999;
	checkTrue("filtered 1 eRPM less: no estimate", kvEstView(&in).kv == KvEstShow::Unknown);
	in.erpm = 156800; in.erpmRaw = 0;
	checkTrue("a reply of 0 under a filtered figure: no estimate",
	          kvEstView(&in).kv == KvEstShow::Unknown);
	in = kvBase(); in.erpm = 4000000000u; in.erpmRaw = 4000000000u - 10000000u;
	checkTrue("past 32 bits in the comparison: no overflow, the estimate",
	          kvEstView(&in).kv == KvEstShow::Value);
	// The decoder's largest eRPM and a gap whose x 1000 wraps 32 bits to
	// 32,704: it must not pass as within 0.5 %.
	in = kvBase(); in.erpm = 55705000u; in.erpmRaw = 60000000u;
	checkTrue("a 7 % gap at 60,000,000 eRPM: no estimate, no overflow",
	          kvEstView(&in).kv == KvEstShow::Unknown);

	// Once passed, the check holds: noisy replies do not flicker the field.
	// The caller feeds KvEstView::erpmHeld back, as ui.cpp does.
	{
		static const uint32_t replies[] = { 156800, 158400, 155200, 157900, 154000,
		                                    156800, 159500, 153500 };
		in = kvBase();
		bool held = false, steady = true;
		for (uint32_t r : replies) {
			in.erpmRaw = r;
			in.erpmHeld = held;
			v = kvEstView(&in);
			held = v.erpmHeld;
			steady = steady && v.kv == KvEstShow::Value;
		}
		checkTrue("noisy replies after a pass: the figure stays", steady);
		in.erpmHeld = false; in.erpmRaw = replies[1];
		checkTrue("the same reply with no hold: no estimate",
		          kvEstView(&in).kv == KvEstShow::Unknown);
		in.erpmHeld = true;
		checkTrue("held, the settle restarted: cleared, no estimate",
		          (in.settled = false, !kvEstView(&in).erpmHeld &&
		                               kvEstView(&in).kv == KvEstShow::Unknown));
		in = kvBase(); in.erpmHeld = true; in.erpmRaw = replies[1]; in.alive = false;
		checkTrue("held, the link stale: cleared", !kvEstView(&in).erpmHeld);
		in.alive = true; in.t = 1000;
		checkTrue("held, t below 75 %: cleared", !kvEstView(&in).erpmHeld);
		in.t = 1500; in.esc3d = true;
		checkTrue("held, the 3D note: cleared", !kvEstView(&in).erpmHeld);
	}

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
