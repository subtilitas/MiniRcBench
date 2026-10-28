/**
 * @file kst_job.h
 * @brief The KST programmer's core1 job: rcbench's session on the SIG pin,
 *        while the pin claim holds it.
 *
 * Core0 owns the screen and sends commands; core1 owns the line
 * (core1_job.h). The two share a command block and a status block under one
 * critical section, as the servo job does. Every pass of `loop1()` the job
 * takes a new command, steps the search (kst_seek.h) or kst_session_step(),
 * and publishes the status when it changed. Nothing here waits:
 * kst_session_step() never does, and a planning step does one slice of
 * `KST_PLAN_SLICE_UNITS`, 100 units.
 *
 * | State | The line | Leaves it |
 * |---|---|---|
 * | Opening | driven low, from the bind | Search: Searching |
 * | Searching | the entry sequence, repeated | Found: InMode; any other end: SearchEnded |
 * | SearchEnded | driven low | Search (RETRY): Searching |
 * | InMode | driven low; the servo is in programming mode | ReadAll, Verify, Edit, Restore: Busy |
 * | Busy | the operation's frames | its end: InMode; a loss: Searching, or SearchEnded with the heartbeat lapsed |
 * | Leaving | the running operation finishes its frame, then low for @ref KST_LEAVE_LOW_MS | Closed |
 * | Closed | an input with its pull-down on (kstLineClose()) | the release |
 *
 * Leave, and the release, take every state but Leaving and Closed to
 * Leaving.
 *
 * The job never calls kst_session_write(), kst_plan_edit() or
 * kst_plan_restore(): they run the planner to its end in one call and their
 * stack is not measured. Edits go through kst_session_edit() and restores
 * through kst_session_restore(), which plan in slices.
 *
 * Heartbeat (core1UiAlive()): with it lapsed no search and no operation
 * starts, and a running search ends after its attempt (KstSeek::NoUi). A
 * running operation finishes, as pin-claim.md decision 1 requires.
 *
 * Release (core1JobStop()) leaves as Leave does, from any state: the running
 * operation finishes its frame, the line stays low for @ref KST_LEAVE_LOW_MS,
 * then it closes. The servo stays in programming mode until its supply is
 * off; whether it follows PWM before that is not measured.
 *
 * **Switched supply** (servo_supply.h, SETTINGS > SERVO SUPPLY at GP0). The
 * bind takes GP0 low and the release makes it an input again; between them:
 *
 * - The search is switched (kst_seek.h): supply off for
 *   @ref KST_SUPPLY_OFF_MS before each attempt, on for the entry.
 * - Reboot, from InMode: supply off for @ref KST_SUPPLY_OFF_MS, on for
 *   @ref KST_SUPPLY_WAIT_MS, then an entry and a read; the line low
 *   throughout, kst_session_power_cycled() at the start. An entry without
 *   an answer starts the switched search.
 * - The supply is on only while the heartbeat is trusted. A lapse ends an off
 *   or on wait at once; a running operation finishes first. Then the supply
 *   goes off, kst_session_power_cycled() runs and the job is SearchEnded with
 *   the search NoUi, locked session or not.
 * - The close rule: once the supply was on in the claim, Leave and the
 *   release close the line only @ref KST_SUPPLY_OFF_MS after the last
 *   switch-off command, which they give first if the supply is on.
 *
 * @see docs/design/kst-programming.md, "Where the session runs".
 */

#pragma once

#include <stdint.h>

#include "core1_job.h"
#include "kst_seek.h"
#include "kst_session.h"

/** @brief The line stays low this long before a leave closes it, ms. */
#define KST_LEAVE_LOW_MS 100u

/** @brief What a command asks of the job. */
enum class KstOp : uint8_t {
	Search,      /**< Begin a search: from Opening, and RETRY from SearchEnded. */
	StopSearch,  /**< STOP SEARCH: end the running search after its frame. */
	ReadAll,     /**< kst_session_read_all(). */
	Verify,      /**< kst_session_verify() against @c KstCmd::target. */
	Edit,        /**< kst_session_edit() to @c KstCmd::target. */
	Restore,     /**< kst_session_restore() to the backup. */
	Abort,       /**< kst_session_abort() of the running operation, or STOP SEARCH. */
	Leave,       /**< LEAVE: end what runs, the line low for @ref KST_LEAVE_LOW_MS, close. */
	Reboot,      /**< REBOOT: switched supply only, from InMode. */
};

/** @brief Where the job is. @see the table in kst_job.h */
enum class KstJobState : uint8_t {
	Opening,      /**< Bound, the line low, nothing started. */
	Searching,    /**< A search runs. */
	SearchEnded,  /**< A search ended without a servo; KstStatus::seek says how. */
	InMode,       /**< In programming mode; no operation runs. */
	Busy,         /**< An operation runs. */
	Leaving,      /**< Leave or the release: finishing, then the low time. */
	Closed,       /**< The line is closed. */
	Rebooting,    /**< Reboot: supply off, on, then an entry and a read. */
};

/** @brief Where a Reboot is. */
enum class KstReboot : uint8_t {
	None,  /**< No reboot runs. */
	Off,   /**< Supply off for @ref KST_SUPPLY_OFF_MS. */
	On,    /**< Supply on for @ref KST_SUPPLY_WAIT_MS. */
	Enter, /**< kst_session_enter() runs. */
	Read,  /**< kst_session_read_all() runs. */
};

/** @brief Why the last command taken did nothing. */
enum class KstRefusal : uint8_t {
	None,   /**< Taken. */
	State,  /**< Not in a state that takes it. */
	NoUi,   /**< The heartbeat has lapsed: no search or operation starts. */
};

/** @brief One command from core0. */
struct KstCmd {
	uint16_t        seq;               /**< Set by kstJobCommand(); core1 echoes it. */
	KstOp           op;                /**< What to do. */
	kst_image_t     target;            /**< Edit's target, Verify's expected image. */
	kst_field_set_t unlocked;          /**< Edit's unlock set, `kst_plan_edit()`'s. */
	bool            confirmUnchecked;  /**< Restore: an unchecked plan may run. */
};

/** @brief What core1 reports. Session fields mirror `kst_session_t`. */
struct KstStatus {
	uint16_t    ackSeq;         /**< Seq of the last command taken. */
	KstJobState job;            /**< Where the job is. */
	KstRefusal  refused;        /**< Of the last command taken. */
	uint8_t     lineError;      /**< `kst_line_err_t` of the bind. */
	KstSeek     seek;           /**< The last search. */
	uint32_t    attempts;       /**< Its attempts. */
	uint32_t    searchMs;       /**< How long it has run, kstSeekMs(). */
	uint8_t     seekRx;         /**< Its last refused reply, `kst_rx_t`; `KST_RX_NO_REPLY` for none. */
	uint8_t     result;         /**< `kst_ses_t` of the last operation or search. */
	uint8_t     lastRx;         /**< `kst_rx_t` of the last transaction. */
	bool        inMode;         /**< The servo is in programming mode. */
	bool        hasImage;       /**< @c image is valid. */
	bool        hasBackup;      /**< @c backup is valid. */
	bool        fingerprintOk;  /**< The image passes the layout fingerprint. */
	bool        locked;         /**< Only a restore is accepted. */
	bool        planning;       /**< The operation plans; nothing is sent. */
	uint8_t     planResult;     /**< `kst_plan_result_t` of the last edit or restore. */
	uint8_t     stepIndex;      /**< Writes of the running plan that are done. */
	uint32_t    badRegs;        /**< Bit n: register n has no 3 equal reads. */
	uint32_t    diffRegs;       /**< Bit n: register n differs from what was expected. */
	uint32_t    wroteRegs;      /**< Bit n: the session wrote register n. */
	uint32_t    frames;         /**< Frames the running or last operation or search sent. */
	bool        stalled;        /**< A heartbeat lapse ended the last search. */
	bool        supplyFitted;   /**< The bench switches the servo's supply. */
	bool        supplyOn;       /**< The level the bench drives on GP0: on. */
	bool        supplyUsed;     /**< The supply was on in this claim: a close waits. */
	uint16_t    supplyLeftDs;   /**< Time left of the wait under way, 0.1 s, rounded up; 0 for none. */
	KstReboot   reboot;         /**< Where a Reboot is. */
	kst_image_t image;          /**< The servo as last read. */
	kst_image_t backup;         /**< The first image read since the servo was found. */
	kst_stats_t stats;          /**< Reply timing since the bind. */
};

/**
 * @brief Send a command. Core0.
 *
 * One command is held at a time, and a new one replaces a command core1 has
 * not taken, with one exception: a StopSearch, Abort or Leave not yet taken
 * stays, and a command sent after it is dropped, unless that is a Leave,
 * which replaces a held StopSearch or Abort. A stop is never lost to a later
 * command.
 *
 * Commands held at the bind are not taken: the job starts with the seq of
 * the block as taken.
 *
 * @param c The command; its seq is ignored.
 * @return The seq of the command held, which KstStatus::ackSeq echoes once
 *         core1 has taken it.
 */
uint16_t kstJobCommand(const KstCmd *c);

/**
 * @brief Read the job's status. Core0, under the job's critical section.
 * @param[out] out The status.
 */
void kstJobStatus(KstStatus *out);

/** @brief The KST job's entry in the job table of core1_job.cpp. */
extern const Core1JobOps KST_JOB_OPS;
