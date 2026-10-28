/**
 * @file hitec_job.h
 * @brief The Hitec programmer's core1 job: rcbench's Hitec session on the
 *        SIG pin, while the pin claim holds it.
 *
 * Hitec is a name or trademark of its owner; it is used here only to say
 * which servos this module works with.
 *
 * Core0 owns the screen and sends commands; core1 owns the line
 * (core1_job.h). The two share a command block and a status block under one
 * critical section, as the KST job does. Every pass of `loop1()` the job
 * takes a new command, then drives the session: hitec_session_next() hands
 * out a step, the job runs it on the line (hitec_line_port.h) or on the
 * switched supply, and hitec_session_feed() takes its result. The session
 * never waits and the line driver returns at once. hitec_session_write()
 * runs hitec_plan() to its end in one call, at most `HITEC_PLAN_MAX` steps.
 *
 * | State | The line | Leaves it |
 * |---|---|---|
 * | Opening | released, from the bind | Enter: Entering |
 * | Entering | the entry: POWER, LISTENs, the first command or the read of 0x00 | OK: Busy with the full read; any other end: EntryEnded |
 * | EntryEnded | released; the supply off | Enter (RETRY): Entering |
 * | InMode | released; the servo is in serial mode | ReadAll, Write: Busy |
 * | Busy | the operation's transactions | its end: InMode; a lapse with the supply on: EntryEnded |
 * | Leaving | a running write finishes, a read or an entry is cut; then the supply off for @ref KST_SUPPLY_OFF_MS, or the D series reboot with `NONE` | Closed |
 * | Closed | an input with the family's pull (hitecLineClose()) | the release |
 *
 * The family, `HITEC_CLASSIC` or `HITEC_D`, is set before the claim
 * with hitecJobSetFamily(). Classic needs the switched supply: without it
 * Enter is refused.
 *
 * Heartbeat (core1UiAlive()): with it lapsed no entry and no operation
 * starts. A running entry is cut: an off phase ends with the supply never
 * on, a LISTEN is cut, a SEND finishes. A running read or write finishes,
 * as pin-claim.md decision 1 requires. With the supply on a lapse is
 * latched; once what runs has ended the supply goes off, the session is
 * initialised again and the job is EntryEnded with `stalled` set.
 *
 * **Switched supply** (servo_supply.h). The bind takes GP0 low and the
 * release makes it an input again. An entry's POWER step switches the
 * supply off for @ref KST_SUPPLY_OFF_MS from the last switch-off command,
 * then on and starts its LISTEN in the same pass, only while the heartbeat
 * is trusted. Leave and the release close the line only
 * @ref KST_SUPPLY_OFF_MS after the last switch-off once the supply was on.
 *
 * The card phase: around a card call on core0, CardBegin opens a phase in
 * which the heartbeat also counts as trusted while hitecCardStamp(), called
 * from the SDIO driver's waits, is younger than the UI's timeout, for at
 * most @ref HITEC_CARD_PHASE_MAX_MS from the acknowledge. CardEnd, or any
 * other command, closes it.
 *
 * @see docs/design/hitec-programming.md, "Where the session runs".
 */

#pragma once

#include <stdint.h>

#include "core1_job.h"
#include "hitec_session.h"

/** @brief A card phase keeps the heartbeat for at most this long, ms. */
#define HITEC_CARD_PHASE_MAX_MS 15000u

/** @brief What a command asks of the job. */
enum class HitecOp : uint8_t {
	Enter,      /**< The entry, then the full read: from Opening, and RETRY from EntryEnded. */
	ReadAll,    /**< The full read: READ AGAIN, from InMode. */
	Write,      /**< hitec_session_write() to @ref HitecCmd::target, from InMode. */
	Abort,      /**< Cut a running entry or read; a write is not aborted. */
	Leave,      /**< LEAVE: a write finishes, a read or an entry is cut, then the close. */
	CardBegin,  /**< A card phase: Opening, EntryEnded or InMode. */
	CardEnd,    /**< The card phase ends; any other command taken ends it too. */
};

/** @brief Where the job is. @see the table in hitec_job.h */
enum class HitecJobState : uint8_t {
	Opening,     /**< Bound, the line released, nothing started. */
	Entering,    /**< The entry runs. */
	EntryEnded,  /**< An entry ended without the mode, or a lapse stalled the session. */
	InMode,      /**< In serial mode; no operation runs. */
	Busy,        /**< A read or a write runs. */
	Leaving,     /**< Leave or the release: finishing, then the off wait or the reboot. */
	Closed,      /**< The line is closed. */
};

/** @brief Why the last command taken did nothing. */
enum class HitecRefusal : uint8_t {
	None,     /**< Taken. */
	State,    /**< Not in a state that takes it, or classic without the switched supply. */
	NoUi,     /**< The heartbeat has lapsed: no entry or operation starts. */
	Unknown,  /**< Write: the image lacks an address a write needs; read first. */
};

/** @brief Where an entry's POWER step is. */
enum class HitecPower : uint8_t {
	None,    /**< No POWER step runs. */
	Off,     /**< The supply off, the off time running. */
	Listen,  /**< On; the LISTEN runs. */
};

/** @brief One command from core0. */
struct HitecCmd {
	uint16_t      seq;     /**< Set by hitecJobCommand(); core1 echoes it. */
	HitecOp       op;      /**< What to do. */
	hitec_image_t target;  /**< Write: the draft, a restore's backup or a loaded file. */
};

/** @brief What core1 reports. Session fields mirror `hitec_session_t`. */
struct HitecStatus {
	uint16_t      ackSeq;        /**< Seq of the last command taken. */
	HitecJobState job;           /**< Where the job is. */
	HitecRefusal  refused;       /**< Of the last command taken. */
	uint8_t       family;        /**< `hitec_family_t` of the claim. */
	uint8_t       lineError;     /**< `hitec_line_err_t` of the bind. */
	uint8_t       result;        /**< `hitec_ses_t` of the last entry or operation. */
	bool          lastWrite;     /**< The last operation was a write. */
	uint8_t       lastRx;        /**< `hitec_rx_t` of the last decoded step. */
	uint8_t       failAddr;      /**< The address of the read or write that failed. */
	uint8_t       tries;         /**< Tries of the running or last transaction. */
	uint16_t      done;          /**< Addresses read by the running read. */
	uint16_t      total;         /**< Addresses it reads. */
	uint8_t       stepIndex;     /**< Writes of the plan that took. */
	uint8_t       planSteps;     /**< Steps of the running or last plan. */
	uint8_t       planResult;    /**< `hitec_plan_result_t` of the last write. */
	bool          inMode;        /**< The servo is in serial mode. */
	bool          hasImage;      /**< A full read has completed since the entry. */
	bool          hasBackup;     /**< @c backup is valid. */
	bool          unknown;       /**< The image lacks an address a write needs. */
	HitecPower    power;         /**< Where an entry's POWER step is. */
	bool          supplyFitted;  /**< The bench switches the servo's supply. */
	bool          supplyOn;      /**< The level the bench drives on GP0: on. */
	bool          supplyUsed;    /**< The supply was on in this claim: a close waits. */
	uint16_t      supplyLeftDs;  /**< Time left of the off wait under way, 0.1 s, rounded up; 0 for none. */
	bool          stalled;       /**< A heartbeat lapse ended the session. */
	bool          stallWrote;    /**< The stalled session had sent a write. */
	bool          cardPhase;     /**< A card phase is open: the card heartbeat counts. */
	uint8_t       diff[HITEC_SES_SET_BYTES];  /**< The last write's read-back: bit a set, address a differs. */
	hitec_image_t image;         /**< What the servo confirmed. */
	hitec_image_t backup;        /**< The first full read after the entry. */
};

/**
 * @brief Set the family of the next claim. Core0, only while no core1 job
 *        runs; the bind reads it, and RETRY after a bind error binds again
 *        with it.
 * @param family `hitec_family_t`: `HITEC_CLASSIC` or `HITEC_D`.
 */
void hitecJobSetFamily(uint8_t family);

/**
 * @brief Send a command. Core0.
 *
 * One command is held at a time, and a new one replaces a command core1 has
 * not taken yet, except that an Abort or Leave not yet taken stays and the
 * later command is dropped; a Leave replaces a held Abort.
 * @return The seq the held command carries; compare with
 *         @ref HitecStatus::ackSeq.
 */
uint16_t hitecJobCommand(const HitecCmd *c);

/** @brief Copy the last status core1 published. Core0. */
void hitecJobStatus(HitecStatus *out);

/**
 * @brief Core0, from the SDIO driver's waits (`sdh_wait_hook`) during a card
 *        phase: stamp the card heartbeat.
 */
void hitecCardStamp();

/** @brief The Hitec job's entry in the job table of core1_job.cpp. */
extern const Core1JobOps HITEC_JOB_OPS;
