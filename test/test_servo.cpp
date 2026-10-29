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
#include "ui_servo.h"
#include "ui_kst.h"
#include "ui_slider.h"
#include "ui.h"
#include "gfx.h"
#include "touch.h"
#include "settings.h"
#include "plat.h"

#include "../src/ui_servo.cpp"   // NOLINT -- see the note above

#include <hardware/gpio.h>
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

/** @brief Whether MENU row @p id shows a status in C_AMBER: PINS BUSY. */
static bool busyShown(UiControlId id) {
	UiRect r = ctlRect(id);
	for (int y = r.y; y < r.y + r.h; y++)
		for (int x = r.x; x < r.x + r.w; x++)
			if (pixel(x, y) == C_AMBER) return true;
	return false;
}

/** @brief Fresh board: default settings, no ESC, the main screen up. */
static void freshUi() {
	fakeFlashClear();
	settingsLoad();
	fakePwmReset();
	noEsc();
	uiInit();
	escTaskConfigure(0, 600, false, 1);
	uiFrames(2);
}

/** @brief Main screen to SERVO, through MENU. */
static void openServo() {
	tapId(UI_FIX(TesterCtl::Menu));
	uiFrames(1);
	checkTrue("MENU opens", uiCurrentScreen() == UiScreenId::Menu);
	tapId(menuRow(&UI_SERVO));
	uiFrames(2);
	checkTrue("SERVO opens from MENU", uiCurrentScreen() == UiScreenId::Servo);
}

/** @brief SERVO's main view to SET. */
static void openSet() {
	tapId(UI_FIX(ServoCtl::Set));
	checkTrue("SET opens: HOLD TO APPLY is there", ctlPresent(UI_FIX(ServoCtl::Apply)));
}

/** @brief BACK from SET: SERVO's main view again. */
static void backFromSet() {
	tapId(UI_FIX(UI_BACK));
	checkTrue("BACK from SET reaches SERVO's main view",
	          uiCurrentScreen() == UiScreenId::Servo && ctlPresent(UI_FIX(ServoCtl::Start)));
}

/** @brief BACK from MENU: the tester. */
static void backFromMenu() {
	tapId(UI_FIX(UI_BACK));
	checkTrue("BACK from MENU reaches the tester", uiCurrentScreen() == UiScreenId::Tester);
}

/** @brief Hold HOLD TO START for 1000 ms, then lift. */
static void holdStart() {
	int x, y;
	ctlCentre(UI_FIX(ServoCtl::Start), &x, &y);
	pressId(UI_FIX(ServoCtl::Start));
	fakeHold(x, y); frameAt(SV_HOLD_MS);
	fakeRelease(); uiFrames(1);
	core1();
}

/** @brief BACK, the unplug page, and RESTART DSHOT: out to MENU. */
static void leaveServo() {
	tapId(UI_FIX(UI_BACK));
	uiFrames(1);
	// The page offers RESTART DSHOT once the job has let go of the pin.
	for (int i = 0; i < 200 && !ctlPresent(UI_FIX(ServoCtl::Restart)); i++) uiFrames(1);
	tapId(UI_FIX(ServoCtl::Restart));
	uiFrames(2);
	checkTrue("RESTART DSHOT leaves SERVO for MENU", uiCurrentScreen() == UiScreenId::Menu);
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

/** @brief SERVO opens from MENU, claims both pins, and starts SAFE. */
static void testScreenOpens() {
	section("Servo screen: MENU opens it, and it starts SAFE");

	freshUi();
	tapId(UI_FIX(TesterCtl::Menu));
	uiFrames(2);
	checkTrue("MENU opens", uiCurrentScreen() == UiScreenId::Menu);
	checkTrue("MENU: SERVO is drawn live", fillOf(ctlRect(menuRow(&UI_SERVO))) == C_PANEL);
	checkTrue("MENU: KST SERVO is drawn live", fillOf(ctlRect(menuRow(&UI_KST))) == C_PANEL);
	tapId(menuRow(&UI_SERVO));
	checkTrue("SERVO opens from MENU", uiCurrentScreen() == UiScreenId::Servo);
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
	fakeHold(x, y); frameAt(SV_HOLD_MS - 1);
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
	fakeHold(x, y); frameAt(SV_HOLD_MS);
	fakeHold(x, y); frameAt(SV_HOLD_MS);
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
	fakeHold(x, y); frameAt(SV_HOLD_MS + 100);
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
	tapId(menuRow(&UI_SERVO));
	checkTrue("SERVO opens from MENU", uiCurrentScreen() == UiScreenId::Servo);
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
	tapId(menuRow(&UI_SERVO));
	checkTrue("SERVO opens from MENU", uiCurrentScreen() == UiScreenId::Servo);
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
	tapId(menuRow(&UI_SERVO));
	checkTrue("SERVO opens from MENU", uiCurrentScreen() == UiScreenId::Servo);
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

/**
 * @brief SamePin and KissLive keep the screen to one servo, through the claim.
 */
static void testAuxRefused() {
	section("Servo screen: TWO is refused when the claim refuses AUX");

	freshUi();
	escTaskConfigure(1, 600, false, 1);   // SETTINGS put the ESC pin on the KISS GPIO
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
	checkTrue("the note names the cause", l1 && strcmp(l1, "ESC AND KISS PIN ARE THE SAME GPIO.") == 0);
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
	checkTrue("the note names the KISS wire", l1 && strcmp(l1, "KISS WIRE ON GP1 IS LIVE.") == 0 &&
	                                          l2 && strcmp(l2, "TWO SERVOS NEED IT UNPLUGGED.") == 0);
	stepSel(+1, 1, IdExpect::Disabled);
	checkTrue("SERVOS stays ONE", s_profile.servos == 1);
	backFromSet();
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

/** @brief The unplug page: BACK keeps the claim, RESTART DSHOT restarts it. */
static void testUnplugPage() {
	section("Servo screen: the unplug page before DShot restarts");

	freshUi();
	openServo();
	holdStart();
	int resumes = fakeResumeCount();
	int cuts = fakePin(0).cutPulses;
	tapId(UI_FIX(UI_BACK));
	checkTrue("BACK while RUNNING: the page waits for the last pulse",
	          s_view == View::Unplug && !pinClaimUnplugDue() &&
	          core1JobPhase() == Core1JobPhase::Stopping);
	checkTrue("the page waits: BACK does not act, RESTART is not there",
	          ctlPresent(UI_FIX(UI_BACK)) && !ctlEnabled(UI_FIX(UI_BACK)) &&
	          !ctlPresent(UI_FIX(ServoCtl::Restart)));
	checkControlTable("the unplug page, the job not yet let go");
	core1();
	checkTrue("core1's next pass: still stopping", core1JobPhase() == Core1JobPhase::Stopping);
	checkTrue("level 0 written, SIG still on PWM",
	          fakePin(0).level == 0 && fakePin(0).function == GPIO_FUNC_PWM);
	checkInt("and no DShot", fakeResumeCount(), resumes);
	// BACK on the page before the job has let go: pinClaimKeep() would
	// refuse, so the page ignores it and stays.
	// The frames are 5 ms apart here, so the press is by hand, at BACK's centre.
	int bx, by;
	ctlCentre(UI_FIX(UI_BACK), &bx, &by);
	fakePress(bx, by); frameAt(5);
	fakeRelease(); frameAt(5);
	checkTrue("BACK while releasing: ignored, the page stays",
	          s_view == View::Unplug && pinClaimState() == ClaimState::Leaving &&
	          !pinClaimUnplugDue());
	uiFrames(2);
	checkTrue("BACK: the unplug page", s_view == View::Unplug && pinClaimUnplugDue());
	checkTrue("the job let go: BACK acts and RESTART DSHOT is there",
	          ctlEnabled(UI_FIX(UI_BACK)) && ctlEnabled(UI_FIX(ServoCtl::Restart)));
	checkControlTable("the unplug page, the job let go");
	checkInt("no pulse was cut short", fakePin(0).cutPulses, cuts);
	checkTrue("the job let go: SIG an input", fakePin(0).function == GPIO_FUNC_SIO);
	checkInt("DShot not yet restarted", fakeResumeCount(), resumes);
	fakeDumpFrame("shot_servo_unplug.ppm");
	tapId(UI_FIX(UI_BACK));
	uiFrames(2);
	checkTrue("BACK on the page: SERVO again, the claim kept",
	          s_view == View::Main && pinClaimState() == ClaimState::Running);
	checkTrue("SAFE again, rebound at level 0",
	          !s_st.running && fakePin(0).function == GPIO_FUNC_PWM && fakePin(0).level == 0);
	checkInt("still no DShot", fakeResumeCount(), resumes);
	tapId(UI_FIX(UI_BACK));
	uiFrames(1);
	tapId(UI_FIX(ServoCtl::Restart));
	checkInt("RESTART DSHOT: DShot back", fakeResumeCount(), resumes + 1);
	uiFrames(2);
	checkTrue("free, and MENU up", pinClaimState() == ClaimState::Free &&
	                                   uiCurrentScreen() == UiScreenId::Menu &&
	                                   fillOf(ctlRect(menuRow(&UI_SERVO))) == C_PANEL);
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
	checkTrue("HELI CYCLIC: held as pending", s_edit.type == ServoType::HeliCyclic && s_pending &&
	                                          s_profile.type == ServoType::Wide);
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
	          s_pending && s_profile.type == ServoType::Wide && fakeSlice(0).top == 19999);
	fakeRelease(); uiFrames(1);
	pressId(UI_FIX(ServoCtl::Apply));
	fakeHold(x, y); frameAt(SV_HOLD_MS - 1); core1();
	checkTrue("held 999 ms: not applied", s_pending && fakeSlice(0).top == 19999);
	checkTrue("the bar shows the hold, the button red",
	          pixel(BTN_APPLY.x + 2, SET_FOOT_Y - 3) == C_LIME && fillOf(BTN_APPLY) == C_RED);
	fakeHold(x, y); frameAt(1); core1();
	checkTrue("1000 ms: applied", !s_pending && s_profile.type == ServoType::HeliCyclic);
	checkInt("rebound at 333 Hz", fakeSlice(0).top, 3002);
	fakeRelease(); uiFrames(1);
	selectRow(3);   // MIN, under the applied heli profile
	stepSel(-1);
	checkTrue("MIN under an applied heli profile: at once, no warning",
	          !s_pending && s_profile.minUs == 810);
	selectRow(1);
	stepSel(-1);
	checkTrue("back to WIDE at 50 Hz: at once", !s_pending && fakeSlice(0).top == 19999);

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
	checkTrue("MIN -30 us at once", s_profile.minUs == 770);
	int minY = rowCentreY(3);
	fakePress(200, minY); uiFrames(1);
	for (int px = 200; px >= 200 - UI_AXIS_LOCK_PX - 2 - UI_SWIPE_STEP_PX; px -= 4) {
		fakeHold(px, minY); uiFrames(1);
	}
	fakeRelease(); uiFrames(1);
	checkInt("a swipe left on MIN: one coarse step, -100 us", s_profile.minUs, 670);

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

/**
 * @brief Claim refusals the UI cannot reach, and TWO chosen before the claim.
 */
static void testClaimCorners() {
	section("Servo screen: refusals the UI cannot reach, and TWO before the claim");

	// Another screen holds the pins. MENU refuses SERVO while the claim is
	// not Free: the row reads PINS BUSY, does not act, and a tap opens
	// nothing. Once the claim is Free, the row opens SERVO, which claims at
	// once.
	freshUi();
	checkTrue("another screen holds the pins", pinClaimBegin(PinUser::Am32, PIN_SIG) == ClaimRefusal::None);
	tapId(UI_FIX(TesterCtl::Menu));
	uiFrames(1);
	checkTrue("MENU opens", uiCurrentScreen() == UiScreenId::Menu);
	checkTrue("MENU: SERVO reads PINS BUSY", busyShown(menuRow(&UI_SERVO)));
	checkTrue("and its row does not act", !ctlEnabled(menuRow(&UI_SERVO)));
	tapId(menuRow(&UI_SERVO), IdExpect::Disabled);
	checkTrue("a tap opens nothing, and SERVO claims nothing",
	          uiCurrentScreen() == UiScreenId::Menu && pinClaimHolder() == PinUser::Am32);
	pinClaimEnd(PinUser::Am32);
	for (int i = 0; i < 20 && !ctlEnabled(menuRow(&UI_SERVO)); i++) uiFrames(1);
	checkTrue("the other claim gone: the row acts again",
	          pinClaimState() == ClaimState::Free && ctlEnabled(menuRow(&UI_SERVO)) &&
	          !busyShown(menuRow(&UI_SERVO)));
	tapId(menuRow(&UI_SERVO));
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
	noEsc();
}

/** @brief A refused bind: the message, RETRY, and the rebind path. */
static void testBindRefused() {
	section("Servo screen: a refused bind and RETRY");

	freshUi();
	fakePwmBreakSliceMap(true);
	openServo();
	checkTrue("the claim is held with the reason", pinClaimJobError() == CORE1_JOB_PWM);
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
	checkTrue("HOLD TO START is an outline", fillOf(BTN_START) == C_BG);
	fakePwmBreakSliceMap(false);
	tapId(UI_FIX(ServoCtl::Retry));
	core1();
	uiFrames(2);
	checkTrue("RETRY rebinds", s_st.bindError == 0 && fakePin(0).function == GPIO_FUNC_PWM);
	leaveServo();
	backFromMenu();
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
	testTwoServos();
	testAuxRefused();
	testEscAnswering();
	testUnplugPage();
	testSetScreen();
	testSetList();
	testClaimCorners();
	testBindRefused();
	freshUi();
}
