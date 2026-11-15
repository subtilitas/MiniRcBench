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

BlAnswer blAnswerClassify(const uint8_t *reply, int n) {
	BlAnswer a = { BlAnswerKind::NotUnderstood, 0, 0 };
	if (n <= 0) {
		a.kind = BlAnswerKind::Silent;
		return a;
	}
	// The fifth byte from the end of a good answer: an AM32 family.
	if (n >= 5 && reply[n - 1] == BL_ANSWER_ACK && am32Family(reply[n - 5])) {
		a.kind = BlAnswerKind::Am32;
		a.family = reply[n - 5];
		return a;
	}
	// `471` and 0xE8 four bytes on, the last such place: the bytes after the
	// 0xE8 are not checked (bl_answer.h).
	for (int i = n - 1 - BL_ANSWER_EFM8_SIG_AT; i >= 0; i--) {
		const uint8_t *e = reply + i;
		if (e[0] != '4' || e[1] != '7' || e[2] != '1' || e[BL_ANSWER_EFM8_SIG_AT] != BL_ANSWER_EFM8_SIG_HI)
			continue;
		a.kind = BlAnswerKind::Efm8;
		uint8_t lo = i + BL_ANSWER_EFM8_SIG_AT + 1 < n ? e[BL_ANSWER_EFM8_SIG_AT + 1] : 0;
		a.signature = (uint16_t)(((uint16_t)BL_ANSWER_EFM8_SIG_HI << 8) | lo);
		return a;
	}
	return a;
}
