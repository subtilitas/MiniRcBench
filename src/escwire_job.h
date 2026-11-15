/**
 * @file escwire_job.h
 * @brief The ESC settings job on core1: rcbench's escwire session on the SIG
 *        pin, for an EFM8 bootloader (Bluejay, BLHeli_S) that answered ESC
 *        SETTINGS' greeting.
 *
 * BLHeli_S and Bluejay are names of their owners' firmware, used here only
 * to say which ESCs this module works with.
 *
 * Core0 owns the screen and sends one command at a time; core1 owns the
 * line (core1_job.h). The two share a command block and a status block
 * under one critical section, as the KST and Hitec jobs do. On every pass
 * of `loop1()` the job takes a new command, then calls `ew_session_step()`
 * while an operation runs; the session never waits and the line driver
 * returns at once (docs/design/bluejay-blheli-s.md, "The escwire session").
 *
 * | State | The line | Leaves it |
 * |---|---|---|
 * | Opening | driven high from the bind (`ew_session_hold()`) | Enter: Busy |
 * | Busy | the entry, a read, a write or the leave | its end: InMode, or Ended |
 * | InMode | on the pull-up; the ESC waits in its bootloader | Read, Write, Leave: Busy |
 * | Ended | parked: the entry failed, a stop cut the session, or the leave ran | the release |
 * | Closed | an input with its pull-up (escwirePortClose()) | the release |
 *
 * A refused bind puts the pull-up on SIG, which the AM32 transport left
 * floating, and touches nothing else.
 *
 * The bind opens the line, which parks it on the pull-down, and drives it
 * high in the same call: the gap after `am32BlEnd()`, which leaves it
 * floating, is the one core1 pass before the bind (decision of the note's
 * "The escwire session"). Enter holds the line 500 ms more, the shortest
 * hold rcbench takes, then makes the first contact and identifies the page.
 * Read reads the whole page. Write is rcbench's write of a draft
 * (`ew_session_write()`): the guard judges it, the page is read again and
 * compared with the first read, then erased, sent and programmed, and read
 * back; a failed step or a read-back that differs runs the procedure again
 * from the erase, 3 times at most. Only the settings page is erased or
 * programmed: rcbench's guard refuses any other address, the bootloader's
 * among them, and a layout that is not written. Leave sends `00 01`, which
 * starts the ESC's firmware, idles 500 us, drives the line low 300 ms and
 * parks it.
 *
 * Heartbeat (core1UiAlive()): with it lapsed no operation starts, and a
 * running entry, read or write is stopped with `ew_session_stop()`, never
 * `ew_session_abort()`. A stop outside a write's erase and program cuts at
 * once and parks the line; a write that has sent its erase or program runs
 * to its read-back, starts no other procedure, and then parks the line
 * (docs/design/bluejay-blheli-s.md, "The escwire session"). A running leave
 * finishes: its frame has gone, and the rest is 300 ms of low line. The
 * release does the same, then closes the line.
 */

#pragma once

#include <stdint.h>

#include "core1_job.h"
#include "ew_session.h"

/** @brief What a command asks of the job. */
enum class EwJobOp : uint8_t {
	Enter,  /**< The hold, the first contact and the identification: from Opening. */
	Read,   /**< The whole page: from InMode. */
	Leave,  /**< `00 01`, then the line low and parked: from InMode. */
	Stop,   /**< `ew_session_stop()`: a running entry or read is cut, a write runs to its read-back; any state. */
	Write,  /**< The draft escwireJobWrite() holds: from InMode. */
};

/** @brief Where the job is. @see the table in escwire_job.h */
enum class EwJobState : uint8_t {
	Opening,  /**< Bound, the line driven high, nothing started. */
	Busy,     /**< The entry, a read, a write or the leave runs. */
	InMode,   /**< The ESC answered and waits; no operation runs. */
	Ended,    /**< The session ended: no ESC in mode on this session. */
	Closed,   /**< The line is closed. */
};

/** @brief Why the last command taken did nothing. */
enum class EwJobRefusal : uint8_t {
	None,   /**< Taken. */
	State,  /**< Not in a state that takes it, or the session refused it. */
	NoUi,   /**< The heartbeat has lapsed: no operation starts. */
};

/** @brief What core1 reports. Session fields mirror `ew_session_t`. */
struct EwJobStatus {
	uint16_t     ackSeq;     /**< Seq of the last command taken. */
	EwJobState   job;        /**< Where the job is. */
	EwJobRefusal refused;    /**< Of the last command taken. */
	uint8_t      lineError;  /**< `ew_line_err_t` of the bind; 0 when it bound. */
	EwJobOp      lastOp;     /**< The last operation started. */
	uint8_t      result;     /**< `ew_ses_t` of the last operation that ended; `EW_SES_BUSY` while one runs. */
	bool         inMode;     /**< The ESC answered and has not left. */
	bool         haveRead;   /**< A read of the whole page succeeded. */
	bool         stalled;    /**< A heartbeat lapse stopped an operation. */
	bool         left;       /**< The leave ran to its end: `00 01` went out. */
	bool         dirty;      /**< An erase or program went and no read-back proved the page: half written. */
	bool         reread;     /**< Half written, and a read came since: a write may follow. */
	bool         writing;    /**< The running write has sent its erase or program. */
	uint8_t      refuse;     /**< The guard's last refusal, an `ew_refuse_t`. */
	uint16_t     badOff;     /**< The byte the guard refused. */
	ew_contact_t contact;    /**< The first contact's answer. */
	ew_ident_t   id;         /**< What identification found. */
	/**
	 * The page as read: `id.len` bytes, the first 112 after the entry alone.
	 * A verified write makes it the page written; while the page is half
	 * written it stays the page as first read.
	 */
	uint8_t      page[EW_PAGE_MAX];
};

/**
 * @brief Send a command. Core0.
 *
 * One command is held at a time, and a new one replaces a command core1 has
 * not taken yet, except that a Stop not yet taken stays and the later
 * command is dropped. Write goes through escwireJobWrite(), which holds the
 * draft with it; Write sent here writes the draft held last.
 * @param op What to do.
 * @return The seq the held command carries; compare with
 *         @ref EwJobStatus::ackSeq.
 */
uint16_t escwireJobCommand(EwJobOp op);

/**
 * @brief Send Write with @p n bytes of @p draft, copied. Core0. The draft
 *        replaces any held earlier, as escwireJobCommand() does.
 * @param draft The page to write, `id.len` bytes.
 * @param n     Its length; more than `EW_PAGE_MAX` is cut to it, and the
 *              session refuses a length other than the page's.
 * @return The seq the held command carries.
 */
uint16_t escwireJobWrite(const uint8_t *draft, uint16_t n);

/** @brief Copy the last status core1 published. Core0. */
void escwireJobStatus(EwJobStatus *out);

/**
 * @brief The bind failure a screen names for an open of the escwire line
 *        (escwirePortOpen()) that was refused. Pure. The S.BUS output's
 *        bootloader probe opens the same line (sbus_out_job.h).
 * @param lineErr The `ew_line_err_t` of the refused open.
 * @return CORE1_JOB_CLOCK, CORE1_JOB_DMA, CORE1_JOB_PIN for a pin the driver
 *         does not take (`EW_LINE_ERR_ARG`), CORE1_JOB_PIO for the rest.
 */
uint8_t escwireJobBindError(uint8_t lineErr);

/** @brief The ESC settings job's entry in the job table of core1_job.cpp. */
extern const Core1JobOps ESCWIRE_JOB_OPS;
