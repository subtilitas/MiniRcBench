/**
 * @file ui_hitec.h
 * @brief HITEC SERVO: Hitec servo programming on the ESC pin.
 *
 * Hitec is a name or trademark of its owner; it is used here only to say
 * which servos this module works with.
 *
 * Entry: MENU → HITEC SERVO. The screen asks for the family, then claims SIG
 * (pin_claim.h); the Hitec job (hitec_job.h) runs rcbench's session on core1,
 * and this file sends it commands and draws its status. The card is reached
 * through hitec_files.h, on core0.
 *
 * | View | What it shows | Leaves it |
 * |---|---|---|
 * | Family | CLASSIC and D SERIES, no pin claimed | a family: Main; BACK: MENU |
 * | Main | the claim, the entry, the read, a stall | the read: List; BACK: Leave in the mode, else Unplug |
 * | List | the parameters, an editor bar, HOLD TO WRITE, REVERT, FILES | HOLD TO WRITE: Write; FILES: Files; BACK: Leave |
 * | Write | the changes, their soft rules, the backup line, HOLD TO WRITE, the result | OK or BACK: List |
 * | Files | the record, the backups, FACTORY VALUES, the files, SAVE AS, LOAD, HOLD TO RESTORE | LOAD: List; BACK: List |
 * | Leave | what LEAVE does, READ AGAIN, LEAVE | LEAVE: Unplug; BACK: List, or Main |
 * | Unplug | the 2 s off wait, UNPLUG THE SERVO, RESTART DSHOT | RESTART DSHOT: MENU; BACK: Main |
 *
 * CLASSIC stays disabled until its line level is measured at bring-up
 * (docs/design/hitec-programming.md, decision 3): `HITEC_CLASSIC_MEASURED`
 * in config.h, 0 by default. It needs SERVO SUPPLY at GP0 as well.
 *
 * @see docs/design/hitec-programming.md, "Screens".
 */

#pragma once

#include <stdint.h>

struct TouchState;

/**
 * @brief Open the screen: the family view, no claim yet.
 */
void uiHitecEnter();

/**
 * @brief Run one frame of the screen.
 * @param t Touch state for this frame.
 * @return false once the screen has left: after RESTART DSHOT, or after
 *         BACK before a claim held the pin.
 */
bool uiHitecTick(const TouchState *t);

/** @brief The Hitec views. @see the table in ui_hitec.h */
enum class HitecView : uint8_t { Family, Main, List, Write, Files, Leave, Unplug };

/** @brief The view that is up. For the host tests. */
HitecView uiHitecView();

/**
 * @brief Whether CLASSIC may be chosen once SERVO SUPPLY is GP0: the
 *        classic line's level measured. `HITEC_CLASSIC_MEASURED` at boot.
 */
bool uiHitecClassicMeasured();

/** @brief Set what uiHitecClassicMeasured() answers. For the host tests and bring-up. */
void uiHitecSetClassicMeasured(bool measured);

/**
 * @brief The Hitec controls outside the lists. The field list's rows and the
 *        file list's rows are `UI_ROW(n, Row)`. @see ui_screen.h
 */
enum class HitecCtl : uint8_t {
	Classic = 1, /**< CLASSIC on the family view. */
	DSeries,     /**< D SERIES on the family view. */
	Retry,       /**< RETRY: after a bind failure, a failed entry or a stall. */
	Stop,        /**< STOP during the entry or a read. */
	ReadAgain,   /**< READ AGAIN after a failed read, or in place of HOLD TO WRITE. */
	Write,       /**< The list's HOLD TO WRITE: opens the write page. */
	Revert,      /**< REVERT: drop the pending changes. */
	Files,       /**< FILES. */
	EditMinus,   /**< The editor bar's -, while an editable row is selected. */
	EditPlus,    /**< The editor bar's +. */
	Repair,      /**< REPAIR a classic checksum, in place of - and +. */
	WriteHold,   /**< The write page's HOLD TO WRITE. */
	WriteOk,     /**< OK under a result. */
	SaveAs,      /**< SAVE AS HTnnn. */
	Load,        /**< LOAD the selected row. */
	Restore,     /**< HOLD TO RESTORE the session's backup. */
	RetryMount,  /**< RETRY MOUNT with no card. */
	LeaveRead,   /**< READ AGAIN on the leave page. */
	Leave,       /**< LEAVE. */
	Restart,     /**< RESTART DSHOT on the unplug page. */
};

struct UiControl;

/**
 * @brief The Hitec view that is up: its controls on the current frame.
 * @param out At most @p max controls.
 * @param max Room in @p out.
 * @return The number written.
 */
uint8_t uiHitecControls(UiControl *out, uint8_t max);

struct UiScreen;
/** @brief HITEC SERVO. */
extern const UiScreen UI_HITEC;
