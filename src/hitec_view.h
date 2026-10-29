/**
 * @file hitec_view.h
 * @brief What the HITEC SERVO screen shows of a servo, as pure functions:
 *        the rows of each layout, their values, the stepper, the soft rules,
 *        and the texts of every result and reply code.
 *
 * Hitec is a name or trademark of its owner; it is used here only to say
 * which servos this module works with.
 *
 * The rows follow rcbench's tabs, MOTION, TRAVEL, PROTECT and INFO, in
 * rcbench's order, for the parameters rcbench's register model has for the
 * layout (`hitec_param()`). Values come from `hitec_param_get()` with their
 * unit and the stored value in brackets; `US` stands for µs, which the font
 * has no sign for.
 *
 * The stepper moves a parameter to the next value `hitec_param_set()` takes
 * on a copy of the draft, so it reaches no value the encoding or the D series
 * order refuses. A value that breaks a soft rule is taken and its rule is
 * shown.
 *
 * Nothing here draws, reads the touch or calls the card.
 *
 * @see docs/design/hitec-programming.md, "Screens".
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "hitec_reg.h"
#include "hitec_session.h"
#include "hitec_wire.h"

/** @brief One row of the field list. */
struct HitecRow {
	const char *group;  /**< MOTION, TRAVEL, PROTECT or INFO. */
	const char *label;  /**< The row's name on the screen. */
	uint8_t     param;  /**< `hitec_param_t`. */
};

/** @brief Rows a layout has at most. */
#define HITEC_ROWS_MAX 24

/**
 * @brief The rows of @p layout in screen order, into @p out.
 * @return The number written, at most `HITEC_ROWS_MAX`.
 */
int hitecRows(uint8_t layout, HitecRow *out);

/** @brief Whether @p param of @p layout is editable: `HITEC_RW`. */
bool hitecEditable(uint8_t layout, uint8_t param);

/**
 * @brief Whether a change of @p param goes to the servo: an editable
 *        parameter, or the classic CHECKSUM that REPAIR rebalances.
 */
bool hitecWritten(uint8_t layout, uint8_t param);

/**
 * @brief @p param of @p img as the screen shows it, with its unit and the
 *        stored value in brackets: `60 % (12)`, `CW (0)`, `LIMP (0)`,
 *        `1500 US (6000)`. A stored value that fits no encoding reads
 *        `RAW 1234`, a byte not read `?`.
 * @param img     The image.
 * @param layout  Its `hitec_layout_t`.
 * @param param   The `hitec_param_t`.
 * @param[out] buf The text, at most @p n bytes.
 * @param n       Room in @p buf.
 * @param[out] raw True for a `RAW` value; may be NULL.
 */
void hitecValueText(const hitec_image_t *img, uint8_t layout, uint8_t param, char *buf,
                    size_t n, bool *raw);

/**
 * @brief Step @p param of @p draft by @p steps steps, a positive sign raising
 *        the value shown, each step to the next value an edit takes
 *        (`hitec_param_set()` on a copy).
 * @return The steps taken; fewer at the encoding's end or where no value
 *         ahead is taken.
 */
int hitecStep(hitec_image_t *draft, uint8_t layout, uint8_t param, int steps);

/** @brief Whether a step of @p param in @p dir, -1 or +1, is possible. */
bool hitecCanStep(const hitec_image_t *draft, uint8_t layout, uint8_t param, int dir);

/** @brief Steps a sideways swipe's coarse step moves @p param: an eleventh of its range, at least 1. */
int hitecCoarse(uint8_t layout, uint8_t param);

/** @brief Whether @p param reads differently in @p a and @p b. */
bool hitecDiffers(const hitec_image_t *a, const hitec_image_t *b, uint8_t layout, uint8_t param);

/** @brief The parameters of @p layout that differ between @p a and @p b and go to the servo (hitecWritten()). */
int hitecChangedCount(const hitec_image_t *a, const hitec_image_t *b, uint8_t layout);

/** @brief The soft rules, rcbench's seven. */
enum class HitecRule : uint8_t {
	SlowSpeed,      /**< SPEED below 30 %. */
	NarrowDeadBand, /**< DEAD BAND below the servo's. */
	OverloadOff,    /**< OVERLOAD switched off. */
	EndPoint,       /**< an end point other than the servo's. */
	Centre,         /**< NEUTRAL or CENTRE other than the servo's. */
	Direction,      /**< DIRECTION reversed. */
	FailsafeFar,    /**< a fail-safe position more than 300 µs from 1500 µs. */
	Count,
};

/** @brief Bit of @p r in a set of rules. */
#define HITEC_RULE_BIT(r) (1u << (unsigned)(r))

/**
 * @brief The soft rules @p draft breaks against @p servo, the last full
 *        read; with @p param below `HITEC_P_COUNT`, only that parameter's.
 */
uint32_t hitecSoftRules(const hitec_image_t *servo, const hitec_image_t *draft, uint8_t layout,
                        uint8_t param = HITEC_P_COUNT);

/** @brief The text of @p r, upper case. */
const char *hitecRuleText(HitecRule r);

/** @brief The colour class of an operation's end. */
enum class HitecTone : uint8_t {
	Done,     /**< It did what was asked. */
	Nothing,  /**< Nothing reached the servo. */
	Doubt,    /**< The servo may hold other values than shown. */
};

/** @brief How a write ended: the text and its tone. */
struct HitecResultText {
	char      text[96];
	HitecTone tone;
};

/**
 * @brief The text of a write's end, one per `hitec_ses_t`: @p values the
 *        count written, @p stepIndex and @p planSteps the plan's progress,
 *        @p diff the read-back's differences, @p failAddr the address that
 *        failed, @p planResult the plan's refusal.
 */
HitecResultText hitecResultText(uint8_t result, int values, uint8_t stepIndex, uint8_t planSteps,
                                const uint8_t *diff, uint8_t failAddr, uint8_t planResult,
                                uint8_t series);

/** @brief A `hitec_rx_t` as a word: `SILENT`, `LATE`, `SUM`. "" for OK. */
const char *hitecRxWord(uint8_t rx);

/**
 * @brief The model of @p img as the strip names it: Hitec's name, `D SERIES
 *        34999` for another D series servo, the name cells for another
 *        classic one, `?` for none.
 */
void hitecModelText(const hitec_image_t *img, char *buf, size_t n);

/** @brief The layout of @p img; the series' layout for a model not known. */
uint8_t hitecLayoutOf(const hitec_image_t *img);

/** @brief Whether @p img's model is one of the three rcbench measured. */
bool hitecModelMeasured(const hitec_image_t *img);
