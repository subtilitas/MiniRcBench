/**
 * @file diag.cpp
 * @brief The restart reason and the uptime. @see diag.h
 */

#include "diag.h"

#include <stdio.h>

#include "pico/time.h"
#include "hardware/watchdog.h"
#include "hardware/structs/powman.h"

#if PICO_ON_DEVICE
#include "hardware/regs/powman.h"
static_assert(DIAG_CR_HAD_WATCHDOG_RESET_RSM == POWMAN_CHIP_RESET_HAD_WATCHDOG_RESET_RSM_BITS, "CHIP_RESET bit");
static_assert(DIAG_CR_HAD_HZD_SYS_RESET_REQ == POWMAN_CHIP_RESET_HAD_HZD_SYS_RESET_REQ_BITS, "CHIP_RESET bit");
static_assert(DIAG_CR_HAD_GLITCH_DETECT == POWMAN_CHIP_RESET_HAD_GLITCH_DETECT_BITS, "CHIP_RESET bit");
static_assert(DIAG_CR_HAD_SWCORE_PD == POWMAN_CHIP_RESET_HAD_SWCORE_PD_BITS, "CHIP_RESET bit");
static_assert(DIAG_CR_HAD_WATCHDOG_RESET_SWCORE == POWMAN_CHIP_RESET_HAD_WATCHDOG_RESET_SWCORE_BITS, "CHIP_RESET bit");
static_assert(DIAG_CR_HAD_WATCHDOG_RESET_POWMAN == POWMAN_CHIP_RESET_HAD_WATCHDOG_RESET_POWMAN_BITS, "CHIP_RESET bit");
static_assert(DIAG_CR_HAD_WATCHDOG_RESET_POWMAN_ASYNC == POWMAN_CHIP_RESET_HAD_WATCHDOG_RESET_POWMAN_ASYNC_BITS, "CHIP_RESET bit");
static_assert(DIAG_CR_HAD_RESCUE == POWMAN_CHIP_RESET_HAD_RESCUE_BITS, "CHIP_RESET bit");
static_assert(DIAG_CR_HAD_DP_RESET_REQ == POWMAN_CHIP_RESET_HAD_DP_RESET_REQ_BITS, "CHIP_RESET bit");
static_assert(DIAG_CR_HAD_RUN_LOW == POWMAN_CHIP_RESET_HAD_RUN_LOW_BITS, "CHIP_RESET bit");
static_assert(DIAG_CR_HAD_BOR == POWMAN_CHIP_RESET_HAD_BOR_BITS, "CHIP_RESET bit");
static_assert(DIAG_CR_HAD_POR == POWMAN_CHIP_RESET_HAD_POR_BITS, "CHIP_RESET bit");
#endif

static RestartReason s_reason = RestartReason::Unknown;

/** @brief One CHIP_RESET flag group and the reason it decodes to, in decoding order. */
struct FlagReason {
	uint32_t      bits;
	RestartReason reason;
};

static const FlagReason FLAG_ORDER[] = {
	{ DIAG_CR_HAD_POR, RestartReason::PowerOn },
	{ DIAG_CR_HAD_BOR, RestartReason::BrownOut },
	{ DIAG_CR_HAD_RUN_LOW, RestartReason::RunPin },
	{ DIAG_CR_HAD_DP_RESET_REQ | DIAG_CR_HAD_RESCUE | DIAG_CR_HAD_HZD_SYS_RESET_REQ,
	  RestartReason::Debugger },
	{ DIAG_CR_HAD_GLITCH_DETECT, RestartReason::Glitch },
	{ DIAG_CR_HAD_WATCHDOG_RESET_RSM | DIAG_CR_HAD_WATCHDOG_RESET_SWCORE |
	  DIAG_CR_HAD_WATCHDOG_RESET_POWMAN | DIAG_CR_HAD_WATCHDOG_RESET_POWMAN_ASYNC,
	  RestartReason::Watchdog },
	{ DIAG_CR_HAD_SWCORE_PD, RestartReason::PowerDown },
};

RestartReason diagDecodeRestart(uint32_t chipReset, bool watchdogCaused,
                                bool watchdogEnableCaused) {
	if (watchdogEnableCaused) return RestartReason::Watchdog;
	if (watchdogCaused) return RestartReason::Reboot;
	for (const FlagReason &f : FLAG_ORDER)
		if (chipReset & f.bits) return f.reason;
	return RestartReason::Unknown;
}

const char *diagRestartText(RestartReason r) {
	switch (r) {
		case RestartReason::PowerOn:   return "POWER-ON";
		case RestartReason::BrownOut:  return "BROWN-OUT";
		case RestartReason::RunPin:    return "RUN PIN";
		case RestartReason::Watchdog:  return "WATCHDOG";
		case RestartReason::Reboot:    return "REBOOT";
		case RestartReason::Debugger:  return "DEBUGGER";
		case RestartReason::Glitch:    return "GLITCH";
		case RestartReason::PowerDown: return "POWER-DOWN";
		case RestartReason::Unknown:   break;
	}
	return "UNKNOWN";
}

void diagCaptureRestart() {
	s_reason = diagDecodeRestart(powman_hw->chip_reset, watchdog_caused_reboot(),
	                             watchdog_enable_caused_reboot());
}

RestartReason diagLastRestart() { return s_reason; }

void diagUptimeText(char *buf, size_t n, uint32_t seconds) {
	snprintf(buf, n, "%lu:%02u:%02u", (unsigned long)(seconds / 3600u),
	         (unsigned)(seconds / 60u % 60u), (unsigned)(seconds % 60u));
}

uint32_t diagUptimeSeconds() { return (uint32_t)(time_us_64() / 1000000u); }
