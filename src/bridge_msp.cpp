/**
 * @file bridge_msp.cpp
 * @brief The USB bridge's MSP answers. @see bridge_msp.h
 */

#include "bridge_msp.h"
#include "version.h"

#include <pico/unique_id.h>

#include <string.h>

/** @brief The name the board gives as its flight controller variant and board (decision 5). */
static const char NAME[4] = { 'M', 'R', 'C', 'B' };

/** @brief `MSP_BUILD_INFO`'s 19 bytes: `MRCB 0.1.0` and spaces. */
static const char BUILD_INFO[] = "MRCB " FW_VERSION_STRING "                   ";
static_assert(sizeof(BUILD_INFO) - 1 >= 19, "19 bytes at least");

void bridgeMspReset(BridgeMspReader *r) { r->state = 0; }

bool bridgeMspInFrame(const BridgeMspReader *r) { return r->state != 0; }

BridgeMspFeed bridgeMspFeed(BridgeMspReader *r, uint8_t b) {
	switch (r->state) {
		case 0:
			if (b != '$') return BridgeMspFeed::Dropped;
			r->state = 1;
			return BridgeMspFeed::More;
		case 1:
			if (b != 'M') break;
			r->state = 2;
			return BridgeMspFeed::More;
		case 2:
			if (b != '<') break;
			r->state = 3;
			return BridgeMspFeed::More;
		case 3:
			// 255 announces a jumbo frame, not taken.
			if (b == 255) break;
			r->len = b;
			r->chk = b;
			r->got = 0;
			r->state = 4;
			return BridgeMspFeed::More;
		case 4:
			r->cmd = b;
			r->chk ^= b;
			r->state = r->len ? 5 : 6;
			return BridgeMspFeed::More;
		case 5:
			r->payload[r->got++] = b;
			r->chk ^= b;
			if (r->got == r->len) r->state = 6;
			return BridgeMspFeed::More;
		case 6:
			r->state = 0;
			return b == r->chk ? BridgeMspFeed::Frame : BridgeMspFeed::Dropped;
		default:
			break;
	}
	r->state = 0;
	return BridgeMspFeed::Dropped;
}

/** @brief A little-endian 16-bit number at @p p. */
static void put16(uint8_t *p, uint16_t v) {
	p[0] = (uint8_t)v;
	p[1] = (uint8_t)(v >> 8);
}

uint16_t bridgeMspAnswer(const BridgeMspReader *r, uint8_t *out, BridgeMspEffect *effect) {
	effect->fourWay = false;
	effect->motorRefused = false;
	uint8_t *p = out + 5;
	uint8_t n = 0;
	bool known = true;
	switch (r->cmd) {
		case MSP_API_VERSION:
			// Protocol 0, API 1.49: Betaflight 134ce0f's msp_protocol.h.
			p[0] = 0;
			p[1] = 1;
			p[2] = 49;
			n = 3;
			break;
		case MSP_FC_VARIANT:
			memcpy(p, NAME, 4);
			n = 4;
			break;
		case MSP_FC_VERSION:
			p[0] = FW_VERSION_MAJOR;
			p[1] = FW_VERSION_MINOR;
			p[2] = FW_VERSION_PATCH;
			n = 3;
			break;
		case MSP_BOARD_INFO:
			memcpy(p, NAME, 4);
			put16(p + 4, 0);
			n = 6;
			break;
		case MSP_BUILD_INFO:
			// In place of Betaflight's build date and time: the name and the
			// firmware version, padded with spaces to 19 bytes, so that two
			// builds of one commit give the same image.
			memcpy(p, BUILD_INFO, 19);
			n = 19;
			break;
		case MSP_FEATURE_CONFIG:
			memset(p, 0, 4);
			n = 4;
			break;
		case MSP_STATUS:
			// Byte 15, the mode bytes that follow, 0; byte 16 the count of
			// arming-disable flags, here the three up to RX_FAILSAFE; bytes 17
			// to 20 the flags: RX_FAILSAFE, bit 2, set, as no receiver is on
			// the board and nothing arms through it.
			memset(p, 0, 22);
			p[16] = 3;
			p[17] = 0x04;
			n = 22;
			break;
		case MSP_MOTOR:
			put16(p, 1000);
			n = 2;
			break;
		case MSP_BATTERY_STATE:
			// No battery: Betaflight's 11 bytes, all 0.
			memset(p, 0, 11);
			n = 11;
			break;
		case MSP_MOTOR_CONFIG:
			put16(p, 1000);
			put16(p + 2, 2000);
			put16(p + 4, 1000);
			p[6] = 1;
			n = 7;
			break;
		case MSP_UID: {
			pico_unique_board_id_t id;
			pico_get_unique_board_id(&id);
			memcpy(p, id.id, 8);
			memset(p + 8, 0, 4);
			n = 12;
			break;
		}
		case MSP_SET_PASSTHROUGH:
			// No payload is the old form of the 4-way mode (Betaflight's
			// mspFcSetPassthroughCommand()); every other mode, the ESC serial
			// ones among them, is refused with 0.
			effect->fourWay = r->len == 0 || r->payload[0] == MSP_PASSTHROUGH_4WAY;
			p[0] = effect->fourWay ? 1 : 0;
			n = 1;
			break;
		case MSP_SET_MOTOR:
			effect->motorRefused = true;
			known = false;
			break;
		default:
			known = false;
			break;
	}
	out[0] = '$';
	out[1] = 'M';
	out[2] = known ? '>' : '!';
	out[3] = n;
	out[4] = r->cmd;
	uint8_t chk = (uint8_t)(n ^ r->cmd);
	for (uint8_t i = 0; i < n; i++) chk ^= p[i];
	p[n] = chk;
	return (uint16_t)(6 + n);
}

const char *bridgeMspName(uint8_t cmd) {
	switch (cmd) {
		case MSP_API_VERSION:     return "API VERSION";
		case MSP_FC_VARIANT:      return "FC VARIANT";
		case MSP_FC_VERSION:      return "FC VERSION";
		case MSP_BOARD_INFO:      return "BOARD INFO";
		case MSP_BUILD_INFO:      return "BUILD INFO";
		case MSP_FEATURE_CONFIG:  return "FEATURE CONFIG";
		case MSP_STATUS:          return "STATUS";
		case MSP_MOTOR:           return "MOTOR";
		case MSP_BATTERY_STATE:   return "BATTERY STATE";
		case MSP_MOTOR_CONFIG:    return "MOTOR CONFIG";
		case MSP_UID:             return "UID";
		case MSP_SET_MOTOR:       return "SET MOTOR";
		case MSP_SET_PASSTHROUGH: return "SET PASSTHROUGH";
		default:                  return nullptr;
	}
}
