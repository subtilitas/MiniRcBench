/**
 * @file efm8_view.cpp
 * @brief Rows and texts of a Bluejay or BLHeli_S page. @see efm8_view.h
 */

#include "efm8_view.h"
#include "ui_am32.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

/** @brief The groups, in the order the list shows them. */
static const char *const GROUPS[] = { "MOTOR", "STARTUP", "BRAKE", "LIMITS", "SOUND", "INPUT", "OTHER" };

/** @brief The place of @p fw's byte @p off among GROUPS. */
static int groupIndex(uint8_t fw, uint8_t off) {
	switch (off) {
		case 0x06: case 0x0B: case 0x15: case 0x1F: return 0;
		// STARTUP POWER is a motor row on BLHeli_S, a startup row on Bluejay.
		case 0x09: return fw == EW_FW_S ? 0 : 1;
		case 0x04: case 0x05: case 0x07: return 1;
		case 0x10: case 0x27: return 2;
		case 0x23: case 0x24: case 0x29: return 3;
		case 0x1B: case 0x1C: case 0x1D: return 4;
		case 0x0F: case 0x19: case 0x1A: case 0x21: return 5;
		default: return 6;
	}
}

const char *efm8Group(uint8_t fw, uint8_t off) { return GROUPS[groupIndex(fw, off)]; }

int efm8Rows(uint8_t fw, uint8_t layout, Efm8Row *out, int max) {
	if (fw != EW_FW_S && fw != EW_FW_BJ) return 0;
	int n = 0;
	for (size_t i = 0; n < max; i++) {
		const ew_field_t *f = ew_map_nth(fw, layout, i);
		if (!f) break;
		// The revision bytes go on the strip.
		if (f->off <= 0x02) continue;
		// By group, then by offset: insert behind every row that sorts before it.
		int at = n;
		int g = groupIndex(fw, f->off);
		while (at > 0) {
			const ew_field_t *p = out[at - 1].f;
			int pg = groupIndex(fw, p->off);
			if (pg < g || (pg == g && p->off <= f->off)) break;
			out[at] = out[at - 1];
			at--;
		}
		out[at].f = f;
		out[at].group = GROUPS[g];
		n++;
	}
	return n;
}

bool efm8ValueText(const ew_field_t *f, uint8_t raw, char *out, size_t n) {
	if (n == 0) return false;
	bool ok = ew_field_ok(f, raw);
	if (ok && f->off == EFM8_OFF_DIRECTION && f->labels && (raw == 3 || raw == 4)) {
		snprintf(out, n, "%s", raw == 3 ? "3D" : "3D REVERSED");
		return true;
	}
	ew_field_text(f, raw, out, n);
	for (char *c = out; *c; c++) *c = (char)toupper((unsigned char)*c);
	return ok;
}

bool efm8Is3d(const uint8_t *page) {
	return page[EFM8_OFF_DIRECTION] == 3 || page[EFM8_OFF_DIRECTION] == 4;
}

const char *efm8FwName(uint8_t fw) {
	if (fw == EW_FW_BJ) return "BLUEJAY";
	if (fw == EW_FW_S) return "BLHELI_S";
	return "EFM8";
}

Efm8View efm8ViewOf(const ew_ident_t *id) {
	if (id->fw != EW_FW_S && id->fw != EW_FW_BJ) return Efm8View::Refused;
	return id->writable ? Efm8View::List : Efm8View::ReadOnly;
}

void efm8Caption(const ew_ident_t *id, const uint8_t *page, uint16_t sig, char *out, size_t n) {
	snprintf(out, n, "%s %u.%u  LAYOUT %u  %04X", efm8FwName(id->fw), (unsigned)page[0],
	         (unsigned)page[1], (unsigned)page[2], (unsigned)sig);
}

bool efm8ReadOnlyLines(const ew_ident_t *id, const uint8_t *page, char *l1, const char **l2) {
	if (efm8ViewOf(id) != Efm8View::ReadOnly) return false;
	if (id->why == EW_WHY_RESET_PAGE) {
		snprintf(l1, EFM8_LINE_CHARS + 1, "%s", EFM8_UI_LOST_TAGS);
		*l2 = EFM8_UI_REFLASH;
	} else if (id->why == EW_WHY_MARKER) {
		snprintf(l1, EFM8_LINE_CHARS + 1, "%s", EFM8_UI_MARKER);
		*l2 = EFM8_UI_SHOWN_ONLY;
	} else {
		// An older layout: `BLUEJAY 0.19, LAYOUT 206: READ-ONLY.`; a version
		// too long for the line is left out.
		int n = snprintf(l1, EFM8_LINE_CHARS + 1, "%s %u.%u, LAYOUT %u: READ-ONLY.", efm8FwName(id->fw),
		                 (unsigned)page[0], (unsigned)page[1], (unsigned)page[2]);
		if (n > EFM8_LINE_CHARS)
			snprintf(l1, EFM8_LINE_CHARS + 1, "%s LAYOUT %u: READ-ONLY.", efm8FwName(id->fw),
			         (unsigned)page[2]);
		*l2 = EFM8_UI_SHOWN_ONLY;
	}
	return true;
}

bool efm8PageEmpty(const uint8_t *page) {
	for (unsigned i = 0; i < EW_MAP_LEN_S; i++)
		if (page[i] != 0xFF) return false;
	return true;
}

void efm8RefusalLines(const ew_ident_t *id, const ew_contact_t *c, const uint8_t *page,
                      char *status, const char **l1, const char **l2) {
	const size_t n = EFM8_LINE_CHARS + 1;
	*l1 = EFM8_UI_REFUSED_1;
	*l2 = EFM8_UI_REFUSED_2;
	// A signature no part has: the contact refused it, and nothing was read.
	if (id->why == EW_WHY_SIGNATURE) {
		snprintf(status, n, "EFM8 %04X: PART NOT KNOWN", (unsigned)c->signature);
		return;
	}
	if (efm8PageEmpty(page)) {
		snprintf(status, n, "EFM8 %04X: SETTINGS PAGE EMPTY", (unsigned)c->signature);
		*l1 = EFM8_UI_EMPTY_1;
		*l2 = EFM8_UI_EMPTY_2;
		return;
	}
	switch (id->why) {
		case EW_WHY_BJ_ON_BB1:
			snprintf(status, n, "BLUEJAY ON EFM8BB1: NOT TAKEN");
			break;
		case EW_WHY_LAYOUT:
			snprintf(status, n, "BLUEJAY LAYOUT %u: NOT TAKEN", (unsigned)page[2]);
			break;
		case EW_WHY_VARIANT:
			snprintf(status, n, "BLHELI_S %u.%u: NOT TAKEN", (unsigned)page[0], (unsigned)page[1]);
			break;
		default:
			snprintf(status, n, "EFM8 %04X: PAGE NOT KNOWN", (unsigned)c->signature);
			break;
	}
}

bool efm8RowNote(const ew_field_t *f, uint8_t raw, const char **l1, const char **l2) {
	if (!f) return false;
	if (f->off == EFM8_OFF_DIRECTION && (raw == 3 || raw == 4)) {
		*l1 = AM32_UI_3D_1;
		*l2 = AM32_UI_3D_2;
		return true;
	}
	if ((f->off == 0x23 || f->off == 0x24) && raw == 0) {
		*l1 = AM32_UI_PROT_1;
		*l2 = AM32_UI_PROT_2;
		return true;
	}
	if (f->off == 0x29) {
		*l1 = EFM8_UI_RATING_1;
		*l2 = EFM8_UI_RATING_2;
		return true;
	}
	return false;
}
