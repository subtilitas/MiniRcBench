/**
 * @file kst_seek.cpp
 * @brief The KST search. @see kst_seek.h
 */

#include "kst_seek.h"

/** @brief End the search as @p state with @p result. */
static KstSeek finish(KstSeekState *s, KstSeek state, kst_ses_t result, uint32_t nowMs) {
	s->state = state;
	s->result = (uint8_t)result;
	s->endMs = nowMs;
	return state;
}

/** @brief Want the supply off, from @p nowMs; a switched search's phases count from it. */
static void supplyOff(KstSeekState *s, uint32_t nowMs) {
	if (s->supply || s->offMs == 0) s->offMs = nowMs;
	s->supply = false;
}

/** @brief End a switched search with the supply off. */
static KstSeek finishOff(KstSeekState *s, KstSeek state, kst_ses_t result, uint32_t nowMs) {
	supplyOff(s, nowMs);
	return finish(s, state, result, nowMs);
}

/** @brief Start one attempt; Failed when the session refuses it. */
static KstSeek attempt(KstSeekState *s, kst_session_t *ses, uint32_t nowMs) {
	kst_ses_t r = kst_session_enter(ses);
	if (r != KST_SES_BUSY) return finishOff(s, KstSeek::Failed, r, nowMs);
	if (!s->switched) s->attempts++;
	return KstSeek::Searching;
}

/** @brief A switched attempt's power-up: the supply on, the attempt counted. */
static void switchOn(KstSeekState *s) {
	s->supply = true;
	s->attempts++;
}

/** @brief A switched attempt's entry, @p nowMs its phase start. */
static KstSeek enter(KstSeekState *s, kst_session_t *ses, uint32_t nowMs) {
	s->phase = KstSupplyPhase::Entry;
	s->phaseMs = nowMs;
	return attempt(s, ses, nowMs);
}

/** @brief The next switched attempt: supply off for @ref KST_SUPPLY_OFF_MS. */
static KstSeek offPhase(KstSeekState *s, uint32_t nowMs) {
	supplyOff(s, nowMs);
	s->phase = KstSupplyPhase::Off;
	s->phaseMs = s->offMs;
	return KstSeek::Searching;
}

void kstSeekBegin(KstSeekState *s, kst_session_t *ses, uint32_t nowMs, bool switched) {
	s->state = KstSeek::Searching;
	s->startMs = nowMs;
	s->endMs = nowMs;
	s->attempts = 0;
	s->result = KST_SES_BUSY;
	s->rx = KST_RX_NO_REPLY;
	s->faults = ses->stats.faults;
	s->stopping = false;
	s->noUi = false;
	s->switched = switched;
	s->delayMs = KST_SUPPLY_ENTRY_DELAY_MS;
	s->phase = KstSupplyPhase::None;
	s->phaseMs = nowMs;
	// Every search starts with the supply wanted off, whatever it was.
	s->supply = false;
	s->offMs = nowMs;
	if (switched) {
		offPhase(s, nowMs);
		return;
	}
	attempt(s, ses, nowMs);
}

/** @brief A switched search's phases outside its entry. */
static KstSeek waitStep(KstSeekState *s, kst_session_t *ses, uint32_t nowMs) {
	if (s->stopping) return finishOff(s, KstSeek::Stopped, KST_SES_ERR_ABORTED, nowMs);
	if (s->noUi) return finishOff(s, KstSeek::NoUi, KST_SES_ERR_NO_SERVO, nowMs);
	if (nowMs - s->startMs >= KST_SEEK_TIMEOUT_MS)
		return finishOff(s, KstSeek::TimedOut, KST_SES_ERR_NO_SERVO, nowMs);
	if (s->phase == KstSupplyPhase::Off) {
		if (nowMs - s->phaseMs < KST_SUPPLY_OFF_MS) return KstSeek::Searching;
		if (s->delayMs < 0) return enter(s, ses, nowMs);
		switchOn(s);
		s->phase = KstSupplyPhase::Delay;
		s->phaseMs = nowMs;
	}
	if (nowMs - s->phaseMs < (uint32_t)s->delayMs) return KstSeek::Searching;
	return enter(s, ses, nowMs);
}

KstSeek kstSeekStep(KstSeekState *s, kst_session_t *ses, bool uiAlive, uint32_t nowMs) {
	if (s->state != KstSeek::Searching) return s->state;
	if (!uiAlive) s->noUi = true;
	if (s->switched && s->phase != KstSupplyPhase::Entry) return waitStep(s, ses, nowMs);
	// A negative delay: the switch-on inside the entry's low, only while the
	// heartbeat is trusted and no stop came.
	if (s->switched && !s->supply && !s->noUi && !s->stopping &&
	    nowMs - s->phaseMs >= (uint32_t)(-s->delayMs))
		switchOn(s);

	kst_ses_t r = kst_session_step(ses);
	// Only a window of this search: last_reply keeps the code of whatever
	// transaction came before, and the fault count rises with each window
	// that held no valid reply.
	if (ses->stats.faults != s->faults) {
		s->faults = ses->stats.faults;
		uint8_t rx = ses->last_reply.result;
		if (rx != KST_RX_OK && rx != KST_RX_NO_REPLY) s->rx = rx;
	}
	if (r == KST_SES_BUSY) return KstSeek::Searching;

	// The session takes an abort before it reads the window, so a stopped
	// attempt never ends KST_SES_OK.
	// A frame that faulted on the wire ends the attempt with its own result,
	// a stop or not; no attempt starts after a stop.
	if (s->stopping)
		return finishOff(s, r == KST_SES_ERR_ABORTED ? KstSeek::Stopped : KstSeek::Failed, r, nowMs);
	if (r == KST_SES_OK) return finish(s, KstSeek::Found, r, nowMs);
	if (r != KST_SES_ERR_NO_SERVO) return finishOff(s, KstSeek::Failed, r, nowMs);
	if (s->noUi) return finishOff(s, KstSeek::NoUi, r, nowMs);
	if (nowMs - s->startMs >= KST_SEEK_TIMEOUT_MS) return finishOff(s, KstSeek::TimedOut, r, nowMs);
	if (s->switched) return offPhase(s, nowMs);
	return attempt(s, ses, nowMs);
}

void kstSeekStop(KstSeekState *s, kst_session_t *ses) {
	if (s->state != KstSeek::Searching) return;
	s->stopping = true;
	// Outside a switched entry nothing runs on the line.
	if (!s->switched || s->phase == KstSupplyPhase::Entry) kst_session_abort(ses);
}

bool kstSeekLost(KstSeekState *s, kst_session_t *ses, kst_ses_t r, bool uiAlive,
                 uint32_t nowMs, bool switched) {
	if (r != KST_SES_ERR_NO_SERVO || !ses->in_mode) return false;
	kst_session_power_cycled(ses);
	if (uiAlive) {
		kstSeekBegin(s, ses, nowMs, switched);
	} else {
		s->startMs = nowMs;
		s->attempts = 0;
		s->rx = KST_RX_NO_REPLY;
		s->stopping = false;
		s->noUi = true;
		s->switched = switched;
		s->phase = KstSupplyPhase::None;
		finishOff(s, KstSeek::NoUi, r, nowMs);
	}
	return true;
}

void kstSeekStalled(KstSeekState *s, kst_session_t *ses, uint32_t nowMs) {
	kst_session_power_cycled(ses);
	s->startMs = nowMs;
	s->attempts = 0;
	s->rx = KST_RX_NO_REPLY;
	s->stopping = false;
	s->noUi = true;
	s->phase = KstSupplyPhase::None;
	s->supply = true;  // so the switch-off below records its time
	finishOff(s, KstSeek::NoUi, KST_SES_ERR_NO_SERVO, nowMs);
}

uint32_t kstSeekOffLeftMs(const KstSeekState *s, uint32_t nowMs) {
	if (s->state != KstSeek::Searching || s->phase != KstSupplyPhase::Off) return 0;
	uint32_t gone = nowMs - s->phaseMs;
	return gone >= KST_SUPPLY_OFF_MS ? 0 : KST_SUPPLY_OFF_MS - gone;
}

uint32_t kstSeekMs(const KstSeekState *s, uint32_t nowMs) {
	switch (s->state) {
		case KstSeek::Idle:      return 0;
		case KstSeek::Searching: return nowMs - s->startMs;
		default:                 return s->endMs - s->startMs;
	}
}
