/**
 * @file ui_hitec.h
 * @brief HITEC SERVO: Hitec servo programming on the ESC pin.
 *
 * Hitec is a name or trademark of its owner; it is used here only to say
 * which servos this module works with.
 *
 * Entry: MENU → SERVO → HITEC D SERIES or HITEC CLASSIC, entries of the
 * hub SERVO (ui_servo_hub.h). The entry point sets the family and claims SIG
 * (pin_claim.h) on the tap's frame: DShot drains and stops, and the pad is
 * left an input with the pull-up on, high and quiet. The Hitec job
 * (hitec_job.h) then runs rcbench's session on core1 for that family; this
 * file sends it commands and draws its status. The card is reached through
 * hitec_files.h, on core0.
 *
 * The job starts with the claim for CLASSIC, and for D SERIES with the
 * switched supply, whose servo stays unpowered until the entry's POWER
 * step. D SERIES with SERVO SUPPLY at OWN stops in Held: the entry reads
 * the model at once, so the main view asks for the servo to be powered and
 * READ starts the job (docs/design/menu-b.md, decision 34). The view
 * asks only once DShot's drain has ended, and from then on no frame goes to
 * SIG until READ.
 *
 * | View | What it shows | Leaves it |
 * |---|---|---|
 * | Main | the claim, READ for D SERIES on OWN, the entry, the read, a stall | the read: List; BACK: Leave in the mode; Leave releasing once SIG was taken; SERVO, the hub, while the claim is refused or Stopping |
 * | List | the parameters, an editor bar, WRITE..., REVERT, FILES | WRITE...: Write; FILES: Files; BACK: Leave |
 * | Write | the changes, their soft rules, the backup line, HOLD TO WRITE or HOLD TO RESTORE, the result | OK or BACK: List; BACK is disabled while the write runs or waits for the card |
 * | Files | the record, the backups, FACTORY VALUES, the files, SAVE AS, LOAD, RESTORE... | RESTORE...: Write, a restore; LOAD: List; BACK: List |
 * | Leave | the leave page (ui_leave.h): the steps, STAY, READ AGAIN, LEAVE; then the 2 s off wait, NEXT SERVO and RESTART DSHOT | STAY: List, or Main; READ AGAIN: the read; LEAVE: the release; NEXT SERVO: Main, a new session in the same family; RESTART DSHOT: PROGRAMMER; BACK is disabled |
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

#include "ui_screen.h"

struct TouchState;

/**
 * @brief Open the screen for a D series servo: SERVO's HITEC D SERIES.
 *        Claims SIG on this frame, asked again every frame while refused.
 *        With SERVO SUPPLY at OWN the claim waits in Held for READ; with the
 *        switched supply the job starts once the claim is Held.
 */
void uiHitecEnterD();

/**
 * @brief Open the screen for a classic servo: SERVO's HITEC CLASSIC,
 *        enabled only with the switched supply. Claims SIG on this frame;
 *        the job starts once the claim is Held, and refuses the entry
 *        without the supply (hitec_job.h).
 */
void uiHitecEnterClassic();

/**
 * @brief Run one frame of the screen.
 * @param t Touch state for this frame.
 * @return false once the screen has left: after RESTART DSHOT, or after
 *         BACK before a claim held the pin.
 */
bool uiHitecTick(const TouchState *t);

/** @brief The Hitec views. @see the table in ui_hitec.h */
enum class HitecView : uint8_t { Main, List, Write, Files, Leave };

/** @brief The view that is up. For the host tests. */
HitecView uiHitecView();

/**
 * @defgroup ui_hitec_texts The screen's texts
 * @brief Scale 1 from x 8, at most 37 characters each.
 * @{
 */
/**
 * @brief The main view's strip, first line, while D SERIES on OWN waits in
 *        Held for READ: green, SIG by role and number.
 */
#define HITEC_UI_HELD_FMT "SIG GP%u IS THE SERVO'S: DSHOT IS OFF."
/** @brief The strip's second line under it: amber, the step before READ. */
#define HITEC_UI_POWER_READ "POWER THE SERVO UP, THEN TAP READ."
/** @brief The main view's first line while an ESC answers on SIG: amber. */
#define HITEC_UI_ESC_FMT "AN ESC ANSWERS ON SIG GP%u: UNPLUG IT."
/** @brief The line under it. */
#define HITEC_UI_ESC_2 "DSHOT STAYS ON THE PIN UNTIL THEN."
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
/** @brief The leave page's first step without the switch: 25 characters from x 20. */
#define HITEC_UI_LEAVE_RESTARTS "LEAVE RESTARTS THE SERVO."
/**
 * @brief The leave page's note once the pin is let go, without the switch:
 *        35 characters. NEXT SERVO returns to the READ wait, which asks for
 *        the power (docs/design/menu-b.md, decision 34).
 */
#define HITEC_UI_NEXT_POWER "NEXT SERVO: POWER IT AFTER THE TAP."

/** @} */

/**
 * @brief The Hitec controls outside the lists. The field list's rows and the
 *        file list's rows are `UI_ROW(n, Row)`. @see ui_screen.h
 */
enum class HitecCtl : uint8_t {
	Read = 1,    /**< READ on the main view: D SERIES on OWN, the servo powered, starts the job. */
	Retry,       /**< RETRY: after a bind failure, a failed entry or a stall. */
	Stop,        /**< STOP during the entry or a read. */
	ReadAgain,   /**< READ AGAIN after a failed read, or in place of `WRITE...`. */
	Write,       /**< The list's WRITE...: a tap opens the write page. */
	Revert,      /**< REVERT: drop the pending changes. */
	Files,       /**< FILES. */
	EditMinus,   /**< The editor bar's -, while an editable row is selected. */
	EditPlus,    /**< The editor bar's +. */
	Repair,      /**< REPAIR a classic checksum, in place of - and +. */
	WriteHold,   /**< The write page's HOLD TO WRITE, or HOLD TO RESTORE: the write. */
	WriteOk,     /**< OK under a result. */
	SaveAs,      /**< SAVE AS HTnnn. */
	Load,        /**< LOAD the selected row. */
	Restore,     /**< RESTORE...: a tap opens the restore page of the session's backup. */
	RetryMount,  /**< RETRY MOUNT with no card. */
	LeaveRead,   /**< READ AGAIN on the leave page, before LEAVE. */
	Leave,       /**< LEAVE on the leave page, before the release. */
	Restart,     /**< RESTART DSHOT in LEAVE's place, live 1000 ms after the pin is let go. */
	Stay,        /**< STAY on the leave page: back to the list, or to the main view. */
	NextServo,   /**< NEXT SERVO on the leave page: the claim kept, a new session. */
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
/**
 * @brief HITEC SERVO. No MENU row opens it: SERVO's entries run
 *        uiHitecEnterD() or uiHitecEnterClassic(). Its own `enter` is the
 *        D series entry, which needs no switched supply.
 */
inline constexpr const UiScreen UI_HITEC = {
	UiScreenId::Hitec, "HITEC SERVO", PinUser::Hitec, nullptr,
	uiHitecEnterD, uiHitecTick, nullptr, uiHitecControls, nullptr, false,
};
