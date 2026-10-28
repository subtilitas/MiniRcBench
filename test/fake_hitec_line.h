/**
 * @file fake_hitec_line.h
 * @brief The host suite's Hitec line: a simulated servo behind
 *        hitec_line_port.h, on the virtual clock.
 *
 * Hitec is a name or trademark of its owner; it is used here only to say
 * which servos this module works with.
 *
 * The servo answers as the servo model of rcbench's `hitec_session_rig.h`
 * does: classic replies 513 us after the start of the master's last byte,
 * D series answers of 7 bytes 6 ms after the request, the classic `+` 160 ms
 * after power-up and the next one 250 ms after the first. Its memory starts
 * from the bench reads of rcbench's `hitec_reg_fix.h`: an HS-5645MG or a
 * D485HW. It follows the level the bench drives on GP0: unpowered it
 * answers nothing, and a classic servo leaves serial mode. A D series servo
 * on the user's supply (@ref HitecSim::userSupply) is powered whatever GP0
 * does.
 *
 * A transaction ends at the virtual clock's time its answer is done, or at
 * its window's end with no answer; poll returns its capture from then on.
 */

#pragma once

#include <stdint.h>

#include "hitec_reg.h"
#include "hitec_wire.h"

/** @brief The simulated servo. Tests read and set the fields directly. */
struct HitecSim {
	uint8_t  family;         /**< `hitec_family_t` of the servo. */
	uint8_t  mem[HITEC_MEM_SIZE];  /**< Cells, or registers low byte first. */
	uint8_t  version;        /**< Classic: the answer to `67`. */
	bool     userSupply;     /**< Powered whatever GP0 does (D series with NONE). */
	bool     dead;           /**< Answers nothing. */
	uint32_t startupMs;      /**< GP0 high to powered. */
	uint32_t plusMs;         /**< Classic: powered to the first `+`. */
	bool     level;          /**< The GP0 level last seen. */
	uint32_t onUs;           /**< When it last went high. */
	uint8_t  cstate;         /**< Classic: 0 before the first `+`, 1 after it, 2 in serial mode. */
	uint32_t firstPlusUs;    /**< Classic: when the first `+` went out. */
	unsigned switchOns;      /**< Rising edges of GP0 seen. */
	uint32_t lastOffUs;      /**< The last falling edge. */
	uint32_t minOffUs;       /**< Shortest time low before a rising edge; UINT32_MAX for none. */
	unsigned writes;         /**< Write frames taken, saves and reboots included. */
	unsigned reboots;        /**< D series reboot frames taken. */
	int      ignoreAddr;     /**< An address whose writes the servo drops; -1 for none. */
	int      dieAfterWrites; /**< Goes dead after this many writes; -1 for never. */
};

/** @brief The simulated servo. */
HitecSim *fakeHitecSim();

/**
 * @brief A fresh servo of @p family, unpowered, the line closed, the counts
 *        at 0, opens accepted.
 */
void fakeHitecLineReset(uint8_t family);

/** @brief The servo sees GP0 at @p level at @p nowUs. Tests call it every pass. */
void fakeHitecSimUpdate(bool level, uint32_t nowUs);

/** @brief Make the next opens fail with @p err, a `hitec_line_err_t`; 0 accepts. */
void fakeHitecLineRefuse(uint8_t err);

/** @brief Opens that succeeded since fakeHitecLineReset(). */
int fakeHitecLineOpens();

/** @brief Closes of an open line since fakeHitecLineReset(). */
int fakeHitecLineCloses();

/** @brief The family of the last open. */
uint8_t fakeHitecLineFamily();

/** @brief time_us_32() at the last close of an open line. */
uint32_t fakeHitecLineClosedUs();

/** @brief Transactions started since fakeHitecLineReset(). */
unsigned fakeHitecStarts();

/** @brief The last transaction started. */
const hitec_xfer_t *fakeHitecLastXfer();

/** @brief time_us_32() when the last transaction started. */
uint32_t fakeHitecLastStartUs();

/** @brief Transactions cut by hitecLineAbort() since fakeHitecLineReset(). */
unsigned fakeHitecAborts();
