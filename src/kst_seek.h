/**
 * @file kst_seek.h
 * @brief The KST search: rcbench's entry sequence repeated until a servo
 *        answers, for servos that enter programming mode only right after
 *        power-on.
 *
 * One attempt is one kst_session_enter() stepped to its end, 116.6 ms to
 * 118.8 ms. @c KST_SES_ERR_NO_SERVO starts the next attempt at once; the
 * entry's own 100 ms low is the spacing, 8.4 to 8.6 attempts a second. The
 * session is rcbench's, unchanged: these functions start, step and abort it
 * and call kst_session_power_cycled(), nothing else.
 *
 * | End | When | KstSeekState::result |
 * |---|---|---|
 * | Found | an attempt ends with @c KST_SES_OK | @c KST_SES_OK |
 * | Stopped | the attempt running at kstSeekStop() has ended aborted | @c KST_SES_ERR_ABORTED |
 * | TimedOut | an attempt ends @ref KST_SEEK_TIMEOUT_MS or later after kstSeekBegin() | @c KST_SES_ERR_NO_SERVO |
 * | NoUi | an attempt ends after a heartbeat lapse | @c KST_SES_ERR_NO_SERVO |
 * | Failed | an attempt ends with any other result, a stopped one too (@c KST_SES_ERR_DRIVER for a frame that never finished), or does not start | that result |
 *
 * An attempt always runs to its end: a stop or a heartbeat lapse lets the
 * frame on the wire finish (kst_session_abort()), and a lapse lets the
 * attempt's remaining frames go out too. The session takes an abort before
 * it reads the window, so a servo that acknowledges the sync on the wire at
 * the stop is in programming mode while the search ends Stopped; the first
 * read of the next search finds it. Between attempts and after the end
 * the line driver holds the line low; nothing here releases it.
 *
 * A reply the session's decoder refuses is not a servo found. The search
 * keeps the last such refusal, a @c kst_rx_t other than @c KST_RX_OK and
 * @c KST_RX_NO_REPLY, so a servo that answers wrongly is told apart from
 * silence. It reads @c last_reply after a step in which the session's count
 * of windows without a valid reply (@c stats.faults) rose, so a code left by
 * an operation before the search is not taken. A step is called at least once
 * per transaction: a transaction takes 2.87 ms or more.
 *
 * **Switched supply.** A search begun with @p switched true (SETTINGS > SERVO
 * SUPPLY at GP0) powers the servo up itself. One attempt:
 *
 * 1. Supply off, for @ref KST_SUPPLY_OFF_MS from the switch-off command.
 * 2. Supply on, only while the heartbeat is trusted; the attempt counts here.
 * 3. kst_session_enter() @ref KST_SUPPLY_ENTRY_DELAY_MS after the switch-on.
 *    A negative delay starts the entry first and switches on that many
 *    milliseconds into its 100 ms low.
 * 4. @c KST_SES_OK is Found, the supply on; otherwise the next attempt.
 *
 * The timeout, a stop and a lapse end an off phase or a positive delay's wait
 * at once, the supply off and never switched on. An entry runs to its end as
 * without a switch, then the supply goes off. The search only says which
 * level it wants (@c KstSeekState::supply) and when it last commanded off
 * (@c KstSeekState::offMs); the caller drives the pin.
 *
 * Every function is pure apart from the session: the clock is a parameter,
 * and the host suite runs them against rcbench's simulated servo.
 *
 * @see docs/design/kst-programming.md, "Entering programming mode".
 */

#pragma once

#include <stdint.h>

#include "kst_session.h"

/** @brief How long a search tries, ms: then TimedOut, which offers RETRY. */
#define KST_SEEK_TIMEOUT_MS 30000u

/** @brief Supply off before each switched attempt, for REBOOT and before a close, ms. */
#define KST_SUPPLY_OFF_MS 2000u

/** @brief REBOOT: supply on before the entry, ms (rcbench's KST_PORT_RAIL_WAIT_MS). */
#define KST_SUPPLY_WAIT_MS 1000u

/**
 * @brief Switched search: the entry's start against the switch-on, ms.
 *        0 enters at the switch-on; negative switches on inside the entry's
 *        100 ms low; positive waits after the switch-on. Bring-up step 7
 *        measures the servo's window and sets it.
 */
#ifndef KST_SUPPLY_ENTRY_DELAY_MS
#define KST_SUPPLY_ENTRY_DELAY_MS 0
#endif
// The switch-on stays inside the entry's 100 ms low, before its first frame.
static_assert(KST_SUPPLY_ENTRY_DELAY_MS > -(int)(KST_ENTRY_LOW_US / 1000u),
              "KST_SUPPLY_ENTRY_DELAY_MS must switch on before the entry's first frame");

/** @brief Where a switched attempt is. */
enum class KstSupplyPhase : uint8_t {
	None,   /**< No switch: the attempts follow each other. */
	Off,    /**< Supply off, waiting @ref KST_SUPPLY_OFF_MS. */
	Delay,  /**< Supply on, waiting a positive entry delay. */
	Entry,  /**< kst_session_enter() runs. */
};

/** @brief Where a search is. */
enum class KstSeek : uint8_t {
	Idle,       /**< Not begun. */
	Searching,  /**< An attempt runs. */
	Found,      /**< A servo answered: the session is in programming mode. */
	Stopped,    /**< kstSeekStop(): STOP SEARCH, or the job's leave. */
	TimedOut,   /**< @ref KST_SEEK_TIMEOUT_MS without a servo. */
	NoUi,       /**< A heartbeat lapse: no attempt starts after it. */
	Failed,     /**< The session ended an attempt with another result. */
};

/** @brief A search. Plain data; kstSeekBegin() sets every field. */
struct KstSeekState {
	KstSeek  state;     /**< Where it is. */
	uint32_t startMs;   /**< Clock at kstSeekBegin(). */
	uint32_t endMs;     /**< Clock when it ended; valid once it has. */
	uint32_t attempts;  /**< Attempts started. */
	uint8_t  result;    /**< @c kst_ses_t that ended it; @c KST_SES_BUSY while it runs. */
	uint8_t  rx;        /**< @c kst_rx_t of the last refused reply; @c KST_RX_NO_REPLY for none. */
	uint32_t faults;    /**< The session's @c stats.faults when @c rx was last looked at. */
	bool     stopping;  /**< kstSeekStop() came; the running attempt finishes its frame. */
	bool     noUi;      /**< The heartbeat lapsed; the running attempt ends, no other starts. */
	bool     switched;  /**< The bench switches the servo's supply. */
	KstSupplyPhase phase; /**< Where a switched attempt is. */
	uint32_t phaseMs;   /**< Clock when the phase began. */
	int32_t  delayMs;   /**< The entry delay, @ref KST_SUPPLY_ENTRY_DELAY_MS unless a test sets it. */
	bool     supply;    /**< The level the search wants: on. */
	uint32_t offMs;     /**< Clock of the last switch-off command. */
};

/**
 * @brief Begin a search, and its first attempt.
 *
 * A session that refuses kst_session_enter(), because another operation
 * runs, ends the search at once as Failed. A switched search begins with
 * the supply off and enters nothing for @ref KST_SUPPLY_OFF_MS.
 *
 * @param[out] s    The search.
 * @param ses       The session; no operation should run.
 * @param nowMs     The clock, ms. It may wrap.
 * @param switched  The bench switches the servo's supply.
 */
void kstSeekBegin(KstSeekState *s, kst_session_t *ses, uint32_t nowMs, bool switched = false);

/**
 * @brief Step the running attempt, and start the next when it ends without
 *        a servo.
 * @param[in,out] s The search.
 * @param ses       The session kstSeekBegin() was given.
 * @param uiAlive   core1UiAlive(): false lets the attempt end and starts none.
 * @param nowMs     The clock, ms.
 * @return Searching while it runs, then how it ended, again on every call.
 */
KstSeek kstSeekStep(KstSeekState *s, kst_session_t *ses, bool uiAlive, uint32_t nowMs);

/**
 * @brief STOP SEARCH or BACK: abort the running attempt. A frame on the wire
 *        finishes; kstSeekStep() returns Stopped once the attempt has ended
 *        aborted, or Failed with the attempt's result when its frame faulted.
 *        In a switched search's off phase or delay it ends at the next step,
 *        the supply off. A no-op unless Searching.
 */
void kstSeekStop(KstSeekState *s, kst_session_t *ses);

/**
 * @brief The loss rule, after an operation of a session in the mode ended.
 *
 * A result of @c KST_SES_ERR_NO_SERVO in a session that was in programming
 * mode means the servo is gone: kst_session_power_cycled(), rcbench's
 * POWER_CYCLED, said by the loss itself. The backup and the lock drop and the
 * next read becomes the new backup. With the heartbeat trusted a new search
 * begins, switched or not, which a switched search starts with the supply
 * off; without, the search is set to NoUi and none begins.
 *
 * @param[out] s    The search, set when the servo is lost.
 * @param ses       The session.
 * @param r         The operation's result.
 * @param uiAlive   core1UiAlive().
 * @param nowMs     The clock, ms.
 * @param switched  The bench switches the servo's supply.
 * @return True when the servo was lost.
 */
bool kstSeekLost(KstSeekState *s, kst_session_t *ses, kst_ses_t r, bool uiAlive,
                 uint32_t nowMs, bool switched = false);

/**
 * @brief The heartbeat lapsed with the supply on and no operation running:
 *        kst_session_power_cycled(), and the search set to NoUi with the
 *        supply wanted off. The caller switches it off.
 */
void kstSeekStalled(KstSeekState *s, kst_session_t *ses, uint32_t nowMs);

/**
 * @brief A switched search's time left in its off phase, ms; 0 outside one.
 */
uint32_t kstSeekOffLeftMs(const KstSeekState *s, uint32_t nowMs);

/**
 * @brief How long the search has run, ms: to now while Searching, to its end
 *        after. 0 before kstSeekBegin().
 */
uint32_t kstSeekMs(const KstSeekState *s, uint32_t nowMs);
