/**
 * @file servo_model.h
 * @brief The servo tester's rules as pure functions: profiles, pulse limits,
 *        steppers, the command and angle mappings, SPEED and the dial.
 *
 * Nothing here touches hardware, so the host suite runs every rule as it
 * ships. The screen (ui_servo.cpp) and the core1 job (servo_job.cpp) both
 * decide through these functions, so the two cannot disagree on a limit.
 *
 * Terms. PWM is pulse-width modulation. A servo pulse is a high level of
 * @ref SERVO_FLOOR_US to @ref SERVO_CEILING_US repeated at a frame rate. A
 * command is a position from 0 to @ref SERVO_SPAN with the centre at
 * @ref SERVO_CMD_CENTRE, as in rcbench (`OUT_SPAN`). Angles are degrees on the
 * drawn dial: 0 is straight up, -90 is MIN and +90 is MAX.
 *
 * The profile table, max_rate_for(), dangerous(), cmd_range(), deg_to_us(),
 * us_to_deg(), slew_of() and on_the_dial() are rewritten from rcbench
 * `shared/ui/servo_screen.c`, commit 453cf334, MIT, the same copyright holder
 * as this project. on_the_dial() is rotated to point up.
 *
 * @see docs/design/servo-tester.md
 */

#pragma once

#include <stdint.h>

#include "servo_sweep.h"

/**
 * @defgroup servo_limits Pulse limits
 * @{
 */
/** @brief Shortest pulse any profile reaches, in us (rcbench `OUT_FLOOR_US`). */
#define SERVO_FLOOR_US        400
/** @brief Longest pulse any profile reaches, in us (rcbench `OUT_CEILING_US`). */
#define SERVO_CEILING_US      2500
/** @brief Slowest frame rate, in Hz. Below it a servo steps audibly. */
#define SERVO_RATE_MIN_HZ     40
/** @brief Fastest frame rate, in Hz: a 760 us tail servo's rate. */
#define SERVO_RATE_MAX_HZ     560
/** @brief Pause after a pulse before the next frame, in us (`PWM_MIN_PAUSE_US`). */
#define SERVO_PAUSE_US        1000
/** @brief That pause for the heli profiles, in us (`HELI_MIN_PAUSE_US`). */
#define SERVO_HELI_PAUSE_US   500
/** @brief Above this frame rate, in Hz, a change needs the warning (`SAFE_RATE_HZ`). */
#define SERVO_SAFE_RATE_HZ    60
/** @brief One step of the MIN, CENTRE and MAX steppers, in us. */
#define SERVO_STEP_US         10
/** @} */

/**
 * @defgroup servo_commands Commands and SPEED
 * @{
 */
/** @brief A command's full travel, MIN to MAX (rcbench `OUT_SPAN`). */
#define SERVO_SPAN            1000
/** @brief The command that rests a servo at CENTRE. */
#define SERVO_CMD_CENTRE      500
/** @brief Lowest SPEED, in percent. */
#define SERVO_SPEED_MIN_PCT   10
/** @brief Highest SPEED, in percent: no slew. */
#define SERVO_SPEED_MAX_PCT   100
/**
 * @brief The slew at SPEED 100 % if it were a slew: two spans a second, in
 *        command units a second. SPEED scales it; 100 % itself is no slew.
 */
#define SERVO_FULL_SPAN_S     (2 * SERVO_SPAN)
/** @} */

/**
 * @defgroup servo_dial Dial geometry
 * @brief The drawn dial on the SERVO screen, which the hit test reads.
 * @{
 */
/** @brief Shaft centre, x, in pixels. */
#define SERVO_DIAL_X          120
/** @brief Shaft centre, y, in pixels. */
#define SERVO_DIAL_Y          184
/** @brief Radius of the drawn arc, in pixels. */
#define SERVO_DIAL_ARC_R      96
/** @brief Inner edge of the hit area, in pixels from the shaft. */
#define SERVO_DIAL_R_IN       24
/** @brief Outer edge of the hit area, in pixels from the shaft. */
#define SERVO_DIAL_R_OUT      124
/** @brief First row of the dial band. */
#define SERVO_DIAL_Y0         56
/** @brief Last row of the dial band. */
#define SERVO_DIAL_Y1         199
/** @brief Angle, in degrees either side of up, where travel ends: MIN and MAX. */
#define SERVO_DIAL_TRAVEL_DEG 90
/** @brief Angle, in degrees either side of up, where the hit area ends. */
#define SERVO_DIAL_EDGE_DEG   104
/** @} */

/** @brief The profiles rcbench drives, in its order. */
enum class ServoType : uint8_t {
	Standard,     /**< STANDARD PWM, 1000/1500/2000 us. */
	Narrow760,    /**< NARROW 760, 660/760/860 us. */
	Wide,         /**< WIDE, 800/1500/2200 us. */
	HeliCyclic,   /**< HELI CYCLIC, 820/1520/2220 us, up to 333 Hz. */
	HeliTail760,  /**< HELI TAIL 760, 410/760/1110 us, up to 560 Hz. */
};
/** @brief Number of ServoType values. */
#define SERVO_TYPE_COUNT 5

/** @brief One row of the profile table. */
struct ServoTypeInfo {
	const char     *name;       /**< As the SET screen shows it. */
	uint16_t        minUs;      /**< MIN the type loads, us. */
	uint16_t        centreUs;   /**< CENTRE the type loads, us. */
	uint16_t        maxUs;      /**< MAX the type loads, us. */
	const uint16_t *rates;      /**< Frame rates offered, Hz, ascending. */
	uint8_t         rateCount;  /**< Entries in @c rates. */
	uint16_t        defaultHz;  /**< Rate the type loads, Hz. */
	uint16_t        maxHz;      /**< The type's own ceiling, Hz; 0 for the pause rule alone. */
	bool            heli;       /**< Choosing it needs the warning; its pause is the heli one. */
};

/**
 * @brief Everything the pins carry, shared by both servos.
 *
 * SIG and AUX are channels B and A of one PWM slice, which has one frame rate,
 * so both servos share one rate and one pulse range.
 */
struct ServoProfile {
	ServoType type;      /**< Decides the rates offered and the pause. */
	uint16_t  minUs;     /**< MIN, us. */
	uint16_t  centreUs;  /**< CENTRE, us. */
	uint16_t  maxUs;     /**< MAX, us. */
	uint16_t  rateHz;    /**< Frame rate, Hz. */
	uint8_t   servos;    /**< 1, or 2 for SIG and AUX. */
	bool      reverseB;  /**< Servo B mirrors A when both follow. */
};

/** @brief What one stepper of the SET screen's PULSE rows edits. */
enum class ServoField : uint8_t {
	Servos,    /**< ONE or TWO. */
	Type,      /**< The profile; loads its MIN, CENTRE, MAX and rate. */
	Rate,      /**< The frame rate, through the type's offered rates. */
	Min,       /**< MIN, in @ref SERVO_STEP_US steps. */
	Centre,    /**< CENTRE, in @ref SERVO_STEP_US steps. */
	Max,       /**< MAX, in @ref SERVO_STEP_US steps. */
	ReverseB,  /**< OFF or ON. */
};

/** @brief What one stepper of the SET screen's SWEEP rows edits. */
enum class SweepField : uint8_t {
	Shape,      /**< Square, sine or triangle. */
	Rate,       /**< Cycles a second, in @ref SERVO_SWEEP_MHZ_STEP steps. */
	Amplitude,  /**< Units either side of the centre, in 5 % steps. */
	Dwell,      /**< Hold at each end, in @ref SERVO_SWEEP_DWELL_STEP_MS steps. */
	Moves,      /**< Ends to reach; 0 is endless. */
};

/** @brief One step of the sweep RATE, in thousandths of a cycle a second. */
#define SERVO_SWEEP_MHZ_STEP      50
/** @brief One step of the sweep AMPLITUDE: 5 % of half the travel, in command units. */
#define SERVO_SWEEP_AMP_STEP      25
/** @brief One step of the sweep DWELL, ms. */
#define SERVO_SWEEP_DWELL_STEP_MS 100
/** @brief Most movements a counted sweep makes. */
#define SERVO_SWEEP_MOVES_MAX     9999

/**
 * @brief The table row of a type.
 * @param t The type.
 * @return The row; the STANDARD PWM row for a value outside the enum.
 */
const ServoTypeInfo *servoTypeInfo(ServoType t);

/**
 * @brief The profile every entry to the screen starts from: STANDARD PWM at
 *        50 Hz, one servo, REVERSE B off.
 * @param[out] p The profile.
 */
void servoProfileDefault(ServoProfile *p);

/**
 * @brief Switch @p p to type @p t: its MIN, CENTRE, MAX and default rate.
 *        The servo count and REVERSE B are kept.
 * @param[in,out] p The profile.
 * @param t         The new type.
 */
void servoProfileLoadType(ServoProfile *p, ServoType t);

/**
 * @brief The fastest frame rate a type allows with pulses up to @p maxUs:
 *        `1,000,000 / (maxUs + pause)`, under the type's own ceiling.
 *
 * 333 Hz for STANDARD PWM at 2000 us.
 *
 * @param t     The type, for the pause and the ceiling.
 * @param maxUs The longest pulse, us.
 * @return The rate, Hz, rounded down.
 */
uint16_t servoMaxRateFor(ServoType t, uint16_t maxUs);

/**
 * @brief The longest pulse a frame rate leaves room for, with the type's
 *        pause after it, and never above @ref SERVO_CEILING_US.
 * @param t      The type, for the pause.
 * @param rateHz The frame rate, Hz; 0 gives 0.
 * @return The pulse, us.
 */
uint16_t servoMaxPulseFor(ServoType t, uint16_t rateHz);

/**
 * @brief Whether applying this type and rate needs the warning: a heli type,
 *        or a rate above @ref SERVO_SAFE_RATE_HZ, at which an analogue servo
 *        overheats.
 * @param t      The type.
 * @param rateHz The frame rate, Hz.
 * @return True when the change waits for HOLD TO APPLY.
 */
bool servoDangerous(ServoType t, uint16_t rateHz);

/**
 * @brief Whether a profile is one the job may drive.
 *
 * MIN at least @ref SERVO_FLOOR_US; MIN, CENTRE and MAX each at least
 * @ref SERVO_STEP_US apart in that order; MAX within servoMaxPulseFor() at the
 * rate; the rate one the type offers and within @ref SERVO_RATE_MIN_HZ to
 * @ref SERVO_RATE_MAX_HZ; one or two servos.
 *
 * @param p The profile; null is not valid.
 * @return True when every rule holds.
 */
bool servoProfileValid(const ServoProfile *p);

/**
 * @brief One step of a PULSE stepper. Only legal values are reached.
 *
 * MIN, CENTRE and MAX move by @ref SERVO_STEP_US and stop at the floor, at
 * servoMaxPulseFor() of the rate and at each other: a step that would leave
 * those limits changes nothing. RATE moves to the next offered rate that the
 * range allows (servoMaxRateFor()). TYPE moves through the table and stops at
 * its ends, loading each type's values. SERVOS and REVERSE B go to TWO and ON
 * for +1 and to ONE and OFF for -1.
 *
 * @param[in,out] p The profile, valid before and after.
 * @param f         The field.
 * @param dir       -1 or +1.
 * @return True when the step changed the profile.
 */
bool servoProfileStep(ServoProfile *p, ServoField f, int dir);

/**
 * @brief Whether servoProfileStep() would change the profile. For drawing a
 *        stepper disabled when it cannot act.
 * @param p   The profile.
 * @param f   The field.
 * @param dir -1 or +1.
 * @return True when a step in @p dir changes something.
 */
bool servoProfileCanStep(const ServoProfile *p, ServoField f, int dir);

/**
 * @brief The sweep every entry to the screen starts from: a sine at 0.50 Hz,
 *        the full half travel, 500 ms at each end, no end to the movements.
 * @param[out] c The sweep.
 */
void servoSweepDefault(sweep_cfg_t *c);

/**
 * @brief One step of a SWEEP stepper. Only values sweep_cfg_valid() takes
 *        are reached; a step past a limit changes nothing.
 *
 * SHAPE moves through square, sine and triangle and stops at the ends. RATE
 * moves by @ref SERVO_SWEEP_MHZ_STEP within `SWEEP_MHZ_MIN` to
 * `SWEEP_MHZ_MAX`, 0.05 Hz to 5.00 Hz. AMPLITUDE moves by
 * @ref SERVO_SWEEP_AMP_STEP within 0 to `SWEEP_AMPLITUDE_MAX`, 0 % to 100 %.
 * DWELL moves by @ref SERVO_SWEEP_DWELL_STEP_MS within 0 ms to
 * `SWEEP_DWELL_MAX_MS`. MOVES moves by 1 within 0, endless, to
 * @ref SERVO_SWEEP_MOVES_MAX.
 *
 * @param[in,out] c The sweep, valid before and after.
 * @param f         The field.
 * @param dir       -1 or +1.
 * @return True when the step changed the sweep.
 */
bool servoSweepStep(sweep_cfg_t *c, SweepField f, int dir);

/**
 * @brief Whether servoSweepStep() would change the sweep.
 * @param c   The sweep.
 * @param f   The field.
 * @param dir -1 or +1.
 * @return True when a step in @p dir changes something.
 */
bool servoSweepCanStep(const sweep_cfg_t *c, SweepField f, int dir);

/**
 * @brief The range a command maps onto: the narrowest one centred on CENTRE
 *        that holds MIN and MAX, so command @ref SERVO_CMD_CENTRE is CENTRE
 *        even when CENTRE is not midway between MIN and MAX.
 * @param p       The profile.
 * @param[out] lo The range's low end, us; may be below MIN, and below 0 when
 *                CENTRE lies closer to 0 us than to MAX.
 * @param[out] hi The range's high end, us; may be above MAX.
 */
void servoCmdRange(const ServoProfile *p, int32_t *lo, int32_t *hi);

/**
 * @brief A command as a pulse: servoCmdRange() scaled, rounded to the
 *        nearest us and clamped to MIN..MAX.
 * @param p   The profile.
 * @param cmd The command, 0 to @ref SERVO_SPAN; larger values count as
 *            @ref SERVO_SPAN.
 * @return The pulse, us.
 */
uint16_t servoCmdToUs(const ServoProfile *p, uint16_t cmd);

/**
 * @brief A pulse as a command in servoCmdRange(), rounded, 0 to
 *        @ref SERVO_SPAN.
 * @param p  The profile.
 * @param us The pulse, us.
 * @return The command.
 */
uint16_t servoUsToCmd(const ServoProfile *p, uint16_t us);

/**
 * @brief An angle on the dial as a pulse. Each side of CENTRE runs to its
 *        own end: -90 deg is MIN, +90 deg is MAX. Angles past
 *        @ref SERVO_DIAL_TRAVEL_DEG saturate at the end.
 * @param p   The profile.
 * @param deg The angle, degrees from straight up, positive towards MAX.
 * @return The pulse, us, rounded.
 */
uint16_t servoDegToUs(const ServoProfile *p, float deg);

/**
 * @brief A pulse as an angle on the dial, the inverse of servoDegToUs().
 * @param p  The profile.
 * @param us The pulse, us.
 * @return The angle, degrees, -90 to +90 for a pulse in MIN..MAX.
 */
float servoUsToDeg(const ServoProfile *p, uint16_t us);

/**
 * @brief An angle on the dial as a command: servoUsToCmd() of servoDegToUs().
 * @param p   The profile.
 * @param deg The angle, degrees.
 * @return The command, 0 to @ref SERVO_SPAN.
 */
uint16_t servoDegToCmd(const ServoProfile *p, float deg);

/**
 * @brief A command mirrored about the centre, for REVERSE B.
 * @param cmd The command, 0 to @ref SERVO_SPAN.
 * @return `SERVO_SPAN - cmd`.
 */
uint16_t servoMirror(uint16_t cmd);

/**
 * @brief SPEED as a slew, in command units a second.
 *
 * `SERVO_FULL_SPAN_S * pct / 100`: at 10 % a full MIN to MAX travel takes
 * 5 s. 100 % and above is no slew, 0. Below @ref SERVO_SPEED_MIN_PCT counts
 * as that minimum.
 *
 * @param pct SPEED, percent.
 * @return The slew; 0 for none.
 */
uint16_t servoSlewOf(int pct);

/**
 * @brief The angle of a point on the panel as seen from the shaft, degrees
 *        from straight up, positive clockwise, -180 to +180.
 * @param x,y The point, framebuffer coordinates.
 * @return The angle.
 */
float servoDialAngle(int x, int y);

/**
 * @brief Whether a press at (@p x, @p y) means the horn.
 *
 * Inside the annulus from @ref SERVO_DIAL_R_IN to @ref SERVO_DIAL_R_OUT
 * around the shaft, within @ref SERVO_DIAL_EDGE_DEG of straight up, and above
 * the dial band's last row, @ref SERVO_DIAL_Y1. The annulus does not reach
 * above the band's first row, @ref SERVO_DIAL_Y0; a static_assert in
 * servo_model.cpp holds that.
 *
 * @param x,y      The press, framebuffer coordinates.
 * @param[out] deg The angle under it, when true. May be null.
 * @return True when the press is on the dial.
 */
bool servoOnDial(int x, int y, float *deg);
