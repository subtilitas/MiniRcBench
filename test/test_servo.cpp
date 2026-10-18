/**
 * @file test_servo.cpp
 * @brief The servo tester suite: the pure model in servo_model.cpp, and the
 *        stub screen it replaces.
 *
 * Every check runs the shipped functions. The limits, the steppers, the
 * mappings, SPEED and the dial hit test follow docs/design/servo-tester.md.
 */

#include "check.h"
#include "pin_claim.h"
#include "servo_model.h"
#include "ui_servo.h"

#include <math.h>

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
	testStubScreen();
}
