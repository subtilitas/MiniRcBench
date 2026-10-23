/**
 * @file ui_kst.h
 * @brief KST SERVO: KST and Chaservo servo programming on the ESC pin.
 *
 * Entry: MENU → KST SERVO. The screen claims SIG (pin_claim.h); the KST job
 * (kst_job.h) runs rcbench's session on core1, and this file sends it
 * commands and draws its status. The card is reached through kst_files.h, on
 * core0.
 *
 * | View | What it shows | Leaves it |
 * |---|---|---|
 * | Main | the claim, the search, the read | the read: List; BACK: Leave after a servo was found, else Unplug |
 * | List | the 27 fields, an editor bar, HOLD TO WRITE, REVERT, FILES | HOLD TO WRITE: Write; FILES: Files; BACK: Leave |
 * | Write | the changes, their soft rules, the backup line, HOLD TO WRITE, the result | OK or BACK: List |
 * | Files | the card's files, SAVE AS, LOAD, HOLD TO RESTORE | LOAD: List; BACK: List |
 * | Leave | POWER THE SERVO OFF, LEAVE | LEAVE: Unplug; BACK: List, or Main after a loss |
 * | Unplug | UNPLUG THE SERVO, RESTART DSHOT | RESTART DSHOT: MENU; BACK: Main |
 *
 * A loss of the servo, a read that answers nothing in the mode, returns
 * every view but Unplug and Leave to Main, where the search runs again. A
 * loss shows only when an operation runs: a servo powered off under an idle
 * field list leaves the list up until the next read or write. BACK on Main
 * during that search stops it and opens Leave.
 *
 * Without a switched supply the servo stays in programming mode until its
 * supply is off, so LEAVE asks the user to power it off. Step 5 of the
 * design adds the switched supply.
 *
 * @see docs/design/kst-programming.md, "Screens".
 */

#pragma once

#include <stdint.h>

struct TouchState;

/** @brief Whether the screen is built. @return true: MENU has a KST SERVO row. */
bool uiKstAvailable();

/**
 * @brief Open the screen: a new session, and a claim of SIG, asked again
 *        every frame until it is accepted.
 */
void uiKstEnter();

/**
 * @brief Run one frame of the screen.
 * @param t Touch state for this frame.
 * @return false once the screen has left: after RESTART DSHOT, or after
 *         BACK before a claim held the pin.
 */
bool uiKstTick(const TouchState *t);

/** @brief The KST views. @see the table in ui_kst.h */
enum class KstView : uint8_t { Main, List, Write, Files, Leave, Unplug };

/** @brief The view that is up. For the host tests. */
KstView uiKstView();

/**
 * @brief The KST controls outside the lists. The field list's rows and the
 *        file list's rows are `UI_ROW(n, Row)`. @see ui_screen.h
 */
enum class KstCtl : uint8_t {
	Retry = 1,   /**< RETRY: after a bind failure, or a search that ended without a servo. */
	StopSearch,  /**< STOP SEARCH. */
	ReadAgain,   /**< READ AGAIN after a failed read. */
	Write,       /**< The list's HOLD TO WRITE: opens the write page. */
	Revert,      /**< REVERT: drop the pending changes. */
	Files,       /**< FILES. */
	EditMinus,   /**< The editor bar's -, while an editable row is selected. */
	EditPlus,    /**< The editor bar's +. */
	WriteHold,   /**< The write page's HOLD TO WRITE. */
	WriteOk,     /**< OK under a result. */
	SaveAs,      /**< SAVE AS KSTnnn. */
	Load,        /**< LOAD the selected file. */
	Restore,     /**< HOLD TO RESTORE the session's backup. */
	RetryMount,  /**< RETRY MOUNT with no card. */
	Leave,       /**< LEAVE. */
	Restart,     /**< RESTART DSHOT on the unplug page. */
};

struct UiControl;

/**
 * @brief The KST view that is up: its controls on the current frame.
 * @param out At most @p max controls.
 * @param max Room in @p out.
 * @return The number written.
 */
uint8_t uiKstControls(UiControl *out, uint8_t max);

struct UiScreen;
/** @brief KST SERVO. */
extern const UiScreen UI_KST;
