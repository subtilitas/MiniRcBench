/**
 * @file esc_3d.h
 * @brief The tester's 3D note: whether the ESC on SIG may run in 3D mode.
 *
 * In 3D mode the three open firmwares run DShot 48 to 1047 the reverse of
 * the set direction at twice the step, and 1048 to 2047 in the set
 * direction from low (docs/design/esc-commands.md, "3D and the tester's
 * throttle"). The tester sends its 0 to 2000 as 47 to 2047 whatever the
 * ESC does with them, so it says so while this flag stands.
 *
 * Two screens write the flag. ESC SETTINGS sets it to what a list read shows,
 * an AM32 list's 3D MODE or a Bluejay or BLHeli_S list's DIRECTION at 3D or
 * 3D REVERSED, though a Bluejay or BLHeli_S page whose settings marker is
 * wrong leaves it; it sets it after a write with 3D MODE on that the ESC
 * accepted, and clears it
 * after a verified write with 3D MODE off. ESC COMMANDS sets it after 3D ON
 * once the change may have reached the ESC: the sequence sent, or stopped
 * after frame 6 of its first block. Its 3D OFF leaves it, as nothing reads
 * the ESC back (decision 4 of esc-commands.md). The flag is not stored: a
 * restart of the bench clears it. Nothing tells it that the ESC on the pin
 * was swapped for another one.
 */

#pragma once

/**
 * @brief The tester's note while esc3dNote() is true: 33 characters,
 *        197 px at scale 1, amber, on the RPM area's bottom row (y 114) in
 *        place of the pole count and the swipe hint, except while a drag on
 *        the pad is engaged: SWIPE ACTIVE takes the row then.
 */
#define ESC_3D_TESTER_NOTE "3D MODE ESC: 0-50% REVERSED AT 2X"

/**
 * @brief Whether the tester's 3D note stands.
 * @return true while the ESC on SIG may run in 3D mode.
 */
bool esc3dNote();

/**
 * @brief Set or clear the tester's 3D note.
 * @param on true when the ESC may run in 3D mode; false only on a reading
 *           that shows it does not.
 */
void esc3dSetNote(bool on);
