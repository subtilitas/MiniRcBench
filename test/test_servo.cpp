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
#include "ui_slider.h"
#include "gfx.h"
#include "touch.h"
#include "plat.h"

#include <hardware/gpio.h>
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
	checkTrue("124 px: inside", servoOnDial(SERVO_DIAL_X + 124, SERVO_DIAL_Y, &deg));
	checkTrue("125 px: outside the outer edge, refused",
	          !servoOnDial(SERVO_DIAL_X + 125, SERVO_DIAL_Y, &deg));

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

#define SIG 29
#define AUX 28

/** @brief The PWM driver against the register fake. */
static void testPwm() {
	section("Servo PWM: bind, write, release and the sharing rules");

	fakePwmReset();
	checkTrue("GP29 at 50 Hz binds", servoPwmBind(SIG, 50));
	FakePin p = fakePin(SIG);
	FakeSlice sl = fakeSlice(6);
	checkTrue("on the PWM function", p.function == GPIO_FUNC_PWM);
	checkInt("at level 0: no edges", p.level, 0);
	checkTrue("with the pulls off", p.pull == FakePull::None);
	checkTrue("slice 6 counting", sl.enabled && sl.inits == 1);
	checkInt("wrap 19999 at 50 Hz", sl.top, 19999);
	checkInt("150 MHz divided by 150: 1 us a count", sl.div16, 150 * 16);
	checkTrue("bound", servoPwmBound(SIG) && !servoPwmBound(AUX));

	checkTrue("again at 50 Hz: accepted", servoPwmBind(SIG, 50));
	checkInt("and the slice is not set up again", fakeSlice(6).inits, 1);
	checkTrue("again at 100 Hz: refused", !servoPwmBind(SIG, 100));
	checkTrue("GP28, same slice, 100 Hz: refused", !servoPwmBind(AUX, 100));
	checkTrue("GP28 at 50 Hz binds", servoPwmBind(AUX, 50));
	checkInt("the running slice is left alone", fakeSlice(6).inits, 1);
	checkTrue("GP12, GP28's compare register: refused", !servoPwmBind(12, 50));
	checkTrue("GP26, both bindings in use: refused", !servoPwmBind(26, 50));
	checkTrue("GP30, not on the package: refused", !servoPwmBind(30, 50));

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
	checkTrue("a plain input", p.function == GPIO_FUNC_SIO && !p.output);
	checkTrue("with no pull", p.pull == FakePull::None);
	checkTrue("the slice runs on for GP29", fakeSlice(6).enabled && servoPwmBound(SIG));
	servoPwmRelease(SIG);
	checkTrue("the last pin released stops the slice", !fakeSlice(6).enabled);
	writes = fakePin(SIG).levelWrites;
	servoPwmRelease(SIG);
	checkInt("a second release does nothing", fakePin(SIG).levelWrites, writes);

	checkTrue("rate 0: refused", !servoPwmBind(SIG, 0));
	checkTrue("15 Hz, a period the counter cannot hold: refused", !servoPwmBind(SIG, 15));
	checkTrue("560 Hz binds", servoPwmBind(SIG, 560));
	checkInt("wrap 1784", fakeSlice(6).top, 1784);
	servoPwmRelease(SIG);
	checkTrue("GP28 binds", servoPwmBind(AUX, 50));
	checkTrue("GP12 on its compare register: refused, a binding free",
	          !servoPwmBind(12, 50));
	servoPwmRelease(AUX);

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

/** @brief Take @p c at @p now and step once. */
static void pass(ServoCore *core, const ServoCmd &c, uint32_t now, bool alive = true) {
	servoCoreTake(core, &c, now, alive);
	servoCoreStep(core, now, alive);
}

/** @brief SAFE, START, positions, two servos and REVERSE B, on the pure core. */
static void testCoreStates() {
	section("Servo job core: SAFE, START, positions, two servos");

	ServoProfile prof = profileOf(ServoType::Standard);
	prof.servos = 2;
	ServoCore c;
	servoCoreReset(&c, &prof, true, true, 7);
	servoCoreStep(&c, 1000, true);
	checkTrue("SAFE: no pulse on either pin", c.outUs[0] == 0 && c.outUs[1] == 0 && !c.running);

	ServoCmd stale = cmdOf(7, ServoCmdKind::Start);
	checkTrue("a command from before the bind is not taken",
	          !servoCoreTake(&c, &stale, 1000, true) && !c.running);

	pass(&c, cmdOf(8, ServoCmdKind::Position, SERVO_MASK_AB, 1000), 1000);
	checkTrue("a position while SAFE moves nothing", !c.running && c.outUs[0] == 0);
	checkInt("and is acknowledged", c.ackSeq, 8);

	pass(&c, cmdOf(9, ServoCmdKind::Start, SERVO_MASK_AB, 0, 200), 1000, false);
	checkTrue("START with the heartbeat lapsed: refused", !c.running);

	// Something was commanded before: START still puts CENTRE out at once.
	c.target[0] = 1000; c.pos[0] = 1000000;
	pass(&c, cmdOf(10, ServoCmdKind::Start, SERVO_MASK_AB, 0, 200), 1000);
	checkTrue("START: RUNNING", c.running);
	checkTrue("CENTRE on both pins in the same pass, slew or not",
	          c.outUs[0] == 1500 && c.outUs[1] == 1500);
	ServoCmd again = cmdOf(10, ServoCmdKind::Safe);
	checkTrue("a seq already taken is not new, whatever it says",
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
	servoCoreReset(&one, &prof, true, false, 0);
	pass(&one, cmdOf(1, ServoCmdKind::Start), 1000);
	checkTrue("one pin bound: CENTRE on A, nothing on B",
	          one.outUs[0] == 1500 && one.outUs[1] == 0);
	ServoCore none;
	servoCoreReset(&none, &prof, false, false, 0);
	pass(&none, cmdOf(1, ServoCmdKind::Start), 1000);
	checkTrue("no pin bound: START refused", !none.running && none.outUs[0] == 0);
}

/** @brief SPEED: the slew per pass, across a clock wrap. */
static void testCoreSlew() {
	section("Servo job core: the slew follows SPEED");

	ServoProfile prof = profileOf(ServoType::Standard);
	ServoCore c;
	servoCoreReset(&c, &prof, true, false, 0);
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
}

/** @brief Sweeps: start, shape, count, stop, and what ends them. */
static void testCoreSweep() {
	section("Servo job core: the sweep and what ends it");

	ServoProfile prof = profileOf(ServoType::Standard);
	prof.servos = 2;
	ServoCore c;
	servoCoreReset(&c, &prof, true, true, 0);
	pass(&c, cmdOf(1, ServoCmdKind::Start), 1000);

	ServoCmd early = sweepOf(2, SWEEP_SINE, 1000, 400, 0, 0);
	servoCoreTake(&c, &early, 1000, false);
	checkTrue("a sweep with the heartbeat lapsed is refused", !c.sweeping && c.running);
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
	servoCoreReset(&c, &prof, true, false, 0);
	servoCoreStep(&c, 100, false);
	checkTrue("a lapse while SAFE latches nothing", !c.tripped);
	pass(&c, cmdOf(1, ServoCmdKind::Start), 200);
	pass(&c, sweepOf(2, SWEEP_SINE, 1000, 400, 0, 0), 200);
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
	checkTrue("a position with the heartbeat lapsed is refused, and the pass trips",
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
	checkInt("at 50 Hz", fakeSlice(6).top, 19999);
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
	int inits = fakeSlice(6).inits;
	checkTrue("two servos at 100 Hz while SAFE: accepted", servoJobProfile(&two));
	core1JobPoll();
	checkTrue("both pins on PWM at level 0",
	          fakePin(SIG).function == GPIO_FUNC_PWM && fakePin(AUX).function == GPIO_FUNC_PWM &&
	          fakePin(SIG).level == 0 && fakePin(AUX).level == 0);
	checkTrue("the slice set up again, at 100 Hz",
	          fakeSlice(6).inits == inits + 1 && fakeSlice(6).top == 9999);
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
	checkTrue("SIG a plain input, no pull", fakePin(SIG).function == GPIO_FUNC_SIO &&
	                                        fakePin(SIG).pull == FakePull::None);
	checkTrue("the slice stopped", !fakeSlice(6).enabled);
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

/** @brief The trip through the job, on the shipped heartbeat rule, across a wrap. */
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
	fakeAdvance(0u - toEdge - 250u);
	checkInt("the clock is back where it was", millis(), start);
	escHeartbeat();
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

/** @brief The slider's event logic: the two press contracts, tap-to-set, presets. */
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

/** @brief The stub screen, until the screen replaces it. */
static void testStubScreen() {
	section("Servo tester: the stub screen");

	checkTrue("the screen is not built", !uiServoAvailable());
	uiServoEnter();
	checkTrue("opening it claims nothing", pinClaimHolder() == PinUser::None);
	checkTrue("it leaves on its first frame", !uiServoTick(nullptr));
}

void runServoTests() {
	testProfiles();
	testValidity();
	testSteppers();
	testMapping();
	testSpeed();
	testDial();
	testPwm();
	testCoreStates();
	testCoreSlew();
	testCoreSweep();
	testCoreTrip();
	testJob();
	testJobTrip();
	testSlider();
	testSliderDraw();
	testStubScreen();
}
