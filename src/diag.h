/**
 * @file diag.h
 * @brief Diagnostics: why the chip last restarted, and the time since boot.
 *
 * The decoding is a pure function of four values the RP2350 exposes through
 * the Pico SDK 2.1.1: the power manager's CHIP_RESET register
 * (`powman_hw->chip_reset`), the watchdog's REASON register
 * (`watchdog_hw->reason`), `watchdog_enable_caused_reboot()` and the boot
 * ROM's `rom_get_last_boot_type()`. diagCaptureRestart() reads them once at
 * boot. SETTINGS shows the result and the uptime under DIAGNOSTICS.
 *
 * Not tested on hardware: which reason a given restart produces on the board
 * is not recorded. @see docs/design/diagnostics.md
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

/**
 * @defgroup diag_chip_reset CHIP_RESET bits the decoding reads
 * @brief The HAD_* flags of POWMAN_CHIP_RESET, from the SDK's
 *        `hardware/regs/powman.h`. The firmware build checks each against
 *        the SDK's macro; the host suite uses these.
 * @{
 */
#define DIAG_CR_HAD_WATCHDOG_RESET_RSM          0x10000000u  /**< Watchdog, power-on state machine. */
#define DIAG_CR_HAD_HZD_SYS_RESET_REQ           0x08000000u  /**< System reset from the RISC-V debugger. */
#define DIAG_CR_HAD_GLITCH_DETECT               0x04000000u  /**< Power supply glitch. */
#define DIAG_CR_HAD_SWCORE_PD                   0x02000000u  /**< Switched core power-down. */
#define DIAG_CR_HAD_WATCHDOG_RESET_SWCORE       0x01000000u  /**< Watchdog, switched core. */
#define DIAG_CR_HAD_WATCHDOG_RESET_POWMAN       0x00800000u  /**< Watchdog, power manager. */
#define DIAG_CR_HAD_WATCHDOG_RESET_POWMAN_ASYNC 0x00400000u  /**< Watchdog, power manager, asynchronous. */
#define DIAG_CR_HAD_RESCUE                      0x00200000u  /**< Rescue reset from the debugger. */
#define DIAG_CR_HAD_DP_RESET_REQ                0x00080000u  /**< Reset request from the Arm debugger. */
#define DIAG_CR_HAD_RUN_LOW                     0x00040000u  /**< The RUN pin. */
#define DIAG_CR_HAD_BOR                         0x00020000u  /**< Brown-out detection. */
#define DIAG_CR_HAD_POR                         0x00010000u  /**< Power-on reset. */
/** @} */

/** @brief rom_get_last_boot_type() after a flash update: the SDK's
 *         BOOT_TYPE_FLASH_UPDATE, checked against it in the firmware build. */
#define DIAG_BOOT_TYPE_FLASH_UPDATE 4

/** @brief Why the chip last restarted, as the decoding reads it. */
enum class RestartReason : uint8_t {
	PowerOn,    /**< HAD_POR: power applied. */
	BrownOut,   /**< HAD_BOR: the supply fell below the brown-out level. */
	RunPin,     /**< HAD_RUN_LOW: the RUN pin was pulled low. */
	Watchdog,   /**< An armed watchdog timed out: watchdog_enable_caused_reboot(), or a HAD_WATCHDOG_* flag. */
	Update,     /**< REASON set and the boot ROM reports a flash update boot: a UF2 copy, per the boot ROM source. */
	Reboot,     /**< REASON set otherwise: a reboot through the watchdog that some program asked for, such as picotool or watchdog_reboot(). */
	Debugger,   /**< HAD_DP_RESET_REQ, HAD_RESCUE or HAD_HZD_SYS_RESET_REQ. */
	Glitch,     /**< HAD_GLITCH_DETECT: the glitch detector. */
	PowerDown,  /**< HAD_SWCORE_PD: the switched core was powered down. */
	Unknown,    /**< None of the above. */
};

/** @brief Characters of the longest diagRestartText(). */
#define DIAG_RESTART_TEXT_WIDEST 10

/**
 * @brief Decode the restart reason. Pure: no register is read.
 *
 * The watchdog decides first, because a watchdog reset that only reaches
 * the power-on state machine need not change CHIP_RESET:
 * watchdog_enable_caused_reboot() is WATCHDOG; a non-zero REASON is UPDATE
 * after a flash update boot and REBOOT otherwise. Then the CHIP_RESET flags,
 * the first set deciding: first those of resets that leave the power
 * manager alone, which can only be newer than the others (GLITCH, the
 * watchdog's RSM and SWCORE, HZD_SYS_RESET_REQ, SWCORE_PD); then those of
 * resets that reset it (POR, BOR, RUN, DP_RESET_REQ and RESCUE, the
 * watchdog's POWMAN and POWMAN_ASYNC).
 *
 * @param chipReset            POWMAN_CHIP_RESET as read.
 * @param watchdogReason       WATCHDOG_REASON as read.
 * @param watchdogEnableCaused watchdog_enable_caused_reboot().
 * @param bootType             rom_get_last_boot_type(); negative for an error.
 * @return The reason; Unknown when nothing is set.
 */
RestartReason diagDecodeRestart(uint32_t chipReset, uint32_t watchdogReason,
                                bool watchdogEnableCaused, int bootType);

/**
 * @brief A reason as short upper-case text: `POWER-ON`, `BROWN-OUT`,
 *        `RUN PIN`, `WATCHDOG`, `UPDATE`, `REBOOT`, `DEBUGGER`, `GLITCH`,
 *        `POWER-DOWN` or `UNKNOWN`.
 * @param r The reason.
 * @return Static text, at most DIAG_RESTART_TEXT_WIDEST characters.
 */
const char *diagRestartText(RestartReason r);

/**
 * @brief Read the four values and keep the decoded reason. Called once,
 *        first thing in setup().
 */
void diagCaptureRestart();

/** @brief The reason diagCaptureRestart() kept; Unknown before it ran. */
RestartReason diagLastRestart();

/** @brief Characters of the longest diagUptimeText(): `1193046:28:15`. */
#define DIAG_UPTIME_TEXT_WIDEST 13

/**
 * @brief Seconds as `H:MM:SS`, hours not capped: `0:00:05`, `27:03:09`.
 * @param buf     Output.
 * @param n       Its size, bytes.
 * @param seconds Seconds since boot.
 */
void diagUptimeText(char *buf, size_t n, uint32_t seconds);

/** @brief Whole seconds since boot, from the 64-bit microsecond timer. */
uint32_t diagUptimeSeconds();
