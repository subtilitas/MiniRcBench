/**
 * @file version.h
 * @brief The firmware version, in one place.
 *
 * CMakeLists.txt reads the three numbers below for the project version and
 * for the program version that `picotool info` shows. The USB (Universal
 * Serial Bus) device release and the SCSI (Small Computer System Interface)
 * product revision of the card reader are derived from them. release.yml
 * refuses a tag that does not name this version: tag v0.1.0 builds 0.1.0.
 *
 * The version names the firmware only. The settings block in flash carries its
 * own layout version, @ref SETTINGS_VERSION, which a firmware release does not
 * change.
 */

#pragma once

#define FW_VERSION_MAJOR 0 /**< Major version, 0 to 99. */
#define FW_VERSION_MINOR 1 /**< Minor version, 0 to 9. */
#define FW_VERSION_PATCH 0 /**< Patch version, 0 to 9. */

#if FW_VERSION_MAJOR > 99 || FW_VERSION_MINOR > 9 || FW_VERSION_PATCH > 9
#error "FW_VERSION_BCD holds two digits of major and one each of minor and patch"
#endif

/** @brief Turns a macro's value into a string literal. */
#define FW_VERSION_STR_(x) #x
/** @brief Expands its argument, then makes it a string literal. */
#define FW_VERSION_STR(x) FW_VERSION_STR_(x)

/** @brief The version as text, "major.minor.patch". */
#define FW_VERSION_STRING                                                     \
	FW_VERSION_STR(FW_VERSION_MAJOR) "." FW_VERSION_STR(FW_VERSION_MINOR) "." \
	FW_VERSION_STR(FW_VERSION_PATCH)

/**
 * @brief The version as a USB `bcdDevice`, binary-coded decimal 0xJJMN.
 *
 * Two digits of major, one of minor and one of patch, which is why the ranges
 * above stop at 99 and 9.
 */
#define FW_VERSION_BCD                                                        \
	((((FW_VERSION_MAJOR / 10) % 10) << 12) | ((FW_VERSION_MAJOR % 10) << 8) | \
	 ((FW_VERSION_MINOR % 10) << 4) | (FW_VERSION_PATCH % 10))
