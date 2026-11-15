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

/** @brief From the `4` of `471` to the signature's 0xE8, bytes. */
#define BL_ANSWER_EFM8_SIG_AT 4

/** @brief The signature's first byte on every EFM8 part the bootloader names. */
#define BL_ANSWER_EFM8_SIG_HI 0xE8u

/** @brief What answered. */
enum class BlAnswerKind : uint8_t {
	Silent,         /**< No byte. */
	Am32,           /**< An AM32 bootloader of a family am32_bl.h knows. */
	Efm8,           /**< The EFM8 bootloader of Bluejay or BLHeli_S: `471`, then 0xE8 four bytes on. */
	NotUnderstood,  /**< Bytes, but neither. */
};

/** @brief The answer, classified. */
struct BlAnswer {
	BlAnswerKind kind;
	/** @brief Am32: the family byte, 0x1F, 0x2B or 0x35; else 0. */
	uint8_t      family;
	/** @brief Efm8: 0xE8 and the byte after it, 0xE8B1, 0xE8B2 or 0xE8B5 on the parts the
	 *         bootloader names; else 0. */
	uint16_t     signature;
	/**
	 * @brief NotUnderstood: the bytes hold a piece of an EFM8 answer, so an
	 *        EFM8 bootloader may wait on the line; false for every other kind.
	 *        @see blAnswerEfm8Fragment()
	 */
	bool         fragment;
};

/**
 * @brief Classify the bytes that came back after a greeting.
 *
 * In order: no byte is Silent; 5 bytes or more ending in
 * @ref BL_ANSWER_ACK whose fifth byte from the end is an AM32 family are
 * Am32, whatever the bytes before it, as am32BlHandshake() has always taken
 * it; `471` with 0xE8 four bytes after its `4`, anywhere in the reply, is
 * Efm8, its signature 0xE8 and the byte after it, the last such `471`
 * counting; anything else is NotUnderstood.
 *
 * The EFM8 rule is loose on purpose (docs/design/bluejay-blheli-s.md,
 * slice 3): a trailing glitch byte, a garbled byte after the 0xE8 or a lost
 * 0x30 still name an EFM8, so the claim keeps DShot off its bootloader, and
 * escwire's own first contact then reads the answer again and checks it in
 * full. An answer cut right after the 0xE8 has the signature 0xE800.
 *
 * A reply not understood that holds a piece of an EFM8 answer, as
 * blAnswerEfm8Fragment() finds it, has @ref BlAnswer::fragment set.
 *
 * @param reply The bytes, in the order they arrived.
 * @param n     How many.
 * @return The classification.
 */
BlAnswer blAnswerClassify(const uint8_t *reply, int n);

/**
 * @brief Whether the bytes hold a piece of an EFM8 bootloader's answer: `471`,
 *        or `71`, any byte and 0xE8, or 0xE8 followed by 0xB1, 0xB2 or 0xB5.
 *
 * An answer with a byte lost, its `d` or its leading `4`, is no Efm8 answer by
 * blAnswerClassify()'s rule, yet the bootloader that sent it waits on the line
 * with no timeout. ESC SETTINGS gives the claim the leave page on such a reply
 * (docs/design/bluejay-blheli-s.md, slice 3): a false hit costs a leave page,
 * a miss DShot into a waiting bootloader. Bytes with none of the pieces keep
 * the leave page off, so noise on the line does not ask for an unplug.
 *
 * @param reply The bytes, in the order they arrived.
 * @param n     How many.
 * @return true when one of the pieces is there.
 */
bool blAnswerEfm8Fragment(const uint8_t *reply, int n);
