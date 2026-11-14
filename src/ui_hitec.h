/**
 * @file ui_hitec.h
 * @brief HITEC SERVO: Hitec servo programming on the ESC pin.
 *
 * Hitec is a name or trademark of its owner; it is used here only to say
 * which servos this module works with.
 *
 * Entry: MENU → HITEC SERVO. The screen claims SIG (pin_claim.h) as it
 * opens: DShot drains and stops, and the pad is left an input with the
 * pull-up on, high and quiet. The family view then asks for the family, and
 * its tap starts the Hitec job (hitec_job.h), which runs rcbench's session
 * on core1 for that family; this file sends it commands and draws its
 * status. The card is reached through hitec_files.h, on core0.
 *
 * | View | What it shows | Leaves it |
 * |---|---|---|
 * | Family | the claim, CLASSIC and D SERIES, once SIG is the servo's | a family: Main; BACK: Unplug once the pin was taken, else MENU |
 * | Main | the claim, the entry, the read, a stall | the read: List; BACK: Leave in the mode, else Unplug |
 * | List | the parameters, an editor bar, HOLD TO WRITE, REVERT, FILES | HOLD TO WRITE: Write; FILES: Files; BACK: Leave |
 * | Write | the changes, their soft rules, the backup line, HOLD TO WRITE, the result | OK or BACK: List |
 * | Files | the record, the backups, FACTORY VALUES, the files, SAVE AS, LOAD, HOLD TO RESTORE | LOAD: List; BACK: List |
 * | Leave | what LEAVE does, READ AGAIN, LEAVE | LEAVE: Unplug; BACK: List, or Main |
 * | Unplug | the 2 s off wait, UNPLUG THE SERVO, RESTART DSHOT | RESTART DSHOT: MENU; BACK: Main, or Family before a family was tapped |
 *
 * CLASSIC needs SERVO SUPPLY at GP28 or GP29: its entry switches the
 * servo's supply. The classic servo drives SIG to its supply, 5 V nominal;
 * SIG is a fault tolerant pin (docs/design/hitec-programming.md,
 * decision 13).
 *
 * @see docs/design/hitec-programming.md, "Screens".
 */

#pragma once

#include <stdint.h>

struct TouchState;

/**
 * @brief Open the screen: the family view. The first frame claims SIG.
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
 * @defgroup ui_hitec_family The family view's notes
 * @brief Scale 1 from x 8, at most 37 characters each.
 * @{
 */
/** @brief The strip's first line once the claim holds SIG: green. */
#define HITEC_UI_HELD_FMT "GP%u IS THE SERVO'S: DSHOT IS OFF."
/** @brief The strip's second line under it. */
#define HITEC_UI_CHOOSE "CONNECT THE SERVO, CHOOSE ITS FAMILY"
/** @brief The strip's first line while an ESC answers on SIG: amber. */
#define HITEC_UI_ESC_FMT "AN ESC ANSWERS ON SIG GP%u: UNPLUG IT."
/** @brief The strip's second line under it. */
#define HITEC_UI_ESC_2 "DSHOT STAYS ON THE PIN UNTIL THEN."
/** @brief Why CLASSIC is drawn disabled, first line: the hardware it needs. */
#define HITEC_UI_CLASSIC_WHY_1 "NEEDS THE CONVERTER OF WIRING PART C"
/** @brief ...and second line: the setting that names the converter's pin. */
#define HITEC_UI_CLASSIC_WHY_2 "AND SETTINGS > SERVO SUPPLY GP28/29."
/**
 * @brief D SERIES' note with SERVO SUPPLY at NONE: the entry reads at once,
 *        so the servo runs before the tap. SIG is the servo's from the
 *        moment the family view shows it.
 */
#define HITEC_UI_D_POWER "POWER THE SERVO UP, THEN TAP."
/**
 * @brief The editor bar's note while the D series ID draft is not 0: three
 *        lines of 22 characters. Whether a servo with another ID answers
 *        frames sent to ID 0 is not measured.
 */
#define HITEC_UI_ID_NOTE_BAR "BENCH ADDRESSES ID 0. A WRITE MAY LOCK OUT READ AND RESTORE."
/** @brief The write page's note for the same change: two lines of 36. */
#define HITEC_UI_ID_NOTE_PAGE \
	"THE BENCH ADDRESSES ID 0 AND MAY NOT READ OR RESTORE THIS SERVO AGAIN."
/**
 * @brief The write page's last line when soft rules are left out, which
 *        each row's editor bar names.
 */
#define HITEC_UI_MORE_RULES_FMT "AND %d MORE RULE%s: SEE EACH ROW."
/** @brief The field list's chip, right of its strip: `PROG MODE`, 65 px. */
#define HITEC_UI_CHIP UI_CHIP_PROG_MODE

/** @} */

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
