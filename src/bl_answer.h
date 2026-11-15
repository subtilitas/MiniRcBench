/**
 * @file bl_answer.h
 * @brief What answered the greeting on SIG: an AM32 bootloader, an EFM8
 *        bootloader (Bluejay or BLHeli_S), or something else.
 *
 * ESC SETTINGS sends one greeting, twelve 0x00, 0x0D, `BLHeli` and 0xF4 0x7D
 * (am32BlHandshake()). An AM32 bootloader and the EFM8 bootloader of Bluejay
 * and BLHeli_S both take it and answer with 9 bytes that end in 0x30. The
 * fifth byte from the end tells them apart: an AM32 bootloader puts its MCU
 * (microcontroller unit) family there, 0x1F, 0x2B or 0x35; an EFM8 bootloader
 * puts the first byte of its signature, 0xE8, there, followed by 0xB1, 0xB2
 * or 0xB5 for EFM8BB1, EFM8BB2 and EFM8BB51 (docs/design/bluejay-blheli-s.md,
 * "The greeting already fits"). Read from the bootloaders' sources; not seen
 * on an ESC.
 *
 * The answer is indexed from its end, as am32BlHandshake() reads it: a host
 * that captures its own transmission has the echo in front, and the answer
 * is still last.
 *
 * Pure; host-tested.
 */

#pragma once

#include <stdint.h>

/** @brief The byte both bootloaders end a good answer with. */
#define BL_ANSWER_ACK 0x30u

/** @brief Bytes of an EFM8 bootloader's answer: `471`, a byte, the signature, two bytes, 0x30. */
#define BL_ANSWER_EFM8_LEN 9

/** @brief The signature's first byte on every EFM8 part the bootloader names. */
#define BL_ANSWER_EFM8_SIG_HI 0xE8u

/** @brief What answered. */
enum class BlAnswerKind : uint8_t {
	Silent,         /**< No byte. */
	Am32,           /**< An AM32 bootloader of a family am32_bl.h knows. */
	Efm8,           /**< The EFM8 bootloader of Bluejay or BLHeli_S, signature E8B1, E8B2 or E8B5. */
	NotUnderstood,  /**< Bytes, but neither. */
};

/** @brief The answer, classified. */
struct BlAnswer {
	BlAnswerKind kind;
	/** @brief Am32: the family byte, 0x1F, 0x2B or 0x35; else 0. */
	uint8_t      family;
	/** @brief Efm8: the signature, 0xE8B1, 0xE8B2 or 0xE8B5; else 0. */
	uint16_t     signature;
};

/**
 * @brief Classify the bytes that came back after a greeting.
 *
 * In order: no byte is Silent; fewer than 5 bytes, or a last byte other than
 * @ref BL_ANSWER_ACK, is NotUnderstood; a fifth byte from the end that is an
 * AM32 family is Am32, whatever the bytes before it, as am32BlHandshake()
 * has always taken it; 9 bytes or more ending in `471`, any byte, 0xE8,
 * 0xB1, 0xB2 or 0xB5, any two bytes and 0x30 are Efm8; anything else is
 * NotUnderstood.
 *
 * @param reply The bytes, in the order they arrived.
 * @param n     How many.
 * @return The classification.
 */
BlAnswer blAnswerClassify(const uint8_t *reply, int n);
