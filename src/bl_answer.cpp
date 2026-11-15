/**
 * @file bl_answer.cpp
 * @brief The greeting's answer, classified. @see bl_answer.h
 */

#include "bl_answer.h"
#include "am32_bl.h"

/** @brief Whether @p b is an AM32 family byte am32_bl.h knows. */
static bool am32Family(uint8_t b) { return blAnswerAm32Settings(b) != 0; }

uint16_t blAnswerAm32Settings(uint8_t family) {
	switch (family) {
		case AM32_ESC_G071_2KB: return 0x7E00;
		case AM32_ESC_F051_1KB: return 0x7C00;
		case AM32_ESC_F3_2KB:   return 0xF800;
		default:                return 0;
	}
}

BlAnswer blAnswerClassify(const uint8_t *reply, int n) {
	BlAnswer a = { BlAnswerKind::NotUnderstood, 0, 0, false };
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
	a.fragment = blAnswerEfm8Fragment(reply, n);
	return a;
}

bool blAnswerEfm8Fragment(const uint8_t *reply, int n) {
	for (int i = 0; i < n; i++) {
		const uint8_t *e = reply + i;
		const int left = n - i;
		if (left >= 3 && e[0] == '4' && e[1] == '7' && e[2] == '1') return true;
		if (left >= 4 && e[0] == '7' && e[1] == '1' && e[3] == BL_ANSWER_EFM8_SIG_HI) return true;
		if (left >= 2 && e[0] == BL_ANSWER_EFM8_SIG_HI && (e[1] == 0xB1u || e[1] == 0xB2u || e[1] == 0xB5u))
			return true;
	}
	return false;
}
