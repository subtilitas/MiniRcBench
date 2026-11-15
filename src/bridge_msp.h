/**
 * @file bridge_msp.h
 * @brief The USB bridge's MSP side: the few MultiWii Serial Protocol (MSP)
 *        requests a desktop ESC tool makes before it asks for the 4-way
 *        interface (docs/design/usb-bridge.md, "MSP and the 4-way
 *        interface", slice 3).
 *
 * ESC-Configurator and the AM32 configurator speak MSP to what they take for
 * a flight controller, then send `MSP_SET_PASSTHROUGH` and the 4-way
 * interface's frames (bridge_4way.h). The board answers as one with a single
 * ESC: its own name, `MRCB` (decision 5), one motor, no battery, no receiver.
 * Nothing here reaches the wire.
 *
 * Only MSP version 1 frames: `$M<`, the payload's length, the command, the
 * payload and the XOR (exclusive or) of the length, the command and the
 * payload. The answer is `$M>` and the same layout, or `$M!` with no payload
 * for a command the board does not answer. A frame whose checksum is wrong is
 * not answered. Version 2 frames, `$X<`, and the jumbo length 255 are not
 * taken: the tools send version 1 for every command up to 254.
 *
 * | Command | Answer | Bytes |
 * |---|---|---|
 * | `MSP_API_VERSION` (1) | protocol 0, API 1.49 | 3 |
 * | `MSP_FC_VARIANT` (2) | `MRCB` | 4 |
 * | `MSP_FC_VERSION` (3) | the firmware version, version.h | 3 |
 * | `MSP_BOARD_INFO` (4) | `MRCB` and version 0 | 6 |
 * | `MSP_BUILD_INFO` (5) | `__DATE__` and `__TIME__` | 19 |
 * | `MSP_FEATURE_CONFIG` (36) | 0 | 4 |
 * | `MSP_STATUS` (101) | 0 but byte 16, 3 flags, and the arming-disable flags at bytes 17 to 20 with bit 2, RX_FAILSAFE, set | 22 |
 * | `MSP_MOTOR` (104) | one motor, 1000 | 2 |
 * | `MSP_BATTERY_STATE` (130) | 0 | 11 |
 * | `MSP_MOTOR_CONFIG` (131) | 1000, 2000, 1000, then 1 motor at byte 6 | 7 |
 * | `MSP_UID` (160) | the chip's 8-byte ID, then 4 bytes of 0 | 12 |
 * | `MSP_SET_PASSTHROUGH` (245) | 1, and the 4-way interface from the next byte, with no payload or mode 0xFF; 0 for any other mode | 1 |
 * | `MSP_SET_MOTOR` (214), any other | `$M!` | 0 |
 *
 * Every number is little-endian, as MSP has it. Each payload is at least as
 * long as the tools read: both read replies with a JavaScript `DataView`,
 * which throws on a read past the end.
 *
 * Pure but for the chip's ID; host-tested.
 *
 * @see docs/design/usb-bridge.md, "MSP and the 4-way interface"
 */

#pragma once

#include <stdint.h>

/** @brief The MSP commands the board answers, and the one it refuses by name. */
enum BridgeMspCmd : uint8_t {
	MSP_API_VERSION     = 1,
	MSP_FC_VARIANT      = 2,
	MSP_FC_VERSION      = 3,
	MSP_BOARD_INFO      = 4,
	MSP_BUILD_INFO      = 5,
	MSP_FEATURE_CONFIG  = 36,
	MSP_STATUS          = 101,
	MSP_MOTOR           = 104,
	MSP_BATTERY_STATE   = 130,
	MSP_MOTOR_CONFIG    = 131,
	MSP_UID             = 160,
	MSP_SET_MOTOR       = 214,
	MSP_SET_PASSTHROUGH = 245,
};

/** @brief `MSP_SET_PASSTHROUGH`'s mode for the 4-way interface; no payload means it too. */
#define MSP_PASSTHROUGH_4WAY 0xFFu

/** @brief The longest answer: `$M>`, length, command, 22 bytes of `MSP_STATUS`, checksum. */
#define BRIDGE_MSP_ANSWER_MAX 28u

/** @brief An MSP frame being read from the PC, byte by byte. */
struct BridgeMspReader {
	uint8_t state;         /**< 0 idle; 1 to 6 the parts read so far. */
	uint8_t len;           /**< The payload's length. */
	uint8_t cmd;           /**< The command. */
	uint8_t got;           /**< Payload bytes read. */
	uint8_t chk;           /**< The XOR so far. */
	uint8_t payload[254];  /**< The payload. */
};

/** @brief What an answered frame asks of the bridge. */
struct BridgeMspEffect {
	bool fourWay;       /**< `MSP_SET_PASSTHROUGH` answered 1: the 4-way interface from the next byte. */
	bool motorRefused;  /**< `MSP_SET_MOTOR` answered `$M!`: the screen says so. */
};

/** @brief The result of one byte. */
enum class BridgeMspFeed : uint8_t {
	More,     /**< Inside a frame: more bytes wanted. */
	Frame,    /**< A whole frame with a good checksum: answer it. */
	Dropped,  /**< The frame ended badly: a wrong second or third byte, the jumbo length or a wrong checksum. */
};

/** @brief Start reading a new frame. @param r The reader. */
void bridgeMspReset(BridgeMspReader *r);

/** @brief Whether a frame is being read: its `$` came. @param r The reader. @return True from `$` to its end. */
bool bridgeMspInFrame(const BridgeMspReader *r);

/**
 * @brief Take one byte from the PC. A byte other than `$` outside a frame
 *        is dropped and returns Dropped.
 * @param r The reader.
 * @param b The byte.
 * @return More, Frame or Dropped; after Frame the reader holds the frame
 *         until the next byte.
 */
BridgeMspFeed bridgeMspFeed(BridgeMspReader *r, uint8_t b);

/**
 * @brief The answer to the frame the reader holds.
 * @param r      The reader, after Frame.
 * @param out    Room for @ref BRIDGE_MSP_ANSWER_MAX bytes.
 * @param effect Out: what the answer asks of the bridge.
 * @return The answer's length.
 */
uint16_t bridgeMspAnswer(const BridgeMspReader *r, uint8_t *out, BridgeMspEffect *effect);

/**
 * @brief The screen's name of an MSP command: `API VERSION`, `SET PASSTHROUGH`.
 * @param cmd The command.
 * @return Its name; null for one the board does not name.
 */
const char *bridgeMspName(uint8_t cmd);
