/**
 * @file test_servo.cpp
 * @brief The servo tester suite: the pure model, the PWM driver, the core1
 *        job, and the stub screen it replaces.
 *
 * Every check runs the shipped functions. The limits, the steppers, the
 * mappings, SPEED and the dial hit test follow docs/design/servo-tester.md.
 * servo_pwm.cpp runs against the PWM and GPIO fake in fakes.cpp, which
 * records what each register was set to; what the SDK does with those calls
 * on the chip is not tested here. The job runs as it ships, from the job
 * table, through the pin claim and the fake DShot pump; the test plays
 * core1's side by calling core1JobPoll() itself.
 *
 * ui_servo.cpp is included rather than linked, so the screen tests can read
 * its file-static state, as test_ui.cpp does with ui_am32.cpp. The Makefile
 * therefore must not compile ui_servo.cpp separately.
 */

#include "check.h"
#include "fakes.h"
#include "core1_job.h"
#include "esc_task.h"
#include "pin_claim.h"
#include "servo_job.h"
#include "servo_model.h"
#include "servo_pwm.h"
#include "servo_supply.h"
#include "ui_servo.h"
#include "ui_kst.h"
#include "ui_menu.h"
#include "ui_slider.h"
#include "ui.h"
#include "gfx.h"
#include "touch.h"
#include "settings.h"
#include "sense.h"
#include "fake_i2c.h"
#include "plat.h"

#include "../src/ui_servo.cpp"   // NOLINT -- see the note above

extern "C" {
#include "servo_sim.h"   // rcbench's servo model: its header has no extern "C"
}

#include <hardware/gpio.h>
#include <hardware/i2c.h>
#include <hardware/pwm.h>
#include <math.h>
#include <string.h>

/** @brief A profile of @p t at its defaults, one servo. */
static ServoProfile profileOf(ServoType t) {
	ServoProfile p;
	servoProfileDefault(&p);
	servoProfileLoadType(&p, t);
	return p;
}

/** @brief Step @p f in @p dir until it stops; return the steps taken. */
static int stepToEnd(ServoProfile *p, ServoField f, int dir) {
	int n = 0;
	while (n < 1000 && servoProfileStep(p, f, dir)) n++;
	return n;
}

/** @brief The table, each profile's limits and the rate rules. */
static void testProfiles() {
	section("Servo model: the profile table and the rate rules");

	static const struct {
		ServoType t; const char *name; uint16_t mn, c, mx, hz, ceiling;
	} want[] = {
		{ ServoType::Standard,    "STANDARD PWM",  1000, 1500, 2000, 50,  0   },
		{ ServoType::Narrow760,   "NARROW 760",    660,  760,  860,  50,  0   },
		{ ServoType::Wide,        "WIDE",          800,  1500, 2200, 50,  0   },
		{ ServoType::HeliCyclic,  "HELI CYCLIC",   820,  1520, 2220, 333, 333 },
		{ ServoType::HeliTail760, "HELI TAIL 760", 410,  760,  1110, 560, 560 },
	};
	bool allValid = true, allInLimits = true, ratesInRange = true;
	bool namesMatch = true, valuesMatch = true;
	for (const auto &w : want) {
		const ServoTypeInfo *ti = servoTypeInfo(w.t);
		ServoProfile p = profileOf(w.t);
		namesMatch = namesMatch && strcmp(ti->name, w.name) == 0;
		valuesMatch = valuesMatch && p.minUs == w.mn && p.centreUs == w.c &&
		              p.maxUs == w.mx && p.rateHz == w.hz && ti->maxHz == w.ceiling;
		allValid = allValid && servoProfileValid(&p);
		allInLimits = allInLimits && p.minUs >= SERVO_FLOOR_US &&
		              p.maxUs <= SERVO_CEILING_US;
		for (uint8_t i = 0; i < ti->rateCount; i++) {
			ratesInRange = ratesInRange && ti->rates[i] >= SERVO_RATE_MIN_HZ &&
			               ti->rates[i] <= SERVO_RATE_MAX_HZ;
			if (i) ratesInRange = ratesInRange && ti->rates[i] > ti->rates[i - 1];
		}
	}
	checkTrue("five profiles, named as rcbench names them", namesMatch);
	checkTrue("each loads rcbench's MIN, CENTRE, MAX and rate", valuesMatch);
	checkTrue("each is valid at its defaults", allValid);
	checkTrue("each lies within 400 us to 2500 us", allInLimits);
	checkTrue("every offered rate is 40 Hz to 560 Hz, ascending", ratesInRange);
	checkTrue("a type outside the enum reads as STANDARD PWM",
	          servoTypeInfo((ServoType)9) == servoTypeInfo(ServoType::Standard));

	ServoProfile d;
	servoProfileDefault(&d);
	checkTrue("the default: STANDARD PWM, 50 Hz, one servo, REVERSE B off",
	          d.type == ServoType::Standard && d.rateHz == 50 && d.servos == 1 &&
	          !d.reverseB);
	d.servos = 2;
	d.reverseB = true;
	servoProfileLoadType(&d, ServoType::Wide);
	checkTrue("loading a type keeps the servo count and REVERSE B",
	          d.servos == 2 && d.reverseB && d.maxUs == 2200);
	servoProfileLoadType(&d, (ServoType)7);
	checkTrue("loading a type outside the enum loads STANDARD PWM",
	          d.type == ServoType::Standard && d.minUs == 1000);

	checkInt("fastest rate for 2000 us plus 1000 us", servoMaxRateFor(ServoType::Standard, 2000), 333);
	checkInt("for 2500 us", servoMaxRateFor(ServoType::Standard, 2500), 285);
	checkInt("HELI CYCLIC: its own ceiling", servoMaxRateFor(ServoType::HeliCyclic, 2220), 333);
	checkInt("HELI TAIL 760: its own ceiling", servoMaxRateFor(ServoType::HeliTail760, 1110), 560);
	checkInt("HELI CYCLIC at 2500 us: the 500 us pause", servoMaxRateFor(ServoType::HeliCyclic, 2500), 333);
	checkInt("HELI TAIL at 2500 us: the 500 us pause", servoMaxRateFor(ServoType::HeliTail760, 2500), 333);

	checkInt("longest pulse at 50 Hz: the 2500 us ceiling", servoMaxPulseFor(ServoType::Standard, 50), 2500);
	checkInt("at 333 Hz: 3003 us less the pause", servoMaxPulseFor(ServoType::Standard, 333), 2003);
	checkInt("HELI TAIL at 560 Hz: 1785 us less 500 us", servoMaxPulseFor(ServoType::HeliTail760, 560), 1285);
	checkInt("at 0 Hz: none", servoMaxPulseFor(ServoType::Standard, 0), 0);
	checkInt("a pause longer than the frame: none", servoMaxPulseFor(ServoType::Standard, 1000), 0);

	checkTrue("STANDARD at 60 Hz: no warning", !servoDangerous(ServoType::Standard, 60));
	checkTrue("STANDARD at 100 Hz: the warning", servoDangerous(ServoType::Standard, 100));
	checkTrue("STANDARD at 61 Hz: the warning", servoDangerous(ServoType::Standard, 61));
	checkTrue("HELI CYCLIC at 50 Hz: the warning", servoDangerous(ServoType::HeliCyclic, 50));
	checkTrue("HELI TAIL: the warning", servoDangerous(ServoType::HeliTail760, 560));
	checkTrue("WIDE at 50 Hz: no warning", !servoDangerous(ServoType::Wide, 50));

	ServoProfile q;
	servoProfileDefault(&q);
	checkTrue("STANDARD 1000-2000 us: within travel", !servoPastTravel(&q));
	q.minUs = 900;  checkTrue("MIN 900 us: within travel", !servoPastTravel(&q));
	q.minUs = 890;  checkTrue("MIN 890 us: past travel", servoPastTravel(&q));
	q.minUs = 1000; q.maxUs = 2100; checkTrue("MAX 2100 us: within travel", !servoPastTravel(&q));
	q.maxUs = 2110; checkTrue("MAX 2110 us: past travel", servoPastTravel(&q));
	servoProfileLoadType(&q, ServoType::Wide);
	checkTrue("WIDE: past travel", servoPastTravel(&q));
	q.minUs = 1000; q.maxUs = 2000;
	checkTrue("WIDE trimmed to 1000-2000 us: still the type", servoPastTravel(&q));
	servoProfileLoadType(&q, ServoType::HeliCyclic);
	checkTrue("HELI CYCLIC 820-2220 us: past travel", servoPastTravel(&q));
	servoProfileLoadType(&q, ServoType::Narrow760);
	checkTrue("NARROW 760 660-860 us: its own range, no note", !servoPastTravel(&q));
	q.maxUs = 960;  checkTrue("NARROW 760, MAX 960 us: 100 us over, no note", !servoPastTravel(&q));
	q.maxUs = 970;  checkTrue("NARROW 760, MAX 970 us: the note", servoPastTravel(&q));
	q.maxUs = 2500; checkTrue("NARROW 760, MAX 2500 us: the note", servoPastTravel(&q));
	q.maxUs = 860;
	q.minUs = 560;  checkTrue("NARROW 760, MIN 560 us: 100 us under, no note", !servoPastTravel(&q));
	q.minUs = 550;  checkTrue("NARROW 760, MIN 550 us: the note", servoPastTravel(&q));
	servoProfileLoadType(&q, ServoType::HeliTail760);
	checkTrue("HELI TAIL 760 410-1110 us: its own range, no note", !servoPastTravel(&q));
	q.maxUs = 1210; checkTrue("HELI TAIL 760, MAX 1210 us: no note", !servoPastTravel(&q));
	q.maxUs = 1220; checkTrue("HELI TAIL 760, MAX 1220 us: the note", servoPastTravel(&q));

	// What waits for HOLD TO APPLY: past the travel, and further out than
	// the applied profile, or of another TYPE.
	ServoProfile a, e;
	servoProfileDefault(&a);
	e = a; e.minUs = 900;
	checkTrue("MIN 900 us from 1000 us: inside, at once", !servoTravelNeedsHold(&e, &a));
	e.minUs = 890;
	checkTrue("MIN 890 us: waits", servoTravelNeedsHold(&e, &a));
	e = a; e.maxUs = 2110;
	checkTrue("MAX 2110 us: waits", servoTravelNeedsHold(&e, &a));
	servoProfileLoadType(&e, ServoType::Wide);
	checkTrue("TYPE WIDE: waits", servoTravelNeedsHold(&e, &a));
	a = e;
	checkTrue("WIDE as applied: at once", !servoTravelNeedsHold(&e, &a));
	e.minUs = 790;
	checkTrue("WIDE, MIN 790 us from 800 us: waits", servoTravelNeedsHold(&e, &a));
	e.minUs = 810;
	checkTrue("WIDE, MIN 810 us: toward the centre, at once", !servoTravelNeedsHold(&e, &a));
	e.minUs = 800; e.maxUs = 2210;
	checkTrue("WIDE, MAX 2210 us from 2200 us: waits", servoTravelNeedsHold(&e, &a));
	e.maxUs = 2190;
	checkTrue("WIDE, MAX 2190 us: at once", !servoTravelNeedsHold(&e, &a));
	e = a; e.rateHz = 60; e.centreUs = 1510;
	checkTrue("RATE or CENTRE on WIDE as applied: at once", !servoTravelNeedsHold(&e, &a));
	e = a; e.servos = 2;
	checkTrue("SERVOS ONE to TWO on WIDE as applied: waits", servoTravelNeedsHold(&e, &a));
	a.servos = 2; e.servos = 1;
	checkTrue("SERVOS TWO to ONE on WIDE: at once", !servoTravelNeedsHold(&e, &a));
	a.servos = 1;
	{
		ServoProfile s1, s2;
		servoProfileDefault(&s1);
		s2 = s1; s2.servos = 2;
		checkTrue("SERVOS ONE to TWO on STANDARD, inside the travel: at once",
		          !servoTravelNeedsHold(&s2, &s1));
	}
	servoProfileLoadType(&a, ServoType::HeliCyclic);
	servoProfileLoadType(&e, ServoType::Wide);
	checkTrue("WIDE from HELI CYCLIC: another TYPE, waits", servoTravelNeedsHold(&e, &a));
	e = a; e.type = ServoType::Wide;
	checkTrue("another TYPE, MIN and MAX the same: waits", servoTravelNeedsHold(&e, &a));
	servoProfileLoadType(&e, ServoType::Standard);
	checkTrue("STANDARD from HELI CYCLIC: inside, at once", !servoTravelNeedsHold(&e, &a));
}

/** @brief servoProfileValid() refuses each broken rule on its own. */
static void testValidity() {
	section("Servo model: what a profile may be");

	ServoProfile ok = profileOf(ServoType::Standard);
	checkTrue("null is not valid", !servoProfileValid(nullptr));
	ServoProfile p = ok; p.type = (ServoType)5;
	checkTrue("a type outside the enum", !servoProfileValid(&p));
	p = ok; p.servos = 0;
	checkTrue("no servo", !servoProfileValid(&p));
	p = ok; p.servos = 3;
	checkTrue("three servos", !servoProfileValid(&p));
	p = ok; p.servos = 2;
	checkTrue("two servos", servoProfileValid(&p));
	p = ok; p.rateHz = 55;
	checkTrue("a rate the type does not offer", !servoProfileValid(&p));
	p = profileOf(ServoType::HeliTail760); p.rateHz = 50;
	checkTrue("50 Hz is not offered for HELI TAIL", !servoProfileValid(&p));
	p = ok; p.minUs = 390;
	checkTrue("MIN below 400 us", !servoProfileValid(&p));
	p = ok; p.minUs = 400;
	checkTrue("MIN at 400 us", servoProfileValid(&p));
	p = ok; p.minUs = 1500;
	checkTrue("MIN on CENTRE", !servoProfileValid(&p));
	p = ok; p.minUs = 1490;
	checkTrue("MIN 10 us under CENTRE", servoProfileValid(&p));
	p = ok; p.maxUs = 1500;
	checkTrue("MAX on CENTRE", !servoProfileValid(&p));
	p = ok; p.maxUs = 2500;
	checkTrue("MAX at 2500 us, 50 Hz", servoProfileValid(&p));
	p = ok; p.maxUs = 2510;
	checkTrue("MAX above 2500 us", !servoProfileValid(&p));
	p = ok; p.rateHz = 333; p.maxUs = 2010;
	checkTrue("MAX past the pause at 333 Hz", !servoProfileValid(&p));
	p.maxUs = 2000;
	checkTrue("MAX 2000 us at 333 Hz", servoProfileValid(&p));
}

/** @brief Each stepper reaches legal values only, and stops at the limits. */
static void testSteppers() {
	section("Servo model: steppers stop at the floor, the ceiling and each other");

	ServoProfile p = profileOf(ServoType::Standard);
	checkInt("MIN down: 60 steps of 10 us", stepToEnd(&p, ServoField::Min, -1), 60);
	checkInt("and stops at the floor", p.minUs, SERVO_FLOOR_US);
	checkTrue("MIN cannot step lower", !servoProfileCanStep(&p, ServoField::Min, -1));
	stepToEnd(&p, ServoField::Min, +1);
	checkInt("MIN up stops 10 us under CENTRE", p.minUs, 1490);
	stepToEnd(&p, ServoField::Centre, -1);
	checkInt("CENTRE down stops 10 us over MIN", p.centreUs, 1500);
	checkTrue("CENTRE cannot step down", !servoProfileCanStep(&p, ServoField::Centre, -1));
	stepToEnd(&p, ServoField::Centre, +1);
	checkInt("CENTRE up stops 10 us under MAX", p.centreUs, 1990);
	stepToEnd(&p, ServoField::Max, -1);
	checkInt("MAX down stops 10 us over CENTRE", p.maxUs, 2000);
	stepToEnd(&p, ServoField::Max, +1);
	checkInt("MAX up at 50 Hz stops at the 2500 us ceiling", p.maxUs, SERVO_CEILING_US);
	checkTrue("still valid", servoProfileValid(&p));

	p = profileOf(ServoType::Standard);
	stepToEnd(&p, ServoField::Rate, +1);
	checkInt("RATE up at MAX 2000 us stops at 333 Hz", p.rateHz, 333);
	stepToEnd(&p, ServoField::Max, +1);
	checkInt("MAX up at 333 Hz stops at 2000 us (2003 us is the limit)", p.maxUs, 2000);
	stepToEnd(&p, ServoField::Rate, -1);
	checkInt("RATE down stops at 50 Hz", p.rateHz, 50);
	stepToEnd(&p, ServoField::Max, +1);
	checkInt("MAX up at 50 Hz: 2500 us", p.maxUs, 2500);
	stepToEnd(&p, ServoField::Rate, +1);
	checkInt("RATE up at MAX 2500 us stops at 250 Hz, under 285 Hz", p.rateHz, 250);
	checkTrue("valid at every stop", servoProfileValid(&p));

	p = profileOf(ServoType::HeliTail760);
	checkTrue("HELI TAIL at 560 Hz cannot step RATE up",
	          !servoProfileCanStep(&p, ServoField::Rate, +1));
	stepToEnd(&p, ServoField::Max, +1);
	checkInt("HELI TAIL MAX up at 560 Hz stops at 1280 us", p.maxUs, 1280);
	stepToEnd(&p, ServoField::Rate, -1);
	checkInt("RATE down stops at 200 Hz", p.rateHz, 200);

	// A rate that is not in the list, as a profile from outside the
	// steppers could carry: RATE steps onto the list from either end.
	p = profileOf(ServoType::Standard);
	p.rateHz = 55;
	checkTrue("from an unlisted rate, + steps to the first offered",
	          servoProfileStep(&p, ServoField::Rate, +1) && p.rateHz == 50);

	p = profileOf(ServoType::Standard);
	checkInt("TYPE up: four steps to the last", stepToEnd(&p, ServoField::Type, +1), 4);
	checkTrue("loads HELI TAIL 760", p.type == ServoType::HeliTail760 &&
	                                 p.minUs == 410 && p.rateHz == 560);
	checkInt("TYPE down: four steps to the first", stepToEnd(&p, ServoField::Type, -1), 4);
	checkTrue("loads STANDARD PWM", p.type == ServoType::Standard && p.maxUs == 2000);

	checkTrue("SERVOS +: TWO", servoProfileStep(&p, ServoField::Servos, +1) && p.servos == 2);
	checkTrue("again: no change", !servoProfileStep(&p, ServoField::Servos, +1));
	checkTrue("SERVOS -: ONE", servoProfileStep(&p, ServoField::Servos, -1) && p.servos == 1);
	checkTrue("again: no change", !servoProfileStep(&p, ServoField::Servos, -1));
	checkTrue("REVERSE B +: ON", servoProfileStep(&p, ServoField::ReverseB, +1) && p.reverseB);
	checkTrue("again: no change", !servoProfileStep(&p, ServoField::ReverseB, +1));
	checkTrue("REVERSE B -: OFF", servoProfileStep(&p, ServoField::ReverseB, -1) && !p.reverseB);
	checkTrue("again: no change", !servoProfileStep(&p, ServoField::ReverseB, -1));

	ServoProfile before = p;
	checkTrue("a step of 0 changes nothing", !servoProfileStep(&p, ServoField::Min, 0));
	checkTrue("a step of 2 changes nothing", !servoProfileStep(&p, ServoField::Max, 2));
	checkTrue("a field outside the enum changes nothing",
	          !servoProfileStep(&p, (ServoField)42, 1));
	checkTrue("and the profile is as it was",
	          p.minUs == before.minUs && p.maxUs == before.maxUs);
	checkTrue("CanStep leaves the profile alone",
	          servoProfileCanStep(&p, ServoField::Max, +1) && p.maxUs == before.maxUs);
}

/** @brief Commands, angles and pulses, symmetric and not. */
static void testMapping() {
	section("Servo model: command, angle and pulse");

	ServoProfile p = profileOf(ServoType::Standard);
	int32_t lo, hi;
	servoCmdRange(&p, &lo, &hi);
	checkTrue("symmetric: the range is MIN..MAX", lo == 1000 && hi == 2000);
	checkInt("command 0: MIN", servoCmdToUs(&p, 0), 1000);
	checkInt("command 500: CENTRE", servoCmdToUs(&p, 500), 1500);
	checkInt("command 1000: MAX", servoCmdToUs(&p, 1000), 2000);
	checkInt("command 250: 1250 us", servoCmdToUs(&p, 250), 1250);
	checkInt("command 1001 counts as 1000", servoCmdToUs(&p, 1001), 2000);
	checkInt("1250 us: command 250", servoUsToCmd(&p, 1250), 250);

	// CENTRE above the middle: the range runs past MAX, and MAX clamps it.
	ServoProfile high = profileOf(ServoType::Standard);
	high.centreUs = 1600;
	checkInt("CENTRE 1600 us: command 1000 clamps to MAX", servoCmdToUs(&high, 1000), 2000);
	checkInt("command 900, 2080 us in the range: MAX", servoCmdToUs(&high, 900), 2000);
	checkInt("command 500: CENTRE", servoCmdToUs(&high, 500), 1600);
	checkInt("command 0: MIN", servoCmdToUs(&high, 0), 1000);

	// CENTRE off the middle: the range is centred on CENTRE and holds both
	// ends, so command 500 rests the servo at CENTRE.
	p.centreUs = 1400;
	servoCmdRange(&p, &lo, &hi);
	checkTrue("asymmetric: 800..2000 us, centred on 1400 us", lo == 800 && hi == 2000);
	checkInt("command 500: CENTRE 1400 us", servoCmdToUs(&p, 500), 1400);
	checkInt("command 0 clamps to MIN", servoCmdToUs(&p, 0), 1000);
	checkInt("command 1000: MAX", servoCmdToUs(&p, 1000), 2000);
	checkInt("command 100: 920 us, clamped to MIN", servoCmdToUs(&p, 100), 1000);
	checkInt("command 250: 1100 us", servoCmdToUs(&p, 250), 1100);
	checkInt("a pulse below the range: command 0", servoUsToCmd(&p, 700), 0);
	checkInt("a pulse above the range: command 1000", servoUsToCmd(&p, 2100), 1000);
	checkInt("1400 us: command 500", servoUsToCmd(&p, 1400), 500);

	checkInt("-90 deg: MIN", servoDegToUs(&p, -90.0f), 1000);
	checkInt("+90 deg: MAX", servoDegToUs(&p, 90.0f), 2000);
	checkInt("0 deg: CENTRE", servoDegToUs(&p, 0.0f), 1400);
	checkInt("+45 deg: half of the upper side", servoDegToUs(&p, 45.0f), 1700);
	checkInt("-45 deg: half of the lower side", servoDegToUs(&p, -45.0f), 1200);
	checkInt("+104 deg saturates at MAX", servoDegToUs(&p, 104.0f), 2000);
	checkInt("-97 deg saturates at MIN", servoDegToUs(&p, -97.0f), 1000);
	checkInt("MAX: +90 deg", (long)lroundf(servoUsToDeg(&p, 2000)), 90);
	checkInt("MIN: -90 deg", (long)lroundf(servoUsToDeg(&p, 1000)), -90);
	checkInt("1700 us: +45 deg", (long)lroundf(servoUsToDeg(&p, 1700)), 45);
	checkInt("+45 deg as a command, and back", servoCmdToUs(&p, servoDegToCmd(&p, 45.0f)), 1700, 1);

	// Every angle, through a command and back to a pulse, for every type.
	long worst = 0;
	for (int t = 0; t < SERVO_TYPE_COUNT; t++) {
		ServoProfile q = profileOf((ServoType)t);
		for (int d = -90; d <= 90; d++) {
			long err = (long)servoCmdToUs(&q, servoDegToCmd(&q, (float)d)) -
			           (long)servoDegToUs(&q, (float)d);
			if (err < 0) err = -err;
			if (err > worst) worst = err;
		}
	}
	checkInt("angle to command to pulse, every type, worst us", worst, 0, 1);

	// CENTRE 10 us over the floor and MAX at the ceiling: the range is
	// centred on CENTRE and starts below 0 us.
	ServoProfile edge = profileOf(ServoType::Standard);
	edge.minUs = 400; edge.centreUs = 410; edge.maxUs = 2500;
	checkTrue("that profile is valid", servoProfileValid(&edge));
	servoCmdRange(&edge, &lo, &hi);
	checkTrue("its range is -1680..2500 us", lo == -1680 && hi == 2500);
	checkInt("command 500 is CENTRE", servoCmdToUs(&edge, 500), 410);
	checkInt("command 0 clamps to MIN", servoCmdToUs(&edge, 0), 400);
	checkInt("command 1000 is MAX", servoCmdToUs(&edge, 1000), 2500);
	checkInt("CENTRE is command 500", servoUsToCmd(&edge, 410), 500);

	// MIN, CENTRE and MAX on one pulse: never valid, and still defined.
	ServoProfile flat = profileOf(ServoType::Standard);
	flat.minUs = flat.centreUs = flat.maxUs = 1500;
	checkInt("an empty range reads as the centre command", servoUsToCmd(&flat, 1700), 500);
	checkInt("and as 0 deg", (long)servoUsToDeg(&flat, 1700), 0);

	checkInt("mirror of 0", servoMirror(0), 1000);
	checkInt("mirror of 500", servoMirror(500), 500);
	checkInt("mirror of 300", servoMirror(300), 700);
	checkInt("mirror of 1200 is 0", servoMirror(1200), 0);
}

/** @brief The SWEEP steppers stay inside what sweep_cfg_valid() takes. */
static void testSweepSteppers() {
	section("Servo model: the SWEEP steppers");

	sweep_cfg_t c;
	servoSweepDefault(&c);
	checkTrue("default: sine, 0.50 Hz, 100 %, 500 ms, endless",
	          c.kind == SWEEP_SINE && c.mhz == 500 && c.amplitude == SWEEP_AMPLITUDE_MAX &&
	          c.dwell_ms == 500 && c.moves == 0 && sweep_cfg_valid(&c));
	checkTrue("SHAPE +: triangle", servoSweepStep(&c, SweepField::Shape, +1) && c.kind == SWEEP_TRIANGLE);
	checkTrue("then stops", !servoSweepStep(&c, SweepField::Shape, +1));
	servoSweepStep(&c, SweepField::Shape, -1);
	checkTrue("SHAPE -: square", servoSweepStep(&c, SweepField::Shape, -1) && c.kind == SWEEP_SQUARE);
	checkTrue("then stops, never OFF", !servoSweepStep(&c, SweepField::Shape, -1) && c.kind == SWEEP_SQUARE);

	int n = 0;
	while (servoSweepStep(&c, SweepField::Rate, -1)) n++;
	checkTrue("RATE down: 9 steps of 0.05 Hz to 0.05 Hz", n == 9 && c.mhz == SWEEP_MHZ_MIN);
	n = 0;
	while (servoSweepStep(&c, SweepField::Rate, +1)) n++;
	checkTrue("RATE up: 99 steps to 5.00 Hz", n == 99 && c.mhz == SWEEP_MHZ_MAX);
	n = 0;
	while (servoSweepStep(&c, SweepField::Amplitude, -1)) n++;
	checkTrue("AMPLITUDE down: 20 steps of 5 % to 0 %", n == 20 && c.amplitude == 0);
	checkTrue("AMPLITUDE up: 25 units", servoSweepStep(&c, SweepField::Amplitude, +1) && c.amplitude == 25);
	n = 0;
	while (servoSweepStep(&c, SweepField::Dwell, +1)) n++;
	checkTrue("DWELL up: 45 steps of 100 ms to 5000 ms", n == 45 && c.dwell_ms == SWEEP_DWELL_MAX_MS);
	n = 0;
	while (servoSweepStep(&c, SweepField::Dwell, -1)) n++;
	checkTrue("DWELL down: 50 steps to 0 ms", n == 50 && c.dwell_ms == 0);
	checkTrue("MOVES down from endless: no change", !servoSweepStep(&c, SweepField::Moves, -1));
	checkTrue("MOVES up: 1", servoSweepStep(&c, SweepField::Moves, +1) && c.moves == 1);
	c.moves = 9999;
	checkTrue("MOVES stops at 9999", !servoSweepStep(&c, SweepField::Moves, +1) && c.moves == 9999);
	checkTrue("MOVES down from 1: endless", (c.moves = 1, servoSweepStep(&c, SweepField::Moves, -1)) && c.moves == 0);
	checkTrue("still valid", sweep_cfg_valid(&c));
	sweep_cfg_t before = c;
	checkTrue("a step of 0 changes nothing", !servoSweepStep(&c, SweepField::Rate, 0));
	checkTrue("a field outside the enum changes nothing", !servoSweepStep(&c, (SweepField)9, 1));
	checkTrue("CanStep leaves the sweep alone",
	          servoSweepCanStep(&c, SweepField::Rate, -1) && c.mhz == before.mhz);
}

/** @brief SPEED as a slew. */
static void testSpeed() {
	section("Servo model: SPEED");

	checkInt("100 %: no slew", servoSlewOf(100), 0);
	checkInt("120 %: no slew", servoSlewOf(120), 0);
	checkInt("99 %: 1980 units a second", servoSlewOf(99), 1980);
	checkInt("50 %: one span a second", servoSlewOf(50), 1000);
	checkInt("10 %: 200 units a second", servoSlewOf(10), 200);
	checkInt("5 % counts as 10 %", servoSlewOf(5), 200);
	// At 10 %, MIN to MAX is 1000 units at 200 a second.
	checkInt("at 10 % a full travel takes 5000 ms",
	         SERVO_SPAN * 1000 / servoSlewOf(10), 5000);
}

/** @brief The dial's hit area, against its constants. */
static void testDial() {
	section("Servo model: the dial's hit area");

	float deg = 99.0f;
	checkTrue("60 px straight up: on the dial",
	          servoOnDial(SERVO_DIAL_X, SERVO_DIAL_Y - 60, &deg));
	checkInt("at 0 deg", (long)lroundf(deg), 0);
	checkTrue("60 px to the right: +90 deg",
	          servoOnDial(SERVO_DIAL_X + 60, SERVO_DIAL_Y, &deg) && lroundf(deg) == 90);
	checkTrue("60 px to the left: -90 deg",
	          servoOnDial(SERVO_DIAL_X - 60, SERVO_DIAL_Y, &deg) && lroundf(deg) == -90);
	checkTrue("up and right at 45 deg",
	          servoOnDial(SERVO_DIAL_X + 50, SERVO_DIAL_Y - 50, &deg) && lroundf(deg) == 45);
	checkTrue("a null angle is allowed", servoOnDial(SERVO_DIAL_X, SERVO_DIAL_Y - 60, nullptr));

	checkTrue("24 px from the shaft: inside", servoOnDial(SERVO_DIAL_X, SERVO_DIAL_Y - 24, &deg));
	checkTrue("23 px: inside the inner edge, refused",
	          !servoOnDial(SERVO_DIAL_X, SERVO_DIAL_Y - 23, &deg));
	checkTrue("122 px: inside", servoOnDial(SERVO_DIAL_X + 122, SERVO_DIAL_Y, &deg));
	checkTrue("123 px: outside the outer edge, refused",
	          !servoOnDial(SERVO_DIAL_X + 123, SERVO_DIAL_Y, &deg));

	// Below the shaft's level: past +-90 deg, down to the band's last row.
	// 41 px right and 10 px down is 103.7 deg; 40 px and 10 px is 104.04 deg.
	checkTrue("99.6 deg, below the shaft: on the dial",
	          servoOnDial(SERVO_DIAL_X + 59, SERVO_DIAL_Y + 10, &deg) && deg > 99.0f && deg < 101.0f);
	checkTrue("103.7 deg: on the dial",
	          servoOnDial(SERVO_DIAL_X + 41, SERVO_DIAL_Y + 10, &deg));
	checkTrue("104.04 deg: past the edge, refused",
	          !servoOnDial(SERVO_DIAL_X + 40, SERVO_DIAL_Y + 10, &deg));
	checkTrue("-104.04 deg: refused", !servoOnDial(SERVO_DIAL_X - 40, SERVO_DIAL_Y + 10, &deg));
	checkTrue("-103.7 deg: on the dial", servoOnDial(SERVO_DIAL_X - 41, SERVO_DIAL_Y + 10, &deg));
	checkTrue("the band's last row, at 97 deg: on the dial",
	          servoOnDial(SERVO_DIAL_X + 120, SERVO_DIAL_Y1, &deg));
	checkTrue("one row below the band, at 98 deg: refused",
	          !servoOnDial(SERVO_DIAL_X + 120, SERVO_DIAL_Y1 + 1, &deg));
	checkTrue("the annulus' top row lies inside the band",
	          servoOnDial(SERVO_DIAL_X, SERVO_DIAL_Y - SERVO_DIAL_R_OUT, &deg) &&
	          SERVO_DIAL_Y - SERVO_DIAL_R_OUT >= SERVO_DIAL_Y0);

	checkInt("the angle straight down is 180 deg",
	         (long)lroundf(fabsf(servoDialAngle(SERVO_DIAL_X, SERVO_DIAL_Y + 50))), 180);
	checkInt("the angle far right, past the hit area, is still 90 deg",
	         (long)lroundf(servoDialAngle(239, SERVO_DIAL_Y)), 90);
}

#define SIG 0
#define AUX 1

/** @brief The PWM driver against the register fake. */
static void testPwm() {
	section("Servo PWM: bind, write, release and the sharing rules");

	fakePwmReset();
	checkTrue("GP0 at 50 Hz binds", servoPwmBind(SIG, 50));
	FakePin p = fakePin(SIG);
	FakeSlice sl = fakeSlice(0);
	checkTrue("on the PWM function", p.function == GPIO_FUNC_PWM);
	checkInt("at level 0: no edges", p.level, 0);
	checkTrue("with the pulls off", p.pull == FakePull::None);
	checkTrue("slice 0 counting", sl.enabled && sl.inits == 1);
	checkInt("wrap 19999 at 50 Hz", sl.top, 19999);
	checkInt("150 MHz divided by 150: 1 us a count", sl.div16, 150 * 16);
	checkTrue("bound", servoPwmBound(SIG) && !servoPwmBound(AUX));

	checkTrue("again at 50 Hz: accepted", servoPwmBind(SIG, 50));
	checkInt("and the slice is not set up again", fakeSlice(0).inits, 1);
	checkTrue("again at 100 Hz: refused", !servoPwmBind(SIG, 100));
	checkTrue("GP1, same slice, 100 Hz: refused", !servoPwmBind(AUX, 100));
	pwm_set_gpio_level(AUX, 777);   // a compare value left from before
	checkTrue("GP1 at 50 Hz binds", servoPwmBind(AUX, 50));
	checkInt("the running slice is left alone", fakeSlice(0).inits, 1);
	checkInt("and GP1 starts at level 0, not at what its register held",
	         fakePin(AUX).level, 0);
	checkTrue("GP17, GP1's compare register: refused", !servoPwmBind(17, 50));
	checkTrue("GP26, both bindings in use: refused", !servoPwmBind(26, 50));
	checkTrue("GP30, a pin the RP2350A does not have: refused", !servoPwmBind(30, 50));

	servoPwmWrite(SIG, 1500);
	checkInt("a 1500 us pulse", fakePin(SIG).level, 1500);
	servoPwmWrite(SIG, 25000);
	checkInt("a pulse past the frame: the frame's last count", fakePin(SIG).level, 19999);
	int writes = fakePin(26).levelWrites;
	servoPwmWrite(26, 1500);
	checkInt("an unbound pin is not written", fakePin(26).levelWrites, writes);
	servoPwmWrite(SIG, 0);
	checkInt("0: level 0", fakePin(SIG).level, 0);

	servoPwmWrite(AUX, 1200);
	servoPwmRelease(AUX);
	p = fakePin(AUX);
	checkInt("release: level 0", p.level, 0);
	checkTrue("an input", p.function == GPIO_FUNC_SIO && !p.output);
	checkTrue("held low by the pull-down", p.pull == FakePull::Down);
	checkTrue("the slice runs on for GP0", fakeSlice(0).enabled && servoPwmBound(SIG));
	servoPwmRelease(SIG);
	checkTrue("the last pin released stops the slice", !fakeSlice(0).enabled);
	writes = fakePin(SIG).levelWrites;
	servoPwmRelease(SIG);
	checkInt("a second release does nothing", fakePin(SIG).levelWrites, writes);

	checkTrue("rate 0: refused", !servoPwmBind(SIG, 0));
	checkTrue("15 Hz, a period the counter cannot hold: refused", !servoPwmBind(SIG, 15));
	checkTrue("560 Hz binds", servoPwmBind(SIG, 560));
	checkInt("wrap 1784", fakeSlice(0).top, 1784);
	servoPwmRelease(SIG);
	checkTrue("GP1 binds", servoPwmBind(AUX, 50));
	checkTrue("GP17 on its compare register: refused, a binding free",
	          !servoPwmBind(17, 50));
	servoPwmRelease(AUX);

	checkTrue("nothing bound", !servoPwmBound(SIG) && !servoPwmBound(AUX));
	checkTrue("GP30, not on the package, with the table empty: refused",
	          !servoPwmBind(30, 50) && !servoPwmBound(30));
	fakePwmBreakSliceMap(true);
	checkTrue("an SDK slice that differs from the fold: refused", !servoPwmBind(SIG, 50));
	fakePwmBreakSliceMap(false);
	checkTrue("nothing bound", !servoPwmBound(SIG) && !servoPwmBound(AUX));
}

/** @brief A command of @p kind for the servos in @p mask. */
static ServoCmd cmdOf(uint16_t seq, ServoCmdKind kind, uint8_t mask = SERVO_MASK_AB,
                      uint16_t command = SERVO_CMD_CENTRE, uint16_t slew = 0) {
	ServoCmd c;
	memset(&c, 0, sizeof(c));
	c.seq = seq;
	c.kind = kind;
	c.mask = mask;
	c.command = command;
	c.slewPerS = slew;
	return c;
}

/** @brief A sweep command. */
static ServoCmd sweepOf(uint16_t seq, sweep_kind_t kind, uint16_t mhz, uint16_t amp,
                        uint16_t dwell, uint16_t moves, uint8_t mask = SERVO_MASK_AB) {
	ServoCmd c = cmdOf(seq, ServoCmdKind::Sweep, mask);
	c.sweep.kind = kind;
	c.sweep.mhz = mhz;
	c.sweep.amplitude = amp;
	c.sweep.dwell_ms = dwell;
	c.sweep.moves = moves;
	return c;
}

/** @brief Take @p c at @p nowMs and step once. */
static void pass(ServoCore *core, const ServoCmd &c, uint32_t nowMs, bool alive = true) {
	servoCoreTake(core, &c, nowMs, alive);
	servoCoreStep(core, nowMs, alive);
}

/**
 * @brief SAFE, START, positions, two servos and REVERSE B, on the pure core.
 */
static void testCoreStates() {
	section("Servo job core: SAFE, START, positions, two servos");

	ServoProfile prof = profileOf(ServoType::Standard);
	prof.servos = 2;
	ServoCore c;
	servoCoreReset(&c, &prof, true, true, 7, false);
	servoCoreStep(&c, 1000, true);
	checkTrue("SAFE: no pulse on either pin", c.outUs[0] == 0 && c.outUs[1] == 0 && !c.running);

	ServoCmd stale = cmdOf(7, ServoCmdKind::Start);
	checkTrue("a command from before the bind is not taken",
	          !servoCoreTake(&c, &stale, 1000, true) && !c.running);

	pass(&c, cmdOf(8, ServoCmdKind::Position, SERVO_MASK_AB, 1000), 1000);
	checkTrue("a position while SAFE moves nothing", !c.running && c.outUs[0] == 0);
	checkInt("and is acknowledged", c.ackSeq, 8);
	ServoCmd safeSweep = sweepOf(80, SWEEP_SINE, 1000, 400, 0, 0);
	servoCoreTake(&c, &safeSweep, 1000, true);
	checkTrue("a sweep while SAFE is refused", !c.sweeping && !c.running);
	c.ackSeq = 8;

	pass(&c, cmdOf(9, ServoCmdKind::Start, SERVO_MASK_AB, 0, 200), 1000, false);
	checkTrue("START with the heartbeat lapsed: refused, so nothing trips",
	          !c.running && !c.tripped);

	// Something was commanded before: START still puts CENTRE out at once.
	c.target[0] = 1000; c.pos[0] = 1000000;
	pass(&c, cmdOf(10, ServoCmdKind::Start, SERVO_MASK_AB, 0, 200), 1000);
	checkTrue("START: RUNNING", c.running);
	checkTrue("CENTRE on both pins in the same pass, slew or not",
	          c.outUs[0] == 1500 && c.outUs[1] == 1500);
	ServoCmd again = cmdOf(10, ServoCmdKind::Safe);
	checkTrue("a seq that was taken is not new, whatever it says",
	          !servoCoreTake(&c, &again, 1000, true) && c.running);

	pass(&c, cmdOf(11, ServoCmdKind::Position, SERVO_MASK_A, 1000), 1000);
	checkTrue("A to MAX at once; B holds", c.outUs[0] == 2000 && c.outUs[1] == 1500);
	pass(&c, cmdOf(12, ServoCmdKind::Position, SERVO_MASK_B, 0), 1000);
	checkTrue("B to MIN; A holds", c.outUs[0] == 2000 && c.outUs[1] == 1000);
	pass(&c, cmdOf(13, ServoCmdKind::Position, SERVO_MASK_AB, 750), 1000);
	checkTrue("AB: both follow", c.outUs[0] == 1750 && c.outUs[1] == 1750);

	c.prof.reverseB = true;
	pass(&c, cmdOf(14, ServoCmdKind::Position, SERVO_MASK_AB, 750), 1000);
	checkTrue("AB, REVERSE B: B mirrors", c.outUs[0] == 1750 && c.outUs[1] == 1250);
	pass(&c, cmdOf(15, ServoCmdKind::Position, SERVO_MASK_B, 800), 1000);
	checkInt("B alone, REVERSE B: B follows unmirrored", c.outUs[1], 1800);
	c.prof.reverseB = false;

	pass(&c, cmdOf(16, ServoCmdKind::Safe), 1000);
	checkTrue("STOP: no pulse from this pass", !c.running && c.outUs[0] == 0 && c.outUs[1] == 0);

	ServoCore one;
	servoCoreReset(&one, &prof, true, false, 0, false);
	pass(&one, cmdOf(1, ServoCmdKind::Start), 1000);
	checkTrue("one pin bound: CENTRE on A, nothing on B",
	          one.outUs[0] == 1500 && one.outUs[1] == 0);
	ServoCore none;
	servoCoreReset(&none, &prof, false, false, 0, false);
	pass(&none, cmdOf(1, ServoCmdKind::Start), 1000);
	checkTrue("no pin bound: START refused", !none.running && none.outUs[0] == 0);
}

/** @brief SPEED: the slew per pass, across a clock wrap. */
static void testCoreSlew() {
	section("Servo job core: the slew follows SPEED");

	ServoProfile prof = profileOf(ServoType::Standard);
	ServoCore c;
	servoCoreReset(&c, &prof, true, false, 0, false);
	const uint32_t t0 = 0xFFFFFC18u;   // 1000 ms before the clock wraps
	pass(&c, cmdOf(1, ServoCmdKind::Start, SERVO_MASK_A, 0, servoSlewOf(10)), t0);
	pass(&c, cmdOf(2, ServoCmdKind::Position, SERVO_MASK_A, 1000, servoSlewOf(10)), t0);
	checkInt("at 10 %, nothing moves in the same millisecond", c.outUs[0], 1500);
	servoCoreStep(&c, t0 + 1, true);
	checkInt("1 ms: 0.2 units, rounded to the same pulse", c.outUs[0], 1500);
	servoCoreStep(&c, t0 + 1000, true);
	checkInt("1000 ms: 200 units", c.outUs[0], 1700);
	servoCoreStep(&c, t0 + 2000, true);
	checkTrue("the clock wrapped", t0 + 2000 < t0);
	checkInt("2000 ms, past the wrap: 400 units", c.outUs[0], 1900);
	servoCoreStep(&c, t0 + 2500, true);
	checkInt("2500 ms: at MAX, 500 units in 2500 ms", c.outUs[0], 2000);
	servoCoreStep(&c, t0 + 9000, true);
	checkInt("and stays there", c.outUs[0], 2000);

	pass(&c, cmdOf(3, ServoCmdKind::Position, SERVO_MASK_A, 0, servoSlewOf(50)), t0 + 9000);
	servoCoreStep(&c, t0 + 9500, true);
	checkInt("at 50 %, 500 ms back: 500 units", c.outUs[0], 1500);
	pass(&c, cmdOf(4, ServoCmdKind::Slew, SERVO_MASK_A, 0, servoSlewOf(10)), t0 + 9500);
	servoCoreStep(&c, t0 + 10000, true);
	checkInt("SPEED to 10 % mid-move: 100 units in 500 ms", c.outUs[0], 1400);
	pass(&c, cmdOf(5, ServoCmdKind::Slew, SERVO_MASK_A, 0, servoSlewOf(100)), t0 + 10000);
	checkInt("SPEED to 100 %: at the target in the same pass", c.outUs[0], 1000);

	// From below, a step longer than the way left lands on the target.
	pass(&c, cmdOf(6, ServoCmdKind::Position, SERVO_MASK_A, 500, servoSlewOf(10)), t0 + 10000);
	servoCoreStep(&c, t0 + 12000, true);
	checkInt("from below: 400 units in 2000 ms", c.pos[0], 400000);
	servoCoreStep(&c, t0 + 13000, true);
	checkTrue("then 200 units owed, 100 left: on the target, not past it",
	          c.pos[0] == 500000 && c.outUs[0] == 1500);
	pass(&c, cmdOf(7, ServoCmdKind::Position, SERVO_MASK_A, 450, servoSlewOf(10)), t0 + 13000);
	servoCoreStep(&c, t0 + 14000, true);
	checkTrue("from above: on the target, not past it",
	          c.pos[0] == 450000 && c.outUs[0] == 1450);
}

/** @brief Sweeps: start, shape, count, stop, and what ends them. */
static void testCoreSweep() {
	section("Servo job core: the sweep and what ends it");

	ServoProfile prof = profileOf(ServoType::Standard);
	prof.servos = 2;
	ServoCore c;
	servoCoreReset(&c, &prof, true, true, 0, false);
	pass(&c, cmdOf(1, ServoCmdKind::Start), 1000);

	pass(&c, sweepOf(3, SWEEP_OFF, 1000, 400, 0, 0), 1000);
	checkTrue("a curve sweep_cfg_valid() refuses does not start", !c.sweeping);

	pass(&c, sweepOf(4, SWEEP_SINE, 1000, 400, 0, 0), 1000);
	checkTrue("SWEEP: sweeping", c.sweeping);
	checkInt("a sine starts at the centre", c.outUs[0], 1500);
	servoCoreStep(&c, 1250, true);
	checkTrue("a quarter of 1 Hz: +400 units on both", c.outUs[0] == 1900 && c.outUs[1] == 1900);
	servoCoreStep(&c, 1750, true);
	checkInt("three quarters: -400 units", c.outUs[0], 1100);

	pass(&c, cmdOf(5, ServoCmdKind::SweepStop), 1875);
	checkTrue("STOP SWEEP: not sweeping, still RUNNING", !c.sweeping && c.running);
	uint16_t held = c.outUs[0];
	servoCoreStep(&c, 3000, true);
	checkTrue("the servo stays where the curve was", c.outUs[0] == held && held != 1500);

	c.prof.reverseB = true;
	pass(&c, sweepOf(6, SWEEP_TRIANGLE, 1000, 400, 0, 0), 3000);
	servoCoreStep(&c, 3250, true);
	checkTrue("AB, REVERSE B: B sweeps mirrored", c.outUs[0] == 1900 && c.outUs[1] == 1100);
	c.prof.reverseB = false;
	pass(&c, sweepOf(7, SWEEP_TRIANGLE, 1000, 400, 0, 0, SERVO_MASK_A), 4000);
	servoCoreStep(&c, 4250, true);
	checkTrue("A alone sweeps; B holds", c.outUs[0] == 1900 && c.outUs[1] == 1100);

	pass(&c, cmdOf(8, ServoCmdKind::Position, SERVO_MASK_A, 0), 4250);
	checkTrue("a position ends the sweep", !c.sweeping && c.outUs[0] == 1000);

	pass(&c, sweepOf(9, SWEEP_SQUARE, 1000, 500, 100, 2), 5000);
	checkInt("a square jumps to +500 units at once", c.outUs[0], 2000);
	servoCoreStep(&c, 5600, true);
	checkInt("and to -500 units at the half", c.outUs[0], 1000);
	servoCoreStep(&c, 5949, true);
	checkTrue("two moves: still sweeping in the last dwell", c.sweeping);
	servoCoreStep(&c, 5950, true);
	checkTrue("then ended, back at the centre", !c.sweeping && c.outUs[0] == 1500 && c.running);

	pass(&c, sweepOf(10, SWEEP_SINE, 1000, 400, 0, 0), 6000);
	pass(&c, cmdOf(11, ServoCmdKind::Start), 6100);
	checkTrue("START again ends the sweep at the centre", !c.sweeping && c.outUs[0] == 1500);
	pass(&c, sweepOf(12, SWEEP_SINE, 1000, 400, 0, 0), 6100);
	pass(&c, cmdOf(13, ServoCmdKind::Safe), 6200);
	checkTrue("STOP ends the sweep and the pulses", !c.sweeping && !c.running && c.outUs[0] == 0);
}

/** @brief The heartbeat trip, latched, on the pure core. */
static void testCoreTrip() {
	section("Servo job core: a heartbeat lapse trips and latches");

	ServoProfile prof = profileOf(ServoType::Standard);
	ServoCore c;
	servoCoreReset(&c, &prof, true, false, 0, false);
	servoCoreStep(&c, 100, false);
	checkTrue("a lapse while SAFE latches nothing", !c.tripped);
	pass(&c, cmdOf(1, ServoCmdKind::Start), 200);
	pass(&c, sweepOf(2, SWEEP_SINE, 1000, 400, 0, 0), 200);
	checkTrue("sweeping", c.sweeping && c.running);
	servoCoreStep(&c, 300, false);
	checkTrue("a lapse while RUNNING: no pulse, no sweep, tripped",
	          !c.running && !c.sweeping && c.tripped && c.outUs[0] == 0);
	servoCoreStep(&c, 400, true);
	checkTrue("the heartbeat back restarts nothing", !c.running && c.tripped && c.outUs[0] == 0);
	pass(&c, cmdOf(3, ServoCmdKind::Position, SERVO_MASK_A, 1000), 400);
	checkTrue("nor does a position", !c.running && c.outUs[0] == 0);
	pass(&c, cmdOf(4, ServoCmdKind::Start), 500);
	checkTrue("a new START clears the trip", c.running && !c.tripped && c.outUs[0] == 1500);
	pass(&c, cmdOf(5, ServoCmdKind::Position, SERVO_MASK_A, 1000), 600, false);
	checkTrue("a position with the heartbeat lapsed: the same pass trips",
	          !c.running && c.tripped);
	pass(&c, cmdOf(6, ServoCmdKind::Safe), 700, false);
	checkTrue("STOP is taken with the heartbeat lapsed", c.ackSeq == 6 && !c.running);
}

// ---------------------------------------------------------------------------
// the switched supply, pure
// ---------------------------------------------------------------------------

/**
 * @brief Inputs of one pass: a reading of @p mv taken at @p takenMs, the
 *        sensor in @p state, the touch controller answering or not.
 */
static ServoPowerIn powerInOf(SenseState state, uint16_t mv = 0, uint32_t takenMs = 0,
                              bool touchOk = true) {
	ServoPowerIn in;
	memset(&in, 0, sizeof(in));
	in.sample.state = (uint8_t)state;
	in.sample.busMv = mv;
	in.sample.takenMs = takenMs;
	in.sample.seq = 1;
	in.touchOk = touchOk;
	return in;
}

/** @brief Take @p c, step the pulses, then the supply, at @p nowMs, as the job does. */
static void powerPass(ServoCore *core, ServoPower *p, const ServoCmd &c, const ServoPowerIn &in,
                      uint32_t nowMs, bool alive = true) {
	bool ok = alive && servoPowerStartBlock(p, &in) == ServoStartBlock::None;
	servoCoreTake(core, &c, nowMs, ok);
	servoCoreStep(core, nowMs, alive);
	servoPowerStep(p, core, &in, nowMs);
}

/** @brief A core bound on SIG alone and a supply, both SAFE. */
static void powerFresh(ServoCore *c, ServoPower *p, bool switched) {
	ServoProfile prof = profileOf(ServoType::Standard);
	servoCoreReset(c, &prof, true, false, 0, false);
	memset(p, 0xA5, sizeof(*p));
	servoPowerReset(p, switched, true);
}

/** @brief OWN: the supply's rules change nothing. */
static void testPowerOwn() {
	section("Servo supply rules: OWN switches nothing");

	ServoCore c;
	ServoPower p;
	powerFresh(&c, &p, false);
	checkTrue("reset: off, no fault, no start-up time",
	          !p.switched && p.phase == ServoPowerPhase::Off && p.fault == ServoPowerFault::None &&
	          p.offCheck == ServoOffCheck::None && p.upMs == SUPPLY_UP_NONE);
	ServoPowerIn silent = powerInOf(SenseState::Absent, 0, 0, false);
	checkTrue("no start block with touch silent and the sensor absent",
	          servoPowerStartBlock(&p, &silent) == ServoStartBlock::None);
	powerPass(&c, &p, cmdOf(1, ServoCmdKind::Start), silent, 1000);
	checkTrue("START: CENTRE at once, no wait", c.running && c.outUs[0] == 1500);
	checkTrue("the supply stays Off", p.phase == ServoPowerPhase::Off);
	ServoPowerIn stuck = powerInOf(SenseState::Stuck, 0, 0, false);
	powerPass(&c, &p, cmdOf(1, ServoCmdKind::Start), stuck, 1300);
	checkTrue("touch silent, the bus stuck: no cut", c.running && !c.tripped &&
	          c.outUs[0] == 1500 && p.fault == ServoPowerFault::None);
	powerPass(&c, &p, cmdOf(2, ServoCmdKind::Safe), stuck, 1400);
	checkTrue("STOP: no read-back armed", !c.running && p.offCheck == ServoOffCheck::None &&
	          p.phase == ServoPowerPhase::Off);
	p.fault = ServoPowerFault::NoStart;
	p.upMs = 40;
	servoPowerReset(&p, false, false);
	checkTrue("a reset with no switch keeps a fault, as the trip, and drops the time",
	          p.fault == ServoPowerFault::NoStart && p.upMs == SUPPLY_UP_NONE);
}

/** @brief START and the wait for the supply, without a sensor and with one. */
static void testPowerStart() {
	section("Servo supply rules: START switches on, the pulses wait for the supply");

	ServoCore c;
	ServoPower p;
	powerFresh(&c, &p, true);
	ServoPowerIn none = powerInOf(SenseState::None);
	checkTrue("reset with a switch: Off", p.switched && p.phase == ServoPowerPhase::Off &&
	          p.fault == ServoPowerFault::None && p.upMs == SUPPLY_UP_NONE);
	powerPass(&c, &p, cmdOf(1, ServoCmdKind::Start), none, 1000);
	checkTrue("START: RUNNING, Starting, no pulse", c.running &&
	          p.phase == ServoPowerPhase::Starting && c.outUs[0] == 0 && c.outUs[1] == 0);
	checkInt("the switch-on's time", p.onMs, 1000);
	powerPass(&c, &p, cmdOf(2, ServoCmdKind::Position, SERVO_MASK_A, 1000), none, 1200);
	checkTrue("a position while Starting: held at CENTRE, no pulse",
	          c.target[0] == SERVO_CMD_CENTRE && c.pos[0] == SERVO_CMD_CENTRE * 1000u &&
	          c.outUs[0] == 0);
	ServoCmd sw = sweepOf(3, SWEEP_SINE, 1000, 400, 0, 0);
	powerPass(&c, &p, sw, none, 1300);
	checkTrue("a sweep while Starting: ended", !c.sweeping && c.outUs[0] == 0);
	powerPass(&c, &p, cmdOf(3, ServoCmdKind::Start), none, 1499);
	checkTrue("no sensor, 499 ms: still Starting", p.phase == ServoPowerPhase::Starting &&
	          c.outUs[0] == 0);
	powerPass(&c, &p, cmdOf(3, ServoCmdKind::Start), none, 1500);
	checkTrue("500 ms: On, CENTRE", p.phase == ServoPowerPhase::On && c.outUs[0] == 1500);
	checkTrue("no start-up time without a sensor", p.upMs == SUPPLY_UP_NONE);
	powerPass(&c, &p, cmdOf(4, ServoCmdKind::Position, SERVO_MASK_A, 1000), none, 1510);
	checkInt("On: positions drive", c.outUs[0], 2000);

	// With a sensor: the first fresh reading after the switch-on at 4.5 V.
	powerFresh(&c, &p, true);
	const uint32_t t0 = 0xFFFFFF00u;   // 256 ms before the clock wraps
	ServoPowerIn before = powerInOf(SenseState::Online, 5000, t0, true);
	checkTrue("online: no start block", servoPowerStartBlock(&p, &before) == ServoStartBlock::None);
	powerPass(&c, &p, cmdOf(1, ServoCmdKind::Start), before, t0);
	checkTrue("a 5 V reading taken in the switch-on's ms does not count",
	          p.phase == ServoPowerPhase::Starting);
	ServoPowerIn lag = powerInOf(SenseState::Online, 5000, t0 + SUPPLY_READ_LAG_MS, true);
	powerPass(&c, &p, cmdOf(1, ServoCmdKind::Start), lag, t0 + SUPPLY_READ_LAG_MS);
	checkTrue("5 V taken 18 ms after: within one conversion, not counted",
	          p.phase == ServoPowerPhase::Starting);
	ServoPowerIn low = powerInOf(SenseState::Online, 4499, t0 + 40, true);
	powerPass(&c, &p, cmdOf(1, ServoCmdKind::Start), low, t0 + 41);
	checkTrue("4499 mV: still Starting", p.phase == ServoPowerPhase::Starting && c.outUs[0] == 0);
	// Taken before the switch-on, so the sensor-lost rule counts from the
	// switch-on: 240 ms, no cut.
	ServoPowerIn old = powerInOf(SenseState::Online, 5000, t0 - 10, true);
	powerPass(&c, &p, cmdOf(1, ServoCmdKind::Start), old, t0 - 10 + SENSE_STALE_MS);
	checkTrue("a 5 V reading 250 ms old: stale, still Starting",
	          p.phase == ServoPowerPhase::Starting && c.running);
	ServoPowerIn up = powerInOf(SenseState::Online, 4500, t0 + 300, true);
	powerPass(&c, &p, cmdOf(1, ServoCmdKind::Start), up, t0 + 301);
	checkTrue("4500 mV taken 300 ms after, across the wrap: On, CENTRE",
	          p.phase == ServoPowerPhase::On && c.outUs[0] == 1500);
	checkInt("the start-up time: the reading's time after the switch-on", p.upMs, 300);
}

/** @brief A supply that does not come up within 500 ms. */
static void testPowerNoStart() {
	section("Servo supply rules: no 4.5 V within 500 ms switches off");

	ServoCore c;
	ServoPower p;
	powerFresh(&c, &p, true);
	ServoPowerIn low = powerInOf(SenseState::Online, 3600, 1000, true);
	powerPass(&c, &p, cmdOf(1, ServoCmdKind::Start), low, 1000);
	low.sample.takenMs = 1499;
	powerPass(&c, &p, cmdOf(1, ServoCmdKind::Start), low, 1499);
	checkTrue("499 ms: still Starting", c.running && p.phase == ServoPowerPhase::Starting);
	low.sample.takenMs = 1500;
	powerPass(&c, &p, cmdOf(1, ServoCmdKind::Start), low, 1500);
	checkTrue("500 ms: off, SAFE, NoStart", !c.running && p.phase == ServoPowerPhase::Off &&
	          p.fault == ServoPowerFault::NoStart && c.outUs[0] == 0);
	checkTrue("not a trip", !c.tripped);
	checkTrue("the read-back armed at the switch-off", p.offCheck == ServoOffCheck::Waiting &&
	          p.offMs == 1500);
	servoPowerReset(&p, true, false);
	checkTrue("a rebind keeps the fault", p.fault == ServoPowerFault::NoStart);
	p.upMs = 40;
	ServoPowerIn ok = powerInOf(SenseState::Online, 0, 1600, true);
	powerPass(&c, &p, cmdOf(2, ServoCmdKind::Start), ok, 1600);
	checkTrue("START clears it, the read-back and the last start-up time",
	          c.running && p.fault == ServoPowerFault::None &&
	          p.offCheck == ServoOffCheck::None && p.upMs == SUPPLY_UP_NONE);
	p.upMs = 40;
	servoPowerReset(&p, true, false);
	checkInt("a rebind keeps the start-up time", p.upMs, 40);
	servoPowerReset(&p, true, true);
	checkTrue("a new session drops a fault and the time",
	          p.fault == ServoPowerFault::None && p.upMs == SUPPLY_UP_NONE);
}

/** @brief Every way the pulses end switches the supply off. */
static void testPowerOff() {
	section("Servo supply rules: STOP, the heartbeat, touch and the bus switch off");

	ServoCore c;
	ServoPower p;
	ServoPowerIn none = powerInOf(SenseState::None);
	// STOP.
	powerFresh(&c, &p, true);
	powerPass(&c, &p, cmdOf(1, ServoCmdKind::Start), none, 0);
	powerPass(&c, &p, cmdOf(1, ServoCmdKind::Start), none, 600);
	powerPass(&c, &p, cmdOf(2, ServoCmdKind::Safe), none, 700);
	checkTrue("STOP: off in the same pass", !c.running && p.phase == ServoPowerPhase::Off &&
	          c.outUs[0] == 0 && p.offMs == 700);
	checkTrue("no sensor: no read-back", p.offCheck == ServoOffCheck::None);
	// STOP while Starting.
	powerPass(&c, &p, cmdOf(3, ServoCmdKind::Start), none, 800);
	powerPass(&c, &p, cmdOf(4, ServoCmdKind::Safe), none, 900);
	checkTrue("STOP while Starting: off", p.phase == ServoPowerPhase::Off && !c.running);
	// The heartbeat.
	powerPass(&c, &p, cmdOf(5, ServoCmdKind::Start), none, 1000);
	powerPass(&c, &p, cmdOf(5, ServoCmdKind::Start), none, 1600);
	powerPass(&c, &p, cmdOf(5, ServoCmdKind::Start), none, 1700, false);
	checkTrue("a heartbeat lapse: tripped, off", c.tripped && p.phase == ServoPowerPhase::Off &&
	          p.fault == ServoPowerFault::None);
	powerPass(&c, &p, cmdOf(6, ServoCmdKind::Start), none, 1800, false);
	checkTrue("START with the heartbeat lapsed: stays off", !c.running &&
	          p.phase == ServoPowerPhase::Off);
	// Touch silent, while On and while Starting.
	ServoPowerIn silent = powerInOf(SenseState::None, 0, 0, false);
	checkTrue("touch silent: START refused", servoPowerStartBlock(&p, &silent) ==
	          ServoStartBlock::TouchSilent);
	powerPass(&c, &p, cmdOf(7, ServoCmdKind::Start), silent, 1900);
	checkTrue("and not taken", !c.running && p.phase == ServoPowerPhase::Off);
	powerPass(&c, &p, cmdOf(8, ServoCmdKind::Start), none, 2000);
	powerPass(&c, &p, cmdOf(8, ServoCmdKind::Start), none, 2500);
	checkTrue("On again, the trip cleared", p.phase == ServoPowerPhase::On && !c.tripped);
	powerPass(&c, &p, cmdOf(8, ServoCmdKind::Start), silent, 2510);
	checkTrue("touch silent while On: off, tripped, TouchSilent",
	          !c.running && c.tripped && p.phase == ServoPowerPhase::Off &&
	          p.fault == ServoPowerFault::TouchSilent && c.outUs[0] == 0);
	powerPass(&c, &p, cmdOf(9, ServoCmdKind::Start), none, 2600);
	powerPass(&c, &p, cmdOf(9, ServoCmdKind::Start), silent, 2610);
	checkTrue("touch silent while Starting: off, TouchSilent",
	          !c.running && p.phase == ServoPowerPhase::Off && p.fault == ServoPowerFault::TouchSilent);
	powerPass(&c, &p, cmdOf(9, ServoCmdKind::Start), none, 2700);
	checkTrue("SAFE: the fault stays", p.fault == ServoPowerFault::TouchSilent && !c.running);
	// The bus stuck, with a sensor online.
	ServoPowerIn on = powerInOf(SenseState::Online, 5000, 2699, true);
	powerPass(&c, &p, cmdOf(10, ServoCmdKind::Start), on, 2700);
	on.sample.takenMs = 2720;
	powerPass(&c, &p, cmdOf(10, ServoCmdKind::Start), on, 2720);
	checkTrue("with a sensor: On at the first 5 V reading", p.phase == ServoPowerPhase::On);
	checkInt("up in 20 ms", p.upMs, 20);
	ServoPowerIn stuck = powerInOf(SenseState::Stuck, 5000, 2740, true);
	checkTrue("a stuck bus: START refused", servoPowerStartBlock(&p, &stuck) ==
	          ServoStartBlock::BusStuck);
	powerPass(&c, &p, cmdOf(10, ServoCmdKind::Start), stuck, 2740);
	checkTrue("a stuck bus while On: off, tripped, BusStuck",
	          !c.running && c.tripped && p.phase == ServoPowerPhase::Off &&
	          p.fault == ServoPowerFault::BusStuck);
	checkTrue("the read-back armed", p.offCheck == ServoOffCheck::Waiting && p.offMs == 2740);
	// A silent touch controller and a stuck bus at once: touch first.
	ServoPowerIn both = powerInOf(SenseState::Stuck, 5000, 2740, false);
	checkTrue("both: TouchSilent first", servoPowerStartBlock(&p, &both) ==
	          ServoStartBlock::TouchSilent);
	on.sample.takenMs = 2799;
	powerPass(&c, &p, cmdOf(11, ServoCmdKind::Start), on, 2800);
	powerPass(&c, &p, cmdOf(11, ServoCmdKind::Start), both, 2810);
	checkTrue("both while Starting: TouchSilent", p.fault == ServoPowerFault::TouchSilent);
	// servoPowerOff() while off keeps the first time.
	servoPowerOff(&p, true, 5000);
	checkInt("off again: the first switch-off's time kept", p.offMs, 2810);
}

/** @brief START refused with the sensor declared and not online. */
static void testPowerStartBlock() {
	section("Servo supply rules: START waits for a declared sensor");

	ServoCore c;
	ServoPower p;
	powerFresh(&c, &p, true);
	const SenseState states[] = { SenseState::None, SenseState::Absent, SenseState::Online,
	                              SenseState::Offline, SenseState::Stuck };
	const ServoStartBlock want[] = { ServoStartBlock::None, ServoStartBlock::NoSensor,
	                                 ServoStartBlock::None, ServoStartBlock::NoSensor,
	                                 ServoStartBlock::BusStuck };
	static const char *const NAMES[] = { "NONE: no block", "absent: NoSensor", "online: no block",
	                                     "offline: NoSensor", "stuck: BusStuck" };
	for (int i = 0; i < 5; i++) {
		ServoPowerIn in = powerInOf(states[i]);
		checkTrue(NAMES[i], servoPowerStartBlock(&p, &in) == want[i]);
	}
	ServoPowerIn absent = powerInOf(SenseState::Absent);
	powerPass(&c, &p, cmdOf(1, ServoCmdKind::Start), absent, 1000);
	checkTrue("absent: START refused, the supply off", !c.running &&
	          p.phase == ServoPowerPhase::Off);
	checkInt("and acknowledged", c.ackSeq, 1);
	// Online at START, then gone before 4.5 V: the sensor-lost cut, before
	// its last reading could count.
	ServoPowerIn on = powerInOf(SenseState::Online, 0, 1999, true);
	powerPass(&c, &p, cmdOf(2, ServoCmdKind::Start), on, 2000);
	ServoPowerIn off = powerInOf(SenseState::Offline, 5000, 2100, true);
	powerPass(&c, &p, cmdOf(2, ServoCmdKind::Start), off, 2100);
	checkTrue("a part offline while Starting: the sensor-lost cut, off, NoSensor",
	          p.phase == ServoPowerPhase::Off && !c.running && c.tripped &&
	          p.fault == ServoPowerFault::NoSensor && c.outUs[0] == 0);
}

/** @brief The read-back after the switch-off: Off and Not off. */
static void testPowerReadBack() {
	section("Servo supply rules: the read-back after the switch-off");

	ServoCore c;
	ServoPower p;
	powerFresh(&c, &p, true);
	ServoPowerIn on = powerInOf(SenseState::Online, 5000, 1001, true);
	powerPass(&c, &p, cmdOf(1, ServoCmdKind::Start), on, 1000);
	powerPass(&c, &p, cmdOf(1, ServoCmdKind::Start), on, 1001);
	powerPass(&c, &p, cmdOf(2, ServoCmdKind::Safe), on, 2000);
	checkTrue("STOP with a sensor: Waiting", p.offCheck == ServoOffCheck::Waiting);
	ServoPowerIn at = powerInOf(SenseState::Online, 900, 2000, true);
	powerPass(&c, &p, cmdOf(2, ServoCmdKind::Safe), at, 2001);
	checkTrue("900 mV taken in the switch-off's ms: not counted",
	          p.offCheck == ServoOffCheck::Waiting);
	ServoPowerIn lagOff = powerInOf(SenseState::Online, 900, 2000 + SUPPLY_READ_LAG_MS, true);
	powerPass(&c, &p, cmdOf(2, ServoCmdKind::Safe), lagOff, 2000 + SUPPLY_READ_LAG_MS);
	checkTrue("900 mV taken 18 ms after: within one conversion, not counted",
	          p.offCheck == ServoOffCheck::Waiting);
	ServoPowerIn high = powerInOf(SenseState::Online, 1001, 2030, true);
	powerPass(&c, &p, cmdOf(2, ServoCmdKind::Safe), high, 2031);
	checkTrue("1001 mV: Waiting", p.offCheck == ServoOffCheck::Waiting);
	ServoPowerIn low = powerInOf(SenseState::Online, 1000, 2060, true);
	powerPass(&c, &p, cmdOf(2, ServoCmdKind::Safe), low, 2061);
	checkTrue("1000 mV: Off", p.offCheck == ServoOffCheck::Off);
	ServoPowerIn back = powerInOf(SenseState::Online, 5000, 4500, true);
	powerPass(&c, &p, cmdOf(2, ServoCmdKind::Safe), back, 4501);
	checkTrue("Off stays Off: the check is done", p.offCheck == ServoOffCheck::Off);

	// Not off: a converter with no disconnect, or EN held high.
	powerPass(&c, &p, cmdOf(3, ServoCmdKind::Start), on, 5000);
	ServoPowerIn on2 = powerInOf(SenseState::Online, 5000, 5010, true);
	powerPass(&c, &p, cmdOf(3, ServoCmdKind::Start), on2, 5010);
	powerPass(&c, &p, cmdOf(4, ServoCmdKind::Safe), on2, 6000);
	ServoPowerIn stays = powerInOf(SenseState::Online, 2501, 7999, true);
	powerPass(&c, &p, cmdOf(4, ServoCmdKind::Safe), stays, 7999);
	checkTrue("2501 mV taken 1999 ms after: Waiting", p.offCheck == ServoOffCheck::Waiting);
	stays.sample.takenMs = 8000;
	stays.sample.busMv = 2500;
	powerPass(&c, &p, cmdOf(4, ServoCmdKind::Safe), stays, 8000);
	checkTrue("2500 mV at 2000 ms: Waiting", p.offCheck == ServoOffCheck::Waiting);
	stays.sample.busMv = 2501;
	powerPass(&c, &p, cmdOf(4, ServoCmdKind::Safe), stays, 8000);
	checkTrue("2501 mV at 2000 ms: NotOff", p.offCheck == ServoOffCheck::NotOff);
	checkTrue("a warning, not a cut: SAFE, no fault", !c.running && p.fault == ServoPowerFault::None);
	ServoPowerIn stale = powerInOf(SenseState::Online, 900, 8100, true);
	powerPass(&c, &p, cmdOf(4, ServoCmdKind::Safe), stale, 8100 + SENSE_STALE_MS);
	checkTrue("a stale reading changes nothing", p.offCheck == ServoOffCheck::NotOff);
	ServoPowerIn fell = powerInOf(SenseState::Online, 1000, 8400, true);
	powerPass(&c, &p, cmdOf(4, ServoCmdKind::Safe), fell, 8401);
	checkTrue("then 1000 mV: Off", p.offCheck == ServoOffCheck::Off);
	powerPass(&c, &p, cmdOf(5, ServoCmdKind::Start), on, 9000);
	checkTrue("START: the read-back ends", p.offCheck == ServoOffCheck::None);
	ServoPowerIn up = powerInOf(SenseState::Online, 5000, 9019, true);
	powerPass(&c, &p, cmdOf(5, ServoCmdKind::Start), up, 9019);
	checkTrue("5 V taken 19 ms after the switch-on: On", p.phase == ServoPowerPhase::On);
	checkInt("up in 19 ms", p.upMs, 19);
	powerPass(&c, &p, cmdOf(6, ServoCmdKind::Safe), up, 9100);
	ServoPowerIn gone = powerInOf(SenseState::Online, 900, 9119, true);
	powerPass(&c, &p, cmdOf(6, ServoCmdKind::Safe), gone, 9119);
	checkTrue("900 mV taken 19 ms after the switch-off: Off", p.offCheck == ServoOffCheck::Off);
}

/** @brief Inputs of one pass: reading @p seq of @p ua at @p mv, taken at @p takenMs. */
static ServoPowerIn readingIn(uint32_t seq, int32_t ua, uint16_t mv, uint32_t takenMs,
                              SenseState state = SenseState::Online) {
	ServoPowerIn in = powerInOf(state, mv, takenMs, true);
	in.sample.seq = seq;
	in.sample.currentUa = ua;
	return in;
}

/** @brief Switched, a sensor: START at @p t, up at a 5 V reading 30 ms later. */
static void cutStart(ServoCore *c, ServoPower *p, uint16_t cmdSeq, uint32_t *seq, uint32_t t) {
	powerPass(c, p, cmdOf(cmdSeq, ServoCmdKind::Start), readingIn(*seq, 0, 0, t - 10), t);
	++*seq;
	powerPass(c, p, cmdOf(cmdSeq, ServoCmdKind::Start), readingIn(*seq, 0, 5000, t + 30), t + 30);
}

/**
 * @brief Readings of @p ua at @p mv every 25 ms from @p from on, while
 *        RUNNING, at most to @p to. @return The time of the reading that
 *        ended RUNNING, or 0.
 */
static uint32_t cutRun(ServoCore *c, ServoPower *p, uint16_t cmdSeq, uint32_t *seq, uint32_t from,
                       uint32_t to, int32_t ua, uint16_t mv) {
	for (uint32_t t = from; t <= to; t += 25) {
		++*seq;
		powerPass(c, p, cmdOf(cmdSeq, ServoCmdKind::Start), readingIn(*seq, ua, mv, t), t);
		if (!c->running) return t;
	}
	return 0;
}

/** @brief The cuts of supply_guard.h in servoPowerStep(), with the switch and with OWN. */
static void testPowerCuts() {
	section("Servo supply rules: the cuts on the sensor's readings");

	ServoCore c;
	ServoPower p;
	uint32_t seq = 1;
	powerFresh(&c, &p, true);
	cutStart(&c, &p, 1, &seq, 1000);
	checkTrue("up: On, CENTRE, the guard armed at the switch-on",
	          p.phase == ServoPowerPhase::On && c.outUs[0] == 1500 && p.guard.armed &&
	          p.guard.armMs == 1000 && p.guard.up);
	powerPass(&c, &p, cmdOf(1, ServoCmdKind::Start), readingIn(++seq, 2999900, 5000, 1055), 1055);
	checkTrue("2999.9 mA: running", c.running && c.outUs[0] == 1500);
	powerPass(&c, &p, cmdOf(1, ServoCmdKind::Start), readingIn(++seq, 3000000, 5000, 1080), 1080);
	checkTrue("3000 mA: the pulses end and the supply goes off in the same pass",
	          !c.running && c.outUs[0] == 0 && c.outUs[1] == 0 && p.phase == ServoPowerPhase::Off &&
	          p.offMs == 1080);
	checkTrue("latched: tripped, Overcurrent, the guard disarmed, the read-back armed",
	          c.tripped && p.fault == ServoPowerFault::Overcurrent && !p.guard.armed &&
	          p.offCheck == ServoOffCheck::Waiting);
	powerPass(&c, &p, cmdOf(1, ServoCmdKind::Start), readingIn(++seq, 3000000, 5000, 1105), 1105);
	checkTrue("SAFE: a later reading changes nothing", p.fault == ServoPowerFault::Overcurrent &&
	          p.phase == ServoPowerPhase::Off);
	servoPowerReset(&p, true, false);
	checkTrue("a rebind keeps the fault", p.fault == ServoPowerFault::Overcurrent && !p.guard.armed);
	cutStart(&c, &p, 2, &seq, 2000);
	checkTrue("START clears it", c.running && !c.tripped && p.fault == ServoPowerFault::None &&
	          p.phase == ServoPowerPhase::On);
	powerPass(&c, &p, cmdOf(2, ServoCmdKind::Start),
	          readingIn(++seq, 100000, 5000, 2055, SenseState::Online), 2055);
	SenseSample clip = readingIn(++seq, 100000, 5000, 2080).sample;
	clip.flags = SENSE_FLAG_CLIPPED;
	ServoPowerIn clipped = readingIn(seq, 100000, 5000, 2080);
	clipped.sample = clip;
	powerPass(&c, &p, cmdOf(2, ServoCmdKind::Start), clipped, 2080);
	checkTrue("a clipped reading: Overcurrent", !c.running && p.fault == ServoPowerFault::Overcurrent);

	// The stall: above 1.0 A for 400 ms.
	cutStart(&c, &p, 3, &seq, 3000);
	checkInt("1.2 A from 3055: cut by the reading 400 ms on",
	         cutRun(&c, &p, 3, &seq, 3055, 4000, 1200000, 4900), 3455);
	checkTrue("Stall, off, tripped", p.fault == ServoPowerFault::Stall && c.tripped &&
	          p.phase == ServoPowerPhase::Off && c.outUs[0] == 0);
	cutStart(&c, &p, 4, &seq, 5000);
	checkInt("1.0 A for 3 s: running", cutRun(&c, &p, 4, &seq, 5055, 8055, 1000000, 4900), 0);
	powerPass(&c, &p, cmdOf(5, ServoCmdKind::Safe), readingIn(++seq, 0, 0, 8080), 8080);

	// The collapse: under 4.0 V for 1000 ms after start-up.
	cutStart(&c, &p, 6, &seq, 9000);
	checkInt("3.9 V from 9055: cut by the reading 1000 ms on",
	         cutRun(&c, &p, 6, &seq, 9055, 11000, 500000, 3900), 10055);
	checkTrue("LowSupply, off", p.fault == ServoPowerFault::LowSupply &&
	          p.phase == ServoPowerPhase::Off && c.tripped);
	// Below 4.5 V from the switch-on: no start-up reading, so the collapse
	// rule never counts; the supply that never came up is NoStart.
	powerPass(&c, &p, cmdOf(7, ServoCmdKind::Start), readingIn(++seq, 0, 0, 11990), 12000);
	checkInt("3.9 V from the switch-on: NoStart at 500 ms, not LowSupply",
	         cutRun(&c, &p, 7, &seq, 12025, 13500, 0, 3900), 12500);
	checkTrue("NoStart", p.fault == ServoPowerFault::NoStart && !c.tripped);

	// The sensor lost.
	cutStart(&c, &p, 8, &seq, 14000);
	powerPass(&c, &p, cmdOf(8, ServoCmdKind::Start), readingIn(seq, 0, 5000, 14030), 14279);
	checkTrue("no reading for 249 ms: running", c.running);
	powerPass(&c, &p, cmdOf(8, ServoCmdKind::Start), readingIn(seq, 0, 5000, 14030), 14280);
	checkTrue("250 ms: NoSensor, off", !c.running && c.tripped &&
	          p.fault == ServoPowerFault::NoSensor && p.phase == ServoPowerPhase::Off &&
	          c.outUs[0] == 0);
	cutStart(&c, &p, 9, &seq, 15000);
	powerPass(&c, &p, cmdOf(9, ServoCmdKind::Start),
	          readingIn(seq, 0, 5000, 15030, SenseState::Offline), 15031);
	checkTrue("the part offline: NoSensor at once", !c.running &&
	          p.fault == ServoPowerFault::NoSensor);
	ServoPowerIn back = readingIn(++seq, 0, 0, 15100);
	checkTrue("online again: START allowed", servoPowerStartBlock(&p, &back) == ServoStartBlock::None);

	// The order: touch silent and the stuck bus before the guard's cuts.
	cutStart(&c, &p, 10, &seq, 16000);
	ServoPowerIn both = readingIn(++seq, 3200000, 5000, 16055);
	both.touchOk = false;
	powerPass(&c, &p, cmdOf(10, ServoCmdKind::Start), both, 16055);
	checkTrue("touch silent and 3.2 A: TouchSilent", p.fault == ServoPowerFault::TouchSilent);
	cutStart(&c, &p, 11, &seq, 17000);
	powerPass(&c, &p, cmdOf(11, ServoCmdKind::Start),
	          readingIn(++seq, 3200000, 5000, 17055, SenseState::Stuck), 17055);
	checkTrue("stuck and 3.2 A: BusStuck", p.fault == ServoPowerFault::BusStuck);
	// STOP disarms the guard.
	cutStart(&c, &p, 12, &seq, 18000);
	powerPass(&c, &p, cmdOf(13, ServoCmdKind::Safe), readingIn(++seq, 0, 5000, 18055), 18055);
	checkTrue("STOP: the guard disarmed", !p.guard.armed && p.fault == ServoPowerFault::None);

	// OWN: hard and stall end the pulses; collapse and sensor lost do not apply.
	powerFresh(&c, &p, false);
	seq = 1;
	powerPass(&c, &p, cmdOf(1, ServoCmdKind::Start), readingIn(seq, 0, 7400, 990), 1000);
	checkTrue("OWN, START: CENTRE at once, the guard armed", c.running && c.outUs[0] == 1500 &&
	          p.guard.armed && p.phase == ServoPowerPhase::Off);
	powerPass(&c, &p, cmdOf(1, ServoCmdKind::Start), readingIn(++seq, 3000000, 7400, 1025), 1025);
	checkTrue("OWN, 3 A: the pulses end in the same pass, Overcurrent, tripped",
	          !c.running && c.outUs[0] == 0 && c.outUs[1] == 0 && c.tripped &&
	          p.fault == ServoPowerFault::Overcurrent && p.phase == ServoPowerPhase::Off);
	servoPowerReset(&p, false, false);
	checkTrue("OWN, a rebind keeps it", p.fault == ServoPowerFault::Overcurrent);
	c.tripped = false;
	p.fault = ServoPowerFault::None;
	powerPass(&c, &p, cmdOf(1, ServoCmdKind::Start), readingIn(++seq, 3200000, 7400, 1500), 1500);
	checkTrue("OWN, SAFE: a 3.2 A reading trips nothing, the guard disarmed",
	          !c.tripped && p.fault == ServoPowerFault::None && !p.guard.armed);
	powerPass(&c, &p, cmdOf(2, ServoCmdKind::Start), readingIn(++seq, 0, 7400, 2000), 2000);
	checkTrue("OWN, START clears it", c.running && p.fault == ServoPowerFault::None);
	checkInt("OWN, 1.5 A: Stall 400 ms on", cutRun(&c, &p, 2, &seq, 2025, 3000, 1500000, 7400), 2425);
	checkTrue("Stall", p.fault == ServoPowerFault::Stall && c.outUs[0] == 0);
	powerPass(&c, &p, cmdOf(3, ServoCmdKind::Start), readingIn(++seq, 0, 7400, 4000), 4000);
	checkInt("OWN, 0 V for 3 s: no collapse", cutRun(&c, &p, 3, &seq, 4025, 7025, 0, 0), 0);
	powerPass(&c, &p, cmdOf(3, ServoCmdKind::Start),
	          readingIn(seq, 0, 0, 7025, SenseState::Offline), 9000);
	checkTrue("OWN, the part offline and no reading for 2 s: running, no sensor-lost cut",
	          c.running && c.outUs[0] == 1500);
	powerPass(&c, &p, cmdOf(3, ServoCmdKind::Start),
	          readingIn(++seq, 3200000, 0, 9000, SenseState::Offline), 9000);
	checkTrue("OWN, a fresh 3.2 A reading from a part now offline: Overcurrent",
	          !c.running && p.fault == ServoPowerFault::Overcurrent);
	// OWN with CURRENT SENSE at NONE: no guard.
	powerPass(&c, &p, cmdOf(4, ServoCmdKind::Start), powerInOf(SenseState::None), 10000);
	ServoPowerIn none = readingIn(++seq, 3200000, 0, 10025, SenseState::None);
	powerPass(&c, &p, cmdOf(4, ServoCmdKind::Start), none, 10025);
	checkTrue("OWN, CURRENT SENSE NONE: nothing judged, running", c.running && !p.guard.armed &&
	          p.fault == ServoPowerFault::None);
	servoPowerReset(&p, false, true);
	checkTrue("a new session drops the fault", p.fault == ServoPowerFault::None);
}

/** @brief The board's wiring, KISS on, the pump bound and past any drain. */
static void freshPump() {
	escTaskConfigure(SIG, 600, true, AUX);
	EscTelemetry t;
	memset(&t, 0, sizeof(t));
	fakeSetTelemetry(&t);
	fakePumpPoll();
	fakeAdvance(1000);
	fakePumpPoll();
}

/** @brief A SERVO claim through to Running, with the shipped job. */
static bool claimRunning(uint8_t roles) {
	if (pinClaimBegin(PinUser::Servo, roles) != ClaimRefusal::None) return false;
	fakePumpPoll();
	pinClaimPoll();
	core1JobPoll();
	return pinClaimPoll() == ClaimState::Running;
}

/** @brief BACK, the unplug page, RESTART DSHOT, and the pump back. */
static bool claimEnd() {
	pinClaimEnd(PinUser::Servo);
	core1JobPoll();
	// Long enough for the last pulse at any rate.
	fakeAdvance(servoQuietMs(SERVO_RATE_MIN_HZ));
	core1JobPoll();
	pinClaimPoll();
	bool page = pinClaimUnplugDue();
	pinClaimRestart(PinUser::Servo);
	fakePumpPoll();
	return page && pinClaimState() == ClaimState::Free;
}

/** @brief Send @p c and give core1 one pass. */
static void send(const ServoCmd &c) {
	servoJobCommand(&c);
	core1JobPoll();
}

/** @brief The job as it ships, on the claim, the PWM fake and the clock. */
static void testJob() {
	section("Servo job: on the claim, through the job table");

	fakePwmReset();
	freshPump();
	escHeartbeat();
	ServoProfile prof;
	servoProfileDefault(&prof);
	checkTrue("a profile while idle is accepted", servoJobProfile(&prof));

	checkTrue("one servo: the claim runs the shipped job", claimRunning(PIN_SIG | PIN_AUX));
	checkTrue("SIG bound to PWM at level 0", fakePin(SIG).function == GPIO_FUNC_PWM &&
	                                         fakePin(SIG).level == 0);
	checkTrue("AUX left alone with one servo", fakePin(AUX).function != GPIO_FUNC_PWM);
	checkInt("at 50 Hz", fakeSlice(0).top, 19999);
	ServoStatus st;
	servoJobStatus(&st);
	checkTrue("status: SAFE, no pulse, no error",
	          !st.running && st.outUs[0] == 0 && st.bindError == 0);

	ServoCmd c = cmdOf(0, ServoCmdKind::Start);
	uint16_t seq = servoJobCommand(&c);
	servoJobStatus(&st);
	checkTrue("not taken before core1's pass", st.ackSeq != seq);
	checkTrue("a profile with a command pending: refused", !servoJobProfile(&prof));
	core1JobPoll();
	servoJobStatus(&st);
	checkTrue("START: taken on the next pass", st.ackSeq == seq && st.running);
	checkInt("CENTRE on SIG", fakePin(SIG).level, 1500);
	checkInt("status reports it", st.outUs[0], 1500);
	checkTrue("a profile while RUNNING: refused", !servoJobProfile(&prof));

	send(cmdOf(0, ServoCmdKind::Position, SERVO_MASK_A, 1000));
	checkInt("a position: MAX", fakePin(SIG).level, 2000);
	int writes = fakePin(SIG).levelWrites;
	core1JobPoll();
	checkInt("an unchanged pulse is not written again", fakePin(SIG).levelWrites, writes);

	// STOP, then a position before core1's pass: the STOP is kept.
	ServoCmd safe = cmdOf(0, ServoCmdKind::Safe);
	uint16_t safeSeq = servoJobCommand(&safe);
	ServoCmd late = cmdOf(0, ServoCmdKind::Position, SERVO_MASK_A, 0);
	checkInt("a command after an untaken STOP is dropped", servoJobCommand(&late), safeSeq);
	core1JobPoll();
	checkInt("STOP: level 0 on the next pass", fakePin(SIG).level, 0);
	servoJobStatus(&st);
	checkTrue("SAFE", !st.running);
	checkInt("a command after a taken STOP is held",
	         servoJobCommand(&late), (uint16_t)(safeSeq + 1));
	core1JobPoll();
	checkInt("and moves nothing while SAFE", fakePin(SIG).level, 0);

	ServoProfile bad = prof;
	bad.maxUs = 2600;
	checkTrue("an invalid profile: refused", !servoJobProfile(&bad));

	// Two servos at 100 Hz, while SAFE: both pins rebound.
	ServoProfile two = prof;
	two.servos = 2;
	two.rateHz = 100;
	int inits = fakeSlice(0).inits;
	checkTrue("two servos at 100 Hz while SAFE: accepted", servoJobProfile(&two));
	core1JobPoll();
	checkTrue("the rebind waits for the last pulse, 41 ms after the STOP",
	          fakeSlice(0).inits == inits && fakePin(AUX).function != GPIO_FUNC_PWM);
	fakeAdvance(servoQuietMs(50) - 1);
	core1JobPoll();
	checkInt("not at 40 ms", fakeSlice(0).inits, inits);
	fakeAdvance(1);
	core1JobPoll();
	checkTrue("both pins on PWM at level 0",
	          fakePin(SIG).function == GPIO_FUNC_PWM && fakePin(AUX).function == GPIO_FUNC_PWM &&
	          fakePin(SIG).level == 0 && fakePin(AUX).level == 0);
	checkTrue("the slice set up again, at 100 Hz",
	          fakeSlice(0).inits == inits + 1 && fakeSlice(0).top == 9999);
	send(cmdOf(0, ServoCmdKind::Start));
	checkTrue("START: CENTRE on both", fakePin(SIG).level == 1500 && fakePin(AUX).level == 1500);
	send(cmdOf(0, ServoCmdKind::Position, SERVO_MASK_B, 1000));
	checkTrue("B alone to MAX", fakePin(SIG).level == 1500 && fakePin(AUX).level == 2000);

	ServoCmd sw = sweepOf(0, SWEEP_TRIANGLE, 1000, 500, 0, 0);
	servoJobCommand(&sw);
	core1JobPoll();
	fakeAdvance(250);
	escHeartbeat();
	core1JobPoll();
	servoJobStatus(&st);
	checkTrue("a sweep on both: +500 units at a quarter",
	          st.sweeping && fakePin(SIG).level == 2000 && fakePin(AUX).level == 2000);
	checkInt("one end reached", st.sweepMoves, 1);
	send(cmdOf(0, ServoCmdKind::SweepStop));
	servoJobStatus(&st);
	checkTrue("STOP SWEEP", !st.sweeping && st.sweepMoves == 0 && st.running);
	send(cmdOf(0, ServoCmdKind::Safe));

	// A rebind the SDK refuses leaves both pins unbound and says why.
	fakePwmBreakSliceMap(true);
	checkTrue("a profile accepted", servoJobProfile(&prof));
	fakeAdvance(servoQuietMs(100));
	core1JobPoll();
	servoJobStatus(&st);
	checkInt("the rebind is refused: PWM", st.bindError, CORE1_JOB_PWM);
	checkTrue("both pins released", fakePin(SIG).function == GPIO_FUNC_SIO &&
	                                fakePin(AUX).function == GPIO_FUNC_SIO);
	send(cmdOf(0, ServoCmdKind::Start));
	servoJobStatus(&st);
	checkTrue("START with nothing bound: refused", !st.running && fakePin(SIG).level == 0);
	fakePwmBreakSliceMap(false);
	checkTrue("the profile again", servoJobProfile(&prof));
	core1JobPoll();
	servoJobStatus(&st);
	checkTrue("binds, no error", st.bindError == 0 && fakePin(SIG).function == GPIO_FUNC_PWM);

	checkTrue("BACK: the unplug page, then DShot", claimEnd());
	checkTrue("SIG an input, pulled down", fakePin(SIG).function == GPIO_FUNC_SIO &&
	                                       fakePin(SIG).pull == FakePull::Down);
	checkTrue("the slice stopped", !fakeSlice(0).enabled);
	servoJobStatus(&st);
	checkTrue("status after the release: SAFE, no pulse", !st.running && st.outUs[0] == 0);

	// The first bind refused: the claim stays Held with the reason, and
	// RETRY binds once the cause is gone.
	fakePwmBreakSliceMap(true);
	checkTrue("the claim begins", pinClaimBegin(PinUser::Servo, PIN_SIG) == ClaimRefusal::None);
	fakePumpPoll();
	pinClaimPoll();
	core1JobPoll();
	checkTrue("held, the bind refused", pinClaimPoll() == ClaimState::Held &&
	                                    pinClaimJobError() == CORE1_JOB_PWM);
	fakePwmBreakSliceMap(false);
	pinClaimRetry(PinUser::Servo);
	core1JobPoll();
	checkTrue("RETRY: running", pinClaimPoll() == ClaimState::Running);
	checkTrue("back to free", claimEnd());
}

/**
 * @brief The trip through the job, on the shipped heartbeat rule, across a
 *        wrap.
 */
static void testJobTrip() {
	section("Servo job: the heartbeat trip at 250 ms, across the millis() wrap");

	fakePwmReset();
	freshPump();
	ServoProfile prof;
	servoProfileDefault(&prof);
	servoJobProfile(&prof);
	escHeartbeat();
	checkTrue("running", claimRunning(PIN_SIG));

	// Stamp the heartbeat 100 ms before the clock wraps, so stamp + 250 ms
	// wraps. The advances below add up to 2^32 ms with the move back.
	uint32_t start = millis();
	uint32_t toEdge = (0u - 100u) - start;
	fakeAdvance(toEdge);
	escHeartbeat();
	uint32_t stamp = millis();
	send(cmdOf(0, ServoCmdKind::Start));
	checkInt("START", fakePin(SIG).level, 1500);
	checkTrue("stamp + 250 ms wraps", stamp + 250u < stamp);
	fakeAdvance(249);
	core1JobPoll();
	checkTrue("the clock wrapped", millis() < stamp);
	ServoStatus st;
	servoJobStatus(&st);
	checkTrue("249 ms after the heartbeat: running", st.running && fakePin(SIG).level == 1500);
	fakeAdvance(1);
	core1JobPoll();
	servoJobStatus(&st);
	checkTrue("250 ms: tripped, level 0", !st.running && st.tripped && fakePin(SIG).level == 0);
	escHeartbeat();
	core1JobPoll();
	servoJobStatus(&st);
	checkTrue("the heartbeat back: still tripped, no pulse",
	          st.tripped && !st.running && fakePin(SIG).level == 0);
	send(cmdOf(0, ServoCmdKind::Start));
	servoJobStatus(&st);
	checkTrue("a new START clears it", st.running && !st.tripped && fakePin(SIG).level == 1500);
	send(cmdOf(0, ServoCmdKind::Safe));
	checkTrue("back to free", claimEnd());
	fakeAdvance(start - millis());
	checkInt("the clock is back where it was", millis(), start);
	escHeartbeat();
}

/** @brief The enable pin as the converter's pull-down alone sees it: SIO, not driven, no pulls, input off. */
static bool enUndriven(uint8_t gpio) {
	FakePin p = fakePin(gpio);
	return p.function == GPIO_FUNC_SIO && !p.output && p.pull == FakePull::None && p.inputOff;
}

/** @brief The enable pin driven low: the converter off. */
static bool enLow(uint8_t gpio) { return fakePin(gpio).output && !fakePin(gpio).high; }

/** @brief The enable pin driven high: the converter on. */
static bool enHigh(uint8_t gpio) { return fakePin(gpio).output && fakePin(gpio).high; }

/** @brief Both stamps core1 trusts, the heartbeat and the touch controller's, at the clock now. */
static void stamps() {
	escHeartbeat();
	fakeTouchStamp();
}

/** @brief @p ms on, the stamps renewed, and core1's pass. */
static void passAfter(uint32_t ms) {
	fakeAdvance(ms);
	stamps();
	core1JobPoll();
}

/** @brief SERVO SUPPLY at @p mode, as SETTINGS sets it outside every claim. */
static void supplyMode(uint8_t mode) {
	settings()->servoSupply = mode;
	servoSupplyConfigure(mode);
}

/**
 * @brief The servo job on the switched supply, through the job table: the
 *        enable pin at every path in and out.
 */
static void testJobSupply() {
	section("Servo job: the switched supply on GP28, every path off");

	const uint8_t EN = 28;
	fakePwmReset();
	freshPump();
	supplyMode(1);
	checkTrue("outside the claim: GP28 undriven", enUndriven(EN));
	ServoProfile prof;
	servoProfileDefault(&prof);
	servoJobProfile(&prof);
	servoJobNewSession();
	stamps();
	checkTrue("the claim runs the shipped job", claimRunning(PIN_SIG));
	checkTrue("bind: GP28 driven low", enLow(EN));
	checkInt("never made an output while high", fakePin(EN).highEnables, 0);
	ServoStatus st;
	servoJobStatus(&st);
	checkTrue("status: switched, Off, no block", st.switched && st.power == ServoPowerPhase::Off &&
	          st.startBlock == ServoStartBlock::None && st.upMs == SUPPLY_UP_NONE);
	passAfter(100);
	checkTrue("SAFE: still low", enLow(EN) && fakePin(SIG).level == 0);

	// START: on at once, the pulses 500 ms later without a sensor.
	send(cmdOf(0, ServoCmdKind::Start));
	servoJobStatus(&st);
	checkTrue("START: GP28 high, no pulse yet", enHigh(EN) && fakePin(SIG).level == 0 &&
	          st.running && st.power == ServoPowerPhase::Starting);
	passAfter(499);
	checkTrue("499 ms: no pulse", fakePin(SIG).level == 0);
	passAfter(1);
	servoJobStatus(&st);
	checkTrue("500 ms: CENTRE on SIG, On", fakePin(SIG).level == 1500 &&
	          st.power == ServoPowerPhase::On);
	send(cmdOf(0, ServoCmdKind::Safe));
	checkTrue("STOP: GP28 low and level 0 in the same pass", enLow(EN) && fakePin(SIG).level == 0);
	checkTrue("and GP28 low first", fakePin(EN).putAt < fakePin(SIG).levelAt);

	// STOP while the supply comes up.
	send(cmdOf(0, ServoCmdKind::Start));
	passAfter(100);
	send(cmdOf(0, ServoCmdKind::Safe));
	checkTrue("STOP while Starting: GP28 low", enLow(EN) && fakePin(SIG).level == 0);

	// The heartbeat trip.
	send(cmdOf(0, ServoCmdKind::Start));
	passAfter(500);
	checkTrue("running", enHigh(EN) && fakePin(SIG).level == 1500);
	fakeAdvance(249);
	fakeTouchStamp();
	core1JobPoll();
	checkTrue("heartbeat 249 ms old: on", enHigh(EN));
	fakeAdvance(1);
	fakeTouchStamp();
	core1JobPoll();
	servoJobStatus(&st);
	checkTrue("250 ms: tripped, GP28 low", enLow(EN) && st.tripped && !st.running &&
	          fakePin(SIG).level == 0 && st.powerFault == ServoPowerFault::None);
	passAfter(25);
	checkTrue("the heartbeat back: stays off", enLow(EN));

	// The touch controller silent for 250 ms, the heartbeat alive.
	send(cmdOf(0, ServoCmdKind::Start));
	passAfter(500);
	checkTrue("running again, the trip cleared", enHigh(EN));
	servoJobStatus(&st);
	checkTrue("status: not tripped", !st.tripped && st.running);
	uint32_t touched = millis();
	fakeAdvance(249);
	escHeartbeat();
	core1JobPoll();
	checkTrue("touch 249 ms old: on", enHigh(EN));
	fakeAdvance(1);
	escHeartbeat();
	core1JobPoll();
	servoJobStatus(&st);
	checkTrue("250 ms: GP28 low, tripped, TouchSilent", enLow(EN) && st.tripped &&
	          st.powerFault == ServoPowerFault::TouchSilent && fakePin(SIG).level == 0);
	checkTrue("and START refused while it is silent",
	          st.startBlock == ServoStartBlock::TouchSilent && millis() - touched == 250);
	send(cmdOf(0, ServoCmdKind::Start));
	checkTrue("a START now: GP28 stays low", enLow(EN));
	fakeTouchStamp();
	core1JobPoll();
	servoJobStatus(&st);
	checkTrue("touch back: no block, the fault kept until START",
	          st.startBlock == ServoStartBlock::None && st.powerFault == ServoPowerFault::TouchSilent);

	// BACK with the supply on: off first, the enable pin released after the
	// last pulse, before the pins return to DShot.
	send(cmdOf(0, ServoCmdKind::Start));
	passAfter(500);
	checkTrue("on before BACK", enHigh(EN) && fakePin(SIG).level == 1500);
	pinClaimEnd(PinUser::Servo);
	core1JobPoll();
	checkTrue("BACK, first pass: GP28 low, still driven; SIG at level 0, still PWM",
	          enLow(EN) && fakePin(SIG).function == GPIO_FUNC_PWM && fakePin(SIG).level == 0);
	fakeAdvance(servoQuietMs(SERVO_RATE_MIN_HZ));
	stamps();
	core1JobPoll();
	checkTrue("after the quiet wait: GP28 undriven, SIG released", enUndriven(EN) &&
	          fakePin(SIG).function == GPIO_FUNC_SIO);
	checkTrue("GP28 an input before SIG leaves PWM", fakePin(EN).dirAt < fakePin(SIG).functionAt);
	servoJobStatus(&st);
	checkTrue("status after the release: not switched, Off", !st.switched &&
	          st.power == ServoPowerPhase::Off);
	pinClaimPoll();
	pinClaimRestart(PinUser::Servo);
	fakePumpPoll();
	checkTrue("DShot back, GP28 still undriven", pinClaimState() == ClaimState::Free &&
	          enUndriven(EN));

	// BACK while the supply comes up.
	stamps();
	checkTrue("the claim again", claimRunning(PIN_SIG) && enLow(EN));
	send(cmdOf(0, ServoCmdKind::Start));
	passAfter(100);
	servoJobStatus(&st);
	checkTrue("COMING UP: GP28 high, no pulse", enHigh(EN) && st.power == ServoPowerPhase::Starting &&
	          fakePin(SIG).level == 0);
	pinClaimEnd(PinUser::Servo);
	core1JobPoll();
	checkTrue("BACK while coming up, first pass: GP28 off and SIG never pulsed: no pulse to "
	          "wait for, both let go at once", enUndriven(EN) && fakePin(SIG).level == 0 &&
	          fakePin(SIG).function == GPIO_FUNC_SIO);
	checkTrue("GP28 low before it became an input", fakePin(EN).putAt < fakePin(EN).dirAt &&
	          !fakePin(EN).high);
	checkTrue("released", claimEnd() && enUndriven(EN));

	// A refused bind leaves the enable pin undriven.
	fakePwmBreakSliceMap(true);
	checkTrue("the claim begins", pinClaimBegin(PinUser::Servo, PIN_SIG) == ClaimRefusal::None);
	fakePumpPoll();
	pinClaimPoll();
	core1JobPoll();
	checkTrue("the bind refused: GP28 not driven", pinClaimPoll() == ClaimState::Held &&
	          enUndriven(EN));
	fakePwmBreakSliceMap(false);
	pinClaimRetry(PinUser::Servo);
	core1JobPoll();
	checkTrue("RETRY binds: GP28 low", pinClaimPoll() == ClaimState::Running && enLow(EN));
	checkTrue("released at the end", claimEnd() && enUndriven(EN));

	// A rebind for a new profile while SAFE keeps GP28 low and driven.
	stamps();
	checkTrue("running again", claimRunning(PIN_SIG) && enLow(EN));
	ServoProfile fast = prof;
	fast.rateHz = 100;
	checkTrue("a new profile while SAFE", servoJobProfile(&fast));
	passAfter(servoQuietMs(100));
	checkTrue("the rebind: GP28 low", enLow(EN) && fakePin(SIG).function == GPIO_FUNC_PWM);
	checkTrue("released", claimEnd() && enUndriven(EN));
	servoJobProfile(&prof);

	// OWN: GP28 and GP29 untouched through START, STOP and the release.
	supplyMode(0);
	fakePwmReset();
	freshPump();
	stamps();
	checkTrue("OWN: the claim runs", claimRunning(PIN_SIG));
	send(cmdOf(0, ServoCmdKind::Start));
	checkTrue("OWN: CENTRE at once", fakePin(SIG).level == 1500);
	fakeAdvance(300);
	escHeartbeat();
	core1JobPoll();
	servoJobStatus(&st);
	checkTrue("OWN: touch silent 300 ms changes nothing", st.running && !st.tripped &&
	          fakePin(SIG).level == 1500 && !st.switched && st.startBlock == ServoStartBlock::None);
	send(cmdOf(0, ServoCmdKind::Safe));
	checkTrue("released", claimEnd());
	FakePin a = fakePin(28), b = fakePin(29);
	checkTrue("OWN: GP28 and GP29 never touched", a.function == 0 && a.puts == 0 && !a.output &&
	          a.pull == FakePull::Untouched && b.function == 0 && b.puts == 0 && !b.output);
	stamps();
}

/** @brief The fake INA219's output follows the enable pin: 5.0 V with it high, 0 V low. */
static void followEn(FakeIna219 *ina, uint8_t en, double onV = 5.0, double offV = 0.0) {
	ina->voutV = enHigh(en) ? onV : offV;
}

/** @brief One frame of core0's sensor read and core1's pass, @p ms after the last. */
static void sensePass(FakeIna219 *ina, uint8_t en, uint32_t ms, double onV = 5.0,
                      double offV = 0.0) {
	fakeAdvance(ms);
	stamps();
	followEn(ina, en, onV, offV);
	senseTick(millis());
	core1JobPoll();
}

/**
 * @brief The servo job on the switched supply with an INA219: the read-back
 *        of on and off, the start-up time, START refused, the stuck bus.
 */
static void testJobSupplySensed() {
	section("Servo job: the switched supply on GP29 read back by the INA219");

	const uint8_t EN = 29;
	fakePwmReset();
	freshPump();
	supplyMode(2);
	fakeI2cReset();
	settings()->currentSense = 1;
	senseConfigure(1);
	ServoProfile prof;
	servoProfileDefault(&prof);
	servoJobProfile(&prof);
	servoJobNewSession();
	stamps();
	senseTick(millis());
	ServoStatus st;
	checkTrue("the claim runs", claimRunning(PIN_SIG) && enLow(EN));
	core1JobPoll();
	servoJobStatus(&st);
	checkTrue("no sensor on the bus: START refused, NoSensor",
	          st.startBlock == ServoStartBlock::NoSensor);
	send(cmdOf(0, ServoCmdKind::Start));
	servoJobStatus(&st);
	checkTrue("a START: GP29 stays low", enLow(EN) && !st.running);

	FakeIna219 *ina = fakeI2cAddIna219(INA219_ADDR);
	sensePass(ina, EN, 1000);   // probed again after 1000 ms
	sensePass(ina, EN, 25);
	servoJobStatus(&st);
	checkTrue("found: START allowed", st.startBlock == ServoStartBlock::None);

	send(cmdOf(0, ServoCmdKind::Start));
	checkTrue("START: GP29 high, no pulse", enHigh(EN) && fakePin(SIG).level == 0);
	sensePass(ina, EN, 25);
	servoJobStatus(&st);
	checkTrue("the first reading after it at 5.0 V: On, CENTRE",
	          st.power == ServoPowerPhase::On && fakePin(SIG).level == 1500);
	checkInt("the start-up time, from the switch-on to that reading", st.upMs, 25);
	ina->currentA = 0.2;
	sensePass(ina, EN, 25);
	send(cmdOf(0, ServoCmdKind::Safe));
	servoJobStatus(&st);
	checkTrue("STOP: GP29 low, Waiting", enLow(EN) && st.offCheck == ServoOffCheck::Waiting);
	sensePass(ina, EN, 25);
	servoJobStatus(&st);
	checkTrue("0 V read back: Off", st.offCheck == ServoOffCheck::Off);
	checkInt("the start-up time kept while SAFE", st.upMs, 25);

	// A converter that stays at 3.6 V: did not come on.
	ina->currentA = 0;
	send(cmdOf(0, ServoCmdKind::Start));
	for (int i = 0; i < 19; i++) sensePass(ina, EN, 25, 3.6);
	servoJobStatus(&st);
	checkTrue("475 ms at 3.6 V: still Starting, no pulse",
	          st.power == ServoPowerPhase::Starting && enHigh(EN) && fakePin(SIG).level == 0);
	sensePass(ina, EN, 25, 3.6);
	servoJobStatus(&st);
	checkTrue("500 ms: GP29 low, SAFE, NoStart", enLow(EN) && !st.running &&
	          st.powerFault == ServoPowerFault::NoStart && fakePin(SIG).level == 0);

	// Not off: the output stays up with EN low.
	send(cmdOf(0, ServoCmdKind::Start));
	sensePass(ina, EN, 25, 5.0, 4.9);
	send(cmdOf(0, ServoCmdKind::Safe));
	servoJobStatus(&st);
	checkTrue("START cleared the fault; STOP: GP29 low", enLow(EN) &&
	          st.powerFault == ServoPowerFault::None);
	for (int i = 0; i < 79; i++) sensePass(ina, EN, 25, 5.0, 4.9);
	servoJobStatus(&st);
	checkTrue("1975 ms at 4.9 V: Waiting", st.offCheck == ServoOffCheck::Waiting);
	sensePass(ina, EN, 25, 5.0, 4.9);
	servoJobStatus(&st);
	checkTrue("2000 ms at 4.9 V: NotOff, GP29 still low", st.offCheck == ServoOffCheck::NotOff &&
	          enLow(EN));
	sensePass(ina, EN, 25);
	servoJobStatus(&st);
	checkTrue("the output falls: Off", st.offCheck == ServoOffCheck::Off);

	// The bus stuck while the supply is on.
	send(cmdOf(0, ServoCmdKind::Start));
	sensePass(ina, EN, 25);
	checkTrue("on", enHigh(EN) && fakePin(SIG).level == 1500);
	fakeI2cHoldSda(true);
	sensePass(ina, EN, 25);
	servoJobStatus(&st);
	checkTrue("SDA held: the bus stuck, GP29 low, tripped, BusStuck", enLow(EN) && st.tripped &&
	          st.powerFault == ServoPowerFault::BusStuck && fakePin(SIG).level == 0);
	checkTrue("START refused while stuck", st.startBlock == ServoStartBlock::BusStuck);
	fakeI2cHoldSda(false);
	for (int i = 0; i < 80 && st.startBlock != ServoStartBlock::None; i++) {
		sensePass(ina, EN, 25);
		servoJobStatus(&st);
	}
	checkTrue("freed: START allowed again, the fault kept until START",
	          st.startBlock == ServoStartBlock::None && st.powerFault == ServoPowerFault::BusStuck);

	checkTrue("released", claimEnd() && enUndriven(EN));
	servoJobStatus(&st);
	checkTrue("the fault and the trip stay past the release",
	          st.powerFault == ServoPowerFault::BusStuck && st.tripped);
	servoJobNewSession();
	servoJobStatus(&st);
	checkTrue("a new screen session clears both at once, before a bind",
	          st.powerFault == ServoPowerFault::None && !st.tripped && st.upMs == SUPPLY_UP_NONE);
	settings()->currentSense = 0;
	senseConfigure(0);
	fakeI2cReset();
	supplyMode(0);
	stamps();
}

/**
 * @brief Senses until the job leaves RUNNING, at most @p n frames.
 * @return The frames it took, or -1.
 */
static int senseUntilCut(FakeIna219 *ina, uint8_t en, int n, double onV = 5.0) {
	ServoStatus st;
	for (int i = 1; i <= n; i++) {
		sensePass(ina, en, 25, onV);
		servoJobStatus(&st);
		if (!st.running) return i;
	}
	return -1;
}

/**
 * @brief The cuts through the job table with an INA219 on the fake bus:
 *        the enable pin low and level 0 in the pass that reads the cut,
 *        and in that order; a transient fault of the bus no cut.
 */
static void testJobCuts() {
	section("Servo job: the cuts with the INA219, on GP29 and with OWN");

	const uint8_t EN = 29;
	fakePwmReset();
	freshPump();
	supplyMode(2);
	fakeI2cReset();
	settings()->currentSense = 1;
	senseConfigure(1);
	FakeIna219 *ina = fakeI2cAddIna219(INA219_ADDR);
	ServoProfile prof;
	servoProfileDefault(&prof);
	servoJobProfile(&prof);
	servoJobNewSession();
	stamps();
	senseTick(millis());
	ServoStatus st;
	checkTrue("the claim runs", claimRunning(PIN_SIG) && enLow(EN));
	sensePass(ina, EN, 25);

	// The hard cut.
	send(cmdOf(0, ServoCmdKind::Start));
	sensePass(ina, EN, 25);
	checkTrue("on: CENTRE", enHigh(EN) && fakePin(SIG).level == 1500);
	ina->currentA = 2.99;
	sensePass(ina, EN, 25);
	sensePass(ina, EN, 25);
	checkTrue("2.99 A: on", enHigh(EN) && fakePin(SIG).level == 1500);
	ina->currentA = 3.0;
	fakeAdvance(25);
	stamps();
	senseTick(millis());
	servoJobStatus(&st);
	checkTrue("3.0 A read: still on before core1's pass", st.running && enHigh(EN));
	core1JobPoll();
	servoJobStatus(&st);
	checkTrue("core1's next pass: GP29 low and SIG at level 0",
	          enLow(EN) && fakePin(SIG).level == 0 && !st.running);
	checkTrue("the enable pin first, then level 0", fakePin(EN).putAt < fakePin(SIG).levelAt);
	checkTrue("tripped, Overcurrent", st.tripped && st.powerFault == ServoPowerFault::Overcurrent);
	sensePass(ina, EN, 25);
	checkTrue("it stays off", enLow(EN) && fakePin(SIG).level == 0);
	ina->currentA = 3.6;
	send(cmdOf(0, ServoCmdKind::Start));
	sensePass(ina, EN, 25);
	servoJobStatus(&st);
	checkTrue("3.6 A, beyond the range: clipped, cut", enLow(EN) && !st.running &&
	          st.powerFault == ServoPowerFault::Overcurrent);
	ina->currentA = -3.6;
	send(cmdOf(0, ServoCmdKind::Start));
	sensePass(ina, EN, 25);
	servoJobStatus(&st);
	checkTrue("-3.6 A, a reversed sensor: cut", enLow(EN) && !st.running);

	// The stall: 1.2 A from the first reading after the start-up one.
	ina->currentA = 0;
	send(cmdOf(0, ServoCmdKind::Start));
	sensePass(ina, EN, 25);
	checkTrue("on again", enHigh(EN) && fakePin(SIG).level == 1500);
	ina->currentA = 1.2;
	checkInt("1.2 A: cut 400 ms after the first reading above, at its 17th",
	         senseUntilCut(ina, EN, 40), 17);
	servoJobStatus(&st);
	checkTrue("Stall, GP29 low, level 0", st.powerFault == ServoPowerFault::Stall && enLow(EN) &&
	          fakePin(SIG).level == 0);
	ina->currentA = 1.0;
	send(cmdOf(0, ServoCmdKind::Start));
	sensePass(ina, EN, 25);
	checkInt("1.0 A: no stall in 3 s", senseUntilCut(ina, EN, 120), -1);
	send(cmdOf(0, ServoCmdKind::Safe));
	sensePass(ina, EN, 25);

	// The collapse: the converter at 3.9 V after start-up.
	ina->currentA = 0.5;
	send(cmdOf(0, ServoCmdKind::Start));
	sensePass(ina, EN, 25);
	checkTrue("up", enHigh(EN));
	checkInt("3.9 V: cut 1000 ms after the first reading under 4.0 V, at its 41st",
	         senseUntilCut(ina, EN, 80, 3.9), 41);
	servoJobStatus(&st);
	checkTrue("LowSupply, GP29 low", st.powerFault == ServoPowerFault::LowSupply && enLow(EN));
	send(cmdOf(0, ServoCmdKind::Start));
	sensePass(ina, EN, 25);
	checkInt("4.05 V at 0.5 A, 4.0 V at the sensor: no cut in 3 s",
	         senseUntilCut(ina, EN, 120, 4.05), -1);
	send(cmdOf(0, ServoCmdKind::Safe));
	sensePass(ina, EN, 25);

	// A transient fault of the bus: a failed read, a reading, then two
	// failed reads in a row, one short of taking the part offline.
	ina->currentA = 0.2;
	send(cmdOf(0, ServoCmdKind::Start));
	sensePass(ina, EN, 25);
	fakeI2cFailNext(1, PICO_ERROR_GENERIC);
	sensePass(ina, EN, 25);
	sensePass(ina, EN, 25);
	fakeI2cFailNext(2, PICO_ERROR_GENERIC);
	sensePass(ina, EN, 25);
	sensePass(ina, EN, 25);
	checkTrue("the part still online", sensePart()->state == SENSE_PART_ONLINE);
	checkInt("three frames without a reading: no cut over 1 s", senseUntilCut(ina, EN, 40), -1);
	servoJobStatus(&st);
	checkTrue("still on, no fault", enHigh(EN) && st.running && st.powerFault == ServoPowerFault::None);

	// Core0 reads nothing for 250 ms while the heartbeat and touch answer.
	for (int i = 0; i < 9; i++) passAfter(25);
	passAfter(24);
	servoJobStatus(&st);
	checkTrue("no reading for 249 ms: on", st.running && enHigh(EN));
	passAfter(1);
	servoJobStatus(&st);
	checkTrue("250 ms: GP29 low, level 0, NoSensor", !st.running && enLow(EN) &&
	          fakePin(SIG).level == 0 && st.tripped && st.powerFault == ServoPowerFault::NoSensor);

	// The part gone: three failed reads take it offline.
	send(cmdOf(0, ServoCmdKind::Start));
	sensePass(ina, EN, 25);
	checkTrue("on", enHigh(EN));
	fakeI2cRemove(INA219_ADDR);
	int frames = senseUntilCut(ina, EN, 20);
	servoJobStatus(&st);
	checkTrue("unplugged: off within three frames, NoSensor",
	          frames >= 1 && frames <= 3 && enLow(EN) && st.powerFault == ServoPowerFault::NoSensor);
	checkTrue("START refused until it is back", st.startBlock == ServoStartBlock::NoSensor);
	checkTrue("released", claimEnd() && enUndriven(EN));

	// OWN: the hard cut ends the pulses; GP28 and GP29 untouched.
	fakePwmReset();
	freshPump();
	supplyMode(0);
	fakeI2cReset();
	ina = fakeI2cAddIna219(INA219_ADDR);
	ina->voutV = 7.4;
	senseConfigure(0);
	senseConfigure(1);
	for (int i = 0; i < 41; i++) { fakeAdvance(25); stamps(); senseTick(millis()); }
	servoJobProfile(&prof);
	servoJobNewSession();
	checkTrue("OWN: the claim runs", claimRunning(PIN_SIG));
	send(cmdOf(0, ServoCmdKind::Start));
	checkTrue("OWN: CENTRE at START", fakePin(SIG).level == 1500);
	ina->currentA = 3.1;
	fakeAdvance(25); stamps(); senseTick(millis()); core1JobPoll();
	servoJobStatus(&st);
	checkTrue("OWN, 3.1 A: level 0 in the pass that reads it, Overcurrent",
	          !st.running && fakePin(SIG).level == 0 && st.tripped &&
	          st.powerFault == ServoPowerFault::Overcurrent && !st.switched);
	checkTrue("OWN: GP28 and GP29 untouched", fakePin(28).puts == 0 && fakePin(29).puts == 0 &&
	          !fakePin(28).output && !fakePin(29).output);
	checkTrue("released", claimEnd());
	settings()->currentSense = 0;
	senseConfigure(0);
	fakeI2cReset();
	supplyMode(0);
	servoJobNewSession();
	stamps();
}

/**
 * @brief rcbench's single-servo model behind the timed fake INA219, driven
 *        by SIG's pulse as core1 changed it. The model's horn starts at the
 *        core1 pass that changes the pulse: the wait for the next PWM frame
 *        is not modelled.
 */
struct SimServo {
	servo_sim_t sim;
	uint16_t    us[4];  /**< SIG's last pulses, newest last; 0 for none. */
	uint64_t    at[4];  /**< time_us_64() of each change. */
	int         n;      /**< How many. */
};

/**
 * @brief The model free of its stops between 800 and 2200 us: 0.12 A at
 *        rest, 0.95 A while it travels at 1.2 us a ms, 20 mA of noise from
 *        end to end.
 */
static void freeServo(SimServo *s) {
	servo_sim_cfg_t c;
	servo_sim_defaults(&c);
	c.stop_lo_us = 800;
	c.stop_hi_us = 2200;
	servo_sim_init(&s->sim, &c);
	s->n = 0;
}

/** @brief Note SIG's pulse now, after a core1 pass. */
static void simTrack(SimServo *s) {
	uint16_t lv = fakePin(SIG).level;
	if (s->n > 0 && s->us[s->n - 1] == lv) return;
	if (s->n == 4) {
		memmove(&s->us[0], &s->us[1], 3 * sizeof(s->us[0]));
		memmove(&s->at[0], &s->at[1], 3 * sizeof(s->at[0]));
		s->n = 3;
	}
	s->us[s->n] = lv;
	s->at[s->n] = time_us_64();
	s->n++;
}

/**
 * @brief The fake INA219's current function: the model at @p t, stepped
 *        with SIG's pulse at that time; 0 A while SIG carries none. The
 *        fake asks for times that only grow.
 */
static double simCurrentAt(uint64_t t, void *ctx) {
	SimServo *s = (SimServo *)ctx;
	uint16_t lv = 0;
	for (int i = 0; i < s->n; i++)
		if (s->at[i] <= t) lv = s->us[i];
	float a = servo_sim_step(&s->sim, lv ? lv : 1500u, (uint32_t)(t / 1000u));
	return lv ? a : 0.0;
}

/**
 * @brief The fake INA219 on its conversion clock, each reading the mean of
 *        @p s's current over the shunt conversion, 17.02 ms of every
 *        17.55 ms.
 */
static void simAttach(FakeIna219 *ina, SimServo *s) {
	ina->timed = true;
	ina->currentAt = simCurrentAt;
	ina->currentCtx = s;
	simTrack(s);
}

/** @brief One frame: core0's read of the model's current, core1's pass, the pulse noted. */
static void simPass(FakeIna219 *ina, SimServo *s, uint8_t en, uint32_t ms) {
	fakeAdvance(ms);
	stamps();
	followEn(ina, en);
	senseTick(millis());
	core1JobPoll();
	simTrack(s);
}

/** @brief Send @p c, core1's pass, the pulse noted. */
static void simSend(SimServo *s, const ServoCmd &c) {
	send(c);
	simTrack(s);
}

/** @brief A step of servo A to @p command, as MIN, CENTRE and MAX send it, at SPEED @p slew. */
static ServoCmd stepOf(uint16_t command, uint16_t slew = 0) {
	ServoCmd c = cmdOf(0, ServoCmdKind::Position, SERVO_MASK_A, command, slew);
	c.step = true;
	return c;
}

/**
 * @brief The move timing through the job table, on the fake INA219 with
 *        rcbench's servo model behind it, each reading the mean over its
 *        shunt conversion: a move timed 16 ms to @ref MOVE_LAG_MS after the
 *        model's arrival, its hold, an unseen step, and the pulses and the
 *        cuts as they are without it.
 */
static void testJobMoveTiming() {
	section("Servo job: move timing from the INA219, with rcbench's servo model");

	const uint8_t EN = 29;
	fakePwmReset();
	freshPump();
	supplyMode(2);
	fakeI2cReset();
	settings()->currentSense = 1;
	senseConfigure(1);
	FakeIna219 *ina = fakeI2cAddIna219(INA219_ADDR);
	ServoProfile prof;
	servoProfileDefault(&prof);
	servoJobProfile(&prof);
	servoJobNewSession();
	stamps();
	senseTick(millis());
	SimServo sim;
	freeServo(&sim);
	ServoStatus st;
	checkTrue("the claim runs", claimRunning(PIN_SIG));
	simAttach(ina, &sim);
	simPass(ina, &sim, EN, 25);
	simSend(&sim, cmdOf(0, ServoCmdKind::Start));
	simPass(ina, &sim, EN, 25);
	servoJobStatus(&st);
	checkTrue("on at CENTRE, nothing timed", fakePin(SIG).level == 1500 &&
	          st.move.state == MoveState::None);

	// MAX 475 ms after the pulses started: not at rest.
	for (int i = 0; i < 19; i++) simPass(ina, &sim, EN, 25);
	simSend(&sim, stepOf(SERVO_SPAN));
	servoJobStatus(&st);
	checkTrue("MAX 475 ms after CENTRE: sent, not timed", fakePin(SIG).level == 2000 &&
	          st.move.state == MoveState::Void && st.move.why == MoveVoid::NotAtRest);

	// CENTRE 3600 ms after MAX: 500 us at 1.2 us a ms, 417 ms.
	for (int i = 0; i < 144; i++) simPass(ina, &sim, EN, 25);
	simSend(&sim, stepOf(SERVO_CMD_CENTRE));
	servoJobStatus(&st);
	checkTrue("CENTRE: sent at once, timed", fakePin(SIG).level == 1500 &&
	          st.move.state == MoveState::Measuring);
	int frames = 0;
	for (; frames < 40 && st.move.state == MoveState::Measuring; frames++) {
		simPass(ina, &sim, EN, 25);
		servoJobStatus(&st);
	}
	// 500 us / 1.2 us a ms, rounded up. The arrival reads near the holding
	// level only in a conversion of at most 6 % moving current, 1.02 ms of
	// 17.02 ms: one that ends 16 ms or more after it. At most it ends 17.55
	// and 17.02 ms after, and is read up to 25 ms later: MOVE_LAG_MS.
	const uint32_t TRUE_MS = 417, SEEN_MS = TRUE_MS + 16;
	checkTrue("arrived", st.move.state == MoveState::Arrived && !st.move.settled);
	checkTrue("timed 16 ms to MOVE_LAG_MS after the model's 417 ms",
	          st.move.ms >= SEEN_MS && st.move.ms <= TRUE_MS + MOVE_LAG_MS);
	checkTrue("the peak: the model's 0.95 A, within its noise",
	          st.move.peakOk && st.move.peakMa >= 940 && st.move.peakMa <= 961);
	checkTrue("no hold yet", !st.move.holdOk);
	for (int i = 0; i < 20; i++) simPass(ina, &sim, EN, 25);
	servoJobStatus(&st);
	checkTrue("the hold, 500 ms on: the model's 0.12 A, within its noise",
	          st.move.holdOk && st.move.holdMa >= 110 && st.move.holdMa <= 130);
	checkTrue("the pulse untouched", fakePin(SIG).level == 1500 && st.running);

	// MIN: 500 us again, judged against CENTRE's level.
	simSend(&sim, stepOf(0));
	for (frames = 0; frames < 60; frames++) {
		simPass(ina, &sim, EN, 25);
		servoJobStatus(&st);
		if (st.move.state != MoveState::Measuring) break;
	}
	checkTrue("MIN: arrived 16 ms to MOVE_LAG_MS after 417 ms",
	          st.move.state == MoveState::Arrived && st.move.ms >= SEEN_MS &&
	          st.move.ms <= TRUE_MS + MOVE_LAG_MS);

	// A step at SPEED below 100 %: the pulse slews, nothing is timed.
	for (int i = 0; i < 25; i++) simPass(ina, &sim, EN, 25);
	ServoStatus before;
	servoJobStatus(&before);
	simSend(&sim, stepOf(SERVO_SPAN, 400));
	simPass(ina, &sim, EN, 25);
	servoJobStatus(&st);
	checkTrue("SPEED below 100 %: the pulse slews, not timed",
	          fakePin(SIG).level > 1000 && fakePin(SIG).level < 2000 &&
	          st.move.state == MoveState::Void && st.move.why == MoveVoid::Slewed);

	// No current change: unseen at the window's end. The slew takes 2.5 s,
	// then the window, then 500 ms of rest.
	ina->currentAt = nullptr;
	for (int i = 0; i < 260; i++) {
		fakeAdvance(25);
		stamps();
		followEn(ina, EN);
		ina->currentA = 0.05;
		senseTick(millis());
		core1JobPoll();
	}
	simSend(&sim, stepOf(SERVO_CMD_CENTRE));
	servoJobStatus(&st);
	checkTrue("CENTRE with the servo unplugged: timed", st.move.state == MoveState::Measuring);
	for (frames = 0; frames < 140; frames++) {
		fakeAdvance(25);
		stamps();
		followEn(ina, EN);
		ina->currentA = 0.05;
		senseTick(millis());
		core1JobPoll();
		servoJobStatus(&st);
		if (st.move.state != MoveState::Measuring) break;
	}
	checkTrue("no current change: unseen", st.move.state == MoveState::Unseen && !st.move.peakOk);
	checkInt("at the first reading 3061 ms or more after the command", frames + 1, 123);

	// The stall cut during a timed move, at its 17th reading as without one.
	ina->currentA = 0.12;
	for (int i = 0; i < 20; i++) simPass(ina, &sim, EN, 25);
	simSend(&sim, stepOf(SERVO_SPAN));
	servoJobStatus(&st);
	checkTrue("MAX: timed", st.move.state == MoveState::Measuring);
	ina->currentA = 1.2;
	checkInt("1.2 A: the stall cut at its 17th reading", senseUntilCut(ina, EN, 40), 17);
	servoJobStatus(&st);
	checkTrue("Stall, GP29 low, level 0; the move void", st.powerFault == ServoPowerFault::Stall &&
	          enLow(EN) && fakePin(SIG).level == 0 && st.move.state == MoveState::Void &&
	          st.move.why == MoveVoid::Ended);
	simAttach(ina, &sim);
	simSend(&sim, cmdOf(0, ServoCmdKind::Start));
	simPass(ina, &sim, EN, 25);
	servoJobStatus(&st);
	checkTrue("START: the pulses again, the result gone", st.running &&
	          fakePin(SIG).level == 1500 && st.move.state == MoveState::None);

	// The release ends a move under way.
	for (int i = 0; i < 144; i++) simPass(ina, &sim, EN, 25);
	simSend(&sim, stepOf(0));
	servoJobStatus(&st);
	checkTrue("MIN 3600 ms after START: timed", st.move.state == MoveState::Measuring);
	checkTrue("released", claimEnd() && enUndriven(EN));
	servoJobStatus(&st);
	checkTrue("the move under way at BACK: void", st.move.state == MoveState::Void &&
	          st.move.why == MoveVoid::Ended);
	servoJobNewSession();
	servoJobStatus(&st);
	checkTrue("a new screen session drops it", st.move.state == MoveState::None);

	// Two servos, in a new session: a step of AUX alone is a step.
	ServoProfile two = prof;
	two.servos = 2;
	checkTrue("two servos accepted", servoJobProfile(&two));
	checkTrue("the claim again, SIG and AUX", claimRunning(PIN_SIG | PIN_AUX));
	servoJobStatus(&st);
	checkTrue("the new session's bind: nothing timed", st.move.state == MoveState::None);
	simSend(&sim, cmdOf(0, ServoCmdKind::Start));
	for (int i = 0; i < 144; i++) simPass(ina, &sim, EN, 25);
	ServoCmd b = stepOf(SERVO_SPAN);
	b.mask = SERVO_MASK_B;
	simSend(&sim, b);
	servoJobStatus(&st);
	checkTrue("MAX on AUX alone: timed", fakePin(AUX).level == 2000 && fakePin(SIG).level == 1500 &&
	          st.move.state == MoveState::Measuring);

	// A new profile: the old one's figure goes with the rebind.
	simSend(&sim, cmdOf(0, ServoCmdKind::Safe));
	servoJobStatus(&st);
	checkTrue("STOP: the move void, kept while SAFE", st.move.state == MoveState::Void);
	checkTrue("one servo again, while SAFE", servoJobProfile(&prof));
	for (int i = 0; i < 4; i++) simPass(ina, &sim, EN, 25);
	servoJobStatus(&st);
	checkTrue("the rebind drops it", st.move.state == MoveState::None && st.bindError == 0);
	checkTrue("released", claimEnd() && enUndriven(EN));
	servoJobProfile(&prof);
	servoJobNewSession();
	settings()->currentSense = 0;
	senseConfigure(0);
	fakeI2cReset();
	supplyMode(0);
	stamps();
}

/** @brief The wait for the last pulse, as pure functions. */
static void testQuietRule() {
	section("Servo model: the last pulse before the pins let go");

	checkInt("50 Hz: two 20 ms frames and 1 ms", servoQuietMs(50), 41);
	checkInt("333 Hz: two 4 ms frames and 1 ms", servoQuietMs(333), 9);
	checkInt("560 Hz: two 2 ms frames and 1 ms", servoQuietMs(560), 5);
	checkInt("rate 0 counts as 1 Hz", servoQuietMs(0), 2001);
	checkTrue("nothing pulsed: at once", servoQuietDone(false, 1000, 1000, 50));
	checkTrue("pulsed, 40 ms after level 0: not yet", !servoQuietDone(true, 1000, 1040, 50));
	checkTrue("41 ms: done", servoQuietDone(true, 1000, 1041, 50));
	checkTrue("41 ms across the wrap: done", servoQuietDone(true, 0xFFFFFFF0u, 25u, 50));
	checkTrue("40 ms across the wrap: not yet", !servoQuietDone(true, 0xFFFFFFF0u, 24u, 50));
}

/** @brief Advance to @p intoMs into the slice's frame of @p periodMs. */
static void toFramePoint(uint32_t intoMs, uint32_t periodMs) {
	uint64_t into = (time_us_64() - fakeSlice(0).startUs) / 1000u % periodMs;
	fakeAdvance((uint32_t)((intoMs + periodMs - into) % periodMs));
}

/** @brief Poll core1 1 ms at a time until the job is idle; the ms it took. */
static uint32_t pollUntilIdle() {
	uint32_t t = 0;
	while (core1JobPhase() != Core1JobPhase::Idle && t < 500) {
		fakeAdvance(1);
		t++;
		core1JobPoll();
	}
	return t;
}

/**
 * @brief No pulse cut short at a release or a rebind, against the counter
 *        model.
 */
static void testLastPulse() {
	section("Servo job: the last pulse completes before the pins let go");

	fakePwmReset();
	freshPump();
	escHeartbeat();
	ServoProfile prof;
	servoProfileDefault(&prof);
	servoJobProfile(&prof);
	checkTrue("running", claimRunning(PIN_SIG));
	send(cmdOf(0, ServoCmdKind::Start));
	fakeAdvance(60);
	escHeartbeat();
	core1JobPoll();
	checkInt("1500 us latched on the counter", fakePwmLatched(SIG), 1500);
	toFramePoint(1, 20);
	pinClaimEnd(PinUser::Servo);
	core1JobPoll();
	checkTrue("BACK 1 ms into a 1500 us pulse: level 0 written, the pin kept",
	          fakePin(SIG).level == 0 && fakePin(SIG).function == GPIO_FUNC_PWM &&
	          core1JobPhase() == Core1JobPhase::Stopping);
	checkInt("the pulse under way stays latched to its end", fakePwmLatched(SIG), 1500);
	ServoStatus wait;
	servoJobStatus(&wait);
	checkTrue("the status during the wait: not RUNNING, no pulse",
	          !wait.running && !wait.sweeping && wait.outUs[0] == 0);
	pinClaimPoll();
	checkTrue("no unplug page yet", !pinClaimUnplugDue());
	uint32_t waited = pollUntilIdle();
	checkInt("released after 41 ms at 50 Hz", waited, 41);
	checkInt("no pulse cut short", fakePin(SIG).cutPulses, 0);
	checkTrue("an input, pulled down", fakePin(SIG).function == GPIO_FUNC_SIO &&
	                                   fakePin(SIG).pull == FakePull::Down);
	pinClaimPoll();
	checkTrue("then the unplug page", pinClaimUnplugDue());
	pinClaimRestart(PinUser::Servo);
	fakePumpPoll();

	// Two servos, BACK while RUNNING, at 560 Hz.
	prof.type = ServoType::HeliTail760;
	servoProfileLoadType(&prof, ServoType::HeliTail760);
	prof.servos = 2;
	checkTrue("HELI TAIL, two servos", servoJobProfile(&prof));
	escHeartbeat();
	checkTrue("running", claimRunning(PIN_SIG | PIN_AUX));
	send(cmdOf(0, ServoCmdKind::Start));
	fakeAdvance(10);
	escHeartbeat();
	core1JobPoll();
	pinClaimEnd(PinUser::Servo);
	core1JobPoll();
	waited = pollUntilIdle();
	checkInt("both released after 5 ms at 560 Hz", waited, 5);
	checkTrue("no pulse cut short on either pin",
	          fakePin(SIG).cutPulses == 0 && fakePin(AUX).cutPulses == 0);
	checkTrue("both pulled down", fakePin(SIG).pull == FakePull::Down &&
	                              fakePin(AUX).pull == FakePull::Down);
	pinClaimPoll();
	pinClaimRestart(PinUser::Servo);
	fakePumpPoll();

	// RUNNING, STOP, and a new profile straight after: the rebind waits.
	servoProfileDefault(&prof);
	servoJobProfile(&prof);
	escHeartbeat();
	checkTrue("running", claimRunning(PIN_SIG));
	send(cmdOf(0, ServoCmdKind::Start));
	fakeAdvance(60);
	escHeartbeat();
	core1JobPoll();
	toFramePoint(1, 20);
	send(cmdOf(0, ServoCmdKind::Safe));
	int inits = fakeSlice(0).inits;
	ServoProfile fast = prof;
	fast.rateHz = 100;
	checkTrue("a profile at once after the STOP: accepted", servoJobProfile(&fast));
	core1JobPoll();
	checkInt("the rebind waits", fakeSlice(0).inits, inits);
	uint32_t t = 0;
	while (fakeSlice(0).inits == inits && t < 500) { fakeAdvance(1); t++; core1JobPoll(); }
	checkInt("and rebinds 41 ms after the STOP", t, 41);
	checkInt("no pulse cut short", fakePin(SIG).cutPulses, 0);
	checkInt("at 100 Hz", fakeSlice(0).top, 9999);
	checkTrue("back to free", claimEnd());
}

/** @brief The trip latch outlives a rebind; a new session forgets it. */
static void testTripLatch() {
	section("Servo job: the trip latch, across rebinds and sessions");

	ServoProfile prof;
	servoProfileDefault(&prof);
	ServoCore c;
	servoCoreReset(&c, &prof, true, false, 0, true);
	checkTrue("a reset keeps the latch it is given", c.tripped && !c.running);
	c.prof.minUs = 900;
	servoCoreReset(&c, &c.prof, true, false, 0, false);
	checkTrue("and may take its profile from the state it resets", c.prof.minUs == 900 &&
	                                                             c.prof.rateHz == 50);

	fakePwmReset();
	freshPump();
	escHeartbeat();
	servoJobNewSession();
	servoJobProfile(&prof);
	checkTrue("running", claimRunning(PIN_SIG));
	send(cmdOf(0, ServoCmdKind::Start));
	fakeAdvance(300);
	core1JobPoll();
	ServoStatus st;
	servoJobStatus(&st);
	checkTrue("a stall trips it", st.tripped && !st.running);
	escHeartbeat();
	ServoProfile fast = prof;
	fast.rateHz = 100;
	checkTrue("a new profile", servoJobProfile(&fast));
	fakeAdvance(100);
	core1JobPoll();
	servoJobStatus(&st);
	checkTrue("the rebind keeps the latch", st.tripped && fakeSlice(0).top == 9999);
	pinClaimEnd(PinUser::Servo);
	core1JobPoll();
	pinClaimPoll();
	pinClaimKeep(PinUser::Servo);
	core1JobPoll();
	pinClaimPoll();
	servoJobStatus(&st);
	checkTrue("BACK on the unplug page, the bind again: still latched",
	          pinClaimState() == ClaimState::Running && st.tripped);
	checkTrue("back to free", claimEnd());
	servoJobNewSession();
	escHeartbeat();
	checkTrue("a new session claims", claimRunning(PIN_SIG));
	servoJobStatus(&st);
	checkTrue("and forgets the old trip", !st.tripped);
	checkTrue("back to free", claimEnd());
}

/** @brief One frame of touch, built by hand. */
static TouchState touchAt(int x, int y, bool pressed, bool down, bool released,
                          int downX = -1, int downY = -1) {
	TouchState t;
	memset(&t, 0, sizeof(t));
	t.x = (int16_t)x; t.y = (int16_t)y;
	t.pressed = pressed; t.down = down; t.released = released;
	t.downX = (int16_t)(downX < 0 ? x : downX);
	t.downY = (int16_t)(downY < 0 ? y : downY);
	return t;
}

/**
 * @brief The slider's event logic: the two press contracts, tap-to-set,
 *        presets.
 */
static void testSlider() {
	section("Slider: delta drag, tap-to-set, presets, the two press contracts");

	// A track of 101 px from 0 to 100: one unit a pixel.
	const UiRect track = {20, 100, 101, 20};
	UiSlider s;
	uiSliderInit(&s, track, 0, 100);
	checkTrue("starts at its minimum, no drag, no preset",
	          s.value == 0 && !s.dragging && s.pressedPreset == -1 && !s.tapToSet);
	uiSliderSet(&s, 40);
	TouchState t = touchAt(100, 110, true, true, false);
	checkTrue("a press on the track commands nothing", !uiSliderEvent(&s, &t) && s.value == 40);
	checkTrue("and starts a drag", s.dragging);
	t = touchAt(110, 110, false, true, false, 100, 110);
	checkTrue("a move of 10 px: +10", uiSliderEvent(&s, &t) && (int)s.value == 50);
	t = touchAt(115, 300, false, true, false, 100, 110);
	checkTrue("off the track the drag goes on", uiSliderEvent(&s, &t) && (int)s.value == 55);
	t = touchAt(115, 300, false, true, false, 100, 110);
	checkTrue("no move: no change", !uiSliderEvent(&s, &t));
	// rcbench re-anchors the value and keeps the press's x, so the next move
	// adds the whole distance from the press to the value set.
	uiSliderSet(&s, 20);
	t = touchAt(120, 300, false, true, false, 100, 110);
	checkTrue("a value set during a drag: the next move adds to it",
	          uiSliderEvent(&s, &t) && (int)s.value == 40);
	t = touchAt(300, 300, false, true, false, 100, 110);
	uiSliderEvent(&s, &t);
	checkTrue("clamped at the maximum", s.value == 100);
	t = touchAt(0, 300, false, false, true, 100, 110);
	checkTrue("the release moves it too, then ends the drag",
	          uiSliderEvent(&s, &t) && s.value == 0 && !s.dragging);

	uiSliderSet(&s, 30);
	t = touchAt(60, 50, true, true, false);
	uiSliderEvent(&s, &t);
	t = touchAt(60, 110, false, true, false, 60, 50);
	checkTrue("a press that began off the track never drags",
	          !uiSliderEvent(&s, &t) && s.value == 30 && !s.dragging);
	t = touchAt(90, 110, false, false, true, 60, 50);
	checkTrue("nor does its release", !uiSliderEvent(&s, &t) && s.value == 30);

	t = touchAt(70, 110, true, true, false);
	uiSliderEvent(&s, &t);
	t = touchAt(70, 110, false, false, false);
	checkTrue("no touch at all while dragging: no change", !uiSliderEvent(&s, &t));
	t = touchAt(40, 110, true, true, false);
	checkTrue("a new press ends a drag whose release was lost",
	          !uiSliderEvent(&s, &t) && s.dragging && s.dragX == 40);
	uiSliderRelease(&s);
	checkTrue("release abandons the drag", !s.dragging);
	t = touchAt(80, 110, false, true, false, 40, 110);
	checkTrue("and a move after it changes nothing", !uiSliderEvent(&s, &t) && s.value == 30);
	checkTrue("a null touch changes nothing", !uiSliderEvent(&s, nullptr));

	uiSliderSetTapToSet(&s, true);
	t = touchAt(120, 110, true, true, false);
	checkTrue("tap-to-set: the far end sets the maximum", uiSliderEvent(&s, &t) && s.value == 100);
	t = touchAt(110, 110, false, true, false, 120, 110);
	checkTrue("and the drag measures from there", uiSliderEvent(&s, &t) && (int)s.value == 90);
	t = touchAt(110, 110, false, false, true, 120, 110);
	uiSliderEvent(&s, &t);
	t = touchAt(70, 110, true, true, false);
	checkTrue("the middle sets 50", uiSliderEvent(&s, &t) && (int)s.value == 50);
	t = touchAt(70, 110, true, true, false);
	checkTrue("the same press again: no change", !uiSliderEvent(&s, &t));
	uiSliderRelease(&s);

	UiSlider thin;
	uiSliderInit(&thin, UiRect{20, 100, 1, 20}, 0, 100);
	uiSliderSetTapToSet(&thin, true);
	t = touchAt(20, 110, true, true, false);
	checkTrue("a 1 px track: a press sets nothing", !uiSliderEvent(&thin, &t) && thin.value == 0);
	t = touchAt(30, 110, false, true, false, 20, 110);
	checkTrue("and a drag moves nothing", !uiSliderEvent(&thin, &t) && thin.value == 0);

	static const float vals[] = {10, 50, 100};
	static const char *const labels[] = {"10", "50", "100"};
	uiSliderSetPresets(&s, vals, labels, 3, UiRect{20, 130, 108, 22});
	checkInt("three presets", s.presetCount, 3);
	uiSliderSet(&s, 30);
	checkTrue("laid out with 4 px gaps", s.presets[0].x == 20 && s.presets[0].w == 33 &&
	                                     s.presets[1].x == 57 && s.presets[2].x == 94);
	t = touchAt(70, 140, true, true, false);
	checkTrue("a press on a preset commands nothing", !uiSliderEvent(&s, &t) && s.pressedPreset == 1);
	t = touchAt(70, 140, false, false, true);
	checkTrue("its release on the preset sets it", uiSliderEvent(&s, &t) && s.value == 50);
	t = touchAt(100, 140, true, true, false);
	uiSliderEvent(&s, &t);
	t = touchAt(100, 200, false, false, true, 100, 140);
	checkTrue("a press slid off its preset sets nothing",
	          !uiSliderEvent(&s, &t) && s.value == 50 && s.pressedPreset == -1);
	t = touchAt(30, 140, true, true, false);
	uiSliderEvent(&s, &t);
	t = touchAt(30, 140, false, false, true);
	checkTrue("the 10 preset", uiSliderEvent(&s, &t) && s.value == 10);
	t = touchAt(30, 140, true, true, false);
	uiSliderEvent(&s, &t);
	t = touchAt(30, 140, false, false, true);
	checkTrue("a preset at the value: no change", !uiSliderEvent(&s, &t));
	t = touchAt(200, 200, true, true, false);
	checkTrue("a press on neither: nothing", !uiSliderEvent(&s, &t) && s.pressedPreset == -1 && !s.dragging);

	UiRect pr = uiSliderPaintedRect(&s);
	checkTrue("the painted rect holds the overhang and the presets",
	          pr.x == 20 && pr.y == 100 - UI_SLIDER_OVERHANG && pr.w == 107 && pr.h == 52 + UI_SLIDER_OVERHANG);
	uiSliderSetPresets(&s, vals, labels, 0, UiRect{20, 130, 108, 22});
	checkInt("no presets", s.presetCount, 0);
	pr = uiSliderPaintedRect(&s);
	checkTrue("then the track and the overhang only",
	          pr.y == 97 && pr.h == 26 && pr.x == 20 && pr.w == 101);
	uiSliderSetPresets(&s, vals, labels, -2, UiRect{20, 130, 108, 22});
	checkInt("a negative count is none", s.presetCount, 0);
	static float many[12];
	static const char *manyL[12];
	for (int i = 0; i < 12; i++) { many[i] = (float)i; manyL[i] = "X"; }
	uiSliderSetPresets(&s, many, manyL, 12, UiRect{0, 130, 240, 22});
	checkInt("more than eight are eight", s.presetCount, UI_SLIDER_MAX_PRESETS);
}

/** @brief The slider's drawing: fill, thumb, dim, presets. */
static void testSliderDraw() {
	section("Slider: the fill, the thumb and the dimmed look");

	gfxInit();
	UiSlider s;
	uiSliderInit(&s, UiRect{20, 100, 101, 20}, 0, 100);
	uiSliderSet(&s, 50);
	uiSliderDraw(&s, C_LIME);
	const uint16_t *fb = gfxBuffer();
	checkTrue("filled to the value", fb[110 * GFX_W + 50] == C_LIME);
	checkTrue("trough past it", fb[110 * GFX_W + 100] == C_PANEL);
	checkTrue("the thumb at the value, above the track",
	          fb[(100 - UI_SLIDER_OVERHANG + 2) * GFX_W + 70] == C_TEXT);
	checkTrue("nothing above the overhang", fb[(100 - UI_SLIDER_OVERHANG - 1) * GFX_W + 70] == 0);

	gfxInit();
	uiSliderSet(&s, 1);
	uiSliderDraw(&s, C_LIME);
	checkTrue("a fill under one track height is one track height wide",
	          fb[110 * GFX_W + 20 + 15] == C_LIME);
	gfxInit();
	uiSliderSet(&s, 0);
	uiSliderDraw(&s, C_LIME);
	checkTrue("at 0 no fill", fb[110 * GFX_W + 60] == C_PANEL);
	checkTrue("the thumb stays over the track's left end", fb[(100 - 1) * GFX_W + 27] == C_TEXT);
	gfxInit();
	uiSliderSet(&s, 100);
	s.dim = true;
	uiSliderDraw(&s, C_LIME);
	checkTrue("dimmed: the fill is grid grey", fb[110 * GFX_W + 50] == C_GRID);
	checkTrue("and the thumb dim, at the right end", fb[(100 - 1) * GFX_W + 114] == C_DIM);
	// High contrast: C_DIM is near black as C_TEXT is, so a dimmed thumb is
	// hollow. The thumb is 14 px from x 107; x 114 is between its grip lines.
	themeSet(Theme::HighContrast);
	gfxInit();
	uiSliderDraw(&s, C_LIME);
	checkTrue("high contrast, dimmed: the thumb hollow, C_BG in a 2 px C_GRID frame",
	          fb[(100 - 1) * GFX_W + 114] == C_BG && fb[110 * GFX_W + 114] == C_BG &&
	          fb[110 * GFX_W + 107] == C_GRID && fb[110 * GFX_W + 108] == C_GRID &&
	          fb[110 * GFX_W + 120] == C_GRID && fb[110 * GFX_W + 112] == C_GRID);
	checkTrue("high contrast, dimmed: the fill still C_GRID", fb[110 * GFX_W + 50] == C_GRID);
	s.dim = false;
	gfxInit();
	uiSliderDraw(&s, C_LIME);
	checkTrue("high contrast, live: the thumb solid, C_TEXT",
	          fb[(100 - 1) * GFX_W + 114] == C_TEXT && fb[110 * GFX_W + 114] == C_TEXT);
	s.dim = true;
	themeSet(Theme::Dark);
	gfxInit();
	UiSlider flat;
	uiSliderInit(&flat, UiRect{20, 100, 101, 20}, 5, 5);
	uiSliderDraw(&flat, C_LIME);
	checkTrue("an empty range draws at the left end", fb[110 * GFX_W + 60] == C_PANEL);

	gfxInit();
	static const float vals[] = {10, 50};
	static const char *const labels[] = {"10", "50"};
	uiSliderSetPresets(&s, vals, labels, 2, UiRect{20, 130, 104, 22});
	s.dim = false;
	uiSliderDraw(&s, C_LIME);
	checkTrue("presets are drawn as buttons", fb[(130 + 2) * GFX_W + 30] == C_PANEL);
}

// ---------------------------------------------------------------------------
// the screens, through the real UI
// ---------------------------------------------------------------------------

/**
 * @brief A frame @p ms after the last: core1's pump and job passes, then the
 *        UI.
 */
static void frameAt(uint32_t ms) {
	fakeAdvance(ms);
	fakePumpPoll();
	core1JobPoll();
	uiTick();
}

/** @brief @p n UI frames of 25 ms. */
static void uiFrames(int n) {
	for (int i = 0; i < n; i++) frameAt(25);
}

// Controls are pressed by id (tapId, pressId, holdId). The tests below read
// each control's rectangle from the table.
#define UI_IDS_FRAMES(n) uiFrames(n)
#include "ui_ids.h"

/**
 * @brief A tap at a point. For taps whose place within a control or outside
 *        one is the test; a tap on a control goes through tapId().
 */
static void uiTap(int x, int y) {
	fakePress(x, y); uiFrames(2); fakeRelease(); uiFrames(1);
}

/** @brief Core1's pass after the frame that sent a command. */
static void core1() { core1JobPoll(); }

/** @brief No ESC on either wire. */
static void noEsc() {
	EscTelemetry t;
	memset(&t, 0, sizeof(t));
	fakeSetTelemetry(&t);
}

/** @brief The point at @p deg on the dial, @p r px from the shaft. */
static int dialX(float deg, int r = 60) {
	return SERVO_DIAL_X + (int)lroundf((float)r * sinf(deg * 3.14159265f / 180.0f));
}
static int dialY(float deg, int r = 60) {
	return SERVO_DIAL_Y - (int)lroundf((float)r * cosf(deg * 3.14159265f / 180.0f));
}

/** @brief The pixel at (@p x, @p y). */
static uint16_t pixel(int x, int y) { return gfxBuffer()[y * GFX_W + x]; }

/**
 * @brief A pixel inside @p r clear of its label: fill when live, background
 *        when disabled.
 */
static uint16_t fillOf(const UiRect &r) { return pixel(r.x + 4, r.y + r.h - 5); }

/** @brief Whether every pixel of the rectangle is @p c. */
static bool allOf(int x, int y, int w, int h, uint16_t c) {
	for (int yy = y; yy < y + h; yy++)
		for (int xx = x; xx < x + w; xx++)
			if (pixel(xx, yy) != c) return false;
	return true;
}

/** @brief Whether the readout shows the two-servo hint, right-aligned to x 232. */
static bool twoHintShown() {
	return fakeTextIs(GFX_W - UI_MARGIN - gfxTextW(SERVO_UI_TWO_1, 1), SV_READ_Y + 3,
	                  SERVO_UI_TWO_1, C_DIM, C_BG, 1) &&
	       fakeTextIs(GFX_W - UI_MARGIN - gfxTextW(SERVO_UI_TWO_2, 1), SV_READ_Y + 14,
	                  SERVO_UI_TWO_2, C_DIM, C_BG, 1);
}

/** @brief Whether the readout's right half, where the hint goes, is blank. */
static bool twoHintBlank() { return allOf(140, SV_READ_Y + 3, GFX_W - 140, 18, C_BG); }


/**
 * @brief Whether MENU row @p id is drawn refused: an outline with the
 *        background inside, where a live row has the panel fill, and the
 *        strip reads DSHOT RESTARTING ON SIG GPn in amber.
 */
static bool busyShown(UiControlId id) {
	UiRect r = ctlRect(id);
	char want[40];
	snprintf(want, sizeof(want), UI_MENU_RESTARTING_FMT, (unsigned)settings()->dshotPin);
	// On the first line of two while MENU's AM32 run cue stands under it.
	return pixel(r.x + 5, r.y + 8) == C_BG &&
	       (fakeTextIs(UI_MARGIN, UI_STRIP_Y + 4, want, C_AMBER, C_BG, 1) ||
	        fakeTextIs(UI_MARGIN, UI_STRIP_LINE1_Y, want, C_AMBER, C_BG, 1));
}

/** @brief Fresh board: default settings, no ESC, the main screen up. */
static void freshUi() {
	fakeFlashClear();
	settingsLoad();
	fakePwmReset();
	noEsc();
	uiInitTester();
	escTaskConfigure(0, 600, false, 1);
	uiFrames(2);
}

/** @brief Main screen to SERVO TESTER, through MENU and SERVO, the hub. */
static void openServo() {
	tapId(UI_FIX(TesterCtl::Menu));
	uiFrames(1);
	checkTrue("MENU opens", uiCurrentScreen() == UiScreenId::Menu);
	tapId(toolEntry(&UI_SERVO));
	uiFrames(2);
	checkTrue("SERVO TESTER opens from SERVO, the hub", navIsTool(UiScreenId::ServoHub, UiScreenId::Servo));
}

/** @brief SERVO's main view to SET. */
static void openSet() {
	tapId(UI_FIX(ServoCtl::Set));
	checkTrue("SET opens: HOLD TO APPLY is there", ctlPresent(UI_FIX(ServoCtl::Apply)));
}

/**
 * @brief BACK from SET: SERVO's main view again. With a change waiting for
 *        HOLD TO APPLY the first BACK arms the guard and SET stays; the
 *        second returns.
 */
static void backFromSet() {
	bool pending = s_pending;
	tapId(UI_FIX(UI_BACK));
	if (pending) {
		checkTrue("a change waits: the first BACK arms the guard, SET stays",
		          s_guard.armed && s_view == View::Set && ctlPresent(UI_FIX(ServoCtl::Apply)));
		tapId(UI_FIX(UI_BACK));
	}
	checkTrue("BACK from SET reaches SERVO's main view",
	          uiCurrentScreen() == UiScreenId::Servo && ctlPresent(UI_FIX(ServoCtl::Start)));
}

/**
 * @brief From MENU, which has no BACK, through its row ESC TESTER: the
 *        tester. From SERVO, the hub, its BACK reaches MENU first.
 */
static void backFromMenu() {
	checkTrue("MENU's row ESC TESTER reaches the tester", backToTester());
}

/** @brief Hold HOLD TO START for 1000 ms, then lift. */
static void holdStart() {
	int x, y;
	ctlCentre(UI_FIX(ServoCtl::Start), &x, &y);
	pressId(UI_FIX(ServoCtl::Start));
	fakeHold(x, y); frameAt(UI_HOLD_MS);
	fakeRelease(); uiFrames(1);
	core1();
}

/**
 * @brief Hold HOLD TO START for 1000 ms in UI frames of 25 ms, then lift.
 *        With the switch, core1 refuses START once the touch controller's
 *        stamp is 250 ms old, which one frame of 1000 ms would make it.
 */
static void holdStartFrames() {
	int x, y;
	ctlCentre(UI_FIX(ServoCtl::Start), &x, &y);
	pressId(UI_FIX(ServoCtl::Start));
	fakeHold(x, y);
	uiFrames(UI_HOLD_MS / 25);
	fakeRelease(); uiFrames(1);
	core1();
}

/**
 * @brief BACK, the leave page, and RESTART DSHOT: out to SERVO, the hub
 *        that opened the tester (menu-c.md owner decision 7), then the
 *        hub's BACK to MENU.
 */
static void leaveServo() {
	tapId(UI_FIX(UI_BACK));
	uiFrames(1);
	// The page offers RESTART DSHOT 1000 ms after the job has let go of the pin.
	for (int i = 0; i < 200 && !ctlEnabled(UI_FIX(ServoCtl::Restart)); i++) uiFrames(1);
	tapId(UI_FIX(ServoCtl::Restart));
	uiFrames(2);
	checkTrue("RESTART DSHOT leaves SERVO TESTER for SERVO, its hub", navIsHub(UiScreenId::ServoHub));
	tapId(UI_FIX(UI_BACK));
	uiFrames(1);
	checkTrue("the hub's BACK: MENU", uiCurrentScreen() == UiScreenId::Menu);
}

/**
 * @brief Drag on the dial from @p deg0 to @p deg1, in 10 deg steps, and lift.
 */
static void dialDrag(float deg0, float deg1, bool lift = true) {
	fakePress(dialX(deg0), dialY(deg0)); uiFrames(1);
	float step = deg1 > deg0 ? 10.0f : -10.0f;
	for (float d = deg0; (step > 0) ? d <= deg1 : d >= deg1; d += step) {
		fakeHold(dialX(d), dialY(d)); uiFrames(1);
	}
	fakeHold(dialX(deg1), dialY(deg1)); uiFrames(1);
	if (lift) { fakeRelease(); uiFrames(1); }
	core1();
}

/**
 * @brief SET's list viewport, read from the rows' rectangles: its first and
 *        last pixel row, and the middle of a row.
 */
static void setViewport(int *top, int *bottom, int *midX) {
	UiControl t[UI_CONTROLS_MAX];
	uint8_t n = uiControls(t, UI_CONTROLS_MAX);
	int lo = 1000, hi = -1;
	*midX = -1;
	for (uint8_t i = 0; i < n; i++) {
		if (t[i].id.row == UI_FIXED || t[i].id.part != (uint8_t)UiPart::Row) continue;
		if (t[i].rect.y < lo) lo = t[i].rect.y;
		if (t[i].rect.y + t[i].rect.h - 1 > hi) hi = t[i].rect.y + t[i].rect.h - 1;
		*midX = t[i].rect.x + t[i].rect.w / 2;
	}
	*top = lo;
	*bottom = hi;
}

/** @brief The y of the middle of SET row @p row, as the table has it. */
static int rowCentreY(int row) {
	int x, y;
	ctlCentre(UI_ROW(row, Row), &x, &y);
	return y;
}

/**
 * @brief The SET row @p i: put it at the top of the viewport and tap it by id.
 *
 * The scroll is set directly, as before: the screenshots show SET at that
 * scroll. revealRow() then makes sure the row is whole.
 */
static void selectRow(int i) {
	uiListScrollTo(&s_list, i * SET_ROW_PITCH);
	s_full = true;
	uiFrames(1);
	revealRow(i);
	tapId(UI_ROW(i, Row));
}

/**
 * @brief Tap + or - in the editor bar @p n times, apart.
 * @param want Whether the test expects the button to act.
 */
static void stepSel(int dir, int n = 1, IdExpect want = IdExpect::Enabled) {
	for (int i = 0; i < n; i++) {
		tapId(UI_FIX(dir > 0 ? ServoCtl::EditPlus : ServoCtl::EditMinus), want);
		uiFrames(16);   // past the repeat delay, so each tap is one step
	}
	core1();
}

/** @brief Hold HOLD TO APPLY for @ref UI_HOLD_MS, then lift. */
static void holdApply() {
	int x, y;
	ctlCentre(UI_FIX(ServoCtl::Apply), &x, &y);
	pressId(UI_FIX(ServoCtl::Apply));
	fakeHold(x, y); frameAt(UI_HOLD_MS);
	fakeRelease(); uiFrames(1);
	core1();
}

/** @brief Swipe SET row @p i sideways by one swipe step in @p dir. */
static void swipeRow(int i, int dir) {
	selectRow(i);
	int x, y;
	ctlCentre(UI_ROW(i, Row), &x, &y);
	fakePress(x, y); uiFrames(1);
	for (int d = 0; d <= UI_AXIS_LOCK_PX + 2 + UI_SWIPE_STEP_PX; d += 4) {
		fakeHold(x + dir * d, y); uiFrames(1);
	}
	fakeRelease(); uiFrames(1);
	core1();
}

/**
 * @brief Whether the header from where `SERVO TESTER` ends, x 150, to BACK
 *        at x 170 is plain panel over the title's 14 rows: no text after it.
 */
static bool titleEndsClear() {
	int x = UI_MARGIN + gfxTextW("SERVO TESTER", 2);
	return allOf(x, (UI_HDR_H - 14) / 2, UI_BACK_X - x, 14, C_PANEL);
}

/** @brief SERVO TESTER opens from MENU, claims both pins, and starts SAFE. */
static void testScreenOpens() {
	section("Servo screen: MENU opens it, and it starts SAFE");

	freshUi();
	tapId(UI_FIX(TesterCtl::Menu));
	uiFrames(2);
	checkTrue("MENU opens", uiCurrentScreen() == UiScreenId::Menu);
	checkTrue("MENU: SERVO, the hub's row, is drawn live",
	          fillOf(ctlRect(menuRow(&UI_SERVO_HUB_SCREEN))) == C_PANEL);
	const UiControlId entry = toolEntry(&UI_SERVO);
	checkTrue("SERVO, the hub: SERVO TESTER is its first entry, drawn live",
	          uiCurrentScreen() == UiScreenId::ServoHub && entry == HUB_ENTRY(0) &&
	          fillOf(ctlRect(entry)) == C_PANEL);
	tapId(entry);
	checkTrue("SERVO TESTER opens from the hub", navIsTool(UiScreenId::ServoHub, UiScreenId::Servo));
	checkTrue("the tap claims the pins for SERVO", pinClaimHolder() == PinUser::Servo);
	uiFrames(3);
	checkTrue("running on the claim", pinClaimState() == ClaimState::Running);
	checkInt("with SIG", pinClaimGpio(PIN_SIG), 0);
	checkInt("and AUX", pinClaimGpio(PIN_AUX), 1);
	checkTrue("SIG on PWM at level 0: SAFE, no pulses",
	          fakePin(0).function == GPIO_FUNC_PWM && fakePin(0).level == 0);
	checkTrue("one servo: AUX not bound", fakePin(1).function != GPIO_FUNC_PWM);
	checkTrue("STANDARD PWM 50 Hz, one servo, SPEED 100 %",
	          s_profile.type == ServoType::Standard && s_profile.rateHz == 50 &&
	          s_profile.servos == 1 && (int)s_speed.value == 100);
	checkTrue("the chip says SAFE", pixel(GFX_W - UI_MARGIN - 4, UI_STRIP_Y + 3) == C_GREEN);
	// menu-b.md decision 13: the title is the label of the entry that opens it.
	checkTrue("the title reads SERVO TESTER",
	          fakeTextIs(UI_MARGIN, (UI_HDR_H - 14) / 2, "SERVO TESTER", C_TEXT, C_PANEL, 2));
	checkTrue("and nothing after it before BACK", titleEndsClear());
	checkTrue("the hub entry's label and the screen's title",
	          strcmp(UI_SERVO_ENTRIES[0].label, SERVO_UI_TITLE) == 0 && UI_SERVO_ENTRIES[0].screen == &UI_SERVO &&
	          strcmp(UI_SERVO.title, SERVO_UI_TITLE) == 0);
	checkTrue("13 characters or fewer: it ends 4 px or more before BACK",
	          strlen(SERVO_UI_TITLE) <= 13 && UI_MARGIN + gfxTextW(SERVO_UI_TITLE, 2) + 4 <= UI_BACK_X);
	checkTrue("HOLD TO START is live", fillOf(BTN_START) == C_PANEL);
	checkControlTable("SERVO, SAFE");
	checkTrue("MIN, CENTRE, MAX and SWEEP are outlines while SAFE",
	          fillOf(BTN_MIN) == C_BG && fillOf(BTN_CENTRE) == C_BG &&
	          fillOf(BTN_MAX) == C_BG && fillOf(BTN_SWEEP) == C_BG);
	fakeDumpFrame("shot_servo_safe.ppm");

	int writes = fakePin(0).levelWrites;
	tapId(UI_FIX(ServoCtl::Max), IdExpect::Disabled);
	core1();
	checkTrue("MAX while SAFE does nothing", fakePin(0).level == 0 && fakePin(0).levelWrites == writes);
	ServoStatus before;
	servoJobStatus(&before);
	checkTrue("SAFE: the dial does not act", !ctlEnabled(UI_FIX(ServoCtl::Dial)));
	dialDrag(-60, 60, false);
	checkTrue("a press on the dial while SAFE takes no drag", !s_dragging);
	fakeRelease(); uiFrames(1); core1();
	ServoStatus after;
	servoJobStatus(&after);
	checkTrue("sends the job no command", after.ackSeq == before.ackSeq);
	checkTrue("and moves nothing", fakePin(0).level == 0 && fakePin(0).levelWrites == writes);
	// uiTick() flushes, which clears the bands, so the screen's own frame is
	// called here with no flush after it.
	uiFrames(2);
	TouchState idle;
	memset(&idle, 0, sizeof(idle));
	gfxClearDirty();
	uiServoTick(&idle);
	checkInt("a still screen repaints nothing", gfxDirtyCount(), 0);
	leaveServo();
}

/** @brief HOLD TO START: 1000 ms, not on a slide off, CENTRE at the start. */
static void testHoldToStart() {
	section("Servo screen: HOLD TO START after 1000 ms, STOP on press");

	freshUi();
	openServo();
	const UiControlId startId = UI_FIX(ServoCtl::Start);
	int x, y;
	ctlCentre(startId, &x, &y);
	pressId(startId);
	fakeHold(x, y); frameAt(UI_HOLD_MS - 1);
	core1();
	checkTrue("held 999 ms: not started", !s_st.running && fakePin(0).level == 0);
	checkTrue("the bar shows the hold", s_hold.holding && pixel(BTN_START.x + 2, SV_BAR_Y + 1) == C_LIME);
	checkTrue("and the button is red while held", fillOf(BTN_START) == C_RED);
	fakeHold(x, y); frameAt(1);
	core1();
	ServoStatus st;
	servoJobStatus(&st);
	checkTrue("1000 ms: started", st.running);
	checkInt("CENTRE at the start", fakePin(0).level, 1500);
	fakeHold(x, y); uiFrames(2);
	core1();
	checkTrue("still held on the button: STOP does not fire", s_st.running);
	// The UI stalls with the finger still on the button, and the job trips.
	// The press that started the servo does not start it again, however
	// long it stays.
	fakeAdvance(300);
	core1();
	for (int i = 0; i < 50; i++) { fakeHold(x, y); uiFrames(1); }
	core1();
	servoJobStatus(&st);
	checkTrue("tripped under the finger, then held 1250 ms: not started again",
	          !st.running && st.tripped && fakePin(0).level == 0);
	fakeRelease(); uiFrames(1);
	holdStart();
	checkTrue("a new press starts it", s_st.running && fakePin(0).level == 1500);
	checkTrue("the chip says RUNNING", pixel(GFX_W - UI_MARGIN - 4, UI_STRIP_Y + 3) == C_RED);
	checkTrue("MIN, CENTRE, MAX, SWEEP are live", fillOf(BTN_MIN) == C_PANEL && fillOf(BTN_SWEEP) == C_PANEL);
	checkControlTable("SERVO, RUNNING");

	pressId(startId);
	core1();
	checkInt("STOP on press: level 0 on core1's next pass", fakePin(0).level, 0);
	servoJobStatus(&st);
	checkTrue("SAFE", !st.running);
	fakeHold(x, y); frameAt(UI_HOLD_MS);
	fakeHold(x, y); frameAt(UI_HOLD_MS);
	core1();
	servoJobStatus(&st);
	checkTrue("the press that stopped it, held 2000 ms, does not start it again",
	          !st.running && !s_hold.holding);
	fakeRelease(); uiFrames(1);

	// A hold that slides off before 1000 ms starts nothing.
	pressId(startId);
	fakeHold(x, y); frameAt(500);
	fakeHold(x, ctlRect(startId).y - 40); frameAt(600);
	fakeHold(x, y); uiFrames(1);
	core1();
	checkTrue("slid off at 500 ms: nothing", !s_st.running && fakePin(0).level == 0);
	fakeRelease(); uiFrames(1);
	// A press that began elsewhere and slid onto the button starts nothing.
	// The readout row holds no control with one servo.
	fakePress(x, SV_READ_Y + 5); uiFrames(1);
	fakeHold(x, y); uiFrames(1);
	fakeHold(x, y); frameAt(UI_HOLD_MS + 100);
	core1();
	checkTrue("a press slid onto it: nothing", fakePin(0).level == 0);
	fakeRelease(); uiFrames(1);
	leaveServo();
}

/**
 * @brief A press that lands on HOLD TO START while it is drawn disabled never
 *        starts the servo, however long it stays after the claim goes live.
 */
static void testStartNeedsLivePress() {
	section("Servo screen: a press on a disabled HOLD TO START never starts it");

	freshUi();
	tapId(UI_FIX(TesterCtl::Menu));
	uiFrames(1);
	checkTrue("MENU opens", uiCurrentScreen() == UiScreenId::Menu);
	tapId(toolEntry(&UI_SERVO));
	checkTrue("SERVO TESTER opens from its hub", uiCurrentScreen() == UiScreenId::Servo);
	int x, y;
	ctlCentre(UI_FIX(ServoCtl::Start), &x, &y);
	pressId(UI_FIX(ServoCtl::Start), IdExpect::Disabled); core1();
	checkTrue("at the press the claim is not live: START is an outline",
	          !live() && fillOf(BTN_START) == C_BG);
	bool wentLive = false, ran = false;
	for (int i = 0; i < 120; i++) {
		fakeHold(x, y); uiFrames(1); core1();
		ServoStatus st;
		servoJobStatus(&st);
		wentLive |= live();
		ran |= st.running;
	}
	checkTrue("the claim goes live under the finger", wentLive);
	checkTrue("held 3000 ms after: the servo never starts", !ran && fakePin(0).level == 0);
	fakeRelease(); uiFrames(1);
	holdStart();
	checkTrue("a new press on the live button starts it", s_st.running);
	tapId(UI_FIX(ServoCtl::Start)); core1();
	leaveServo();
}

/**
 * @brief The horn: absolute, held on release, refused while SAFE, owned by its
 *        press.
 */
static void testHorn() {
	section("Servo screen: the horn is the control");

	freshUi();
	openServo();
	holdStart();
	dialDrag(45, 45, false);
	checkInt("a press at +45 deg: 1750 us", fakePin(0).level, 1750);
	checkTrue("a drag", s_dragging);
	fakeHold(dialX(-45), dialY(-45)); uiFrames(1); core1();
	checkInt("moved to -45 deg: 1250 us", fakePin(0).level, 1250);
	fakeHold(dialX(30, 150), dialY(30, 150)); uiFrames(1); core1();
	checkInt("off the dial, the drag goes on: +30 deg", fakePin(0).level, 1667, 1);
	fakeHold(dialX(100, 60), dialY(100, 60)); uiFrames(1); core1();
	checkInt("past +90 deg: MAX", fakePin(0).level, 2000);
	fakeHold(dialX(-97, 60), dialY(-97, 60)); uiFrames(1); core1();
	checkInt("past -90 deg: MIN", fakePin(0).level, 1000);
	fakeRelease(); uiFrames(2); core1();
	checkTrue("released: the servo holds", fakePin(0).level == 1000 && !s_dragging);
	fakeDumpFrame("shot_servo_running.ppm");

	fakePress(dialX(0, 140), dialY(0, 140)); uiFrames(1);
	fakeHold(dialX(30), dialY(30)); uiFrames(2); core1();
	checkTrue("a press that began off the dial never drags",
	          !s_dragging && fakePin(0).level == 1000);
	fakeRelease(); uiFrames(1);

	// A drag under way when the screen stalls: the trip ends the pulses and
	// the drag, and moving the same finger drives nothing afterwards.
	dialDrag(20, 20, false);
	checkTrue("dragging", s_dragging && fakePin(0).level > 1500);
	fakeAdvance(300);
	core1();
	checkTrue("the stall trips the job", fakePin(0).level == 0);
	fakeHold(dialX(60), dialY(60)); uiFrames(1); core1();
	checkTrue("the drag ended at the edge", !s_dragging && fakePin(0).level == 0);
	checkTrue("STALLED on the chip", pixel(GFX_W - UI_MARGIN - 4, UI_STRIP_Y + 3) == C_MAGENTA);
	checkTrue("STALLED is not SAFE: no two-servo hint", s_st.tripped && twoHintBlank());
	fakeDumpFrame("shot_servo_stalled.ppm");
	fakeRelease(); uiFrames(1);
	holdStart();
	checkTrue("a new HOLD TO START: CENTRE again", fakePin(0).level == 1500 && !s_st.tripped);

	// Stalled, left, opened again: the new session starts SAFE, not STALLED.
	fakeAdvance(300);
	core1();
	uiFrames(1);
	checkTrue("stalled again", s_st.tripped);
	leaveServo();
	tapId(toolEntry(&UI_SERVO));
	checkTrue("SERVO TESTER opens from its hub", uiCurrentScreen() == UiScreenId::Servo);
	uiFrames(1);
	checkTrue("opened again, first frame: the job not yet bound",
	          pinClaimState() != ClaimState::Running);
	checkTrue("and the chip is not STALLED", !s_st.tripped &&
	          pixel(GFX_W - UI_MARGIN - 4, UI_STRIP_Y + 3) != C_MAGENTA);
	uiFrames(3);
	checkTrue("opened again: SAFE, the old trip forgotten",
	          pinClaimState() == ClaimState::Running && !s_st.tripped &&
	          pixel(GFX_W - UI_MARGIN - 4, UI_STRIP_Y + 3) == C_GREEN);

	// Stalled, left, and opened while an ESC answers: refused, and the chip
	// does not carry the old session's trip beside the reason.
	holdStart();
	fakeAdvance(300);
	core1();
	uiFrames(1);
	checkTrue("stalled a third time", s_st.tripped);
	leaveServo();
	EscTelemetry t;
	memset(&t, 0, sizeof(t));
	t.lastRpmMs = millis();
	fakeSetTelemetry(&t);
	tapId(toolEntry(&UI_SERVO));
	checkTrue("SERVO TESTER opens from its hub", uiCurrentScreen() == UiScreenId::Servo);
	uiFrames(1);
	checkTrue("refused while an ESC answers, first frame: not STALLED",
	          s_refusal == ClaimRefusal::EscAnswering && !s_st.tripped &&
	          pixel(GFX_W - UI_MARGIN - 4, UI_STRIP_Y + 3) != C_MAGENTA);
	uiFrames(4);
	checkTrue("nor later in the refused visit", !s_claimed && !s_st.tripped &&
	          pixel(GFX_W - UI_MARGIN - 4, UI_STRIP_Y + 3) != C_MAGENTA);
	noEsc();
	frameAt(1);
	uiFrames(3);
	checkTrue("the ESC gone: claimed, SAFE, not STALLED",
	          pinClaimState() == ClaimState::Running && !s_st.tripped);
	leaveServo();
}

/** @brief MIN, CENTRE, MAX, SWEEP, SPEED, and what ends a sweep. */
static void testButtonsAndSweep() {
	section("Servo screen: MIN, CENTRE, MAX, SWEEP and SPEED");

	freshUi();
	openServo();
	holdStart();
	tapId(UI_FIX(ServoCtl::Max)); core1();
	checkInt("MAX", fakePin(0).level, 2000);
	tapId(UI_FIX(ServoCtl::Min)); core1();
	checkInt("MIN", fakePin(0).level, 1000);
	tapId(UI_FIX(ServoCtl::Centre)); core1();
	checkInt("CENTRE", fakePin(0).level, 1500);

	ServoStatus st;
	tapId(UI_FIX(ServoCtl::Sweep)); core1();
	servoJobStatus(&st);
	checkTrue("SWEEP: sweeping", st.sweeping);
	uiFrames(24); core1();
	checkTrue("the curve moves the servo", fakePin(0).level != 1500);
	uiFrames(1);
	checkTrue("the screen reads the ends reached", s_st.sweeping && s_st.sweepMoves >= 1);
	fakeDumpFrame("shot_servo_sweep.ppm");
	tapId(UI_FIX(ServoCtl::Sweep)); core1();
	servoJobStatus(&st);
	checkTrue("STOP SWEEP: still RUNNING, not sweeping", st.running && !st.sweeping);

	tapId(UI_FIX(ServoCtl::Sweep)); core1();
	tapId(UI_FIX(ServoCtl::Max)); core1();
	servoJobStatus(&st);
	checkTrue("MAX ends a sweep", !st.sweeping && fakePin(0).level == 2000);
	tapId(UI_FIX(ServoCtl::Sweep)); core1();
	dialDrag(0, 0);
	servoJobStatus(&st);
	checkTrue("a drag ends a sweep", !st.sweeping && fakePin(0).level == 1500);
	tapId(UI_FIX(ServoCtl::Sweep)); core1();
	pressId(UI_FIX(ServoCtl::Start)); core1();
	fakeRelease(); uiFrames(1);
	servoJobStatus(&st);
	checkTrue("STOP ends a sweep and the pulses", !st.sweeping && !st.running && fakePin(0).level == 0);

	// SPEED: tap-to-set at the left end of the track is 10 %.
	UiRect speed = ctlRect(UI_FIX(ServoCtl::Speed));
	uiTap(speed.x, speed.y + 10);
	checkInt("SPEED at the left end: 10 %", (long)lroundf(s_speed.value), 10);
	holdStart();
	tapId(UI_FIX(ServoCtl::Max)); core1();
	checkTrue("at 10 % MAX is not reached at once", fakePin(0).level < 1600);
	for (int i = 0; i < 40; i++) uiFrames(1);
	core1();
	checkInt("1000 ms later: 200 units of 1000, 1700 us", fakePin(0).level, 1700, 10);
	uiTap(speed.x + speed.w - 1, speed.y + 10);
	core1();
	checkInt("SPEED to 100 % mid-move: at MAX at once", fakePin(0).level, 2000);
	leaveServo();
}

/**
 * @brief A touch that does not answer stops a running servo as STOP does,
 *        sweeping or at a position, on either view; HOLD TO START waits for
 *        an answer.
 */
static void testTouchLossStops() {
	section("Servo screen: a touch that does not answer stops the servo");

	freshUi();
	openServo();
	holdStart();
	tapId(UI_FIX(ServoCtl::Sweep)); core1();
	ServoStatus st;
	servoJobStatus(&st);
	checkTrue("sweeping", st.running && st.sweeping);

	fakeTouchAnswers(false);
	uiFrames(3);
	servoJobStatus(&st);
	checkTrue("75 ms without an answer: still sweeping", st.running && st.sweeping);
	uiFrames(1); core1();
	servoJobStatus(&st);
	checkTrue("100 ms: stopped, as STOP stops it", !st.running && !st.sweeping);
	uiFrames(2);
	checkInt("no pulses", fakePin(0).level, 0);
	checkTrue("TOUCH NOT ANSWERING in amber on the dial band",
	          fakeTextCentredIs(64, UI_TOUCH_LOST_TEXT, C_AMBER, C_BG, 1));
	checkTrue("HOLD TO START is drawn disabled", fillOf(ctlRect(UI_FIX(ServoCtl::Start))) == C_BG);
	checkTrue("the run ended: THE SERVO IS STOPPED.",
	          fakeTextCentredIs(76, SERVO_UI_TOUCH_LOST_2, C_DIM, C_BG, 1));

	fakeTouchAnswers(true);
	uiFrames(2);
	servoJobStatus(&st);
	checkTrue("answering again: the note goes, still stopped",
	          !fakeTextCentredIs(64, UI_TOUCH_LOST_TEXT, C_AMBER, C_BG, 1) && !st.running);

	// Holding a position, on SET.
	holdStart();
	tapId(UI_FIX(ServoCtl::Max)); core1();
	openSet();
	servoJobStatus(&st);
	checkTrue("at MAX, SET open", st.running && fakePin(0).level == 2000);
	fakeTouchAnswers(false);
	uiFrames(4); core1();
	servoJobStatus(&st);
	checkTrue("a position on SET: stopped too", !st.running);
	fakeTouchAnswers(true);
	uiFrames(1);
	backFromSet();

	// The start refusal itself: a driver that still reports the finger while
	// the controller counts as lost does not start the servo.
	fakeTouchAnswers(false);
	uiFrames(4);
	int sx, sy;
	ctlCentre(UI_FIX(ServoCtl::Start), &sx, &sy);
	// The touch-loss stop would undo a start a frame later; the pin's level
	// writes show whether one happened at all.
	const int writes = fakePin(0).levelWrites;
	pressId(UI_FIX(ServoCtl::Start), IdExpect::Disabled);
	fakeHold(sx, sy); frameAt(UI_HOLD_MS);
	fakeRelease(); uiFrames(1);
	core1();
	servoJobStatus(&st);
	checkTrue("HOLD TO START held while lost: not running", !st.running);
	checkInt("not even for a frame: no pulse level written", fakePin(0).levelWrites, writes);
	fakeTouchAnswers(true);
	uiFrames(2);
	holdStart();
	servoJobStatus(&st);
	checkTrue("answering: HOLD TO START starts", st.running);
	pressId(UI_FIX(ServoCtl::Start)); core1();
	fakeRelease(); uiFrames(1);

	// Core0 stalls 260 ms with the touch silent: core1's heartbeat trip ends
	// the run before core0 counts the touch as lost.
	holdStart();
	servoJobStatus(&st);
	checkTrue("running again", st.running);
	fakeTouchAnswers(false);
	fakeAdvance(260);
	core1();
	servoJobStatus(&st);
	checkTrue("260 ms stall: the heartbeat trip, no fault named",
	          !st.running && st.tripped && st.powerFault == ServoPowerFault::None &&
	          !uiTouchLost());
	uiFrames(4);
	checkTrue("lost on core0 after the trip: TOUCH NOT ANSWERING",
	          uiTouchLost() && fakeTextCentredIs(64, UI_TOUCH_LOST_TEXT, C_AMBER, C_BG, 1));
	checkTrue("the trip ended the run: THE SERVO IS STOPPED.",
	          fakeTextCentredIs(76, SERVO_UI_TOUCH_LOST_2, C_DIM, C_BG, 1));
	fakeTouchAnswers(true);
	uiFrames(2);
	leaveServo();
}

/** @brief Two servos: SERVOS TWO on SET, the selector, REVERSE B. */
static void testTwoServos() {
	section("Servo screen: two servos, the selector and REVERSE B");

	freshUi();
	openServo();
	openSet();
	uiFrames(1);
	checkTrue("SET is open", s_view == View::Set);
	checkControlTable("SET");
	selectRow(ROW_SERVOS);
	checkInt("SERVOS selected", s_sel, ROW_SERVOS);
	checkControlTable("SET, a row selected");
	stepSel(+1);
	checkTrue("SERVOS TWO, applied at once", s_profile.servos == 2);
	checkTrue("AUX bound to PWM at level 0",
	          fakePin(1).function == GPIO_FUNC_PWM && fakePin(1).level == 0);
	selectRow(6);
	stepSel(+1);
	checkTrue("REVERSE B on", s_profile.reverseB);
	fakeDumpFrame("shot_servo_set.ppm");
	backFromSet();
	checkTrue("BACK: SERVO again", s_view == View::Main);
	bool clean = true;
	// 56 and 61: the first and last row between the strip, y 40 to 55, and
	// the dial band, from y 62.
	const int gapRows[] = {56, 61, SV_LOW_Y, SV_BTN_Y - 1};
	for (int x = 0; x < GFX_W; x++)
		for (int y : gapRows)
			clean = clean && pixel(x, y) == C_BG;
	checkTrue("nothing of SET is left between SERVO's regions", clean);
	holdStart();
	checkTrue("START: CENTRE on both", fakePin(0).level == 1500 && fakePin(1).level == 1500);
	checkControlTable("SERVO, two servos with the selector");
	checkInt("AB by default", s_mask, SERVO_MASK_AB);
	dialDrag(45, 45);
	checkTrue("AB with REVERSE B: B mirrors A", fakePin(0).level == 1750 && fakePin(1).level == 1250);
	fakeDumpFrame("shot_servo_two.ppm");
	tapId(UI_FIX(ServoCtl::SelA));
	dialDrag(-90, -90);
	checkTrue("A: A follows, B holds", fakePin(0).level == 1000 && fakePin(1).level == 1250);
	tapId(UI_FIX(ServoCtl::SelB));
	tapId(UI_FIX(ServoCtl::Max)); core1();
	checkTrue("B: B follows unmirrored, A holds", fakePin(0).level == 1000 && fakePin(1).level == 2000);
	tapId(UI_FIX(ServoCtl::SelAB));
	tapId(UI_FIX(ServoCtl::Min)); core1();
	checkTrue("AB: MIN mirrors B to MAX", fakePin(0).level == 1000 && fakePin(1).level == 2000);
	pressId(UI_FIX(ServoCtl::Start)); core1();
	fakeRelease(); uiFrames(1);
	checkTrue("STOP stops both", fakePin(0).level == 0 && fakePin(1).level == 0);
	// B selected, then SERVOS ONE: the selection no longer applies, and the
	// one servo on SIG follows the dial.
	tapId(UI_FIX(ServoCtl::SelB));
	openSet();
	uiFrames(1);
	selectRow(ROW_SERVOS);
	stepSel(-1);
	checkTrue("SERVOS ONE", s_profile.servos == 1);
	backFromSet();
	holdStart();
	tapId(UI_FIX(ServoCtl::Max)); core1();
	checkInt("ONE after B selected: MAX moves A", fakePin(0).level, 2000);
	leaveServo();
}

static bool textAt(int x, int y, const char *text, uint16_t col, int scale);

/** @brief Whether any pixel of the rectangle is @p col. */
static bool someOf(int x, int y, int w, int h, uint16_t c) {
	for (int j = y; j < y + h; j++)
		for (int i = x; i < x + w; i++)
			if (pixel(i, j) == c) return true;
	return false;
}

/** @brief Whether SET row @p row's value region, scale 2 or 1, holds @p c. */
static bool setValueHas(int row, uint16_t c) {
	UiRect r = ctlRect(UI_ROW(row, Row));
	return someOf(100, r.y + 13, 132, 14, c);
}

/**
 * @brief Whether SET row @p row shows its value as a setting as it stands,
 *        `C_CYAN` text on the row, or as a waiting one, `C_ONACCENT` on its
 *        `C_CYAN` rectangle; @p selected names the row's fill.
 */
static bool setValueShows(int row, bool waiting, bool selected) {
	char v[24];
	valueText(row, v, sizeof(v));
	UiRect r = ctlRect(UI_ROW(row, Row));
	int sc = gfxTextW(v, 2) <= UI_FIELD_VALUE_W2 ? 2 : 1;
	int x = GFX_W - UI_MARGIN - gfxTextW(v, sc), y = r.y + (SET_ROW_H - 1 - 7 * sc + 1) / 2;
	uint16_t ground = selected ? C_PANEL : C_BG;
	return waiting ? fakeChangedIs(x, y, v, ground, sc) : fakeTextIs(x, y, v, C_CYAN, ground, sc);
}

/** @brief Whether SET's editor bar shows the selected row's value, as setValueShows(). */
static bool setBarShows(bool waiting) {
	char v[24];
	valueText(s_sel, v, sizeof(v));
	int cx = BTN_MINUS.x + BTN_MINUS.w, cw = BTN_PLUS.x - cx;
	int sc = gfxTextW(v, 2) <= cw ? 2 : 1;
	int x = cx + (cw - gfxTextW(v, sc)) / 2, y = SET_EDIT_Y + 16;
	return waiting ? fakeChangedIs(x, y, v, C_BG, sc) : fakeTextIs(x, y, v, C_CYAN, C_BG, sc);
}

/**
 * @brief The colour roles on SERVO TESTER and SERVO SET (docs/design/menu-b.md,
 *        decisions 8 and 11, as docs/design/ui-theme.md revises them):
 *        command labels in the text colour, SPEED's fill `C_GRID`, servo B
 *        blue; a setting's value cyan text; a waiting row dark on a cyan
 *        rectangle with a cyan bar until HOLD TO APPLY, in the list and in
 *        the editor bar; a selected row's value never lime, the PULSE rows
 *        faint while RUNNING.
 */
static void testColourRoles() {
	section("Servo screen: the colour roles");
	freshUi();
	openServo();
	uiFrames(1);
	checkTrue("SET's label in the text colour",
	          fakeTextIs(BTN_SET.x + (BTN_SET.w - gfxTextW("SET", 1)) / 2, BTN_SET.y + (BTN_SET.h - 7) / 2,
	                     "SET", C_TEXT, C_PANEL, 1));
	UiRect sr = uiSliderPaintedRect(&s_speed);
	// SPEED opens at 100 %: the fill runs the track's length, and the thumb
	// stands at its right end.
	const UiRect &tr = s_speed.track;
	checkTrue("SPEED's fill is C_GRID, inside the track at its left end and its middle",
	          (int)s_speed.value == 100 && pixel(tr.x + 3, tr.y + tr.h / 2) == C_GRID &&
	          pixel(tr.x + tr.w / 2, tr.y + tr.h / 2) == C_GRID);
	checkTrue("no C_DIM and no cyan in the slider: the fill is under the readout",
	          !someOf(sr.x, sr.y, sr.w, sr.h, C_DIM) && !someOf(sr.x, sr.y, sr.w, sr.h, C_CYAN));
	checkTrue("the thumb in the text colour", someOf(sr.x, sr.y, sr.w, sr.h, C_TEXT));

	openSet();
	selectRow(1);   // TYPE
	uiFrames(1);
	UiRect r = ctlRect(UI_ROW(1, Row));
	checkTrue("TYPE selected, as applied: a setting's value, cyan text and no rectangle; never lime or the text colour; no bar",
	          setValueShows(1, false, true) && !setValueHas(1, C_TEXT) && !setValueHas(1, C_LIME) &&
	          !setValueHas(1, C_ONACCENT) && pixel(1, r.y + 5) == C_PANEL);
	checkTrue("RATE under it, not selected: cyan text on C_BG", setValueShows(2, false, false));
	checkTrue("the editor bar's value cyan text, no rectangle",
	          setBarShows(false) && !someOf(52, SET_EDIT_Y + 14, 136, 18, C_ONACCENT) &&
	          !someOf(52, SET_EDIT_Y + 16, 136, 14, C_LIME));
	stepSel(+1, 3);
	uiFrames(1);
	r = ctlRect(UI_ROW(1, Row));
	checkTrue("HELI CYCLIC pending: the value dark on its cyan rectangle, a 3 px cyan bar",
	          s_pending && setValueShows(1, true, true) && !setValueHas(1, C_TEXT) &&
	          pixel(0, r.y + 5) == C_CYAN && pixel(2, r.y + 5) == C_CYAN && pixel(3, r.y + 5) == C_PANEL);
	// HELI CYCLIC is the widest value at scale 2, 130 px of the bar's 140:
	// its rectangle is 136 px wide and ends 2 px before - and +.
	checkTrue("the editor bar's value dark on a cyan rectangle too, 2 px clear of - and +",
	          setBarShows(true) && !someOf(BTN_MINUS.x + BTN_MINUS.w, SET_EDIT_Y + 12, 2, 22, C_CYAN) &&
	          !someOf(BTN_PLUS.x - 2, SET_EDIT_Y + 12, 2, 22, C_CYAN) &&
	          pixel(BTN_MINUS.x + BTN_MINUS.w + 2, SET_EDIT_Y + 14) == C_CYAN);
	UiRect rr = ctlRect(UI_ROW(2, Row));
	checkTrue("RATE, loaded with the type, pending too: its rectangle and its bar, not selected",
	          setValueShows(2, true, false) && pixel(1, rr.y + 5) == C_CYAN);
	{
		int walked = fakeChangedWalked();
		checkControlTable("SERVO SET, two PULSE rows waiting");
		checkTrue("the walk met the changed value", fakeChangedWalked() > walked);
	}
	holdApply();
	uiFrames(1);
	r = ctlRect(UI_ROW(1, Row));
	checkTrue("applied: the value cyan text again, the rectangle and the bar gone",
	          !s_pending && setValueShows(1, false, true) && !setValueHas(1, C_ONACCENT) &&
	          pixel(1, r.y + 5) == C_PANEL);
	checkTrue("and the editor bar's value", setBarShows(false) && !someOf(52, SET_EDIT_Y + 14, 136, 18, C_ONACCENT));

	// One row waiting among rows as they stand: where a waiting value and a
	// setting's cyan text share the list.
	selectRow(2);   // RATE
	stepSel(ctlEnabled(UI_FIX(ServoCtl::EditPlus)) ? +1 : -1, 1);
	uiFrames(1);
	checkTrue("RATE alone waits: its rectangle and its bar",
	          s_pending && setValue(2) == UiValue::Changed && setValue(3) == UiValue::Setting &&
	          setValueShows(2, true, true) && pixel(1, ctlRect(UI_ROW(2, Row)).y + 5) == C_CYAN);
	checkTrue("MIN under it, as it stands: cyan text with no rectangle and no bar",
	          setValueShows(3, false, false) && !setValueHas(3, C_ONACCENT) &&
	          pixel(1, ctlRect(UI_ROW(3, Row)).y + 5) == C_BG);
	{
		int walked = fakeChangedWalked();
		checkControlTable("SERVO SET, RATE waiting");
		checkTrue("the walk met the changed value", fakeChangedWalked() > walked);
	}
	holdApply();
	uiFrames(1);
	checkTrue("applied: nothing waits", !s_pending && setValueShows(2, false, true));

	// Every PULSE row can wait: the widest value of each beside its label.
	{
		int worst = 1000;
		char worstAt[48] = "";
		auto gapOf = [&](const char *label, const char *v) {
			int sc = gfxTextW(v, 2) <= UI_FIELD_VALUE_W2 ? 2 : 1;
			int gap = (GFX_W - UI_MARGIN - gfxTextW(v, sc) - UI_FIELD_FILL_DX) - (UI_MARGIN + gfxTextW(label, 1));
			if (gap < worst) {
				worst = gap;
				snprintf(worstAt, sizeof(worstAt), "%s %s", label, v);
			}
		};
		gapOf("SERVOS", "ONE"); gapOf("SERVOS", "TWO");
		for (int t = 0; t < SERVO_TYPE_COUNT; t++) gapOf("TYPE", servoTypeInfo((ServoType)t)->name);
		gapOf("RATE", "333 HZ"); gapOf("MIN", "2500 US"); gapOf("CENTRE", "2500 US"); gapOf("MAX", "2500 US");
		gapOf("REVERSE B", "OFF");
		printf("  the narrowest: %s\n", worstAt);
		checkTrue("a waiting value's rectangle starts 4 px or more after its label on every PULSE row",
		          worst >= 4);
		// The editor bar: 140 px between - and +, and the rectangle 6 px
		// wider than the value.
		int cw = BTN_PLUS.x - (BTN_MINUS.x + BTN_MINUS.w);
		bool fits = cw == 140;
		for (int t = 0; t < SERVO_TYPE_COUNT; t++) {
			const char *v = servoTypeInfo((ServoType)t)->name;
			int sc = gfxTextW(v, 2) <= cw ? 2 : 1;
			if (gfxTextW(v, sc) + 2 * UI_FIELD_FILL_DX > cw - 4) fits = false;   // 2 px at each side
		}
		checkTrue("the bar's rectangle fits the 140 px between - and + for every type's name", fits);
	}
	backFromSet();
	leaveServo();
	backFromMenu();

	freshUi();
	openServo();
	openSet();
	selectRow(ROW_SERVOS);
	stepSel(+1);
	backFromSet();
	holdStart();
	core1();
	uiFrames(2);
	checkTrue("two servos running: the B label blue",
	          s_profile.servos == 2 && textAt(UI_MARGIN + 50, SV_READ_Y + 3, "B", C_BLUE, 1));
	checkTrue("MIN's label in the text colour while running",
	          fakeTextIs(BTN_MIN.x + (BTN_MIN.w - gfxTextW("MIN", 1)) / 2, BTN_MIN.y + (BTN_MIN.h - 7) / 2,
	                     "MIN", C_TEXT, C_PANEL, 1));
	tapId(UI_FIX(ServoCtl::SelB));
	uiFrames(1);
	checkTrue("B selected: its selector fills blue", fillOf(BTN_SEL_B) == C_BLUE);
	tapId(UI_FIX(ServoCtl::Max)); core1();
	uiFrames(2);
	checkTrue("B at MAX, apart from A: servo B's horn blue on the dial",
	          fakePin(1).level == 2000 && someOf(0, SERVO_DIAL_Y0, GFX_W, 120, C_BLUE));
	tapId(UI_FIX(ServoCtl::SelA));
	uiFrames(2);
	checkTrue("A selected: servo B's thin horn still blue",
	          s_mask == SERVO_MASK_A &&
	          someOf(0, SERVO_DIAL_Y0, GFX_W, SERVO_DIAL_Y1 - SERVO_DIAL_Y0 + 1, C_BLUE));
	checkTrue("SWEEP's label in the text colour while running",
	          fakeTextIs(BTN_SWEEP.x + (BTN_SWEEP.w - gfxTextW("SWEEP", 1)) / 2,
	                     BTN_SWEEP.y + (BTN_SWEEP.h - 7) / 2, "SWEEP", C_TEXT, C_PANEL, 1));
	tapId(UI_FIX(ServoCtl::Sweep)); core1();
	uiFrames(24); core1();
	uiFrames(1);
	char sw[24];
	sweepText(sw, sizeof(sw));
	checkTrue("two servos sweeping: the count under the letters, lime",
	          sw[0] && textAt(UI_MARGIN, SV_READ_Y + 14, sw, C_LIME, 1));
	// STOP SWEEP, 59 px, in a button 69 px wide: the label centred, 5 px of
	// fill between it and the frame at each side, and the frame whole.
	const int swW = gfxTextW("STOP SWEEP", 1), swX = BTN_SWEEP.x + (BTN_SWEEP.w - swW) / 2;
	const int swY = BTN_SWEEP.y + (BTN_SWEEP.h - 7) / 2;
	checkTrue("sweeping: the button reads STOP SWEEP, centred",
	          fakeTextIs(swX, swY, "STOP SWEEP", C_TEXT, C_PANEL, 1));
	checkTrue("its label starts and ends 4 px or more inside the button",
	          swX - BTN_SWEEP.x >= 4 && BTN_SWEEP.x + BTN_SWEEP.w - (swX + swW) >= 4);
	checkTrue("the frame beside the label is whole",
	          allOf(BTN_SWEEP.x, swY - 2, 1, 11, C_GRID) &&
	          allOf(BTN_SWEEP.x + BTN_SWEEP.w - 1, swY - 2, 1, 11, C_GRID));
	checkTrue("and 4 px of fill lie between the frame and the label",
	          allOf(BTN_SWEEP.x + 1, swY - 2, 4, 11, C_PANEL) &&
	          allOf(BTN_SWEEP.x + BTN_SWEEP.w - 5, swY - 2, 4, 11, C_PANEL));
	checkTrue("no pixel of the row lies past x 231", allOf(GFX_W - UI_MARGIN, SV_BTN_Y, UI_MARGIN, SV_BTN_H, C_BG));
	tapId(UI_FIX(ServoCtl::Sweep)); core1();
	openSet();
	uiFrames(1);
	checkTrue("RUNNING locks the PULSE rows: TYPE's value grey",
	          setValueHas(1, C_GRID) && !setValueHas(1, C_TEXT));
	selectRow(1);
	uiFrames(1);
	checkTrue("and TYPE selected while RUNNING: the editor bar's value grey",
	          someOf(52, SET_EDIT_Y + 16, 136, 14, C_GRID) &&
	          !someOf(52, SET_EDIT_Y + 16, 136, 14, C_TEXT));
	backFromSet();
	pressId(UI_FIX(ServoCtl::Start)); core1();
	fakeRelease(); uiFrames(1);
	openSet();
	selectRow(ROW_SERVOS);
	stepSel(-1);
	backFromSet();
	leaveServo();
	backFromMenu();

	// One servo, no sensor: the count sits right-aligned on the readout row.
	freshUi();
	openServo();
	holdStart();
	tapId(UI_FIX(ServoCtl::Sweep)); core1();
	uiFrames(24); core1();
	uiFrames(1);
	sweepText(sw, sizeof(sw));
	checkTrue("one servo sweeping: the count right on the readout row, lime",
	          sw[0] && textAt(GFX_W - UI_MARGIN - gfxTextW(sw, 1), SV_READ_Y + 9, sw, C_LIME, 1));
	tapId(UI_FIX(ServoCtl::Sweep)); core1();
	pressId(UI_FIX(ServoCtl::Start)); core1();
	fakeRelease(); uiFrames(1);
	leaveServo();
}

/**
 * @brief SamePin and KissLive keep the screen to one servo, through the claim.
 */
static void testAuxRefused() {
	section("Servo screen: TWO is refused when the claim refuses AUX");

	freshUi();
	escTaskConfigure(1, 600, false, 1);   // SIG and AUX on one GPIO, which SETTINGS no longer allows
	uiFrames(2);
	openServo();
	checkTrue("one GPIO for both: the screen claims SIG alone",
	          pinClaimState() == ClaimState::Running && pinClaimGpio(PIN_AUX) == PIN_CLAIM_NO_GPIO);
	checkTrue("and keeps why", s_auxRefusal == ClaimRefusal::SamePin);
	openSet();
	selectRow(ROW_SERVOS);
	checkTrue("SERVOS + is drawn disabled", fillOf(BTN_PLUS) == C_BG);
	stepSel(+1, 1, IdExpect::Disabled);
	checkTrue("and does not act", s_profile.servos == 1 && s_edit.servos == 1);
	const char *l1, *l2; uint16_t col;
	setNote(&l1, &l2, &col);
	checkTrue("the note names the cause", l1 && strcmp(l1, "SIG AND AUX ARE THE SAME GPIO.") == 0);
	checkTrue("and the rows that set them", l2 && strcmp(l2, "SET SIG PIN OR AUX PIN IN SETTINGS.") == 0);
	fakeDumpFrame("shot_servo_samepin.ppm");
	selectRow(3);
	setNote(&l1, &l2, &col);
	checkTrue("another row selected: the note says TWO is not offered",
	          l1 && strstr(l1, "NOT OFFERED") != nullptr);
	backFromSet();
	leaveServo();
	backFromMenu();
	uiFrames(1);

	freshUi();
	EscTelemetry t;
	memset(&t, 0, sizeof(t));
	t.kissLastMs = millis();
	t.haveKiss = true;
	fakeSetTelemetry(&t);
	openServo();
	checkTrue("KISS live on AUX: SIG alone", pinClaimGpio(PIN_AUX) == PIN_CLAIM_NO_GPIO &&
	                                         s_auxRefusal == ClaimRefusal::KissLive);
	openSet();
	selectRow(ROW_SERVOS);
	setNote(&l1, &l2, &col);
	checkTrue("the note names the KISS wire", l1 && strcmp(l1, "KISS WIRE ON AUX GP1 IS LIVE.") == 0 &&
	                                          l2 && strcmp(l2, "TWO SERVOS NEED IT UNPLUGGED.") == 0);
	stepSel(+1, 1, IdExpect::Disabled);
	checkTrue("SERVOS stays ONE", s_profile.servos == 1);
	backFromSet();
	uiFrames(2);
	checkTrue("AUX refused: no two-servo hint", !twoHintShown() && twoHintBlank());
	leaveServo();
	noEsc();
}

/**
 * @brief An ESC on SIG: no claim, the message, and the claim 500 ms after it
 *        goes quiet.
 */
static void testEscAnswering() {
	section("Servo screen: an ESC answering on SIG holds the claim off");

	freshUi();
	EscTelemetry t;
	memset(&t, 0, sizeof(t));
	t.lastRpmMs = millis();   // the last eRPM frame, at this instant
	fakeSetTelemetry(&t);
	openServo();
	checkTrue("no claim while it answers", !s_claimed && pinClaimHolder() == PinUser::None);
	checkTrue("the reason", s_refusal == ClaimRefusal::EscAnswering);
	checkTrue("HOLD TO START is an outline", fillOf(BTN_START) == C_BG);
	checkControlTable("SERVO, refused: an ESC answers");
	fakeDumpFrame("shot_servo_esc.ppm");
	int x, y;
	ctlCentre(UI_FIX(ServoCtl::Start), &x, &y);
	pressId(UI_FIX(ServoCtl::Start), IdExpect::Disabled);
	fakeHold(x, y); uiFrames(4);
	checkTrue("and a hold on it starts no hold", !s_hold.holding);
	fakeRelease(); uiFrames(1);
	uint32_t since = millis() - t.lastRpmMs;
	checkTrue("499 ms after the last frame: still no claim", since < 499 && !s_claimed);
	frameAt(499 - since);
	checkTrue("at 499 ms: none", !s_claimed);
	frameAt(1);
	checkTrue("500 ms quiet: the screen claims by itself", s_claimed);
	uiFrames(3);
	checkTrue("and runs, SAFE", pinClaimState() == ClaimState::Running && !s_st.running);
	leaveServo();
}

/**
 * @brief The leave page (docs/design/menu-b.md, decision 5): it opens
 *        releasing, BACK is drawn disabled in every phase, BACK TO SERVO
 *        keeps the claim once the job has let go, and RESTART DSHOT takes a
 *        press that lands 1000 ms or more after that.
 */
static void testLeavePage() {
	section("Servo screen: the leave page before DShot restarts");

	freshUi();
	openServo();
	holdStart();
	int resumes = fakeResumeCount();
	int cuts = fakePin(0).cutPulses;
	tapId(UI_FIX(UI_BACK));
	checkTrue("BACK while RUNNING: the page waits for the last pulse",
	          s_view == View::Leave && !pinClaimUnplugDue() &&
	          core1JobPhase() == Core1JobPhase::Stopping);
	checkTrue("releasing: BACK, BACK TO SERVO and RESTART DSHOT disabled",
	          ctlPresent(UI_FIX(UI_BACK)) && !ctlEnabled(UI_FIX(UI_BACK)) &&
	          ctlPresent(UI_FIX(ServoCtl::BackToServo)) && !ctlEnabled(UI_FIX(ServoCtl::BackToServo)) &&
	          ctlPresent(UI_FIX(ServoCtl::Restart)) && !ctlEnabled(UI_FIX(ServoCtl::Restart)));
	checkControlTable("the leave page, the job not yet let go");
	checkTrue("and BACK is drawn so: an outline, its label grey",
	          backLook() == BackLook::Disabled);
	checkTrue("no step before LEAVE: UNPLUG THE SERVO. is step 1, current",
	          leaveStepIs(UI_LEAVE_STEP_Y, '1', "UNPLUG THE SERVO.", true) &&
	          leaveStepIs(UI_LEAVE_STEP_Y + 12, 0, "DSHOT RESTARTS ON SIG GP0 AND READS", true) &&
	          leaveStepIs(UI_LEAVE_STEP_Y + 24, 0, "AS A 970 US PULSE AT 1000 HZ.", true) &&
	          leaveStepIs(UI_LEAVE_STEP_Y + 38, '2', "RESTART DSHOT.", false));
	checkTrue("the note: RELEASING THE PINS, dim", leaveNoteIs(0, "RELEASING THE PINS", C_DIM));
	checkTrue("the buttons drawn disabled", leaveLeftIs("BACK TO SERVO", false) &&
	          leaveWideIs("RESTART DSHOT", false));
	checkTrue("the strip names both pins", fakeTextIs(UI_MARGIN, UI_STRIP_Y + 4, "SIG GP0  AUX GP1",
	                                                  C_DIM, C_BG, 1));
	core1();
	checkTrue("core1's next pass: still stopping", core1JobPhase() == Core1JobPhase::Stopping);
	checkTrue("level 0 written, SIG still on PWM",
	          fakePin(0).level == 0 && fakePin(0).function == GPIO_FUNC_PWM);
	checkInt("and no DShot", fakeResumeCount(), resumes);
	// BACK on the page does nothing in any phase. The frames are 5 ms apart
	// here, so the press is by hand, at BACK's centre.
	int bx, by;
	ctlCentre(UI_FIX(UI_BACK), &bx, &by);
	fakePress(bx, by); frameAt(5);
	checkTrue("a press on it draws no pressed look", backLook() == BackLook::Disabled);
	fakeRelease(); frameAt(5);
	checkTrue("BACK while releasing: ignored, the page stays",
	          s_view == View::Leave && pinClaimState() == ClaimState::Leaving &&
	          !pinClaimUnplugDue());
	uiFrames(2);
	checkTrue("the job let go: released", s_view == View::Leave && pinClaimUnplugDue());
	uint32_t t0 = millis();
	checkTrue("BACK TO SERVO live, RESTART DSHOT still waits, BACK still disabled",
	          ctlEnabled(UI_FIX(ServoCtl::BackToServo)) && !ctlEnabled(UI_FIX(ServoCtl::Restart)) &&
	          !ctlEnabled(UI_FIX(UI_BACK)));
	checkControlTable("the leave page, the job let go");
	checkTrue("BACK drawn disabled, BACK TO SERVO live, RESTART DSHOT not",
	          backLook() == BackLook::Disabled && leaveLeftIs("BACK TO SERVO", true) &&
	          leaveWideIs("RESTART DSHOT", false) && leaveNoteBlank(0));
	checkTrue("step 1 stays current", leaveStepIs(UI_LEAVE_STEP_Y, '1', "UNPLUG THE SERVO.", true));
	checkTrue("the page keeps the title SERVO TESTER",
	          fakeTextIs(UI_MARGIN, (UI_HDR_H - 14) / 2, "SERVO TESTER", C_TEXT, C_PANEL, 2) &&
	          titleEndsClear());
	checkInt("no pulse was cut short", fakePin(0).cutPulses, cuts);
	checkTrue("the job let go: SIG an input", fakePin(0).function == GPIO_FUNC_SIO);
	checkInt("DShot not yet restarted", fakeResumeCount(), resumes);
	tapId(UI_FIX(ServoCtl::Restart), IdExpect::Disabled);
	checkTrue("a tap in the wait restarts nothing", fakeResumeCount() == resumes &&
	          s_view == View::Leave);
	tapId(UI_FIX(UI_BACK), IdExpect::Disabled);
	checkTrue("BACK once released: nothing", s_view == View::Leave &&
	          pinClaimState() == ClaimState::Leaving);
	for (int i = 0; i < 60 && !ctlEnabled(UI_FIX(ServoCtl::Restart)); i++) uiFrames(1);
	checkTrue("RESTART DSHOT live 1000 ms after the release",
	          ctlEnabled(UI_FIX(ServoCtl::Restart)) && millis() - t0 >= 1000 && millis() - t0 < 1025);
	uiFrames(1);
	checkTrue("and drawn live", leaveWideIs("RESTART DSHOT", true));
	fakeDumpFrame("shot_leave_servo.ppm");
	tapId(UI_FIX(ServoCtl::BackToServo));
	uiFrames(2);
	checkTrue("BACK TO SERVO: SERVO again, the claim kept",
	          s_view == View::Main && pinClaimState() == ClaimState::Running);
	checkTrue("SAFE again, rebound at level 0",
	          !s_st.running && fakePin(0).function == GPIO_FUNC_PWM && fakePin(0).level == 0);
	checkInt("still no DShot", fakeResumeCount(), resumes);
	checkTrue("BACK live again on SERVO TESTER", ctlEnabled(UI_FIX(UI_BACK)) &&
	          backLook() == BackLook::Live);

	// SAFE: the job lets go within a frame, so a tap in the wide button
	// right after BACK lands in the released phase, inside the wait.
	tapId(UI_FIX(UI_BACK));
	checkTrue("BACK while SAFE: the leave page", s_view == View::Leave);
	tapId(UI_FIX(ServoCtl::Restart), IdExpect::Disabled);
	checkTrue("a tap on RESTART DSHOT 75 ms after BACK: nothing", s_view == View::Leave &&
	          fakeResumeCount() == resumes && pinClaimState() == ClaimState::Leaving);
	for (int i = 0; i < 60 && !ctlEnabled(UI_FIX(ServoCtl::Restart)); i++) uiFrames(1);
	tapId(UI_FIX(ServoCtl::Restart));
	checkInt("RESTART DSHOT: DShot back", fakeResumeCount(), resumes + 1);
	uiFrames(2);
	checkTrue("free, and SERVO, the hub, up, its entry live",
	          pinClaimState() == ClaimState::Free && navIsHub(UiScreenId::ServoHub) &&
	          fillOf(ctlRect(HUB_ENTRY(0))) == C_PANEL);
	tapId(UI_FIX(UI_BACK));
	uiFrames(1);
	backFromMenu();
}

/**
 * @brief Decision 7 of docs/design/menu-b.md on SERVO SET: with a change
 *        waiting for HOLD TO APPLY the first BACK arms the guard, whose two
 *        amber lines name the count; a press elsewhere disarms it; the second
 *        BACK returns and drops the change; with nothing waiting BACK
 *        returns at once.
 */
static void testSetGuard() {
	section("Servo SET: BACK on a change not applied arms the guard");
	freshUi();
	openServo();
	openSet();
	selectRow(2);   // RATE
	stepSel(+1);
	checkTrue("60 Hz: applied at once", !s_pending && s_profile.rateHz == 60);
	tapId(UI_FIX(UI_BACK));
	checkTrue("nothing waits: one BACK returns", s_view == View::Main && !s_guard.armed);
	openSet();
	selectRow(2);
	stepSel(+1);
	checkTrue("100 Hz waits for HOLD TO APPLY", s_pending && s_edit.rateHz == 100 && s_profile.rateHz == 60);
	uint16_t top = fakeSlice(0).top;
	tapId(UI_FIX(UI_BACK));
	checkTrue("the first BACK: SET stays, the guard armed", s_view == View::Set && s_guard.armed);
	checkTrue("  1 CHANGE NOT APPLIED. / BACK AGAIN DISCARDS IT., amber",
	          fakeTextIs(UI_MARGIN, UI_STRIP_LINE1_Y, "1 CHANGE NOT APPLIED.", UI_COL_WARN, C_BG, 1) &&
	          fakeTextIs(UI_MARGIN, UI_STRIP_LINE2_Y, "BACK AGAIN DISCARDS IT.", UI_COL_WARN, C_BG, 1));
	checkInt("21 characters, 125 px", gfxTextW("1 CHANGE NOT APPLIED.", 1), 125);
	checkControlTable("SET, the guard armed");
	tapId(UI_ROW(2, Row));
	checkTrue("a press on a row disarms it: the warning returns", !s_guard.armed &&
	          fakeTextIs(UI_MARGIN, UI_STRIP_Y + 2, "ABOVE 60 HZ AN ANALOGUE", C_AMBER, C_BG, 1) &&
	          s_pending && s_view == View::Set);
	tapId(UI_FIX(UI_BACK));
	checkTrue("BACK arms it again", s_guard.armed && s_view == View::Set);
	tapId(UI_FIX(UI_BACK));
	checkTrue("the second BACK: SERVO, the change dropped, the pins as applied",
	          s_view == View::Main && !s_pending && s_edit.rateHz == 60 && s_profile.rateHz == 60 &&
	          fakeSlice(0).top == top);

	openSet();
	selectRow(1);   // TYPE
	stepSel(+1, 3);
	int n = setPending();
	checkTrue("HELI CYCLIC waits, more than one row changed", s_pending && n >= 2);
	tapId(UI_FIX(UI_BACK));
	char l1[40];
	snprintf(l1, sizeof(l1), "%d CHANGES NOT APPLIED.", n);
	checkTrue("several: n CHANGES NOT APPLIED. / BACK AGAIN DISCARDS THEM.",
	          s_guard.armed && fakeTextIs(UI_MARGIN, UI_STRIP_LINE1_Y, l1, UI_COL_WARN, C_BG, 1) &&
	          fakeTextIs(UI_MARGIN, UI_STRIP_LINE2_Y, "BACK AGAIN DISCARDS THEM.", UI_COL_WARN, C_BG, 1));
	holdApply();
	checkTrue("HOLD TO APPLY disarms it and applies", !s_guard.armed && !s_pending &&
	          s_profile.type == ServoType::HeliCyclic);
	tapId(UI_FIX(UI_BACK));
	checkTrue("nothing waits: one BACK returns", s_view == View::Main);
	leaveServo();
	backFromMenu();
}

/**
 * @brief SET: the warning, HOLD TO APPLY, the SAFE gate, sweep rows, DEFAULTS.
 */
static void testSetScreen() {
	section("Servo SET: PULSE rows while SAFE, the warning and HOLD TO APPLY");

	freshUi();
	openServo();
	openSet();
	selectRow(1);   // TYPE
	stepSel(+1);
	checkTrue("NARROW 760 at 50 Hz: applied at once",
	          s_profile.type == ServoType::Narrow760 && !s_pending);
	stepSel(+1, 2);
	checkTrue("WIDE, then HELI CYCLIC: held as pending, NARROW 760 still applied",
	          s_edit.type == ServoType::HeliCyclic && s_pending &&
	          s_profile.type == ServoType::Narrow760);
	checkInt("the pins still at 50 Hz", fakeSlice(0).top, 19999);
	const char *l1, *l2; uint16_t col;
	setNote(&l1, &l2, &col);
	checkTrue("the note names the heli risk", l1 && strstr(l1, "HELI PROFILE") != nullptr);
	checkTrue("HOLD TO APPLY is live", fillOf(BTN_APPLY) == C_PANEL);
	fakeDumpFrame("shot_servo_set_warning.ppm");
	int x, y;
	ctlCentre(UI_FIX(ServoCtl::Apply), &x, &y);
	// A hold cut short starts again from zero at the next press.
	pressId(UI_FIX(ServoCtl::Apply));
	fakeHold(x, y); frameAt(500);
	fakeRelease(); uiFrames(1);
	frameAt(1000);
	pressId(UI_FIX(ServoCtl::Apply));
	checkTrue("held 500 ms, lifted, pressed 1 s later: still pending",
	          s_pending && s_profile.type == ServoType::Narrow760 && fakeSlice(0).top == 19999);
	fakeRelease(); uiFrames(1);
	pressId(UI_FIX(ServoCtl::Apply));
	fakeHold(x, y); frameAt(UI_HOLD_MS - 1); core1();
	checkTrue("held 999 ms: not applied", s_pending && fakeSlice(0).top == 19999);
	checkTrue("the bar shows the hold, the button red",
	          pixel(BTN_APPLY.x + 2, SET_FOOT_Y - 3) == C_LIME && fillOf(BTN_APPLY) == C_RED);
	fakeHold(x, y); frameAt(1); core1();
	checkTrue("1000 ms: applied", !s_pending && s_profile.type == ServoType::HeliCyclic);
	checkInt("rebound at 333 Hz", fakeSlice(0).top, 3002);
	fakeRelease(); uiFrames(1);
	checkTrue("HELI CYCLIC's 820 us to 2220 us are past the travel: one hold applied both",
	          servoPastTravel(&s_profile) && s_profile.minUs == 820 && s_profile.maxUs == 2220);
	selectRow(3);   // MIN, under the applied heli profile
	setNote(&l1, &l2, &col);
	checkTrue("applied: the end-stop note, not the heli warning",
	          !s_pending && l1 && strcmp(l1, "PAST ITS TRAVEL A SERVO STALLS AND") == 0);
	stepSel(-1);
	checkTrue("MIN 810 us under an applied heli profile: further out, pending",
	          s_pending && s_edit.minUs == 810 && s_profile.minUs == 820);
	setNote(&l1, &l2, &col);
	checkTrue("the warning is the end-stop note, not the heli one",
	          l1 && strcmp(l1, "PAST ITS TRAVEL A SERVO STALLS AND") == 0 && col == C_AMBER);
	stepSel(+1);
	checkTrue("back to 820 us: nothing pending", !s_pending && s_profile.minUs == 820);
	stepSel(+1);
	checkTrue("830 us, toward the centre: at once", !s_pending && s_profile.minUs == 830);
	selectRow(1);
	stepSel(-1);
	checkTrue("back to WIDE at 50 Hz: 800 us, further out, pending",
	          s_pending && s_edit.type == ServoType::Wide && s_profile.type == ServoType::HeliCyclic);
	holdApply();
	checkTrue("held: WIDE at 50 Hz", !s_pending && s_profile.type == ServoType::Wide &&
	                                 fakeSlice(0).top == 19999);

	selectRow(2);   // RATE
	stepSel(+1);
	checkTrue("60 Hz: at once", !s_pending && s_profile.rateHz == 60);
	stepSel(+1);
	checkTrue("100 Hz: pending", s_pending && s_profile.rateHz == 60 && s_edit.rateHz == 100);
	setNote(&l1, &l2, &col);
	checkTrue("the note names the overheating", l1 && strstr(l1, "ABOVE 60 HZ") != nullptr);
	backFromSet();
	checkTrue("BACK drops the pending change", !s_pending && s_edit.rateHz == 60);
	openSet();
	selectRow(3);   // MIN
	stepSel(-1, 3);
	checkTrue("MIN -30 us on WIDE: pending", s_pending && s_edit.minUs == 770 && s_profile.minUs == 800);
	holdApply();
	checkTrue("held: applied", !s_pending && s_profile.minUs == 770);
	int minY = rowCentreY(3);
	fakePress(200, minY); uiFrames(1);
	for (int px = 200; px >= 200 - UI_AXIS_LOCK_PX - 2 - UI_SWIPE_STEP_PX; px -= 4) {
		fakeHold(px, minY); uiFrames(1);
	}
	fakeRelease(); uiFrames(1);
	checkInt("a swipe left on MIN: one coarse step, -100 us", s_edit.minUs, 670);
	checkTrue("further out: pending", s_pending && s_profile.minUs == 770);
	holdApply();
	checkInt("held: applied", s_profile.minUs, 670);

	// While RUNNING the PULSE rows are locked; the SWEEP rows are not.
	backFromSet();
	holdStart();
	openSet();
	selectRow(3);
	checkControlTable("SET, RUNNING");
	checkTrue("RUNNING: MIN's - is an outline", fillOf(BTN_MINUS) == C_BG);
	stepSel(-1, 1, IdExpect::Disabled);
	checkTrue("and does nothing", s_profile.minUs == 670 && s_edit.minUs == 670);
	setNote(&l1, &l2, &col);
	checkTrue("the note says STOP first", l1 && strcmp(l1, "STOP TO CHANGE THE PROFILE") == 0);
	checkTrue("the footer shows STOP in place of DEFAULTS", fillOf(BTN_DEFAULTS) == C_RED);
	fakeDumpFrame("shot_servo_set_running.ppm");
	// A pending change while RUNNING is not reachable through the UI; forced
	// here, HOLD TO APPLY is drawn disabled and does not apply it.
	uint16_t rate = s_profile.rateHz;
	s_pending = true;
	s_edit.rateHz = 100;
	pressId(UI_FIX(ServoCtl::Apply), IdExpect::Disabled);
	for (int i = 0; i < 48; i++) { fakeHold(x, y); uiFrames(1); core1(); }
	checkTrue("forced pending while RUNNING: HOLD TO APPLY is an outline",
	          fillOf(BTN_APPLY) == C_BG);
	fakeRelease(); uiFrames(1);
	checkTrue("and held 1200 ms, does not apply", s_profile.rateHz == rate && s_pending);
	s_pending = false;
	s_edit = s_profile;
	selectRow(9);   // AMPLITUDE
	stepSel(-1, 2);
	checkInt("AMPLITUDE -10 % while RUNNING", s_sweep.amplitude, 450);
	selectRow(11);  // MOVES
	stepSel(+1, 3);
	checkInt("MOVES 3", s_sweep.moves, 3);
	backFromSet();
	tapId(UI_FIX(ServoCtl::Sweep)); core1();
	servoJobStatus(&s_st);
	checkTrue("SWEEP takes the new rows", s_st.sweeping);
	openSet();
	int fx, fy;
	ctlCentre(UI_FIX(ServoCtl::Defaults), &fx, &fy);
	pressId(UI_FIX(ServoCtl::Defaults)); core1();
	ServoStatus after;
	servoJobStatus(&after);
	checkTrue("STOP on SET acts on press: level 0 on core1's next pass",
	          fakePin(0).level == 0 && !after.running && !after.sweeping);
	fakeHold(fx, fy); uiFrames(2);
	fakeRelease(); uiFrames(1);
	checkTrue("its release is not a tap on DEFAULTS",
	          s_profile.type == ServoType::Wide && s_profile.minUs == 670 && s_sweep.moves == 3);
	checkTrue("SAFE: DEFAULTS is back", fillOf(BTN_DEFAULTS) == C_PANEL);
	tapId(UI_FIX(ServoCtl::Defaults));
	checkTrue("and a new tap in the same visit applies it",
	          s_profile.type == ServoType::Standard && s_profile.minUs == 1000 &&
	          s_sweep.moves == 0);
	backFromSet();

	openSet();
	selectRow(7);   // SHAPE, so the swipe below is the selection's change
	revealRow(8);
	int rateY = rowCentreY(8);
	fakePress(60, rateY); uiFrames(1);
	for (int px = 60; px <= 60 + UI_AXIS_LOCK_PX + 2 + UI_SWIPE_STEP_PX; px += 4) {
		fakeHold(px, rateY); uiFrames(1);
	}
	fakeRelease(); uiFrames(1);
	checkInt("a sideways swipe selects the row it began on", s_sel, 8);
	checkInt("and steps it coarse: +0.50 Hz", s_sweep.mhz, 1000);
	int scroll = s_list.scrollPx;
	fakePress(60, rateY); uiFrames(1);
	for (int py = rateY; py <= rateY + 40; py += 8) { fakeHold(60, py); uiFrames(1); }
	fakeRelease(); uiFrames(1);
	checkTrue("a vertical drag scrolls and edits nothing",
	          s_list.scrollPx < scroll && s_sweep.mhz == 1000 && s_sel == 8);
	tapId(UI_FIX(ServoCtl::Defaults));
	checkTrue("DEFAULTS: STANDARD PWM, 50 Hz, one servo, sine at 0.50 Hz",
	          s_profile.type == ServoType::Standard && s_profile.rateHz == 50 &&
	          s_profile.servos == 1 && s_sweep.kind == SWEEP_SINE && s_sweep.mhz == 500);
	// DWELL from 500 ms, 100 ms a step: a coarse step of 1000 ms down would
	// pass 0 and stops there. The anchor moves on by whole coarse steps, so
	// the way back starts 16 px past it.
	selectRow(10);
	int dwY = rowCentreY(10);
	fakePress(200, dwY); uiFrames(1);
	fakeHold(193, dwY); uiFrames(1);   // the lock: the anchor
	fakeHold(153, dwY); uiFrames(1);   // 40 px: two coarse steps, the anchor at 161
	checkInt("DWELL: two coarse steps down from 500 ms stop at 0 ms", s_sweep.dwell_ms, 0);
	fakeHold(170, dwY); uiFrames(1);
	checkInt("9 px back past the anchor: none", s_sweep.dwell_ms, 0);
	fakeHold(177, dwY); uiFrames(1);
	checkInt("16 px back: one coarse step up, 1000 ms", s_sweep.dwell_ms, 1000);
	fakeRelease(); uiFrames(1);
	tapId(UI_FIX(ServoCtl::Defaults));
	// A choice moves one value per swipe step, a number ten.
	swipeRow(1, +1);
	checkTrue("TYPE: one swipe step is one type", s_profile.type == ServoType::Narrow760);
	swipeRow(7, -1);
	checkTrue("SHAPE: to square", s_sweep.kind == SWEEP_SQUARE);
	swipeRow(7, +1);
	checkTrue("then one step back to sine", s_sweep.kind == SWEEP_SINE);
	backFromSet();
	UiRect speed = ctlRect(UI_FIX(ServoCtl::Speed));
	uiTap(speed.x, speed.y + 10);
	openSet();
	setNote(&l1, &l2, &col);
	checkTrue("SPEED 10 %: SPEED LIMITS THE SWEEP", l1 && strcmp(l1, "SPEED LIMITS THE SWEEP") == 0);
	backFromSet();
	leaveServo();
}

/** @brief Whether SET's note band shows @p l1 and @p l2 in @p col. */
static bool noteIs(const char *l1, const char *l2, uint16_t col) {
	const char *a, *b; uint16_t c;
	setNote(&a, &b, &c);
	auto same = [](const char *x, const char *y) {
		return (!x && !y) || (x && y && strcmp(x, y) == 0);
	};
	return same(a, l1) && same(b, l2) && (!l1 || c == col);
}

/**
 * @brief SET: the end-stop note for WIDE, MIN below 900 us, MAX above
 *        2100 us, and its place among the other notes.
 */
static void testTravelNote() {
	section("Servo SET: the end-stop note and its priority");
	static const char *const T1 = "PAST ITS TRAVEL A SERVO STALLS AND";
	static const char *const T2 = "HEATS. TRY MIN AND MAX HORN-FREE.";
	checkTrue("line 1 fits the band", UI_MARGIN + gfxTextW(T1, 1) <= GFX_W - UI_MARGIN);
	checkTrue("line 2 fits the band", UI_MARGIN + gfxTextW(T2, 1) <= GFX_W - UI_MARGIN);

	freshUi();
	openServo();
	openSet();
	tapId(UI_FIX(ServoCtl::Defaults));
	selectRow(1);   // TYPE
	checkTrue("STANDARD: no note", noteIs(nullptr, nullptr, 0));
	stepSel(+1, 2);
	checkTrue("WIDE at 50 Hz: past the travel, pending, NARROW 760 still applied",
	          s_edit.type == ServoType::Wide && s_pending && s_profile.type == ServoType::Narrow760);
	checkTrue("the pending warning is the end-stop note, amber", noteIs(T1, T2, C_AMBER));
	uiFrames(1);
	checkTrue("HOLD TO APPLY is live", fillOf(BTN_APPLY) == C_PANEL);
	fakeDumpFrame("shot_servo_set_travel.ppm");
	selectRow(7);   // SHAPE
	checkTrue("pending: the note on every row", noteIs(T1, T2, C_AMBER));
	selectRow(1);
	holdApply();
	checkTrue("held: WIDE applied", s_profile.type == ServoType::Wide && !s_pending);
	checkTrue("TYPE selected: the end-stop note stays, amber", noteIs(T1, T2, C_AMBER));
	uiFrames(1);
	checkTrue("drawn on the band's first line",
	          fakeTextIs(UI_MARGIN, UI_STRIP_Y + 2, T1, C_AMBER, C_BG, 1));
	checkTrue("and its second", fakeTextIs(UI_MARGIN, UI_STRIP_Y + 12, T2, C_AMBER, C_BG, 1));
	selectRow(7);   // SHAPE
	checkTrue("another row, no other note: the end-stop note", noteIs(T1, T2, C_AMBER));

	// The heli warning and HOLD TO APPLY come first.
	selectRow(1);
	stepSel(+1);
	checkTrue("HELI CYCLIC: pending", s_pending);
	checkTrue("the heli note, not the end-stop note",
	          noteIs("A HELI PROFILE CAN DESTROY", "A SERVO NOT MADE FOR IT.", C_AMBER));
	stepSel(-1);
	checkTrue("back to WIDE: nothing pending", !s_pending && s_edit.type == ServoType::Wide);
	backFromSet();
	openSet();
	checkTrue("WIDE applied, SET opened again: nothing pending, the end-stop note",
	          !s_pending && s_profile.type == ServoType::Wide && noteIs(T1, T2, C_AMBER));
	selectRow(ROW_SERVOS);
	stepSel(+1);
	checkTrue("TWO under an applied WIDE: pending, one servo still driven",
	          s_pending && s_edit.servos == 2 && s_profile.servos == 1 && noteIs(T1, T2, C_AMBER));
	holdApply();
	checkTrue("held: TWO on WIDE", !s_pending && s_profile.servos == 2);
	selectRow(5);   // MAX
	stepSel(+1);
	checkTrue("TWO servos, MAX 2210 us: pending for both", s_pending && s_profile.servos == 2 &&
	                                                     s_profile.maxUs == 2200 && s_edit.maxUs == 2210);
	stepSel(+1);
	checkTrue("a step further while pending: still pending", s_pending && s_edit.maxUs == 2220 &&
	                                                       s_profile.maxUs == 2200);
	holdApply();
	checkTrue("held: MAX 2220 us on both", !s_pending && s_profile.maxUs == 2220 && s_profile.servos == 2);
	stepSel(-1);
	checkTrue("MAX 2210 us, back toward the centre: at once", !s_pending && s_profile.maxUs == 2210);
	selectRow(ROW_SERVOS);
	stepSel(-1);
	checkTrue("TWO to ONE on WIDE: at once", !s_pending && s_profile.servos == 1);
	selectRow(5);
	stepSel(-1);
	checkTrue("ONE, MAX 2200 us", s_profile.servos == 1 && s_profile.maxUs == 2200);

	// A refused TWO keeps its cause on SERVOS and its pointer on the
	// rows that do not set the ends.
	ClaimRefusal was = s_auxRefusal;
	s_auxRefusal = ClaimRefusal::SamePin;
	selectRow(ROW_SERVOS);
	checkTrue("SERVOS: the cause of the refusal",
	          noteIs("SIG AND AUX ARE THE SAME GPIO.", "SET SIG PIN OR AUX PIN IN SETTINGS.", C_AMBER));
	selectRow(7);
	checkTrue("SHAPE: TWO SERVOS ARE NOT OFFERED",
	          noteIs("TWO SERVOS ARE NOT OFFERED: SEE SERVOS", nullptr, C_DIM));
	selectRow(5);   // MAX
	checkTrue("MAX: the end-stop note", noteIs(T1, T2, C_AMBER));
	s_auxRefusal = was;

	// SPEED LIMITS THE SWEEP keeps the SWEEP rows.
	backFromSet();
	UiRect speed = ctlRect(UI_FIX(ServoCtl::Speed));
	uiTap(speed.x, speed.y + 10);
	openSet();
	selectRow(10);  // DWELL
	checkTrue("DWELL at SPEED 10 %: SPEED LIMITS THE SWEEP",
	          noteIs("SPEED LIMITS THE SWEEP", nullptr, C_DIM));
	selectRow(3);   // MIN
	checkTrue("MIN: the end-stop note", noteIs(T1, T2, C_AMBER));

	// STANDARD with MIN or MAX moved past the limits.
	tapId(UI_FIX(ServoCtl::Defaults));
	selectRow(3);
	// SPEED stays at 10 %: its dim note is what the end-stop note replaces.
	const char *sl = "SPEED LIMITS THE SWEEP";
	checkTrue("STANDARD again: MIN shows SPEED LIMITS", noteIs(sl, nullptr, C_DIM));
	stepSel(-1, 10);
	checkTrue("MIN 900 us: no end-stop note", s_profile.minUs == 900 && noteIs(sl, nullptr, C_DIM));
	stepSel(-1);
	checkTrue("MIN 890 us: pending, the end-stop note", s_pending && s_profile.minUs == 900 &&
	          s_edit.minUs == 890 && noteIs(T1, T2, C_AMBER));
	{
		// BACK drops a wait for the travel as it drops a heli or 60 Hz one.
		const FakeSlice sl0 = fakeSlice(0);
		backFromSet();
		const FakeSlice sl1 = fakeSlice(0);
		checkTrue("BACK with MIN 890 us pending: MIN stays 900 us, the slice untouched",
		          s_profile.minUs == 900 && sl1.top == sl0.top && sl1.div16 == sl0.div16 &&
		          sl1.inits == sl0.inits && sl1.enabled == sl0.enabled);
		openSet();
		checkTrue("SET again: nothing pending, MIN 900 us", !s_pending && s_edit.minUs == 900);
		selectRow(3);
		stepSel(-1);
		checkTrue("MIN 890 us again: pending", s_pending && s_edit.minUs == 890);
	}
	stepSel(-1);
	checkTrue("880 us: still pending", s_pending && s_edit.minUs == 880 && s_profile.minUs == 900);
	stepSel(+1, 2);
	checkTrue("back to 900 us, inside the margin: the wait ends, applied",
	          !s_pending && s_profile.minUs == 900 && noteIs(sl, nullptr, C_DIM));
	stepSel(+1);
	selectRow(5);
	stepSel(+1, 10);
	checkTrue("MAX 2100 us: no end-stop note", s_profile.maxUs == 2100 && noteIs(sl, nullptr, C_DIM));
	stepSel(+1);
	checkTrue("MAX 2110 us: pending, the end-stop note", s_pending && s_edit.maxUs == 2110 &&
	          s_profile.maxUs == 2100 && noteIs(T1, T2, C_AMBER));
	holdApply();
	checkTrue("held: applied, the end-stop note stays",
	          !s_pending && s_profile.maxUs == 2110 && noteIs(T1, T2, C_AMBER));

	// NARROW 760 against its own 660 us to 860 us.
	tapId(UI_FIX(ServoCtl::Defaults));
	selectRow(1);
	stepSel(+1);
	checkTrue("NARROW 760: no end-stop note on TYPE",
	          s_profile.type == ServoType::Narrow760 && !noteIs(T1, T2, C_AMBER));
	selectRow(5);
	stepSel(+1, 10);
	checkTrue("NARROW 760, MAX 960 us: no end-stop note",
	          s_profile.maxUs == 960 && !noteIs(T1, T2, C_AMBER));
	stepSel(+1);
	checkTrue("NARROW 760, MAX 970 us: pending, the end-stop note",
	          s_pending && s_edit.maxUs == 970 && noteIs(T1, T2, C_AMBER));
	tapId(UI_FIX(ServoCtl::Defaults));

	// While RUNNING the profile is locked and STOP TO CHANGE THE PROFILE wins.
	backFromSet();
	holdStart();
	openSet();
	selectRow(5);
	checkTrue("RUNNING: STOP TO CHANGE THE PROFILE",
	          noteIs("STOP TO CHANGE THE PROFILE", nullptr, C_AMBER));
	tapId(UI_FIX(ServoCtl::Defaults));   // STOP
	core1(); uiFrames(2);
	tapId(UI_FIX(ServoCtl::Defaults));   // DEFAULTS
	backFromSet();
	leaveServo();
}

/** @brief The SET list on ui_list: its scroll range and its edges. */
static void testSetList() {
	section("Servo SET: the list's extents and edges, as before ui_list");

	freshUi();
	openServo();
	openSet();
	checkInt("SET opens at 0 px", s_list.scrollPx, 0);
	// The viewport's edges and middle come from the rows' rectangles: the
	// first row starts at the viewport's top, the last is cut at its bottom.
	int vt, vb, vx;
	setViewport(&vt, &vb, &vx);
	checkTrue("the rows span the viewport, y 62 to y 215", vt == SET_LIST_Y0 && vb == SET_LIST_Y1);
	for (int i = 0; i < 8; i++) {
		fakePress(vx, vb - 5); uiFrames(1);
		for (int y = vb - 5; y >= vt + 4; y -= 30) { fakeHold(vx, y); uiFrames(1); }
		fakeRelease(); uiFrames(1);
	}
	checkInt("dragged up: 12 rows at a 42 px pitch less 154 px", s_list.scrollPx,
	         SET_ROWS * SET_ROW_PITCH - SET_LIST_H);
	// The last row's body is y 174 to 213; its 2 px gap ends the viewport.
	checkTrue("the last row's rule on y 213, then its gap to y 215",
	          pixel(230, SET_LIST_Y1 - 2) == C_PANEL && pixel(230, SET_LIST_Y1 - 3) == C_BG &&
	          pixel(230, SET_LIST_Y1 - 1) == C_BG && pixel(230, SET_LIST_Y1) == C_BG);
	for (int i = 0; i < 8; i++) {
		fakePress(vx, vt + 4); uiFrames(1);
		for (int y = vt + 4; y <= vb - 5; y += 30) { fakeHold(vx, y); uiFrames(1); }
		fakeRelease(); uiFrames(1);
	}
	checkInt("dragged down: 0 px", s_list.scrollPx, 0);
	checkInt("and no drag selected a row", s_sel, -1);

	// A row cut by the viewport's edge takes a press only while its whole
	// 40 px body shows. At 0 px row 0 is y 62 to 101, and row 3 shows 28 px,
	// y 188 to 215.
	uiTap(vx, vt);
	checkInt("at 0 px, y 62: row 0, whole", s_sel, 0);
	s_sel = -1;
	uiTap(vx, vb);
	checkInt("y 215: row 3, 28 px of it showing: no row", s_sel, -1);
	checkTrue("and row 3's control does not act",
	          ctlPresent(UI_ROW(3, Row)) && !ctlEnabled(UI_ROW(3, Row)));
	// At 1 px row 0 shows 39 px, one short of the floor.
	uiListScrollTo(&s_list, 1);
	s_full = true;
	uiFrames(1);
	uiTap(vx, vt);
	checkInt("at 1 px, y 62: row 0, 39 px of it showing: no row", s_sel, -1);
	// At 12 px row 3 is y 176 to 215, whole down to the viewport's last row,
	// and row 0 shows 28 px.
	const int edgePx = 12;
	uiListScrollTo(&s_list, edgePx);
	s_full = true;
	uiFrames(1);
	uiTap(vx, vb);
	checkInt("at 12 px, y 215: row 3, whole", s_sel, 3);
	s_sel = -1;
	uiTap(vx, vt);
	checkInt("y 62: row 0, 28 px of it showing: no row", s_sel, -1);
	uiTap(vx, vt - 1);
	uiTap(vx, vb + 1);
	checkInt("y 61 and y 216, outside the viewport: no row", s_sel, -1);

	uiTap(vx, vt + 40);
	uiFrames(1);
	uint32_t below = fakeRegionHash(0, vb + 1, GFX_W, GFX_H - vb - 1);
	fakePress(vx, vb - 65); uiFrames(1);
	fakeHold(vx, vb - 95); uiFrames(1);
	fakeRelease(); uiFrames(2);
	checkTrue("a scroll leaves the editor bar and the footer as they were",
	          s_list.scrollPx == edgePx + 30 &&
	          fakeRegionHash(0, vb + 1, GFX_W, GFX_H - vb - 1) == below);
	backFromSet();
	openSet();
	checkTrue("SET opened again: at 0 px, no row selected", s_list.scrollPx == 0 && s_sel == -1);

	// HOLD TO APPLY with nothing pending: drawn disabled, and a hold on it
	// hands the job nothing, so the pins are not rebound.
	int inits = fakeSlice(0).inits;
	int ax, ay;
	ctlCentre(UI_FIX(ServoCtl::Apply), &ax, &ay);
	pressId(UI_FIX(ServoCtl::Apply), IdExpect::Disabled);
	for (int i = 0; i < 48; i++) { fakeHold(ax, ay); uiFrames(1); core1(); }
	checkTrue("nothing pending: HOLD TO APPLY is an outline", fillOf(BTN_APPLY) == C_BG);
	// A change forced pending under the same press, which the UI cannot do
	// with one finger: the press landed on a disabled button, so it is not
	// timed.
	s_pending = true;
	s_edit.rateHz = 60;
	for (int i = 0; i < 48; i++) { fakeHold(ax, ay); uiFrames(1); core1(); }
	fakeRelease(); uiFrames(1); core1();
	checkTrue("held 2400 ms: no rebind, and the forced change not applied",
	          fakeSlice(0).inits == inits && s_profile.rateHz == 50 && s_pending);
	s_pending = false;
	s_edit = s_profile;
	backFromSet();
	leaveServo();
}

/** @brief Stamp the fake ESC's eRPM now, so it keeps answering on SIG. */
static void escAnswers(EscTelemetry *t) {
	t->lastRpmMs = millis();
	fakeSetTelemetry(t);
}

/**
 * @brief From the tester, with an ESC answering so nothing is claimed: SERVO,
 *        SET, WIDE applied with HOLD TO APPLY, then TWO pending on it.
 */
static void wideTwoPendingUnclaimed(EscTelemetry *t) {
	memset(t, 0, sizeof(*t));
	escAnswers(t);
	openServo();
	escAnswers(t);
	openSet();
	escAnswers(t);
	selectRow(1);   // TYPE
	escAnswers(t);
	stepSel(+1);    // NARROW 760
	escAnswers(t);
	stepSel(+1);    // WIDE
	escAnswers(t);
	int x, y;
	ctlCentre(UI_FIX(ServoCtl::Apply), &x, &y);
	pressId(UI_FIX(ServoCtl::Apply));
	for (int i = 0; i < 41; i++) { escAnswers(t); fakeHold(x, y); uiFrames(1); }
	fakeRelease();
	escAnswers(t);
	uiFrames(1);
	selectRow(ROW_SERVOS);
	escAnswers(t);
	stepSel(+1);
	escAnswers(t);
}

/** @brief The ESC goes quiet with KISS live on AUX: the claim takes SIG alone. */
static void claimSigAloneKiss(EscTelemetry *t) {
	t->lastRpmMs = 0;
	t->kissLastMs = millis();
	t->haveKiss = true;
	fakeSetTelemetry(t);
	uiFrames(2);
}


/**
 * @brief Claim refusals the UI cannot reach, and TWO chosen before the claim.
 */
static void testClaimCorners() {
	section("Servo screen: refusals the UI cannot reach, and TWO before the claim");

	// Another screen holds the pins. SERVO, the hub, refuses SERVO TESTER
	// while the claim is not Free: the entry is drawn disabled, does not
	// act, and a tap opens nothing. Once the claim is Free, the entry opens
	// SERVO TESTER, which claims at once.
	freshUi();
	checkTrue("another screen holds the pins", pinClaimBegin(PinUser::Am32, PIN_SIG) == ClaimRefusal::None);
	tapId(UI_FIX(TesterCtl::Menu));
	uiFrames(1);
	checkTrue("MENU opens", uiCurrentScreen() == UiScreenId::Menu);
	checkTrue("MENU: SERVO, the hub's row, acts while the claim is not Free",
	          ctlEnabled(menuRow(&UI_SERVO_HUB_SCREEN)));
	const UiControlId entry = toolEntry(&UI_SERVO);
	uiFrames(1);
	checkTrue("the hub: SERVO TESTER drawn disabled, the strip DSHOT RESTARTING",
	          uiCurrentScreen() == UiScreenId::ServoHub && busyShown(entry));
	checkTrue("and its entry does not act", !ctlEnabled(entry));
	tapId(entry, IdExpect::Disabled);
	checkTrue("a tap opens nothing, and SERVO TESTER claims nothing",
	          uiCurrentScreen() == UiScreenId::ServoHub && pinClaimHolder() == PinUser::Am32);
	pinClaimEnd(PinUser::Am32);
	for (int i = 0; i < 20 && !ctlEnabled(entry); i++) uiFrames(1);
	checkTrue("the other claim gone: the entry acts again",
	          pinClaimState() == ClaimState::Free && ctlEnabled(entry) && !busyShown(entry));
	tapId(entry);
	checkTrue("SERVO opens, and claims on the tap",
	          uiCurrentScreen() == UiScreenId::Servo && s_claimed &&
	          pinClaimHolder() == PinUser::Servo);
	uiFrames(3);
	checkTrue("with AUX, once held", pinClaimGpio(PIN_AUX) == 1 && s_auxRefusal == ClaimRefusal::None);
	leaveServo();
	backFromMenu();
	uiFrames(1);

	// An AUX refusal from an earlier frame, then a frame whose claim of SIG
	// and AUX goes through: the refusal is forgotten. An ESC answering on SIG
	// keeps SERVO open without a claim; the refusal is forced.
	freshUi();
	{
		EscTelemetry esc;
		memset(&esc, 0, sizeof(esc));
		esc.lastRpmMs = millis();
		fakeSetTelemetry(&esc);
	}
	openServo();
	checkTrue("an ESC answers: no claim yet", !s_claimed && s_refusal == ClaimRefusal::EscAnswering);
	s_auxRefusal = ClaimRefusal::KissLive;
	noEsc();
	uiFrames(3);
	checkTrue("the ESC quiet: SERVO claims", s_claimed && pinClaimHolder() == PinUser::Servo);
	checkTrue("with AUX, and the old AUX refusal cleared",
	          pinClaimGpio(PIN_AUX) == 1 && s_auxRefusal == ClaimRefusal::None);
	leaveServo();
	backFromMenu();
	uiFrames(1);

	// Armed, entered without MENU, which would have disarmed.
	freshUi();
	pressId(UI_FIX(TesterCtl::Arm));
	holdId(UI_FIX(TesterCtl::Arm), 60);
	fakeRelease(); uiFrames(1);
	checkTrue("armed on the main screen", uiArmed());
	uiServoEnter();
	TouchState idle;
	memset(&idle, 0, sizeof(idle));
	uiServoTick(&idle);
	checkTrue("SERVO while armed: DISARM FIRST, nothing claimed",
	          s_refusal == ClaimRefusal::Armed && pinClaimHolder() == PinUser::None);
	// SERVO was entered by hand, so uiControls() still reports the tester:
	// BACK's centre comes from SERVO's own table.
	UiControl tab[UI_CONTROLS_MAX];
	uint8_t tn = uiServoControls(tab, UI_CONTROLS_MAX);
	int bx = -1, by = -1;
	for (uint8_t i = 0; i < tn; i++)
		if (tab[i].id == UI_FIX(UI_BACK)) {
			bx = tab[i].rect.x + tab[i].rect.w / 2;
			by = tab[i].rect.y + tab[i].rect.h / 2;
		}
	checkTrue("SERVO's table lists BACK", bx >= 0);
	TouchState back = idle;
	back.released = true; back.x = back.downX = (int16_t)bx; back.y = back.downY = (int16_t)by;
	checkTrue("BACK with no claim leaves at once", !uiServoTick(&back));
	freshUi();

	// TWO set on SET while an ESC holds the claim off; when it claims, the
	// KISS wire refuses AUX and the screen goes back to one servo.
	EscTelemetry t;
	memset(&t, 0, sizeof(t));
	t.lastRpmMs = millis();
	fakeSetTelemetry(&t);
	openServo();
	// The fake ESC is a snapshot: stamp it again so it keeps answering.
	t.lastRpmMs = millis(); fakeSetTelemetry(&t);
	openSet();
	t.lastRpmMs = millis(); fakeSetTelemetry(&t);
	selectRow(ROW_SERVOS);
	t.lastRpmMs = millis(); fakeSetTelemetry(&t);
	stepSel(+1);
	checkTrue("TWO chosen before the claim", s_profile.servos == 2 && !s_claimed);
	t.lastRpmMs = 0;
	t.kissLastMs = millis();
	t.haveKiss = true;
	fakeSetTelemetry(&t);
	uiFrames(2);
	checkTrue("claimed SIG alone: KISS live on AUX",
	          s_claimed && s_auxRefusal == ClaimRefusal::KissLive);
	checkTrue("and back to one servo", s_profile.servos == 1 && s_edit.servos == 1);
	uiFrames(2);
	checkTrue("the job bound SIG alone", fakePin(0).function == GPIO_FUNC_PWM &&
	                                     fakePin(1).function != GPIO_FUNC_PWM);
	backFromSet();
	leaveServo();
	backFromMenu();
	noEsc();

	// The same with TWO pending, not applied: WIDE applied while the ESC
	// holds the claim off, then TWO waits for HOLD TO APPLY as decision 22
	// has it. The claim of SIG alone drops the pending TWO and ends the
	// wait, so no hold can apply a profile for an AUX that is not bound.
	memset(&t, 0, sizeof(t));
	t.lastRpmMs = millis(); fakeSetTelemetry(&t);
	openServo();
	t.lastRpmMs = millis(); fakeSetTelemetry(&t);
	openSet();
	t.lastRpmMs = millis(); fakeSetTelemetry(&t);
	selectRow(1);   // TYPE
	t.lastRpmMs = millis(); fakeSetTelemetry(&t);
	stepSel(+1);    // NARROW 760
	t.lastRpmMs = millis(); fakeSetTelemetry(&t);
	stepSel(+1);    // WIDE
	t.lastRpmMs = millis(); fakeSetTelemetry(&t);
	checkTrue("WIDE pending before the claim", s_pending && s_edit.type == ServoType::Wide && !s_claimed);
	{
		int x, y;
		ctlCentre(UI_FIX(ServoCtl::Apply), &x, &y);
		pressId(UI_FIX(ServoCtl::Apply));
		for (int i = 0; i < 41; i++) {
			t.lastRpmMs = millis(); fakeSetTelemetry(&t);
			fakeHold(x, y); uiFrames(1);
		}
		fakeRelease();
		t.lastRpmMs = millis(); fakeSetTelemetry(&t);
		uiFrames(1);
	}
	checkTrue("WIDE applied before the claim", !s_pending && s_profile.type == ServoType::Wide && !s_claimed);
	selectRow(ROW_SERVOS);
	t.lastRpmMs = millis(); fakeSetTelemetry(&t);
	stepSel(+1);
	checkTrue("TWO on WIDE pending before the claim",
	          s_pending && s_edit.servos == 2 && s_profile.servos == 1 && !s_claimed);
	t.lastRpmMs = millis(); fakeSetTelemetry(&t);
	selectRow(4);   // CENTRE
	t.lastRpmMs = millis(); fakeSetTelemetry(&t);
	stepSel(+1);
	t.lastRpmMs = millis(); fakeSetTelemetry(&t);
	checkTrue("CENTRE 1510 us joins the wait", s_pending && s_edit.centreUs == 1510 &&
	          s_profile.centreUs == 1500 && !s_claimed);
	t.lastRpmMs = 0;
	t.kissLastMs = millis();
	t.haveKiss = true;
	fakeSetTelemetry(&t);
	uiFrames(2);
	checkTrue("claimed SIG alone: KISS live on AUX",
	          s_claimed && s_auxRefusal == ClaimRefusal::KissLive);
	checkTrue("the pending TWO dropped to one, the wait ended",
	          s_edit.servos == 1 && s_profile.servos == 1 && !s_pending &&
	          s_profile.type == ServoType::Wide);
	checkTrue("and CENTRE, which waited with it, applied", s_profile.centreUs == 1510);
	{
		UiControl c;
		checkTrue("HOLD TO APPLY drawn disabled: nothing waits",
		          ctlFind(UI_FIX(ServoCtl::Apply), &c) && !c.enabled);
		int x, y;
		ctlCentre(UI_FIX(ServoCtl::Apply), &x, &y);
		fakePress(x, y); uiFrames(1);
		for (int i = 0; i < 41; i++) { fakeHold(x, y); uiFrames(1); }
		fakeRelease(); uiFrames(1);
		core1();
	}
	uiFrames(2);
	checkTrue("held on it all the same: still one servo, AUX not driven",
	          s_profile.servos == 1 && !twoServos() && fakePin(0).function == GPIO_FUNC_PWM &&
	          fakePin(1).function != GPIO_FUNC_PWM);
	backFromSet();
	leaveServo();
	backFromMenu();
	noEsc();

	// A wait that holds for more than TWO outlasts the claim of SIG alone:
	// TWO drops to one, and the other change still waits for the hold.
	wideTwoPendingUnclaimed(&t);
	selectRow(1);   // TYPE
	escAnswers(&t);
	stepSel(+1);    // HELI CYCLIC: the heli warning
	escAnswers(&t);
	checkTrue("WIDE applied, TWO and HELI CYCLIC pending before the claim",
	          s_pending && s_edit.type == ServoType::HeliCyclic && s_edit.servos == 2 && !s_claimed);
	claimSigAloneKiss(&t);
	checkTrue("SIG alone: TWO dropped, HELI CYCLIC still waits, WIDE on one servo applied",
	          s_claimed && s_auxRefusal == ClaimRefusal::KissLive && s_pending &&
	          s_edit.servos == 1 && s_profile.type == ServoType::Wide && s_profile.servos == 1);
	backFromSet();
	leaveServo();
	backFromMenu();
	noEsc();

	wideTwoPendingUnclaimed(&t);
	selectRow(3);   // MIN
	escAnswers(&t);
	stepSel(-1);    // 790 us: further past the travel
	escAnswers(&t);
	checkTrue("WIDE applied, TWO and MIN 790 us pending before the claim",
	          s_pending && s_edit.minUs == 790 && s_edit.servos == 2 && !s_claimed);
	claimSigAloneKiss(&t);
	checkTrue("SIG alone: TWO dropped, MIN 790 us still waits, MIN 800 us applied",
	          s_claimed && s_pending && s_edit.servos == 1 && s_edit.minUs == 790 &&
	          s_profile.minUs == 800 && s_profile.servos == 1);
	backFromSet();
	leaveServo();
	backFromMenu();
	noEsc();

	wideTwoPendingUnclaimed(&t);
	selectRow(2);   // RATE
	escAnswers(&t);
	stepSel(+1);    // 60 Hz
	escAnswers(&t);
	stepSel(+1);    // 100 Hz: the 60 Hz warning
	escAnswers(&t);
	checkTrue("WIDE applied, TWO and 100 Hz pending before the claim",
	          s_pending && s_edit.rateHz == 100 && s_edit.servos == 2 && !s_claimed);
	claimSigAloneKiss(&t);
	checkTrue("SIG alone: TWO dropped, 100 Hz still waits, 50 Hz applied",
	          s_claimed && s_pending && s_edit.servos == 1 && s_profile.rateHz == 50 &&
	          s_profile.servos == 1);
	backFromSet();
	leaveServo();
	noEsc();
}

/** @brief A refused bind: the message, RETRY, and the rebind path. */
static void testBindRefused() {
	section("Servo screen: a refused bind and RETRY");

	freshUi();
	fakePwmBreakSliceMap(true);
	openServo();
	checkTrue("the claim is held with the reason", pinClaimJobError() == CORE1_JOB_PWM);
	checkTrue("GP0 COULD NOT BE TAKEN: no two-servo hint", twoHintBlank());
	checkControlTable("SERVO, a job error");
	checkTrue("a job error offers RETRY", ctlEnabled(UI_FIX(ServoCtl::Retry)));
	checkTrue("the message replaces the dial", !ctlPresent(UI_FIX(ServoCtl::Dial)));
	checkTrue("HOLD TO START is an outline", fillOf(BTN_START) == C_BG);
	fakeDumpFrame("shot_servo_bind.ppm");
	fakePwmBreakSliceMap(false);
	tapId(UI_FIX(ServoCtl::Retry));
	uiFrames(2);
	checkTrue("RETRY: running", pinClaimState() == ClaimState::Running && fillOf(BTN_START) == C_PANEL);

	openSet();
	selectRow(2);
	fakePwmBreakSliceMap(true);
	stepSel(+1);
	uiFrames(1);
	checkInt("a rebind refused: the job says why", s_st.bindError, CORE1_JOB_PWM);
	backFromSet();
	uiFrames(1);
	checkTrue("a refused rebind: no two-servo hint", twoHintBlank());
	checkTrue("HOLD TO START is an outline", fillOf(BTN_START) == C_BG);
	fakePwmBreakSliceMap(false);
	tapId(UI_FIX(ServoCtl::Retry));
	core1();
	uiFrames(2);
	checkTrue("RETRY rebinds", s_st.bindError == 0 && fakePin(0).function == GPIO_FUNC_PWM);
	leaveServo();
	backFromMenu();
}


/**
 * @brief Whether @p text at scale @p scale, in @p col, is what the panel
 *        shows with its top left at (@p x, @p y): the box redrawn with it
 *        hashes the same.
 */
static bool textAt(int x, int y, const char *text, uint16_t col, int scale) {
	int w = gfxTextW(text, scale), h = 7 * scale;
	static uint16_t keep[14][GFX_W];
	uint16_t *fb = gfxBuffer();
	for (int r = 0; r < h; r++) memcpy(keep[r], fb + (y + r) * GFX_W, sizeof(keep[r]));
	uint32_t shown = fakeRegionHash(x, y, w, h);
	gfxRect(x, y, w, h, C_BG);
	gfxText(x, y, text, col, scale);
	bool same = fakeRegionHash(x, y, w, h) == shown;
	for (int r = 0; r < h; r++) memcpy(fb + (y + r) * GFX_W, keep[r], sizeof(keep[r]));
	return same;
}

/** @brief Whether any pixel of the box is @p col. */
static bool anyOf(int x0, int y0, int w, int h, uint16_t col) {
	for (int y = y0; y < y0 + h; y++)
		for (int x = x0; x < x0 + w; x++)
			if (pixel(x, y) == col) return true;
	return false;
}

/**
 * @brief The readout row with CURRENT SENSE at INA219: the current at
 *        scale 2 right of the pulse, the bus voltage under it, the clip in
 *        amber, `-- A` once 250 ms pass without a reading, `A+B` with two
 *        servos, and the sweep's count moved to the dial band.
 */
static void testSenseReadout() {
	section("Servo screen: the current sensor's readout");
	freshUi();
	fakeI2cReset();
	FakeIna219 *ina = fakeI2cAddIna219(INA219_ADDR);
	ina->currentA = 0.42;
	ina->voutV = 5.062;   // 5.020 V at IN-
	openServo();
	uiFrames(1);
	checkTrue("NONE: SAFE with one servo, the two-servo hint on the right",
	          twoHintShown() && !anyOf(120, SV_READ_Y, GFX_W - 120, SV_READ_H, C_TEXT));
	settings()->currentSense = 1;
	senseConfigure(1);
	uiFrames(2);
	const int curY = SV_READ_Y + SV_CUR_DY, voltY = SV_READ_Y + SV_VOLT_DY;
	checkTrue("INA219: the hint gives the row to the readout", !twoHintShown());
	checkTrue("0.42 A at scale 2, right of the pulse",
	          textAt(GFX_W - UI_MARGIN - gfxTextW("0.42 A", 2), curY, "0.42 A", C_TEXT, 2));
	checkTrue("5.02 V under it at scale 1",
	          textAt(GFX_W - UI_MARGIN - gfxTextW("5.02 V", 1), voltY, "5.02 V", C_DIM, 1));
	checkTrue("the pulse where it was", textAt(UI_MARGIN, SV_READ_Y + 5, "NO PULSE", C_DIM, 2));
	fakeDumpFrame("shot_servo_sense.ppm");
	ina->currentA = 4.0;
	uiFrames(1);
	checkTrue("clipped: >3.2 A in amber",
	          textAt(GFX_W - UI_MARGIN - gfxTextW(">3.2 A", 2), curY, ">3.2 A", C_AMBER, 2));
	ina->currentA = 0.42;
	uiFrames(1);

	holdStart();
	tapId(UI_FIX(ServoCtl::Sweep)); core1();
	uiFrames(24); core1();
	uiFrames(1);
	char sw[24];
	sweepText(sw, sizeof(sw));
	checkTrue("sweeping: the count on the dial band's first line, lime: a live reading",
	          sw[0] && textAt((GFX_W - gfxTextW(sw, 1)) / 2, 64, sw, C_LIME, 1));
	checkTrue("and not on the readout row",
	          !textAt(GFX_W - UI_MARGIN - gfxTextW(sw, 1), SV_READ_Y + 9, sw, C_LIME, 1) &&
	          !anyOf(UI_MARGIN, voltY, 100, 7, C_CYAN));
	pressId(UI_FIX(ServoCtl::Start)); core1();
	fakeRelease(); uiFrames(1);

	s_profile.servos = 2;
	s_full = true;
	drawReadout();
	s_full = false;
	checkTrue("two servos: A+B 0.42 A on the second line",
	          textAt(UI_MARGIN, SV_READ_Y + 14, "A+B 0.42 A", C_TEXT, 1));
	checkTrue("clear of the selector", UI_MARGIN + gfxTextW("A+B <-3.2 A", 1) + 4 <= BTN_SEL_A.x);
	s_profile.servos = 1;
	s_full = true;

	fakeI2cRemove(INA219_ADDR);
	uiFrames(9);
	checkTrue("225 ms after the last reading: still shown",
	          textAt(GFX_W - UI_MARGIN - gfxTextW("0.42 A", 2), curY, "0.42 A", C_TEXT, 2));
	uiFrames(1);
	checkTrue("250 ms: -- A in dim",
	          textAt(GFX_W - UI_MARGIN - gfxTextW("-- A", 2), curY, "-- A", C_DIM, 2));
	checkTrue("and -- V", textAt(GFX_W - UI_MARGIN - gfxTextW("-- V", 1), voltY, "-- V", C_DIM, 1));
	checkTrue("no reading: still no hint", !twoHintShown());
	settings()->currentSense = 0;
	senseConfigure(0);
	uiFrames(1);
	checkTrue("back to NONE: the hint returns, no current",
	          twoHintShown() && !anyOf(120, SV_READ_Y, GFX_W - 120, SV_READ_H, C_TEXT));

	leaveServo();
	settings()->currentSense = 0;
	senseConfigure(0);
	fakeI2cReset();
}

/**
 * @brief The switched supply's line on the strip, and the two-servo hint.
 *
 * With SERVO SUPPLY at GP28 or GP29 the screen switches the servo's supply
 * (servo-tester.md decision 20): the strip says where it is under its
 * caption, the notes warn of an ESC on the lead, and the pulses and the
 * buttons wait for the supply. SAFE with one servo, the readout names where
 * the second servo is switched on.
 */
static void testSupplyLineAndHint() {
	section("Servo screen: the switched supply's line; two servos are possible");

	freshUi();
	tapId(UI_FIX(TesterCtl::Menu));
	uiFrames(1);
	tapId(toolEntry(&UI_SERVO));
	uiTick();   // SERVO's first frame, at once: the pump is still draining
	checkTrue("the first frame: the claim is not Running yet",
	          uiCurrentScreen() == UiScreenId::Servo && pinClaimState() != ClaimState::Running);
	checkTrue("RELEASING or TAKING SIG GP0: no two-servo hint", twoHintBlank());
	uiFrames(3);
	checkTrue("then running on the claim", pinClaimState() == ClaimState::Running);
	char cap[40];
	snprintf(cap, sizeof(cap), "SIG GP%u  %u HZ  %u-%u US", 0u, (unsigned)s_profile.rateHz,
	         (unsigned)s_profile.minUs, (unsigned)s_profile.maxUs);
	checkStr("the caption names SIG by role and number", cap, "SIG GP0  50 HZ  1000-2000 US");
	checkTrue("OWN: the caption alone, centred in the strip at y 44",
	          fakeTextIs(UI_MARGIN, UI_STRIP_Y + 4, cap, C_DIM, C_BG, 1));
	checkTrue("and no second line", allOf(0, UI_STRIP_Y + 11, 190, UI_BODY_Y - UI_STRIP_Y - 11, C_BG));
	checkTrue("SAFE, one servo: the readout names TWO SERVOS: SET > SERVOS", twoHintShown());
	// "NO PULSE" at scale 2 ends at x 101; the hint's widest line starts at
	// x 161.
	checkTrue("clear of NO PULSE by 4 px or more",
	          UI_MARGIN + gfxTextW("NO PULSE", 2) + 4 <= GFX_W - UI_MARGIN - gfxTextW(SERVO_UI_TWO_2, 1));
	holdStart();
	uiFrames(2);
	checkTrue("running: no hint", s_st.running && twoHintBlank());
	tapId(UI_FIX(ServoCtl::Start));
	uiFrames(2);
	checkTrue("SAFE again: the hint is back", !s_st.running && twoHintShown());
	openSet();
	selectRow(ROW_SERVOS);
	stepSel(+1);
	checkTrue("SERVOS TWO, applied at once", s_profile.servos == 2);
	backFromSet();
	uiFrames(2);
	checkTrue("two servos: no hint", s_profile.servos == 2 && !twoHintShown());
	leaveServo();
	backFromMenu();

	// GP28, then GP29: the line names the pin and the supply's phase.
	// SETTINGS is closed while the tool runs, so the supply is set before
	// the screen opens.
	for (uint8_t mode = 1; mode <= 2; mode++) {
		freshUi();
		settings()->servoSupply = mode;
		servoSupplyConfigure(mode);
		const uint8_t en = servoSupplyPinOf(mode);
		openServo();
		uiFrames(3);
		char want[40];
		snprintf(want, sizeof(want), SERVO_UI_SUPPLY_FMT, (unsigned)en);
		checkStr("the line", want, mode == 1 ? "GP28 SUPPLY ON AT START" : "GP29 SUPPLY ON AT START");
		checkTrue("the caption moves up to y 42",
		          fakeTextIs(UI_MARGIN, UI_STRIP_LINE1_Y, cap, C_DIM, C_BG, 1));
		checkTrue("the supply's line under it at y 52, amber",
		          fakeTextIs(UI_MARGIN, UI_STRIP_LINE2_Y, want, C_AMBER, C_BG, 1));
		checkTrue("and above the dial", UI_STRIP_LINE2_Y + 7 <= SERVO_DIAL_Y0);
		checkTrue("the chip still says SAFE", pixel(GFX_W - UI_MARGIN - 4, UI_STRIP_Y + 3) == C_GREEN);
		checkTrue("SAFE: the enable pin driven low", fakePin(en).output && !fakePin(en).high);
		char esc[48];
		snprintf(esc, sizeof(esc), SERVO_UI_ESC_1_FMT, 0u);
		checkStr("the ESC note", esc, "PULSES ON GP0 ARE THROTTLE TO AN ESC,");
		checkTrue("SAFE: the ESC notes at y 64 and 76, amber",
		          textAt((GFX_W - gfxTextW(esc, 1)) / 2, 64, esc, C_AMBER, 1) &&
		          textAt((GFX_W - gfxTextW(SERVO_UI_ESC_2, 1)) / 2, 76, SERVO_UI_ESC_2, C_AMBER, 1));
		if (mode == 1) fakeDumpFrame("shot_servo_supply.ppm");
		holdStartFrames();
		uiFrames(1);
		snprintf(want, sizeof(want), SERVO_UI_SUPPLY_UP_FMT, (unsigned)en);
		checkTrue("START: the enable pin high, COMING UP, no pulse yet",
		          fakePin(en).high && s_st.running && fakePin(SIG).level == 0 &&
		          fakeTextIs(UI_MARGIN, UI_STRIP_LINE2_Y, want, C_AMBER, C_BG, 1));
		checkTrue("RUNNING chip, STOP live, MIN and the dial not yet",
		          pixel(GFX_W - UI_MARGIN - 4, UI_STRIP_Y + 3) == C_RED &&
		          ctlEnabled(UI_FIX(ServoCtl::Start)) && !ctlEnabled(UI_FIX(ServoCtl::Min)) &&
		          !ctlEnabled(UI_FIX(ServoCtl::Dial)));
		uiFrames(20);
		snprintf(want, sizeof(want), SERVO_UI_SUPPLY_ON_FMT, (unsigned)en);
		checkTrue("500 ms on: SUPPLY ON, CENTRE, the buttons live",
		          fakeTextIs(UI_MARGIN, UI_STRIP_LINE2_Y, want, C_AMBER, C_BG, 1) &&
		          fakePin(SIG).level == 1500 && ctlEnabled(UI_FIX(ServoCtl::Min)));
		checkTrue("and nothing after it on the line",
		          allOf(UI_MARGIN + gfxTextW(want, 1) + 1, UI_STRIP_LINE2_Y, 60, 7, C_BG));
		openSet();
		checkTrue("SET with no sensor: no start-up time under STOP TO CHANGE THE PROFILE",
		          allOf(0, UI_STRIP_Y + 12, GFX_W, 7, C_BG));
		backFromSet();
		tapId(UI_FIX(ServoCtl::Min));
		core1();
		checkInt("MIN drives", fakePin(SIG).level, 1000);
		pressId(UI_FIX(ServoCtl::Start));
		core1();
		checkTrue("STOP on press: the enable pin low, level 0", !fakePin(en).high &&
		          fakePin(en).output && fakePin(SIG).level == 0);
		fakeRelease();
		uiFrames(2);
		snprintf(want, sizeof(want), SERVO_UI_SUPPLY_FMT, (unsigned)en);
		checkTrue("SAFE: ON AT START again", fakeTextIs(UI_MARGIN, UI_STRIP_LINE2_Y, want, C_AMBER,
		                                                C_BG, 1));
		holdStartFrames();
		uiFrames(21);
		checkTrue("on again before BACK", fakePin(en).high && fakePin(SIG).level == 1500);
		leaveServo();
		checkTrue("BACK: the enable pin undriven again, as outside every claim",
		          enUndriven(en));
		backFromMenu();
	}

	// Every text fits: the strip's lines end 4 px before the widest chip,
	// STALLED, at x 179; the dial band's within the margins.
	const char *strip[] = { SERVO_UI_SUPPLY_FMT, SERVO_UI_SUPPLY_UP_FMT, SERVO_UI_SUPPLY_ON_FMT,
	                        SERVO_UI_SUPPLY_NOT_OFF_FMT };
	for (const char *f : strip) {
		char t[40];
		snprintf(t, sizeof(t), f, 29u);
		checkTrue(t, UI_MARGIN + gfxTextW(t, 1) + 4 <=
		                 GFX_W - UI_MARGIN - gfxTextW("STALLED", 1) - 12);
	}
	char b1[48], b2[48];
	// SIG is GP0 or GP1: one digit.
	snprintf(b1, sizeof(b1), SERVO_UI_ESC_1_FMT, 1u);
	snprintf(b2, sizeof(b2), SERVO_UI_NO_START_2_FMT, 29u);
	const char *band[] = { b1, SERVO_UI_ESC_2, SERVO_UI_NO_START_1, b2, SERVO_UI_NO_SENSOR_1,
	                       SERVO_UI_NO_SENSOR_2, SERVO_UI_TOUCH_SILENT, SERVO_UI_BUS_STUCK,
	                       SERVO_UI_BUS_WAIT_1, SERVO_UI_BUS_WAIT_2, SERVO_UI_TOUCH_WAIT_1,
	                       SERVO_UI_TOUCH_WAIT_2, SERVO_UI_TOUCH_LOST_2 };
	for (const char *t : band) checkTrue(t, gfxTextW(t, 1) <= GFX_W - 2 * UI_MARGIN);
	settings()->servoSupply = 0;
	servoSupplyConfigure(0);
	freshUi();
}

/** @brief @p n UI frames of 25 ms, the fake INA219's output following GP28 before each. */
static void senseFrames(FakeIna219 *ina, int n, double onV = 5.0, double offV = 0.0) {
	for (int i = 0; i < n; i++) {
		followEn(ina, 28, onV, offV);
		frameAt(25);
	}
}

/** @brief Whether the dial band's two notes read @p a in @p ca and @p b in @p cb. */
static bool notesAre(const char *a, uint16_t ca, const char *b, uint16_t cb) {
	return textAt((GFX_W - gfxTextW(a, 1)) / 2, 64, a, ca, 1) &&
	       textAt((GFX_W - gfxTextW(b, 1)) / 2, 76, b, cb, 1);
}

/**
 * @brief The switched supply's faults and read-back on the screen: START
 *        waiting for a declared sensor, the start-up time on SET, a supply
 *        that did not come up, one not off, and the touch and bus cuts.
 */
static void testSupplyScreenFaults() {
	section("Servo screen: the switched supply's read-back and cuts");

	freshUi();
	fakeI2cReset();
	settings()->servoSupply = 1;
	servoSupplyConfigure(1);
	settings()->currentSense = 1;
	senseConfigure(1);
	openServo();
	uiFrames(3);
	checkTrue("a declared sensor not found: START refused, the notes say so",
	          !ctlEnabled(UI_FIX(ServoCtl::Start)) &&
	          notesAre(SERVO_UI_NO_SENSOR_1, C_AMBER, SERVO_UI_NO_SENSOR_2, C_DIM));
	checkTrue("HOLD TO START drawn disabled", fillOf(BTN_START) == C_BG);
	fakeDumpFrame("shot_servo_no_sensor.ppm");

	FakeIna219 *ina = fakeI2cAddIna219(INA219_ADDR);
	senseFrames(ina, 42);
	checkTrue("found within a second: START live, the ESC notes back",
	          ctlEnabled(UI_FIX(ServoCtl::Start)) && s_st.startBlock == ServoStartBlock::None);
	holdStartFrames();
	senseFrames(ina, 3);
	checkTrue("up at the first 5 V reading: CENTRE", s_st.power == ServoPowerPhase::On &&
	          fakePin(SIG).level == 1500);
	uint16_t up = s_st.upMs;
	checkTrue("a start-up time of one or two frames", up >= 1 && up <= 50);
	openSet();
	char t[32];
	snprintf(t, sizeof(t), SERVO_UI_UP_FMT, (unsigned)up);
	checkTrue("SET while running: STOP TO CHANGE THE PROFILE, then SUPPLY UP IN n MS, dim",
	          textAt(UI_MARGIN, UI_STRIP_Y + 2, "STOP TO CHANGE THE PROFILE", C_AMBER, 1) &&
	          textAt(UI_MARGIN, UI_STRIP_Y + 12, t, C_DIM, 1));
	fakeDumpFrame("shot_servo_set_up.ppm");
	backFromSet();
	pressId(UI_FIX(ServoCtl::Start));
	core1();
	fakeRelease();
	senseFrames(ina, 2);
	openSet();
	checkTrue("SET while SAFE: the start-up time kept, on the second line",
	          textAt(UI_MARGIN, UI_STRIP_Y + 12, t, C_DIM, 1) &&
	          !anyOf(0, UI_STRIP_Y + 2, GFX_W, 7, C_AMBER));
	selectRow(3);   // MIN, stepped past the travel: a two-line note
	stepSel(-1, 11);
	checkTrue("a two-line note takes the second line: no start-up time over it",
	          s_pending && textAt(UI_MARGIN, UI_STRIP_Y + 12, "HEATS. TRY MIN AND MAX HORN-FREE.",
	                              C_AMBER, 1) &&
	          !anyOf(0, UI_STRIP_Y + 12, GFX_W, 7, C_DIM));
	backFromSet();

	// The output stays at 4.9 V with EN low: NOT OFF after 2000 ms.
	holdStartFrames();
	senseFrames(ina, 3, 5.0, 4.9);
	pressId(UI_FIX(ServoCtl::Start));
	core1();
	fakeRelease();
	senseFrames(ina, 78, 5.0, 4.9);
	char want[40];
	snprintf(want, sizeof(want), SERVO_UI_SUPPLY_NOT_OFF_FMT, 28u);
	checkTrue("1950 ms: not yet", !fakeTextIs(UI_MARGIN, UI_STRIP_LINE2_Y, want, C_AMBER, C_BG, 1));
	senseFrames(ina, 3, 5.0, 4.9);
	checkTrue("2000 ms at 4.9 V: GP28 SUPPLY NOT OFF, amber",
	          fakeTextIs(UI_MARGIN, UI_STRIP_LINE2_Y, want, C_AMBER, C_BG, 1));
	checkTrue("a warning: SAFE, the enable pin low", !s_st.running && !fakePin(28).high);
	fakeDumpFrame("shot_servo_not_off.ppm");
	senseFrames(ina, 2);
	snprintf(want, sizeof(want), SERVO_UI_SUPPLY_FMT, 28u);
	checkTrue("the output falls: ON AT START again",
	          fakeTextIs(UI_MARGIN, UI_STRIP_LINE2_Y, want, C_AMBER, C_BG, 1));

	// A converter stuck at 3.6 V: did not come up.
	holdStartFrames();
	senseFrames(ina, 22, 3.6);
	char b2[40];
	snprintf(b2, sizeof(b2), SERVO_UI_NO_START_2_FMT, 28u);
	checkTrue("500 ms below 4.5 V: SAFE, the enable pin low, the notes say why",
	          !s_st.running && !fakePin(28).high && fakePin(SIG).level == 0 &&
	          notesAre(SERVO_UI_NO_START_1, C_AMBER, b2, C_DIM));
	checkTrue("the chip says SAFE", pixel(GFX_W - UI_MARGIN - 4, UI_STRIP_Y + 3) == C_GREEN);

	// The touch controller stops answering while the supply is on, UI frames
	// 25 ms apart: core0's stop (uiTouchLost()) at four failed reads and
	// 100 ms, by STOP's path, before core1's 250 ms cut.
	holdStartFrames();
	senseFrames(ina, 3);
	checkTrue("on", fakePin(28).high && fakePin(SIG).level == 1500);
	fakeTouchAnswers(false);
	senseFrames(ina, 3);
	checkTrue("75 ms silent: on", fakePin(28).high);
	senseFrames(ina, 2);
	checkTrue("100 ms: STOP's path: the enable pin low, level 0, no trip, the touch note",
	          !fakePin(28).high && fakePin(SIG).level == 0 && !s_st.running && !s_st.tripped &&
	          notesAre(UI_TOUCH_LOST_TEXT, C_AMBER, SERVO_UI_TOUCH_LOST_2, C_DIM));
	checkTrue("HOLD TO START refused while silent", !ctlEnabled(UI_FIX(ServoCtl::Start)));
	fakeTouchAnswers(true);
	senseFrames(ina, 2);
	checkTrue("touch back: START live", ctlEnabled(UI_FIX(ServoCtl::Start)));

	// UI frames 100 ms apart: two failed reads, not yet lost on core0, when
	// the touch stamp reaches 250 ms on core1: the job's own cut.
	holdStartFrames();
	senseFrames(ina, 3);
	checkTrue("on again", fakePin(28).high && fakePin(SIG).level == 1500);
	fakeTouchAnswers(false);
	followEn(ina, 28); frameAt(100);
	followEn(ina, 28); frameAt(100);
	checkTrue("200 ms, two failed reads: on", fakePin(28).high && !uiTouchLost());
	fakeAdvance(50);
	core1();
	followEn(ina, 28); frameAt(25);
	checkTrue("250 ms: the enable pin low, level 0, STALLED with the cause",
	          !fakePin(28).high && fakePin(SIG).level == 0 && s_st.tripped &&
	          !uiTouchLost() &&
	          pixel(GFX_W - UI_MARGIN - 4, UI_STRIP_Y + 3) == C_MAGENTA &&
	          notesAre(SERVO_UI_TOUCH_SILENT, C_MAGENTA, "HOLD TO START AGAIN.", C_DIM));
	checkTrue("HOLD TO START refused while silent", !ctlEnabled(UI_FIX(ServoCtl::Start)));
	// The fourth failed read, 100 ms on: lost on core0 too, after core1's cut
	// ended the run. Its note says the servo is stopped.
	followEn(ina, 28); frameAt(100);
	checkTrue("lost on core0 after core1's cut: the touch note, THE SERVO IS STOPPED.",
	          uiTouchLost() && s_st.tripped && !s_st.running && !fakePin(28).high &&
	          notesAre(UI_TOUCH_LOST_TEXT, C_AMBER, SERVO_UI_TOUCH_LOST_2, C_DIM));
	fakeTouchAnswers(true);
	senseFrames(ina, 2);
	checkTrue("touch back: START live, the cause kept",
	          ctlEnabled(UI_FIX(ServoCtl::Start)) && s_st.powerFault == ServoPowerFault::TouchSilent);

	// The bus stuck while the supply comes up: STOP gives way to HOLD TO
	// START, drawn disabled, and the notes say why.
	holdStartFrames();
	senseFrames(ina, 2, 3.6);
	checkTrue("coming up: STOP in red", s_st.running && supplyStarting() &&
	          fillOf(BTN_START) == C_RED);
	fakeI2cHoldSda(true);
	senseFrames(ina, 2, 3.6);
	checkTrue("SDA held while coming up: off, STOP gone, HOLD TO START disabled",
	          !s_st.running && !fakePin(28).high && fillOf(BTN_START) == C_BG &&
	          !ctlEnabled(UI_FIX(ServoCtl::Start)));
	fakeI2cHoldSda(false);
	senseFrames(ina, 80);
	checkTrue("freed", s_st.startBlock == ServoStartBlock::None);

	// START refused with no trip: the notes name the block.
	fakeI2cHoldSda(true);
	senseFrames(ina, 2);
	s_st.tripped = false;   // as after a new visit: the block alone
	s_full = true;
	drawDial();
	checkTrue("SAFE, bus stuck: START WAITS: THE I2C BUS IS STUCK.",
	          s_st.startBlock == ServoStartBlock::BusStuck &&
	          notesAre(SERVO_UI_BUS_WAIT_1, C_AMBER, SERVO_UI_BUS_WAIT_2, C_DIM));
	fakeI2cHoldSda(false);
	senseFrames(ina, 80);
	// Frames 100 ms apart again: core1 refuses START at 250 ms, core0 does
	// not count the touch as lost yet.
	fakeTouchAnswers(false);
	followEn(ina, 28); frameAt(100);
	followEn(ina, 28); frameAt(100);
	fakeAdvance(60);
	followEn(ina, 28); frameAt(25);
	s_st.tripped = false;
	s_full = true;
	drawDial();
	checkTrue("SAFE, touch silent: START WAITS: TOUCH DOES NOT ANSWER.",
	          s_st.startBlock == ServoStartBlock::TouchSilent && !uiTouchLost() &&
	          notesAre(SERVO_UI_TOUCH_WAIT_1, C_AMBER, SERVO_UI_TOUCH_WAIT_2, C_DIM));
	followEn(ina, 28); frameAt(25);
	s_st.tripped = false;
	s_full = true;
	drawDial();
	checkTrue("four failed reads: lost on core0 too, its note first; no run ended: START waits",
	          s_st.startBlock == ServoStartBlock::TouchSilent && uiTouchLost() &&
	          notesAre(UI_TOUCH_LOST_TEXT, C_AMBER, SERVO_UI_TOUCH_WAIT_2, C_DIM));
	fakeTouchAnswers(true);
	senseFrames(ina, 2);

	// The shared bus stuck while the supply is on.
	holdStartFrames();
	senseFrames(ina, 3);
	checkTrue("START cleared it: on", fakePin(28).high && !s_st.tripped);
	fakeI2cHoldSda(true);
	senseFrames(ina, 2);
	checkTrue("SDA held: the enable pin low, STALLED, the cause",
	          !fakePin(28).high && s_st.tripped &&
	          notesAre(SERVO_UI_BUS_STUCK, C_MAGENTA, "HOLD TO START AGAIN.", C_DIM));
	fakeI2cHoldSda(false);
	senseFrames(ina, 80);

	leaveServo();
	checkTrue("BACK: GP28 undriven", enUndriven(28));
	settings()->currentSense = 0;
	senseConfigure(0);
	fakeI2cReset();
	settings()->servoSupply = 0;
	servoSupplyConfigure(0);
	freshUi();
}

/** @brief Whether the strip's chip reads @p text on @p fill, ending at x 232. */
static bool chipIs(const char *text, uint16_t fill) {
	int x = uiChipX(text);
	return pixel(x, UI_STRIP_Y + 3) == fill && pixel(x - 1, UI_STRIP_Y + 3) == C_BG &&
	       fakeTextIs(x + 6, UI_STRIP_Y + 2 + (UI_CHIP_H - 7) / 2, text, C_ONACCENT, fill, 1);
}

/**
 * @brief The cuts on the screen: each cut's chip and notes, the caption
 *        beside the widest chip, the strip's second line, START clearing it.
 */
static void testCutScreens() {
	section("Servo screen: the cuts' chips and notes");

	freshUi();
	fakeI2cReset();
	settings()->servoSupply = 1;
	servoSupplyConfigure(1);
	settings()->currentSense = 1;
	senseConfigure(1);
	FakeIna219 *ina = fakeI2cAddIna219(INA219_ADDR);
	openServo();
	senseFrames(ina, 42);
	checkTrue("found: SAFE, the full caption", chipIs("SAFE", C_GREEN) &&
	          fakeTextIs(UI_MARGIN, UI_STRIP_LINE1_Y, "SIG GP0  50 HZ  1000-2000 US", C_DIM, C_BG, 1));

	// The hard cut.
	holdStartFrames();
	senseFrames(ina, 3);
	checkTrue("RUNNING, the caption kept", chipIs("RUNNING", C_RED) &&
	          fakeTextIs(UI_MARGIN, UI_STRIP_LINE1_Y, "SIG GP0  50 HZ  1000-2000 US", C_DIM, C_BG, 1));
	ina->currentA = 3.2;
	senseFrames(ina, 3);
	checkTrue("3.2 A: the enable pin low, level 0", !fakePin(28).high && fakePin(SIG).level == 0);
	checkTrue("the chip OVERCURRENT, magenta", chipIs(SERVO_UI_CHIP_HARD, C_MAGENTA));
	checkTrue("the caption by its role, 4 px before the chip",
	          fakeTextIs(UI_MARGIN, UI_STRIP_LINE1_Y, "SIG  50 HZ  1000-2000 US", C_DIM, C_BG, 1) &&
	          UI_MARGIN + gfxTextW("SIG  50 HZ  1000-2000 US", 1) + 4 <= uiChipX(SERVO_UI_CHIP_HARD));
	char line2[40];
	snprintf(line2, sizeof(line2), SERVO_UI_SUPPLY_FMT, 28u);
	{
		ServoProfile keep = s_profile;
		s_profile.rateHz = 333;
		s_st.tripped = false;
		drawTop(SERVO_UI_TITLE, true);
		checkTrue("333 HZ beside SAFE: menu-b.md's caption, by its role",
		          fakeTextIs(UI_MARGIN, UI_STRIP_LINE1_Y, "SIG  333 HZ  1000-2000 US", C_DIM, C_BG, 1));
		s_st.tripped = true;
		drawTop(SERVO_UI_TITLE, true);
		checkTrue("333 HZ beside OVERCURRENT: the travel goes",
		          fakeTextIs(UI_MARGIN, UI_STRIP_LINE1_Y, "SIG  333 HZ", C_DIM, C_BG, 1) &&
		          !someOf(UI_MARGIN + gfxTextW("SIG  333 HZ", 1) + 1, UI_STRIP_LINE1_Y,
		                  uiChipX(SERVO_UI_CHIP_HARD) - UI_MARGIN - gfxTextW("SIG  333 HZ", 1) - 2, 7,
		                  C_DIM));
		s_profile = keep;
		drawTop(SERVO_UI_TITLE, true);
	}
	checkTrue("the second line: ON AT START, clear of the chip",
	          fakeTextIs(UI_MARGIN, UI_STRIP_LINE2_Y, line2, C_AMBER, C_BG, 1) &&
	          UI_MARGIN + gfxTextW(line2, 1) + 4 <= uiChipX(SERVO_UI_CHIP_HARD));
	snprintf(line2, sizeof(line2), SERVO_UI_SUPPLY_NOT_OFF_FMT, 29u);
	checkTrue("NOT OFF on GP29 is clear of it too",
	          UI_MARGIN + gfxTextW(line2, 1) + 4 <= uiChipX(SERVO_UI_CHIP_HARD));
	checkTrue("the notes: the cause in magenta, HOLD TO START AGAIN.",
	          notesAre(SERVO_UI_CUT_HARD, C_MAGENTA, "HOLD TO START AGAIN.", C_DIM));
	checkTrue("HOLD TO START live", ctlEnabled(UI_FIX(ServoCtl::Start)));
	fakeDumpFrame("shot_servo_overcurrent.ppm");
	// The guard's cut ended the run, not the touch: a touch lost after it
	// shows HOLD TO START's wait, not THE SERVO IS STOPPED.
	fakeTouchAnswers(false);
	senseFrames(ina, 5);
	checkTrue("lost after OVERCURRENT: the wait, not THE SERVO IS STOPPED.",
	          uiTouchLost() && s_st.powerFault == ServoPowerFault::Overcurrent &&
	          notesAre(UI_TOUCH_LOST_TEXT, C_AMBER, SERVO_UI_TOUCH_WAIT_2, C_DIM));
	fakeTouchAnswers(true);
	senseFrames(ina, 2);
	checkTrue("answering again: the cause in magenta again",
	          notesAre(SERVO_UI_CUT_HARD, C_MAGENTA, "HOLD TO START AGAIN.", C_DIM));
	ina->currentA = 0;
	holdStartFrames();
	senseFrames(ina, 3);
	checkTrue("START: RUNNING, the full caption, the notes gone",
	          chipIs("RUNNING", C_RED) && s_st.powerFault == ServoPowerFault::None &&
	          fakeTextIs(UI_MARGIN, UI_STRIP_LINE1_Y, "SIG GP0  50 HZ  1000-2000 US", C_DIM, C_BG, 1));

	// The stall.
	ina->currentA = 1.5;
	senseFrames(ina, 20);
	checkTrue("1.5 A for 400 ms: STALL", !fakePin(28).high && chipIs(SERVO_UI_CHIP_STALL, C_MAGENTA) &&
	          notesAre(SERVO_UI_CUT_STALL, C_MAGENTA, "HOLD TO START AGAIN.", C_DIM));
	checkTrue("STALL is narrower than RUNNING: the full caption",
	          fakeTextIs(UI_MARGIN, UI_STRIP_LINE1_Y, "SIG GP0  50 HZ  1000-2000 US", C_DIM, C_BG, 1));

	// START and a new cut between two screen frames: the screen never sees
	// RUNNING, and the chip still names the new cut.
	ina->currentA = 3.2;
	{
		int x, y;
		ctlCentre(UI_FIX(ServoCtl::Start), &x, &y);
		pressId(UI_FIX(ServoCtl::Start));
		fakeHold(x, y);
		uiFrames(UI_HOLD_MS / 25);   // the hold completes: START sent
	}
	ServoStatus mid;
	servoJobStatus(&mid);
	checkTrue("the screen's last frame saw no RUNNING", !s_st.running && !mid.running);
	core1JobPoll();
	servoJobStatus(&mid);
	checkTrue("START taken by core1 after that frame", mid.running);
	fakeAdvance(25);
	followEn(ina, 28);
	senseTick(millis());
	core1JobPoll();
	servoJobStatus(&mid);
	checkTrue("the next reading, 3.2 A, cuts before the screen's next frame",
	          !mid.running && mid.powerFault == ServoPowerFault::Overcurrent && !fakePin(28).high);
	fakeRelease();
	uiTick();
	checkTrue("the screen's next frame: OVERCURRENT, not STALL",
	          chipIs(SERVO_UI_CHIP_HARD, C_MAGENTA) &&
	          notesAre(SERVO_UI_CUT_HARD, C_MAGENTA, "HOLD TO START AGAIN.", C_DIM));
	ina->currentA = 0;
	senseFrames(ina, 2);

	// The collapse.
	ina->currentA = 0.3;
	holdStartFrames();
	senseFrames(ina, 3);
	senseFrames(ina, 42, 3.8);
	checkTrue("3.8 V for 1000 ms: LOW SUPPLY", !fakePin(28).high &&
	          chipIs(SERVO_UI_CHIP_LOW, C_MAGENTA) &&
	          notesAre(SERVO_UI_CUT_LOW, C_MAGENTA, "HOLD TO START AGAIN.", C_DIM) &&
	          fakeTextIs(UI_MARGIN, UI_STRIP_LINE1_Y, "SIG  50 HZ  1000-2000 US", C_DIM, C_BG, 1));

	// The sensor lost: unplugged while on.
	holdStartFrames();
	senseFrames(ina, 3);
	fakeI2cRemove(INA219_ADDR);
	senseFrames(ina, 5);
	checkTrue("unplugged: NO SENSOR, START waits for it", !fakePin(28).high &&
	          chipIs(SERVO_UI_CHIP_LOST, C_MAGENTA) &&
	          notesAre(SERVO_UI_CUT_LOST, C_MAGENTA, SERVO_UI_NO_SENSOR_1, C_DIM) &&
	          !ctlEnabled(UI_FIX(ServoCtl::Start)));
	checkTrue("NO SENSOR beside the caption by its role",
	          fakeTextIs(UI_MARGIN, UI_STRIP_LINE1_Y, "SIG  50 HZ  1000-2000 US", C_DIM, C_BG, 1));
	fakeDumpFrame("shot_servo_no_sensor_cut.ppm");
	ina = fakeI2cAddIna219(INA219_ADDR);
	senseFrames(ina, 44);
	checkTrue("back: HOLD TO START AGAIN., the cause kept",
	          ctlEnabled(UI_FIX(ServoCtl::Start)) &&
	          notesAre(SERVO_UI_CUT_LOST, C_MAGENTA, "HOLD TO START AGAIN.", C_DIM));

	leaveServo();
	checkTrue("BACK: GP28 undriven", enUndriven(28));

	// OWN: a sensor that stops answering while RUNNING leaves no cut; the
	// dial says so.
	freshUi();
	servoSupplyConfigure(0);
	fakeI2cReset();
	senseConfigure(0);
	int w = gfxTextW(SERVO_UI_OWN_NO_CUTS, 1);
	openServo();
	holdStartFrames();
	uiFrames(2);
	checkTrue("OWN, CURRENT SENSE NONE, RUNNING: no note",
	          s_st.running && !someOf(0, 76, GFX_W, 7, C_DIM));
	pressId(UI_FIX(ServoCtl::Start));
	core1();
	fakeRelease();
	uiFrames(2);
	settings()->currentSense = 1;
	senseConfigure(1);
	ina = fakeI2cAddIna219(INA219_ADDR);
	senseFrames(ina, 42);
	holdStartFrames();
	senseFrames(ina, 2);
	checkTrue("OWN, RUNNING, the sensor online: no note",
	          s_st.running && !textAt((GFX_W - w) / 2, 76, SERVO_UI_OWN_NO_CUTS, C_DIM, 1));
	fakeI2cRemove(INA219_ADDR);
	senseFrames(ina, 5);
	checkTrue("the sensor gone: still RUNNING, the dim note",
	          s_st.running && textAt((GFX_W - w) / 2, 76, SERVO_UI_OWN_NO_CUTS, C_DIM, 1));
	checkTrue("the note fits the panel", w <= GFX_W - 2 * UI_MARGIN);
	pressId(UI_FIX(ServoCtl::Start));
	core1();
	fakeRelease();
	senseFrames(ina, 2);
	char safe2[48];
	snprintf(safe2, sizeof(safe2), "PULSES ON GP%u ARE THROTTLE TO AN ESC.", (unsigned)gpioOf(PIN_SIG));
	checkTrue("STOP: the note gone, SAFE's notes alone", !s_st.running &&
	          notesAre("THE SERVO MOVES TO CENTRE AT START.", C_DIM, safe2, C_DIM));
	leaveServo();
	settings()->currentSense = 0;
	senseConfigure(0);
	fakeI2cReset();
	freshUi();
}

/** @brief Whether the dial band's second line reads @p text, centred, in @p col. */
static bool logLineIs(const char *text, uint16_t col) {
	return textAt((GFX_W - gfxTextW(text, 1)) / 2, 76, text, col, 1);
}

/**
 * @brief The servo log on SERVO TESTER (servo_log.h, faked here): opened by
 *        HOLD TO START before the Start command, told the Start's seq and
 *        BACK; the dial band's second line while RUNNING.
 */
static void testLogScreen() {
	section("Servo screen: the servo log's calls and its line while RUNNING");
	freshUi();
	ServoLogStatus lg = {};
	lg.active = true;
	lg.fileNumber = 12;
	fakeServoLogSet(&lg);
	fakeServoLogOpen(true, 0);
	openServo();
	uiFrames(2);
	checkTrue("SAFE: no log line, no log opened",
	          !logLineIs("LOGGING TO SRV00012.CSV", C_DIM) && fakeServoLogCalls().starts == 0);
	holdStart();
	uiFrames(2);
	core1();
	FakeServoLogCalls calls = fakeServoLogCalls();
	ServoStatus st;
	servoJobStatus(&st);
	checkTrue("HOLD TO START opens the log once, with the job not running",
	          calls.starts == 1 && !calls.ranAtStart && st.running);
	checkTrue("and gives it the Start command's seq", calls.follows == 1 && calls.seq == st.ackSeq);
	checkTrue("RUNNING: LOGGING TO SRV00012.CSV, dim", logLineIs("LOGGING TO SRV00012.CSV", C_DIM));
	fakeDumpFrame("shot_servo_log.ppm");

	struct { bool active, limit; uint32_t dropped; SdLogEnd fault; const char *text; uint16_t col;
	         bool slow = false, noCache = false; } rows[] = {
		{ true,  false, 12, SdLogEnd::None,         "SRV00012.CSV: 12 ROWS DROPPED",  C_AMBER },
		{ true,  true,  12, SdLogEnd::None,         SERVO_UI_LOG_LIMIT,               C_AMBER },
		{ true,  false, 0,  SdLogEnd::CardFull,     SERVO_UI_LOG_FULL,                C_AMBER },
		{ false, false, 0,  SdLogEnd::CardFull,     SERVO_UI_LOG_NO_ROOM,             C_AMBER },
		{ true,  false, 3,  SdLogEnd::WriteFailed,  SERVO_UI_LOG_FAILED,              C_AMBER },
		{ false, false, 0,  SdLogEnd::OpenFailed,   SERVO_UI_LOG_NO_OPEN,             C_AMBER },
		{ false, false, 0,  SdLogEnd::NoFreeNumber, SERVO_UI_LOG_NO_NUMBER,           C_AMBER },
		{ false, false, 0,  SdLogEnd::None,         SERVO_UI_LOG_SLOW,                C_AMBER, true },
		{ false, false, 0,  SdLogEnd::None,         SERVO_UI_LOG_NO_CACHE,            C_AMBER, false, true },
	};
	for (auto &r : rows) {
		lg.active = r.active;
		lg.atLimit = r.limit;
		lg.slow = r.slow;
		lg.noCache = r.noCache;
		lg.dropped = r.dropped;
		lg.fault = r.fault;
		fakeServoLogSet(&lg);
		uiFrames(1);
		char t[80];
		snprintf(t, sizeof(t), "RUNNING: %s", r.text);
		checkTrue(t, logLineIs(r.text, r.col));
	}
	lg = ServoLogStatus{};
	lg.fileNumber = 12;
	fakeServoLogSet(&lg);
	uiFrames(1);
	checkTrue("no log (no card, no sensor): the line empty",
	          !someOf(0, 76, GFX_W, 7, C_DIM) && !someOf(0, 76, GFX_W, 7, C_AMBER));

	// STOP: SAFE's notes take the line; the log's text is gone.
	lg.active = true;
	fakeServoLogSet(&lg);
	uiFrames(1);
	pressId(UI_FIX(ServoCtl::Start));
	core1();
	fakeRelease();
	uiFrames(2);
	checkTrue("STOP: the line gone with RUNNING",
	          !s_st.running && !logLineIs("LOGGING TO SRV00012.CSV", C_DIM));
	char safe2[48];
	snprintf(safe2, sizeof(safe2), "PULSES ON GP%u ARE THROTTLE TO AN ESC.", (unsigned)gpioOf(PIN_SIG));
	checkTrue("SAFE's notes whole on both lines, the log open or not",
	          notesAre("THE SERVO MOVES TO CENTRE AT START.", C_DIM, safe2, C_DIM));
	checkInt("STOP opens no log", (long)fakeServoLogCalls().starts, 0);
	leaveServo();
	checkInt("BACK tells the log", (long)fakeServoLogCalls().backs, 1);

	// The widest texts fit between the margins.
	char widest[48];
	snprintf(widest, sizeof(widest), SERVO_UI_LOG_DROPPED_FMT, 99999ul, 4294967295ul);
	const char *texts[] = { widest, SERVO_UI_LOG_FULL, SERVO_UI_LOG_FAILED, SERVO_UI_LOG_NO_OPEN,
	                        SERVO_UI_LOG_NO_NUMBER, SERVO_UI_LOG_NO_ROOM, SERVO_UI_LOG_LIMIT,
	                        SERVO_UI_LOG_SLOW, SERVO_UI_LOG_NO_CACHE, SERVO_UI_LOG_PREP_1,
	                        SERVO_UI_LOG_PREP_2 };
	for (const char *t : texts) {
		char what[80];
		snprintf(what, sizeof(what), "fits: %s", t);
		checkTrue(what, gfxTextW(t, 1) <= GFX_W - 2 * UI_MARGIN);
	}

	// OWN with the declared sensor gone: the note that the cuts are blind
	// takes the line from the log's.
	freshUi();
	lg = ServoLogStatus{};
	lg.active = true;
	lg.fileNumber = 3;
	fakeServoLogSet(&lg);
	servoSupplyConfigure(0);
	settings()->currentSense = 1;
	senseConfigure(1);
	FakeIna219 *ina = fakeI2cAddIna219(INA219_ADDR);
	openServo();
	senseFrames(ina, 42);
	holdStartFrames();
	senseFrames(ina, 2);
	checkTrue("OWN, the sensor online: the log's line", logLineIs("LOGGING TO SRV00003.CSV", C_DIM));
	fakeI2cRemove(INA219_ADDR);
	senseFrames(ina, 5);
	checkTrue("the sensor gone: the blind note instead",
	          s_st.running && logLineIs(SERVO_UI_OWN_NO_CUTS, C_DIM) &&
	          !logLineIs("LOGGING TO SRV00003.CSV", C_DIM));
	pressId(UI_FIX(ServoCtl::Start));
	core1();
	fakeRelease();
	uiFrames(2);
	leaveServo();
	settings()->currentSense = 0;
	senseConfigure(0);
	fakeI2cReset();
	lg = ServoLogStatus{};
	fakeServoLogSet(&lg);
	fakeServoLogOpen(false, 0);
	freshUi();
}

/**
 * @brief HOLD TO START with a log to open: the note in the frame the hold
 *        fires, the open in the next, the Start in the one after, so an open
 *        that holds core0 800 ms, past the heartbeat's 250 ms, never makes
 *        core1 refuse the Start. With the switch, the touch rule too.
 */
static void testLogStartOrder() {
	section("Servo screen: a slow log open, then a later frame's Start");
	for (int sw = 0; sw < 2; sw++) {
		freshUi();
		settings()->servoSupply = (uint8_t)sw;
		servoSupplyConfigure((uint8_t)sw);
		ServoLogStatus lg = {};
		lg.active = true;
		lg.fileNumber = 5;
		fakeServoLogSet(&lg);
		fakeServoLogOpen(true, 800);
		openServo();
		uiFrames(2);
		int x, y;
		ctlCentre(UI_FIX(ServoCtl::Start), &x, &y);
		pressId(UI_FIX(ServoCtl::Start));
		for (int i = 0; i < 80 && s_startStep == StartStep::None; i++) { fakeHold(x, y); uiFrames(1); }
		fakeRelease();
		checkTrue(sw ? "switched: the hold fired: the note, no open yet" : "the hold fired: the note, no open yet",
		          s_startStep == StartStep::Note && fakeServoLogCalls().starts == 0 &&
		          notesAre(SERVO_UI_LOG_PREP_1, C_DIM, SERVO_UI_LOG_PREP_2, C_DIM));
		if (!sw) fakeDumpFrame("shot_servo_log_prep.ppm");
		uint32_t before = millis();
		uiFrames(1);
		FakeServoLogCalls c = fakeServoLogCalls();
		checkTrue("the next frame opens the log, 800 ms on core0, no Start sent",
		          c.starts == 1 && c.follows == 0 && millis() - before >= 800 &&
		          s_startStep == StartStep::Open);
		core1();
		ServoStatus st;
		servoJobStatus(&st);
		checkTrue("core1 after the open: still SAFE, no Start to take", !st.running && st.ackSeq == c.ackAtStart);
		uiFrames(1);
		core1();
		c = fakeServoLogCalls();
		servoJobStatus(&st);
		checkTrue("the frame after: the Start sent and taken, the heartbeat and touch fresh",
		          c.follows == 1 && c.startsAtFollow == 1 && st.running && c.seq == st.ackSeq &&
		          s_startStep == StartStep::None);
		uiFrames(1);
		pressId(UI_FIX(ServoCtl::Start));
		core1();
		fakeRelease();
		uiFrames(2);
		leaveServo();
	}
	// The touch lost while the Start waits: the touch note over the log's,
	// no Start, the log told to end. Three failed reads on record and 100 ms
	// make the open's frame the 4th failed read, lost.
	{
		freshUi();
		settings()->servoSupply = 0;
		servoSupplyConfigure(0);
		ServoLogStatus lg = {};
		lg.active = true;
		fakeServoLogSet(&lg);
		fakeServoLogOpen(true, 800);
		openServo();
		uiFrames(2);
		int x, y;
		ctlCentre(UI_FIX(ServoCtl::Start), &x, &y);
		pressId(UI_FIX(ServoCtl::Start));
		for (int i = 0; i < 80 && s_startStep == StartStep::None; i++) { fakeHold(x, y); uiFrames(1); }
		fakeRelease();
		fakeTouchAnswers(false);
		fakeTouchFails(3);
		fakeAdvance(100);
		uiFrames(1);
		checkTrue("lost in the open's frame: opened, the touch note over the log's",
		          s_startStep == StartStep::Open && uiTouchLost() &&
		          notesAre(UI_TOUCH_LOST_TEXT, C_AMBER, SERVO_UI_TOUCH_WAIT_2, C_DIM));
		ServoStatus st;
		servoJobStatus(&st);
		uint16_t ack = st.ackSeq;
		uiFrames(1);
		core1();
		servoJobStatus(&st);
		FakeServoLogCalls c = fakeServoLogCalls();
		checkTrue("the Start's frame: no Start, the log given the last seq",
		          uiTouchLost() && !st.running && c.follows == 1 && c.seq == ack &&
		          s_startStep == StartStep::None);
		checkTrue("the touch note stays, START waits",
		          notesAre(UI_TOUCH_LOST_TEXT, C_AMBER, SERVO_UI_TOUCH_WAIT_2, C_DIM));
		fakeTouchAnswers(true);
		uiFrames(2);
		leaveServo();
	}

	// BACK between the open and the Start: no Start, the log told to end.
	freshUi();
	settings()->servoSupply = 0;
	servoSupplyConfigure(0);
	ServoLogStatus open = {};
	open.active = true;
	fakeServoLogSet(&open);
	fakeServoLogOpen(true, 0);
	openServo();
	uiFrames(2);
	int x, y;
	ctlCentre(UI_FIX(ServoCtl::Start), &x, &y);
	pressId(UI_FIX(ServoCtl::Start));
	for (int i = 0; i < 80 && s_startStep == StartStep::None; i++) { fakeHold(x, y); uiFrames(1); }
	// BACK pressed in the open's frame and lifted in the next, which takes
	// BACK before the Start.
	UiControl back;
	ctlGet(UI_FIX(UI_BACK), &back, IdExpect::Enabled);
	fakePress(back.rect.x + back.rect.w / 2, back.rect.y + back.rect.h / 2);
	uiFrames(1);
	checkTrue("opened, the Start due next frame", s_startStep == StartStep::Open);
	ServoStatus st;
	servoJobStatus(&st);
	uint16_t ack = st.ackSeq;
	fakeRelease();
	uiFrames(1);
	core1();
	servoJobStatus(&st);
	FakeServoLogCalls c = fakeServoLogCalls();
	checkTrue("BACK: no Start, the log given the last seq", !st.running && c.follows == 1 &&
	          c.seq == ack && s_startStep == StartStep::None);
	for (int i = 0; i < 200 && !ctlEnabled(UI_FIX(ServoCtl::Restart)); i++) uiFrames(1);
	tapId(UI_FIX(ServoCtl::Restart));
	uiFrames(2);
	fakeServoLogOpen(false, 0);
	ServoLogStatus none = {};
	fakeServoLogSet(&none);
	freshUi();
}

/** @brief @p n UI frames of 25 ms, rcbench's servo model behind the timed fake INA219. */
static void uiSimFrames(FakeIna219 *ina, SimServo *sim, int n) {
	for (int i = 0; i < n; i++) {
		followEn(ina, 28);
		frameAt(25);
		simTrack(sim);
	}
}

/**
 * @brief The move timing on SET: the note band's second line when free, in
 *        C_DIM, over the start-up time; kept while SAFE; gone at START;
 *        under a two-line note.
 */
static void testMoveScreen() {
	section("Servo screen: the move timing on SET");

	freshUi();
	fakeI2cReset();
	settings()->servoSupply = 1;
	servoSupplyConfigure(1);
	settings()->currentSense = 1;
	senseConfigure(1);
	FakeIna219 *ina = fakeI2cAddIna219(INA219_ADDR);
	SimServo sim;
	freeServo(&sim);
	simAttach(ina, &sim);
	openServo();
	// 38 frames: the way through SERVO, the hub, took 4 of the 42 that
	// set the sensor's phase against the first step.
	uiSimFrames(ina, &sim, 38);
	holdStartFrames();
	uiSimFrames(ina, &sim, 3);
	checkTrue("running at CENTRE", s_st.running && fakePin(SIG).level == 1500);
	openSet();
	char up[32];
	snprintf(up, sizeof(up), SERVO_UI_UP_FMT, (unsigned)s_st.upMs);
	checkTrue("no step yet: the start-up time on the second line",
	          s_st.move.state == MoveState::None && textAt(UI_MARGIN, UI_STRIP_Y + 12, up, C_DIM, 1));
	backFromSet();

	uiSimFrames(ina, &sim, 144);
	tapId(UI_FIX(ServoCtl::Max));
	uiSimFrames(ina, &sim, 1);
	checkTrue("MAX at rest: timed", s_st.move.state == MoveState::Measuring &&
	          fakePin(SIG).level == 2000);
	uiSimFrames(ina, &sim, 45);
	checkTrue("arrived, and the hold in", s_st.move.state == MoveState::Arrived && s_st.move.holdOk &&
	          s_st.move.peakOk);
	tapId(UI_FIX(ServoCtl::Centre));
	uiSimFrames(ina, &sim, 1);
	checkTrue("CENTRE, half a second on: timed", s_st.move.state == MoveState::Measuring &&
	          fakePin(SIG).level == 1500);
	uiSimFrames(ina, &sim, 45);
	tapId(UI_FIX(ServoCtl::Max));
	uiSimFrames(ina, &sim, 1);
	checkTrue("MAX again: timed", s_st.move.state == MoveState::Measuring);
	uiSimFrames(ina, &sim, 45);
	checkTrue("arrived, the hold in", s_st.move.state == MoveState::Arrived && s_st.move.holdOk);
	// Both stamps are frame times, so the result is a multiple of the 25 ms
	// frame: the first frame at or after the model's 417 ms, and no later
	// than MOVE_LAG_MS after it. Which of 425, 450 and 475 depends on the
	// sensor's phase against the frames, which the frame count before START sets.
	checkTrue("a frame multiple, 417 ms to MOVE_LAG_MS after the model's 417 ms",
	          s_st.move.ms % 25 == 0 && s_st.move.ms >= (417 + 24) / 25 * 25 &&
	          s_st.move.ms <= 417 + MOVE_LAG_MS);
	char line[MOVE_TEXT_MAX + 1];
	moveTimerText(&s_st.move, line, sizeof(line));
	checkTrue("the line: MOVE, PEAK and HOLD", strncmp(line, "MOVE 0.4", 8) == 0 &&
	          strstr(line, "  PEAK 0.9") != nullptr && strstr(line, "  HOLD 0.1") != nullptr);
	openSet();
	checkTrue("SET while running: STOP TO CHANGE THE PROFILE, then the move, dim",
	          textAt(UI_MARGIN, UI_STRIP_Y + 2, "STOP TO CHANGE THE PROFILE", C_AMBER, 1) &&
	          textAt(UI_MARGIN, UI_STRIP_Y + 12, line, C_DIM, 1));
	checkTrue("ends at or before the right margin, 8 px",
	          UI_MARGIN + gfxTextW(line, 1) <= GFX_W - UI_MARGIN);
	fakeDumpFrame("shot_servo_set_move.ppm");
	tapId(UI_FIX(ServoCtl::Defaults));   // STOP while RUNNING
	uiSimFrames(ina, &sim, 2);
	checkTrue("STOP on SET: SAFE, the line kept", !s_st.running &&
	          textAt(UI_MARGIN, UI_STRIP_Y + 12, line, C_DIM, 1));
	// A new profile: MIN 900 us applies at once and rebinds, which drops the
	// result; MIN 800 us, past the travel, waits for HOLD TO APPLY.
	selectRow(3);
	stepSel(-1, 10);
	uiSimFrames(ina, &sim, 3);
	snprintf(up, sizeof(up), SERVO_UI_UP_FMT, (unsigned)s_st.upMs);
	checkTrue("MIN 900 us applied: the rebind drops the result, the start-up time back",
	          !s_pending && s_profile.minUs == 900 && s_st.move.state == MoveState::None &&
	          textAt(UI_MARGIN, UI_STRIP_Y + 12, up, C_DIM, 1));
	stepSel(-1, 10);
	holdApply();
	uiSimFrames(ina, &sim, 3);
	checkTrue("MIN 800 us applied", !s_pending && s_profile.minUs == 800);
	backFromSet();
	// A move timed on a profile past its travel: SAFE's SET shows the
	// end-stop note on both lines, over the move.
	holdStartFrames();
	uiSimFrames(ina, &sim, 144);
	tapId(UI_FIX(ServoCtl::Max));
	uiSimFrames(ina, &sim, 45);
	pressId(UI_FIX(ServoCtl::Start));
	core1();
	fakeRelease();
	uiSimFrames(ina, &sim, 2);
	openSet();
	checkTrue("a move timed, SAFE", !s_st.running && s_st.move.state == MoveState::Arrived);
	checkTrue("a two-line note takes the second line: no move line over it",
	          textAt(UI_MARGIN, UI_STRIP_Y + 12, "HEATS. TRY MIN AND MAX HORN-FREE.", C_AMBER, 1) &&
	          !anyOf(0, UI_STRIP_Y + 12, GFX_W, 7, C_DIM));
	backFromSet();
	holdStartFrames();
	uiSimFrames(ina, &sim, 3);
	openSet();
	snprintf(up, sizeof(up), SERVO_UI_UP_FMT, (unsigned)s_st.upMs);
	checkTrue("START: the move gone, the start-up time back",
	          s_st.move.state == MoveState::None && textAt(UI_MARGIN, UI_STRIP_Y + 12, up, C_DIM, 1));
	backFromSet();

	// A step too soon after START: dashes.
	tapId(UI_FIX(ServoCtl::Min));
	uiSimFrames(ina, &sim, 2);
	openSet();
	checkTrue("MIN within 3.5 s of START: MOVE -- S, not timed",
	          s_st.move.state == MoveState::Void &&
	          textAt(UI_MARGIN, UI_STRIP_Y + 12, "MOVE -- S  PEAK -- A  HOLD -- A", C_DIM, 1));
	backFromSet();
	pressId(UI_FIX(ServoCtl::Start));
	core1();
	fakeRelease();
	uiFrames(2);
	leaveServo();
	settings()->currentSense = 0;
	senseConfigure(0);
	fakeI2cReset();
	settings()->servoSupply = 0;
	servoSupplyConfigure(0);
	freshUi();
}

/**
 * @brief The move timing on SET with CURRENT SENSE at NONE: a step leaves
 *        the second line as it was, no move and no dashes.
 */
static void testMoveScreenNone() {
	section("Servo screen: a step with CURRENT SENSE at NONE leaves SET's line");

	freshUi();
	fakeI2cReset();
	settings()->currentSense = 0;
	senseConfigure(0);
	openServo();
	// No sensor: nothing here depends on a phase, so the count is free.
	uiFrames(42);
	holdStartFrames();
	uiFrames(3);
	checkTrue("running at CENTRE, no sensor", s_st.running && fakePin(SIG).level == 1500);
	openSet();
	checkTrue("before a step: no line on the second row",
	          s_st.move.state == MoveState::None && !anyOf(0, UI_STRIP_Y + 12, GFX_W, 7, C_DIM));
	backFromSet();

	uiFrames(144);
	tapId(UI_FIX(ServoCtl::Max));
	uiFrames(2);
	checkTrue("MAX at rest: sent", fakePin(SIG).level == 2000);
	uiFrames(160);
	tapId(UI_FIX(ServoCtl::Centre));
	uiFrames(2);
	checkTrue("CENTRE: sent", fakePin(SIG).level == 1500);
	openSet();
	checkTrue("after two steps: nothing timed, the line as before, no dashes",
	          s_st.move.state == MoveState::None &&
	          !textAt(UI_MARGIN, UI_STRIP_Y + 12, "MOVE -- S  PEAK -- A  HOLD -- A", C_DIM, 1) &&
	          !anyOf(0, UI_STRIP_Y + 12, GFX_W, 7, C_DIM));
	backFromSet();
	pressId(UI_FIX(ServoCtl::Start));
	core1();
	fakeRelease();
	uiFrames(2);
	leaveServo();
}

/**
 * @brief SERVO names SIG by role and number: the caption, the claim's
 *        messages, and the refusal no UI path reaches.
 */
static void testPinNames() {
	section("Servo screen: SIG by role and number, the role alone where the strip is full");

	freshUi();
	// The caption, with the strip's widest chip, RUNNING, at x 179.
	char cap[40];
	ServoProfile keep = s_profile;
	s_profile.rateHz = 50; s_profile.minUs = 1000; s_profile.maxUs = 2000;
	servoCaption(cap, sizeof(cap), false, 0);
	checkStr("one servo: SIG GP0", cap, "SIG GP0  50 HZ  1000-2000 US");
	servoCaption(cap, sizeof(cap), true, 0);
	checkStr("two servos: SIG+AUX", cap, "SIG+AUX  50 HZ  1000-2000 US");
	s_profile.rateHz = 333;
	servoCaption(cap, sizeof(cap), false, 0);
	checkStr("333 HZ, one servo: SIG alone, the number has no room", cap, "SIG  333 HZ  1000-2000 US");
	servoCaption(cap, sizeof(cap), true, 0);
	checkStr("333 HZ, two servos: the travel goes", cap, "SIG+AUX  333 HZ");
	int wide = uiChipX(SERVO_UI_CHIP_HARD) - 4;
	servoCaption(cap, sizeof(cap), false, 0, wide);
	checkStr("333 HZ beside OVERCURRENT, one servo: the travel goes too", cap, "SIG  333 HZ");
	s_profile.rateHz = 50;
	servoCaption(cap, sizeof(cap), false, 0, wide);
	checkStr("50 HZ beside OVERCURRENT: by its role, the travel kept", cap, "SIG  50 HZ  1000-2000 US");
	checkTrue("and it ends 4 px before the chip", UI_MARGIN + gfxTextW(cap, 1) <= wide);
	servoCaption(cap, sizeof(cap), true, 0, wide);
	checkStr("two servos beside OVERCURRENT", cap, "SIG+AUX  50 HZ");
	servoCaption(cap, sizeof(cap), false, 0, uiChipX(SERVO_UI_CHIP_LOW) - 4);
	checkStr("50 HZ beside LOW SUPPLY: by its role", cap, "SIG  50 HZ  1000-2000 US");
	s_profile.rateHz = 333;
	s_profile.minUs = 820; s_profile.maxUs = 2220;
	servoCaption(cap, sizeof(cap), false, 1);
	checkStr("820-2220 at 333 HZ on GP1: 28 characters fit", cap, "SIG GP1  333 HZ  820-2220 US");
	checkTrue("and end 4 px before RUNNING",
	          UI_MARGIN + gfxTextW(cap, 1) + 4 <= GFX_W - UI_MARGIN - gfxTextW("RUNNING", 1) - 12);
	s_profile = keep;

	// An ESC answers on SIG: the claim waits, and the dial band says where.
	EscTelemetry esc;
	memset(&esc, 0, sizeof(esc));
	esc.lastRpmMs = millis();
	fakeSetTelemetry(&esc);
	openServo();
	const char *l1, *l2, *l3; uint16_t col; bool retry;
	claimMessage(&l1, &l2, &l3, &col, &retry);
	checkTrue("AN ESC ANSWERS ON SIG GP0.", l1 && strcmp(l1, "AN ESC ANSWERS ON SIG GP0.") == 0);
	// A claim refused for another holder: unreachable through MENU.
	s_refusal = ClaimRefusal::Busy;
	claimMessage(&l1, &l2, &l3, &col, &retry);
	checkTrue("another claim holds: WAIT: DSHOT RESTARTING, amber",
	          l1 && strcmp(l1, UI_WAIT_RESTART_TEXT) == 0 && col == C_AMBER);
	tapId(UI_FIX(UI_BACK));
	noEsc();
	uiFrames(2);
	backFromMenu();

	// The first frame of a claim: the pump still drains.
	freshUi();
	tapId(UI_FIX(TesterCtl::Menu));
	uiFrames(1);
	tapId(toolEntry(&UI_SERVO));
	uiTick();
	claimMessage(&l1, &l2, &l3, &col, &retry);
	checkTrue("RELEASING SIG GP0 while the pump drains",
	          pinClaimState() == ClaimState::Stopping && l1 && strcmp(l1, "RELEASING SIG GP0") == 0);
	uiFrames(3);
	leaveServo();
	backFromMenu();

	// A bind the PWM refuses.
	freshUi();
	fakePwmBreakSliceMap(true);
	openServo();
	claimMessage(&l1, &l2, &l3, &col, &retry);
	checkTrue("SIG GP0 COULD NOT BE TAKEN (PWM)",
	          l1 && strcmp(l1, "SIG GP0 COULD NOT BE TAKEN (PWM)") == 0 && retry);
	fakePwmBreakSliceMap(false);
	leaveServo();
	backFromMenu();
	freshUi();
}

void runServoTests() {
	testProfiles();
	testValidity();
	testSteppers();
	testMapping();
	testSweepSteppers();
	testSpeed();
	testDial();
	testPwm();
	testCoreStates();
	testCoreSlew();
	testCoreSweep();
	testCoreTrip();
	testJob();
	testJobTrip();
	testPowerOwn();
	testPowerStart();
	testPowerNoStart();
	testPowerOff();
	testPowerStartBlock();
	testPowerReadBack();
	testPowerCuts();
	testJobSupply();
	testJobSupplySensed();
	testJobCuts();
	testJobMoveTiming();
	testQuietRule();
	testLastPulse();
	testTripLatch();
	testSlider();
	testSliderDraw();
	testScreenOpens();
	testHoldToStart();
	testStartNeedsLivePress();
	testHorn();
	testButtonsAndSweep();
	testTouchLossStops();
	testTwoServos();
	testAuxRefused();
	testEscAnswering();
	testLeavePage();
	testSetScreen();
	testSetGuard();
	testTravelNote();
	testSetList();
	testClaimCorners();
	testBindRefused();
	testSenseReadout();
	testSupplyLineAndHint();
	testSupplyScreenFaults();
	testCutScreens();
	testLogScreen();
	testLogStartOrder();
	testMoveScreen();
	testMoveScreenNone();
	testColourRoles();
	testPinNames();
	freshUi();
}
