/**
 * @file bl_answer.cpp
 * @brief The greeting's answer, classified. @see bl_answer.h
 */

#include "bl_answer.h"
#include "am32_bl.h"

/** @brief Whether @p b is an AM32 family byte am32_bl.h knows. */
static bool am32Family(uint8_t b) {
	return b == AM32_ESC_G071_2KB || b == AM32_ESC_F051_1KB || b == AM32_ESC_F3_2KB;
}

/** @brief Whether @p b follows 0xE8 in the signature of an EFM8 part the bootloader names. */
static bool efm8Part(uint8_t b) { return b == 0xB1u || b == 0xB2u || b == 0xB5u; }

BlAnswer blAnswerClassify(const uint8_t *reply, int n) {
	BlAnswer a = { BlAnswerKind::NotUnderstood, 0, 0 };
	if (n <= 0) {
		a.kind = BlAnswerKind::Silent;
		return a;
	}
	if (n < 5 || reply[n - 1] != BL_ANSWER_ACK) return a;
	// The fifth byte from the end: an AM32 family, or the signature's 0xE8.
	const uint8_t at = reply[n - 5];
	if (am32Family(at)) {
		a.kind = BlAnswerKind::Am32;
		a.family = at;
		return a;
	}
	if (n < BL_ANSWER_EFM8_LEN) return a;
	const uint8_t *e = reply + n - BL_ANSWER_EFM8_LEN;
	if (e[0] != '4' || e[1] != '7' || e[2] != '1') return a;
	if (at != BL_ANSWER_EFM8_SIG_HI || !efm8Part(e[5])) return a;
	a.kind = BlAnswerKind::Efm8;
	a.signature = (uint16_t)(((uint16_t)at << 8) | e[5]);
	return a;
}
