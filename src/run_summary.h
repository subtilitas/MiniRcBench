/**
 * @file run_summary.h
 * @brief The run summary of one blackbox log: its extremes, the charge used,
 *        the armed time, the text of its `.TXT` file and SD LOG's rows.
 *        Pure; host-testable.
 *
 * The logger hands every frame's snapshot and merged reading to
 * runSumAdd() (@ref RunSumSample). A value counts only while it is fresh by
 * the firmware's windows: KISS for @ref KISS_STALE_MS, an EDT field for
 * @ref EDT_STALE_MS, DShot eRPM for @ref ESC_LINK_STALE_MS. It counts once
 * per arrival: a value whose arrival time equals the last one's from the
 * same source is not new.
 *
 * Each extreme is kept twice. The raw extreme counts every new value. The
 * held extreme counts a value only when it holds for three consecutive new
 * values: the highest of the lowest of the last three for a maximum, the
 * lowest of the highest for a minimum. One damaged frame sets no held value.
 *
 * Voltage, current and temperature each have two figures. The per-frame
 * figure takes KISS's field while a KISS frame is fresh and EDT's while it
 * is not, as escMerge() does for the tester's readout; with values from both
 * it is reported as `MIXED`. The EDT-only figure counts EDT's field whenever
 * it is fresh, and is the one reported when every fresh KISS frame of the
 * log carried 0 in that field: an ESC without that sensor. The minimum
 * voltage counts only while the tester is armed, and a reading of 0.00 V is
 * no reading.
 *
 * Times are the 64-bit microsecond timer (`time_us_64()`): the 32-bit
 * micros() wraps after 71.6 minutes.
 *
 * Slice 1 of docs/design/run-summary.md. sd_log.cpp feeds it and ends it
 * (slice 2).
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "esc_merge.h"
#include "sd_log.h"

struct EscTelemetry;

/** @brief How a log ended, as its summary records it. */
enum class RunSumEnd : uint8_t {
	Running = 0,  /**< The log runs: runSumEnd() has not been called. */
	Stop,         /**< STOP on SD LOG. */
	Disarm,       /**< The disarm that stops a log arming started. */
	ByKst,        /**< KST SERVO took the card. */
	ByHitec,      /**< HITEC SERVO took the card. */
	ByAm32,       /**< AM32 CONFIG took the card. */
	CardFull,     /**< A write during the log found the card full. */
	WriteFailed,  /**< A write during the log failed. */
};

/** @brief What became of the summary's `.TXT` file. */
enum class RunSumFile : uint8_t {
	NotWritten = 0,  /**< Not yet tried: the log runs, or its stop has not written it. */
	Saved,           /**< Written and closed. */
	CardFull,        /**< Not saved: the card is full. */
	WriteFailed,     /**< Not saved: a write failed. */
};

/** @brief Where a reported value came from. */
enum class RunSumFrom : uint8_t {
	None = 0,  /**< No value. */
	DShot,     /**< Bidirectional DShot eRPM. */
	Kiss,      /**< KISS telemetry. */
	Edt,       /**< Extended DShot Telemetry. */
	Mixed,     /**< KISS while fresh, EDT in its gaps. */
};

/**
 * @brief One extreme of one figure: its raw and held value and the last
 *        three new values.
 *
 * Sources are kept as "from KISS" or not: the other source is the DShot
 * line, EDT for voltage, current and temperature, DShot eRPM for eRPM.
 */
struct RunSumExtreme {
	int32_t  win[3];      /**< The last three new values, the newest last. */
	bool     winKiss[3];  /**< Whether each came from KISS. */
	uint8_t  run;         /**< New values in the window, 0 to 3. */
	bool     heldOk;      /**< A held value exists: three new values have come. */
	bool     heldKiss;    /**< The held value came from KISS. */
	int32_t  held;        /**< The held extreme. */
	bool     rawOk;       /**< A raw value exists: one new value has come. */
	bool     rawKiss;     /**< The raw value came from KISS. */
	int32_t  raw;         /**< The raw extreme. */
	bool     anyKiss;     /**< A value from KISS was counted. */
	bool     anyLine;     /**< A value from the DShot line was counted. */
	uint32_t kissMs;      /**< Arrival time of the last value from KISS. */
	uint32_t lineMs;      /**< Arrival time of the last value from the DShot line. */
};

/**
 * @brief The running summary of one log. Zeroed and started by
 *        runSumBegin(); 464 bytes on the RP2350.
 *
 * Values are integers: voltage and current in 0.01 V and 0.01 A, temperature
 * in degrees Celsius, eRPM as the ESC sends it.
 */
struct RunSum {
	RunSumEnd  end;           /**< How the log ended, Running until runSumEnd(). */
	RunSumFile file;          /**< What became of the `.TXT`. */
	bool       everArmed;     /**< A sample, or the start, was armed. */
	bool       prevArmed;     /**< The arm state of the last sample. */
	uint64_t   startUs;       /**< runSumBegin()'s time. */
	uint64_t   lastUs;        /**< The last sample's time. */
	uint64_t   stopUs;        /**< runSumEnd()'s time. */
	uint64_t   armedUs;       /**< Time armed within the log. */

	RunSumExtreme erpm;       /**< DShot eRPM, maximum. */
	RunSumExtreme kissErpm;   /**< KISS eRPM, maximum. */
	RunSumExtreme amps;       /**< Current per frame, maximum. */
	RunSumExtreme ampsEdt;    /**< EDT current alone, maximum. */
	RunSumExtreme temp;       /**< Temperature per frame, maximum. */
	RunSumExtreme tempEdt;    /**< EDT temperature alone, maximum. */
	RunSumExtreme volts;      /**< Voltage per frame, minimum, while armed. */
	RunSumExtreme voltsEdt;   /**< EDT voltage alone, minimum, while armed. */

	bool     kissSeen;        /**< A KISS frame was fresh in some sample. */
	bool     kissVoltsSet;    /**< A fresh KISS frame carried a voltage above 0. */
	bool     kissAmpsSet;     /**< ... a current above 0. */
	bool     kissTempSet;     /**< ... a temperature other than 0. */
	bool     kissMahSet;      /**< ... a counter other than 0. */

	bool     mahSeen;         /**< The KISS counter has a last value. */
	uint16_t mahLast;         /**< Its last value. */
	uint32_t mahMs;           /**< Its arrival time. */
	uint32_t mahRise;         /**< The sum of its increases, mAh. */

	int32_t  curFrame;        /**< The last sample's per-frame current, 0.01 A; -1 for none. */
	bool     curFrameKiss;    /**< It came from KISS. */
	int32_t  curEdt;          /**< The last sample's EDT current, 0.01 A; -1 for none. */
	uint64_t ampUs;           /**< Per-frame current times time, 0.01 A × µs. */
	uint64_t ampKnownUs;      /**< Time with a per-frame current. */
	bool     ampSumKiss;      /**< KISS current went into @ref ampUs. */
	bool     ampSumEdt;       /**< EDT current went into @ref ampUs. */
	uint64_t ampEdtUs;        /**< EDT current alone times time, 0.01 A × µs. */
	uint64_t ampEdtKnownUs;   /**< Time with an EDT current. */
};

/**
 * @brief One logged frame, as the logger hands it over: the snapshot and the
 *        reading merged from it, the arm state and the time.
 */
struct RunSumSample {
	const EscTelemetry *tel;  /**< escSnapshot()'s block. */
	const EscReading *reading;/**< escMerge() of @ref tel at @ref nowMs, with KISS_STALE_MS and EDT_STALE_MS. */
	uint32_t nowMs;           /**< millis() of the merge. */
	uint64_t nowUs;           /**< time_us_64() of the frame. */
	bool     armed;           /**< sdLogSetArmed()'s state. */
};

/** @brief What a value of the summary reports, resolved from its figures. */
struct RunSumValue {
	RunSumFrom from;      /**< The source tag; None when no value came. */
	bool    heldOk;       /**< The held value exists. */
	int32_t held;         /**< The held value. */
	bool    heldKiss;     /**< It came from KISS. */
	bool    rawOk;        /**< The raw value exists. */
	int32_t raw;          /**< The raw value. */
	bool    rawKiss;      /**< It came from KISS. */
	bool    kissSentZero; /**< The EDT-only figure, as KISS sent 0 in every fresh frame. */
};

/** @brief A charge figure: the KISS counter's rise, or the current's integral. */
struct RunSumCharge {
	RunSumFrom from;      /**< The source tag; None when unknown. */
	uint32_t mah;         /**< The charge, mAh. */
	uint64_t knownUs;     /**< From current: the time with a current. */
	bool     kissSentZero;/**< From current: EDT alone, as KISS sent 0 A. */
	bool     counterZero; /**< ESC counter: KISS frames came, the counter stayed 0. */
};

/**
 * @brief Start a summary.
 * @param s       The summary, zeroed here.
 * @param startUs time_us_64() at the log's start.
 * @param armed   The arm state at the start.
 */
void runSumBegin(RunSum *s, uint64_t startUs, bool armed);

/**
 * @brief Count one logged frame. Constant time. Ignored after runSumEnd().
 *
 * The time since the previous sample, or since the start, counts as armed
 * when the previous sample was armed, and with the previous sample's current
 * for at most its source's stale window: 500 ms for KISS, 1000 ms for EDT.
 *
 * @param s The summary.
 * @param x The frame.
 */
void runSumAdd(RunSum *s, const RunSumSample *x);

/**
 * @brief End the summary: the last interval to @p stopUs, and how the log
 *        ended. A second call is ignored. A log ended by @ref
 *        RunSumEnd::CardFull or @ref RunSumEnd::WriteFailed writes no file:
 *        its @ref RunSum::file reads not saved with the same cause.
 * @param s      The summary.
 * @param stopUs time_us_64() at the stop, or at the failed write.
 * @param how    How the log ended; not @ref RunSumEnd::Running.
 */
void runSumEnd(RunSum *s, uint64_t stopUs, RunSumEnd how);

/**
 * @brief Record what became of the `.TXT`.
 * @param s The summary.
 * @param f The result.
 */
void runSumFileResult(RunSum *s, RunSumFile f);

/** @brief Maximum eRPM: DShot's, KISS's only when no DShot eRPM came. @param s The summary. @return The value. */
RunSumValue runSumMaxErpm(const RunSum *s);
/** @brief Maximum current, 0.01 A. @param s The summary. @return The value. */
RunSumValue runSumMaxAmps(const RunSum *s);
/** @brief Maximum temperature, degrees Celsius. @param s The summary. @return The value. */
RunSumValue runSumMaxTemp(const RunSum *s);
/** @brief Minimum voltage while armed, 0.01 V. @param s The summary. @return The value. */
RunSumValue runSumMinVolts(const RunSum *s);
/** @brief The KISS counter's rise over the log. @param s The summary. @return The charge. */
RunSumCharge runSumUsedCounter(const RunSum *s);
/** @brief The charge from the current's integral. @param s The summary. @return The charge. */
RunSumCharge runSumUsedCurrent(const RunSum *s);

/**
 * @brief RPM from eRPM, as escFoldReply() converts it: divided by half the
 *        pole count, unchanged below 2 poles. Pure.
 * @param erpm  Electrical RPM.
 * @param poles MOTOR POLES.
 * @return Mechanical RPM.
 */
static inline uint32_t runSumRpm(uint32_t erpm, uint8_t poles) {
	return poles >= 2 ? erpm / (poles / 2u) : erpm;
}

/** @brief Bytes runSumTxtName() writes, the NUL included. */
#define RUN_SUM_NAME_BYTES 13

/**
 * @brief The summary's file name, `LOGnnnnn.TXT`, the number of its `.BFL`.
 * @param n   The log's number, 0 to @ref SD_LOG_NUMBER_MAX.
 * @param out At least @ref RUN_SUM_NAME_BYTES bytes.
 */
void runSumTxtName(uint32_t n, char *out);

/** @brief The file's format number, on its first line. */
#define RUN_SUM_FORMAT 1

/**
 * @brief The text of the `.TXT` file: ASCII, one value a line, line feeds.
 *
 * The clock lines read `not set`: the clock is slice 4's. The counters from
 * Frames to Worst flush are the logger's, from @p st.
 *
 * @param s     An ended summary.
 * @param st    The logger's counters at the stop; its file number names the log.
 * @param poles MOTOR POLES at the stop.
 * @param out   Destination; 1000 bytes hold the longest text.
 * @param n     Its size.
 * @return The text's length, as snprintf() counts it: n or more means cut.
 */
size_t runSumText(const RunSum *s, const SdLogStatus *st, uint8_t poles,
                  char *out, size_t n);

/** @brief SD LOG's rows 2 to 8 after a stop. */
enum class RunSumRow : uint8_t {
	MaxRpm = 0,  /**< `MAX RPM`. */
	MaxCurrent,  /**< `MAX CURRENT`. */
	MaxTemp,     /**< `MAX TEMP`. */
	MinVoltage,  /**< `MIN VOLTAGE`. */
	Used,        /**< `USED` or `USED FROM CURRENT`. */
	Armed,       /**< `ARMED`. */
	Summary,     /**< `SUMMARY`. */
	Count,       /**< The number of rows. */
};

/** @brief What one of SD LOG's run rows shows. */
struct RunSumRowText {
	const char *label;  /**< The row's label. */
	char value[24];     /**< The value, or `--` when unknown; holds the longest,
	                         `NOT SAVED: WRITE FAILED`, and its NUL. */
	const char *tag;    /**< `KISS`, `EDT` or `MIXED`, or nullptr for none. */
	bool unknown;       /**< The value is `--`. */
	bool alert;         /**< MAX TEMP at 90 °C or above, or SUMMARY not saved. */
};

/**
 * @brief The text of one of SD LOG's run rows. Values follow the tester's
 *        tiles: `14.82V`, `23.41A`, `312mAh`, and a temperature with a
 *        backtick before the C, which the font draws as a degree sign; EDT
 *        current in whole amperes, `23A`.
 * @param s     An ended summary.
 * @param row   The row.
 * @param poles MOTOR POLES at the stop.
 * @param fileNumber The log's number, for the SUMMARY row.
 * @param out   Filled in.
 */
void runSumRowText(const RunSum *s, RunSumRow row, uint8_t poles,
                   uint32_t fileNumber, RunSumRowText *out);

/** @brief The temperature from which MAX TEMP is red, as the tester's tile. */
#define RUN_SUM_TEMP_HOT_C 90
