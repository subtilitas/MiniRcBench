/**
 * @file sd_log.h
 * @brief Blackbox logging to the microSD slot.
 *
 * Ties together @ref BlackboxEncoder (which turns telemetry into bytes),
 * @ref LogRing (which absorbs card stalls) and FatFs (which does the writing).
 *
 * @section sdlog_cores Which core
 *
 * Core0, always. Core1's only job is DShot frame timing, and an SD card that
 * pauses 30 ms for an internal erase would destroy it. Core0 already tolerates
 * multi-millisecond display DMA bursts and the UI is not real-time critical.
 *
 * The encoder therefore runs on core0 too, reading telemetry through
 * escSnapshot() like the UI does.
 *
 * @section sdlog_loss What happens when the card cannot keep up
 *
 * Bytes are dropped and counted; nothing ever blocks waiting for the card. A
 * gap in the log is recoverable, a stalled UI with a live motor is not. Drops
 * are surfaced rather than swallowed — a silently lossy log is worse than an
 * obviously lossy one, because it looks like data.
 *
 * @see docs/design/blackbox-logging.md
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "config.h"

/** @brief What the logger is currently doing. */
enum class SdLogState : uint8_t {
	NoCard = 0,  /**< No card, or mount failed. Not an error; just unavailable. */
	Idle,        /**< Card present and ready. */
	Logging,     /**< A file is open and being written. */
	Error,       /**< A write or open failed. Card needs re-mounting. */
};

/**
 * @brief Why the last log ended, when it was not STOP on SD LOG.
 *
 * Kept until the next start. A mount (sdLogBegin(), which RETRY MOUNT, the
 * end of USB DRIVE and a tool's remount run) forgets a card's fault, as the
 * card in the slot may be another one by then, and keeps a tool's stop
 * (sdLogMountForget()). SD LOG prints it on its note lines.
 */
enum class SdLogEnd : uint8_t {
	None = 0,     /**< Nothing to report: no log yet, a STOP, or a start or mount since. */
	CardFull,     /**< A write found no free cluster: FatFs wrote fewer bytes than asked. */
	WriteFailed,  /**< A write, sync or close failed: the card removed, or a card fault. */
	OpenFailed,   /**< START found no file to open: the directory, its scan or the open failed. */
	ByKst,        /**< KST SERVO took the card for its backup and stopped the log. */
	ByHitec,      /**< HITEC SERVO did. */
	ByAm32,       /**< AM32 CONFIG did. */
	NoFreeNumber, /**< START found `LOG99999.BFL` in `LOGS/`: no number follows it. */
};

/** @brief The highest N in `LOGS/LOGnnnnn.BFL`: five digits. */
#define SD_LOG_NUMBER_MAX 99999u

/**
 * @brief The number in a log's file name. Pure; host-testable.
 *
 * A log's name is `LOG`, five decimal digits and `.BFL`, nothing before or
 * after: `LOG00000.BFL` to `LOG99999.BFL`. Letter case is ignored, because
 * FAT compares names without it: `log00007.bfl` is the file `LOG00007.BFL`
 * opens, so it counts. Four or six digits, another extension, a sign, a
 * space or any longer name is not a log's name.
 *
 * @param name A directory entry's name, NUL-terminated; nullptr reads as none.
 * @param[out] n The number, set only on a match.
 * @return True if @p name is a log's name.
 */
static inline bool sdLogNameNumber(const char *name, uint32_t *n) {
	if (!name) return false;
	static const char HEAD[] = "LOG", TAIL[] = ".BFL";
	for (int i = 0; i < 3; i++) {
		char c = name[i];
		if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
		if (c != HEAD[i]) return false;
	}
	uint32_t v = 0;
	for (int i = 3; i < 8; i++) {
		char c = name[i];
		if (c < '0' || c > '9') return false;
		v = v * 10u + (uint32_t)(c - '0');
	}
	for (int i = 0; i < 4; i++) {
		char c = name[8 + i];
		if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
		if (c != TAIL[i]) return false;
	}
	if (name[12] != '\0') return false;
	*n = v;
	return true;
}

/**
 * @brief Fold one directory entry's name into the highest log number seen.
 *        Pure; host-testable.
 * @param[in,out] highest The highest number so far, 0 before any; a name
 *        that is not a log's leaves it.
 * @param name The entry's name.
 */
static inline void sdLogNoteName(uint32_t *highest, const char *name) {
	uint32_t n;
	if (sdLogNameNumber(name, &n) && n > *highest) *highest = n;
}

/**
 * @brief The number a new log takes: the highest existing one plus 1.
 *        Pure; host-testable.
 *
 * An empty or new `LOGS/`, or one holding only `LOG00000.BFL`, gives 1.
 * After `LOG99999.BFL` there is none: the start is refused
 * (@ref SdLogEnd::NoFreeNumber) rather than wrapped onto a number a file
 * may still hold.
 *
 * @param highest The highest number in `LOGS/`, 0 for none.
 * @return 1 to @ref SD_LOG_NUMBER_MAX, or 0 for no free number.
 */
static inline uint32_t sdLogNextNumber(uint32_t highest) {
	return highest >= SD_LOG_NUMBER_MAX ? 0u : highest + 1u;
}

/** @brief True for an end a tool's claim caused. Pure. */
static inline bool sdLogEndByTool(SdLogEnd e) {
	return e == SdLogEnd::ByKst || e == SdLogEnd::ByHitec || e == SdLogEnd::ByAm32;
}

/**
 * @brief What a mount does to the end: a card's fault is forgotten, the card
 *        in the slot may be another one; a tool's stop is kept until the next
 *        start, as it stays true of the last log. Pure; host-testable.
 * @param[in,out] e The end.
 */
static inline void sdLogMountForget(SdLogEnd *e) {
	if (!sdLogEndByTool(*e)) *e = SdLogEnd::None;
}

/**
 * @brief What a failed f_write() says about the card. Pure; host-testable.
 *
 * FatFs ends a write that finds no free cluster with FR_OK and fewer bytes
 * written than asked (ff.c R0.15, f_write(): "disk full"). A card that is
 * gone or fails answers with an error: FR_DISK_ERR from the block write, or
 * FR_INVALID_OBJECT when the drive reports itself uninitialised (validate()).
 * A card fault and a removed card are not told apart.
 *
 * @param fr    The FRESULT, 0 for FR_OK.
 * @param wrote Bytes FatFs reports written.
 * @param asked Bytes handed to it.
 * @return CardFull or WriteFailed.
 */
static inline SdLogEnd sdLogWriteEnd(int fr, uint32_t wrote, uint32_t asked) {
	return (fr == 0 && wrote < asked) ? SdLogEnd::CardFull : SdLogEnd::WriteFailed;
}

/** @brief Counters for the UI. Copied out by sdLogStatus(). */
struct SdLogStatus {
	SdLogState state;
	/**
	 * @brief Last mount result, as a FatFs `FRESULT`. Zero is FR_OK.
	 *
	 * Surfaced because "not detected" covers several very different faults and
	 * collapsing them all to "no card" makes the problem unsolvable from the
	 * bench: 3 (FR_NOT_READY) means the card did not initialise, either
	 * because nothing answered or because a card answered and failed
	 * (@ref cardInit tells which), 13 (FR_NO_FILESYSTEM) means the card is
	 * talking fine but has no partition FatFs recognises, and those want
	 * different fixes.
	 */
	uint8_t    mountResult;
	uint8_t    cardType;      /**< card_type_t: 0 none, 1 v1, 2 v2, 3 v2 HC/XC. */
	uint32_t   cardSizeMB;    /**< Capacity, 0 if the card never initialised. */
	/**
	 * @brief What the last card initialisation found, an SD_SDIO_INIT_ value
	 *        of `sdio/sd_card_cache.h`: 0 initialised, 1 nothing answered
	 *        (no card), 2 a card answered and failed, 3 the bus not claimed.
	 */
	uint8_t    cardInit;
	uint32_t   bytesWritten;  /**< Bytes handed to the card this session. */
	uint32_t   framesLogged;  /**< Frames encoded this session. */
	uint32_t   bytesDropped;  /**< Bytes the ring refused. */
	uint32_t   dropEvents;    /**< Frames lost to a full ring. */
	uint32_t   peakBuffer;    /**< Ring high-water mark, bytes. */
	uint32_t   worstFlushMs;  /**< Longest single card write. Sizes the buffer. */
	/**
	 * @brief N in LOGS/LOGnnnnn.BFL: the open log's while logging, else the
	 *        last log's, kept until the next start or mount. 0 before the
	 *        first, and after a mount; the counters reset with it.
	 */
	uint32_t   fileNumber;
	SdLogEnd   end;           /**< Why the last log ended, @ref SdLogEnd. */
};

/**
 * @brief Bring up the card. Safe to call when no card is fitted.
 *
 * @return True if a card mounted. False leaves the state at
 *         @ref SdLogState::NoCard and every other call a no-op.
 */
bool sdLogBegin();

/**
 * @brief Open `LOGS/LOGnnnnn.BFL`, N the highest number in `LOGS/` plus 1
 *        (sdLogNextNumber()), and start encoding.
 *
 * The first start after a mount reads `LOGS/` once, every directory entry
 * in turn (f_readdir()); later starts take the number after the last log's
 * from memory. A mount, which RETRY MOUNT, the end of USB DRIVE and a tool's
 * remount run, forgets it: a computer may have added or removed files.
 * The file is created new, never over an existing one.
 *
 * @return True if the file opened. False with @ref SdLogEnd::OpenFailed for
 *         a directory that cannot be made or read or a file that does not
 *         open, @ref SdLogEnd::NoFreeNumber after `LOG99999.BFL`.
 */
bool sdLogStart();

/** @brief Finish the current log: end event, flush, close. No-op when idle. */
void sdLogStop();

/**
 * @brief Stop a running log for a tool that takes the card, and record which
 *        one: SD LOG then reads `RECORDING STOPPED BY` and the tool's name
 *        until the next start; a mount keeps it (sdLogMountForget()).
 *        No-op when no log runs.
 * @param tool @ref SdLogEnd::ByKst, @ref SdLogEnd::ByHitec or
 *        @ref SdLogEnd::ByAm32. A stop that fails keeps the cause it
 *        recorded: CardFull or WriteFailed from the last flush, WriteFailed
 *        from the sync or close.
 */
void sdLogStopFor(SdLogEnd tool);

/**
 * @brief Try to mount the card again.
 *
 * sdLogBegin() runs once at boot, so a card inserted afterwards was previously
 * invisible until a power cycle — which is its own way of looking like a card
 * that "is not detected".
 *
 * @return True if a card mounted.
 */
bool sdLogRemount();

/**
 * @brief Give the card up so something else can own it exclusively.
 *
 * Flushes, closes and unmounts. Every other entry point here is inert
 * afterwards, and only sdLogReacquire() undoes it — including sdLogRemount(),
 * whose whole job is the helpful remount that would corrupt a card a USB host
 * is reading.
 *
 * The unmount is the part that matters. FatFs caches, so without it the host
 * reads a filesystem missing whatever is still in this side's buffers.
 *
 * @return False if a log is open. Stopping a recording is the user's decision,
 *         not this function's; @see mscRefusal().
 */
bool sdLogRelease();

/**
 * @brief Take the card back after sdLogRelease().
 *
 * A full re-mount rather than a re-attach: the card may have been pulled out
 * while the host had it, and re-reading its type and size is how the screen
 * tells "it is back" apart from "it is gone".
 *
 * @return True if a card mounted. Safe to call when not released.
 */
bool sdLogReacquire();

/** @brief True while a file is open. @return Logging state. */
bool sdLogActive();

/**
 * @brief Encode one frame if the log rate says it is due.
 *
 * Call from the core0 loop as often as convenient; it rate-limits internally to
 * @ref SD_LOG_RATE_HZ. Cheap and non-blocking — it only fills the ring.
 *
 * @param nowUs    Current micros().
 * @param throttle Commanded throttle, 0..2000. Passed in rather than read back
 *                 from the ESC task: the UI is what commands it, and the value
 *                 core1 last transmitted is not necessarily the one the
 *                 operator asked for (it zeroes on disarm and on heartbeat
 *                 loss). The log should record the command.
 */
void sdLogTick(uint32_t nowUs, uint16_t throttle);

/**
 * @brief Push buffered bytes at the card.
 *
 * Separate from sdLogTick() because this is the call that can block for
 * milliseconds. Keeping them apart means the caller decides when to take that
 * cost, rather than discovering it inside a function that looks cheap.
 *
 * Writes at most one @ref SD_LOG_CHUNK_BYTES chunk per call.
 */
void sdLogFlush();

/**
 * @brief Read the counters.
 * @param[out] out Filled in.
 */
void sdLogStatus(SdLogStatus *out);

/**
 * @brief Where the mounted volume keeps its serial number: the boot sector's
 *        LBA (logical block address) and the offset in it, 67 on FAT32, 100
 *        on exFAT, 39 on FAT12 and FAT16. The write cache compares those 4
 *        bytes to tell a swapped card (sd_card_cache.h).
 * @return False without a mounted card.
 */
bool sdLogVolumeId(uint32_t *lba, uint32_t *offset);

/** @brief What a change in arm state should do to the log. */
enum class SdLogArmAction : uint8_t {
	None,   /**< Leave the log alone. */
	Start,  /**< Begin an auto log. */
	Stop,   /**< End the auto log this arming began. */
};

/**
 * @brief Decide what an arm-state change should do. Pure; host-testable.
 *
 * Split out from sdLogSetArmed() so the host tests exercise the shipped rule
 * rather than a copy of it — sd_log.cpp pulls in FatFs and cannot be linked
 * into the test binary, and a fake that reimplements the policy proves only
 * that the fake agrees with itself.
 *
 * The rule that matters: auto-stop undoes only what auto-start did. Entering
 * MENU force-disarms, and the logging screen is reached through it, so without that distinction walking over to check on a hand-started log
 * is what ends it.
 *
 * @param nowArmed    The new arm state.
 * @param wasArmed    The previous arm state.
 * @param logging     Whether a log is currently open.
 * @param autoStarted Whether the open log was begun by arming.
 * @return            What to do.
 */
static inline SdLogArmAction sdLogArmAction(bool nowArmed, bool wasArmed,
                                            bool logging, bool autoStarted) {
	if (nowArmed == wasArmed) return SdLogArmAction::None;
#if SD_LOG_AUTO_ON_ARM
	if (nowArmed) return logging ? SdLogArmAction::None : SdLogArmAction::Start;
	return autoStarted ? SdLogArmAction::Stop : SdLogArmAction::None;
#else
	(void)logging; (void)autoStarted;
	return SdLogArmAction::None;
#endif
}

/**
 * @brief Tell the logger the tester armed or disarmed.
 *
 * Starts and stops a log when @ref SD_LOG_AUTO_ON_ARM is set; records an
 * explicit start otherwise. Idempotent.
 *
 * @param armed New arm state.
 */
void sdLogSetArmed(bool armed);
