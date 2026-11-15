/**
 * @file kv_est.h
 * @brief The tester's RPM/V reading and kV estimate. Pure; compiled into the
 *        host suite.
 *
 * Slice 3 of docs/design/esc-commands.md, "The kV estimate", with the owner's
 * decisions 6 and 7 of 2026-11-15. RPM is the mechanical speed, eRPM over half
 * the pole count; V the ESC's supply as the ESC reports it, KISS where fresh,
 * EDT (Extended DShot Telemetry) otherwise; t the throttle the tester sends,
 * 0 to 2000.
 *
 * - RPM/V = RPM / V, the measured ratio.
 * - kV estimate = RPM / (V x t / 2000) = RPM x 2000 / (V x t).
 *
 * The estimate assumes the ESC's duty is t / 2000 and that the motor, with no
 * propeller, turns at kV times the voltage that duty gives it. Neither holds
 * exactly: AM32 2.22's duty map puts it at x 0.92 to x 1.08 of the rating at
 * 75 % throttle, with RUNNING LEVEL 10 and on a target built without
 * USE_STARTUP_BOOST, and the maps of Bluejay, BLHeli_S and BLHeli_32 are not
 * verified. It is therefore labelled `KV EST`, never `KV`, and shown only
 * under the note's conditions; @see kvEstView().
 *
 * Both figures are rounded half up and clamped to @ref KV_EST_MAX, the width
 * the field has. Neither is held after the disarm.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "config.h"

/** @brief The largest figure either field shows, 5 digits. */
#define KV_EST_MAX           99999u

/**
 * @brief The voltage floor, in hundredths of a volt: a reading must be above
 *        it. 0.5 V, rcbench's floor for its `rpm/V`.
 */
#define KV_EST_MIN_VOLTS100  50

/** @brief The full throttle scale the tester sends, 0 to this. */
#define KV_EST_FULL_THROTTLE 2000

static_assert(KV_EST_MIN_THROTTLE > 0 && KV_EST_MIN_THROTTLE <= KV_EST_FULL_THROTTLE,
              "the estimate's minimum throttle is outside the tester's scale");
static_assert(KV_EST_MIN_THROTTLE * 100 % KV_EST_FULL_THROTTLE == 0,
              "the 75%+ text names the minimum in whole percent");

/** @brief The estimate's text with no figure: an input missing or out of range. */
#define KV_EST_TEXT_UNKNOWN  "KV EST --"
/** @brief The estimate's text with a figure: `KV EST 1750`, `KV EST 99999` at most. */
#define KV_EST_TEXT_FMT      "KV EST %lu"
/** @brief The estimate's text below the minimum throttle: `KV EST 75%+`. */
#define KV_EST_TEXT_LOW_FMT  "KV EST %d%%+"
/** @brief The widest estimate text, in characters: `KV EST 99999`. */
#define KV_EST_TEXT_WIDEST   12
/**
 * @brief The widest ERPM line, in characters: `ERPM 60000000  RPM/V 99999`.
 *
 * The DShot decoder's largest eRPM is 60,000,000, a period of 1 us
 * (`erpm_from_payload()`, src/dshot/dshot_telem.c), 8 digits.
 */
#define KV_EST_ERPM_LINE_WIDEST 26

/**
 * @brief Clamp a figure to the field.
 * @param v The rounded figure.
 * @return  @p v, or @ref KV_EST_MAX above it.
 */
static inline uint32_t kvEstClamp(uint64_t v) {
	return v > KV_EST_MAX ? KV_EST_MAX : (uint32_t)v;
}

/**
 * @brief RPM per volt, rounded half up and clamped.
 * @param rpm      Mechanical RPM.
 * @param volts100 The ESC's supply in hundredths of a volt.
 * @return The ratio; 0 for a voltage of 0, which kvEstView() never passes.
 */
static inline uint32_t kvEstRpmPerVolt(uint32_t rpm, uint32_t volts100) {
	if (volts100 == 0) return 0;
	return kvEstClamp(((uint64_t)rpm * 100 + volts100 / 2) / volts100);
}

/**
 * @brief The kV estimate, RPM x 2000 / (V x t), rounded half up and clamped.
 * @param rpm      Mechanical RPM.
 * @param volts100 The ESC's supply in hundredths of a volt.
 * @param t        The throttle sent, 0 to 2000.
 * @return The estimate; 0 for a voltage or throttle of 0, which kvEstView()
 *         never passes.
 */
static inline uint32_t kvEstKv(uint32_t rpm, uint32_t volts100, uint32_t t) {
	uint64_t den = (uint64_t)volts100 * t;
	if (den == 0) return 0;
	return kvEstClamp(((uint64_t)rpm * KV_EST_FULL_THROTTLE * 100 + den / 2) / den);
}

/**
 * @brief Whether the throttle has stood within @ref KV_EST_SETTLE_BAND of
 *        one value for @ref KV_EST_SETTLE_MS.
 *
 * A finger held on the throttle bar sets the throttle from its x on every
 * frame, and at MAX 100% one pixel is about 11 units: a settle that
 * restarted on every change would never end under a resting finger. So it
 * restarts only when the throttle leaves the band around the value it
 * started at, and that value becomes the new start.
 *
 * Zero-initialised it holds a throttle of 0 since time 0. Once settled it
 * stays settled until the throttle leaves the band, so a throttle held
 * across the millis() wrap does not read unsettled again.
 */
struct KvSettle {
	uint16_t t;        /**< The throttle the settle started at. */
	uint32_t sinceMs;  /**< When it started, millis(). */
	bool     settled;  /**< In the band for KV_EST_SETTLE_MS or more. */
};

/**
 * @brief Step the settle once per frame with the throttle being sent.
 * @param[in,out] s Settle state.
 * @param t         The throttle sent on this frame.
 * @param nowMs     millis() of this frame.
 * @return True once @p t has stayed within KV_EST_SETTLE_BAND of the value
 *         the settle started at for KV_EST_SETTLE_MS.
 */
static inline bool kvSettleStep(KvSettle *s, uint16_t t, uint32_t nowMs) {
	const int32_t d = (int32_t)t - (int32_t)s->t;
	if (d > KV_EST_SETTLE_BAND || d < -KV_EST_SETTLE_BAND) {
		s->t = t;
		s->sinceMs = nowMs;
		s->settled = false;
	} else if ((uint32_t)(nowMs - s->sinceMs) >= KV_EST_SETTLE_MS) {
		s->settled = true;
	}
	return s->settled;
}

/** @brief What a field shows. */
enum class KvEstShow : uint8_t {
	Unknown,  /**< `--`: an input is missing, stale or out of range. */
	Low,      /**< The estimate only: the throttle is below the minimum. */
	Value,    /**< The figure. */
};

/** @brief The inputs, as the tester holds them on one frame. */
struct KvEstIn {
	bool     armed;       /**< The tester is armed. */
	bool     alive;       /**< An eRPM answer is younger than ESC_LINK_STALE_MS. */
	uint32_t erpm;        /**< The filtered eRPM the readout shows. */
	uint32_t erpmRaw;     /**< The last eRPM reply, unfiltered. */
	uint8_t  poles;       /**< MOTOR POLES. */
	bool     voltsFresh;  /**< The merged voltage has a fresh source, KISS or EDT. */
	int32_t  volts100;    /**< That voltage in hundredths of a volt. */
	uint16_t t;           /**< The throttle sent, 0 to 2000. */
	bool     settled;     /**< kvSettleStep() for @p t. */
	bool     esc3d;       /**< The tester's 3D note stands. @see esc3dNote() */
	bool     erpmHeld;    /**< KvEstView::erpmHeld from the previous frame. */
};

/** @brief The two fields. A figure is meaningful only at KvEstShow::Value. */
struct KvEstView {
	KvEstShow rpv;    /**< RPM/V: Unknown or Value, never Low. */
	uint32_t  rpvValue;
	KvEstShow kv;     /**< The estimate. */
	uint32_t  kvValue;
	/**
	 * The eRPM check has passed and stands: the caller passes it back as
	 * KvEstIn::erpmHeld on the next frame. Cleared when the settle restarts or
	 * a condition fails.
	 */
	bool      erpmHeld;
};

/**
 * @brief Decide both fields, docs/design/esc-commands.md "When it is shown".
 *
 * RPM/V needs 1 and 2; the estimate needs all five:
 *
 * 1. Armed, the link alive, the pole count 2 or more, and RPM above 0 (eRPM
 *    over half the pole count, so eRPM above 0 with it).
 * 2. A fresh voltage above 0.5 V.
 * 3. t at least KV_EST_MIN_THROTTLE (else KvEstShow::Low) and at most 2000.
 * 4. t settled, KV_EST_SETTLE_MS within KV_EST_SETTLE_BAND, and the
 *    filtered eRPM within KV_EST_ERPM_SETTLE_PERMILLE of the last reply: a
 *    motor still spinning up, or a filter still catching up, shows none.
 *    Once the eRPM check has passed it holds (KvEstView::erpmHeld) until the
 *    settle restarts, a condition fails, the gap exceeds
 *    KV_EST_ERPM_HOLD_PERMILLE or the reply is 0, so noisy replies do not
 *    flicker the field between the figure and `--`.
 * 5. The 3D note not standing.
 *
 * A missing 1 or 2 reads Unknown before a low throttle reads Low: with no
 * data, raising the throttle would not make a figure appear.
 *
 * @param in The frame's inputs.
 * @return Both fields.
 */
static inline KvEstView kvEstView(const KvEstIn *in) {
	KvEstView v = { KvEstShow::Unknown, 0, KvEstShow::Unknown, 0, false };
	if (!in->armed || !in->alive || in->poles < 2) return v;
	uint32_t rpm = in->erpm / (uint32_t)(in->poles / 2);
	if (rpm == 0) return v;
	if (!in->voltsFresh || in->volts100 <= KV_EST_MIN_VOLTS100) return v;
	const uint32_t volts100 = (uint32_t)in->volts100;
	v.rpv = KvEstShow::Value;
	v.rpvValue = kvEstRpmPerVolt(rpm, volts100);
	if (in->esc3d || in->t > KV_EST_FULL_THROTTLE) return v;
	if (in->t < KV_EST_MIN_THROTTLE) {
		v.kv = KvEstShow::Low;
		return v;
	}
	if (!in->settled) return v;
	const uint64_t gap = in->erpm > in->erpmRaw ? in->erpm - in->erpmRaw
	                                            : in->erpmRaw - in->erpm;
	// A held check releases at the wider KV_EST_ERPM_HOLD_PERMILLE. A reply of
	// 0 is always past it, as the filtered eRPM is above 0 here: a stalled
	// motor shows `--` on its first frame, not after the filter has decayed.
	const uint32_t permille = in->erpmHeld ? KV_EST_ERPM_HOLD_PERMILLE
	                                       : KV_EST_ERPM_SETTLE_PERMILLE;
	if (gap * 1000 > (uint64_t)in->erpmRaw * permille)
		return v;
	v.erpmHeld = true;
	v.kv = KvEstShow::Value;
	v.kvValue = kvEstKv(rpm, volts100, in->t);
	return v;
}

/**
 * @brief The ERPM line: `ERPM 39200`, and while armed `  RPM/V 875` or
 *        `  RPM/V --` after it.
 * @param buf   Output, at least KV_EST_ERPM_LINE_WIDEST + 1 bytes.
 * @param n     Its size.
 * @param erpm  The filtered eRPM.
 * @param armed The tester is armed: RPM/V is on the line.
 * @param v     The fields.
 */
static inline void kvEstErpmLine(char *buf, size_t n, uint32_t erpm, bool armed,
                                 const KvEstView *v) {
	if (!armed)
		snprintf(buf, n, "ERPM %lu", (unsigned long)erpm);
	else if (v->rpv == KvEstShow::Value)
		snprintf(buf, n, "ERPM %lu  RPM/V %lu", (unsigned long)erpm,
		         (unsigned long)v->rpvValue);
	else
		snprintf(buf, n, "ERPM %lu  RPM/V --", (unsigned long)erpm);
}

/**
 * @brief The estimate's text: `KV EST 1750`, `KV EST 75%+` or `KV EST --`.
 * @param buf Output, at least KV_EST_TEXT_WIDEST + 1 bytes.
 * @param n   Its size.
 * @param v   The fields.
 */
static inline void kvEstText(char *buf, size_t n, const KvEstView *v) {
	if (v->kv == KvEstShow::Value)
		snprintf(buf, n, KV_EST_TEXT_FMT, (unsigned long)v->kvValue);
	else if (v->kv == KvEstShow::Low)
		snprintf(buf, n, KV_EST_TEXT_LOW_FMT,
		         KV_EST_MIN_THROTTLE * 100 / KV_EST_FULL_THROTTLE);
	else
		snprintf(buf, n, "%s", KV_EST_TEXT_UNKNOWN);
}
