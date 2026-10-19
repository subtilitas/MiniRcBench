/**
 * @file kst_view.h
 * @brief What the KST screen shows of a servo, as pure functions: the field
 *        rows, their values, the stepper, and the texts of every rule,
 *        result and reply code.
 *
 * The rows follow rcbench's pages: TUNE, PULSE and FAILSAFE hold the 16
 * fields of access class `KST_ACCESS_EDITABLE`, LOCKED the 11 named fields
 * of `KST_ACCESS_LOCKED` and `KST_ACCESS_NEVER`, read-only. Values come from
 * kst_field_display() with their unit; `US` stands for µs, which the font
 * has no sign for.
 *
 * The stepper moves a field's raw value with kst_field_edit() and skips every
 * value that adds a hard rule (kst_limits_edit() against the backup), so
 * stepping reaches no value the write would refuse. A value that breaks a
 * soft rule is taken and its rule is shown.
 *
 * Nothing here draws, reads the touch or calls the card.
 *
 * @see docs/design/kst-programming.md, "Screens".
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "kst_file.h"
#include "kst_limits.h"
#include "kst_reg.h"
#include "kst_session.h"

/** @brief One row of the field list. */
struct KstRow {
	const char     *group;  /**< TUNE, PULSE, FAILSAFE or LOCKED. */
	const char     *label;  /**< The row's name on the screen. */
	kst_field_id_t  field;  /**< The field it shows. */
};

/** @brief The rows in screen order, 27 of them. */
const KstRow *kstRows(int *count);

/** @brief Whether row @p i is editable: not LOCKED. */
bool kstRowEditable(int i);

/** @brief The row of @p field; -1 for a field no row shows. */
int kstRowOf(kst_field_id_t field);

/**
 * @brief A raw value as the screen shows it, with its unit: `96 %`,
 *        `300.0 US`, `ON`. A raw value whose display is not established
 *        reads `RAW 6`.
 */
void kstValueText(kst_field_id_t field, uint16_t raw, char *buf, size_t n);

/**
 * @brief A value before and after a change, as kstValueText() shows them.
 *        When two raw values read the same, both carry their raw value:
 *        `96 % (245)` and `96 % (244)`.
 * @param field The field.
 * @param from  The raw value before.
 * @param to    The raw value after.
 * @param a     The text before, @p n bytes.
 * @param b     The text after, @p n bytes.
 * @param n     Room in @p a and in @p b.
 */
void kstValuePair(kst_field_id_t field, uint16_t from, uint16_t to, char *a, char *b, size_t n);

/** @brief The hard rules @p target breaks as an edit of @p backup. */
kst_rules_t kstHardRules(const kst_image_t *backup, const kst_image_t *target);

/** @brief The soft rules @p target breaks as an edit of @p backup. */
kst_rules_t kstSoftRules(const kst_image_t *backup, const kst_image_t *target);

/**
 * @brief Step @p field of @p target by @p steps steps, a positive sign
 *        raising the value shown and a negative one lowering it, each step to
 *        the next raw value that adds no hard rule to those @p target already
 *        breaks. DEAD BAND's raw value falls as its value shown rises.
 * @return The steps taken; fewer when the field's end or no legal value lies
 *         ahead.
 */
int kstStep(const kst_image_t *backup, kst_image_t *target, kst_field_id_t field,
            int steps);

/** @brief Whether a step of @p field in @p dir, -1 or +1 as for kstStep(), is possible. */
bool kstCanStep(const kst_image_t *backup, const kst_image_t *target,
                kst_field_id_t field, int dir);

/** @brief Raw steps a sideways swipe's coarse step moves @p field: an eleventh of its raw range, rounded up, at least 1. */
int kstCoarse(kst_field_id_t field);

/** @brief The editable fields in which @p a and @p b differ. */
kst_field_set_t kstChanged(const kst_image_t *a, const kst_image_t *b);

/** @brief The text of rule @p rule, upper case; "" outside the rules. */
const char *kstRuleText(kst_rule_t rule);

/** @brief The colour class of an operation's end. */
enum class KstTone : uint8_t {
	Done,     /**< It did what was asked. */
	Nothing,  /**< Nothing reached the servo. */
	Doubt,    /**< The servo may hold other values than shown. */
};

/** @brief How an operation ended, as one line, and its tone. */
struct KstResultText {
	const char *text;
	KstTone     tone;
};

/**
 * @brief The text of a write's, restore's or read's end: one per
 *        `kst_ses_t`; `KST_SES_ERR_NO_PLAN` by @p planResult.
 */
KstResultText kstResultText(kst_ses_t result, uint8_t planResult);

/** @brief A `kst_rx_t` as a word: `LATE`, `RATE`. "" for OK. */
const char *kstRxWord(uint8_t rx);

/** @brief The text of a `kst_file_err_t`, without the line. */
const char *kstFileErrText(kst_file_err_t err);

/**
 * @brief A field's name as an edited file's line @p line gives it, upper
 *        case, into @p buf; empty when the line has no name.
 */
void kstFileLineName(const char *text, size_t len, unsigned line, char *buf, size_t n);

/**
 * @brief Break @p text into lines of at most @p width characters, at spaces.
 *        A width below 4 counts as 4, above 47 as 47.
 * @return The lines written into @p lines, at most @p maxLines; the last
 *         line ends in `...` when the text did not fit.
 */
int kstWrap(const char *text, int width, char lines[][48], int maxLines);
