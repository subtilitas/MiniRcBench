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

/** @brief Start one attempt; Failed when the session refuses it. */
static KstSeek attempt(KstSeekState *s, kst_session_t *ses, uint32_t nowMs) {
	kst_ses_t r = kst_session_enter(ses);
	if (r != KST_SES_BUSY) return finish(s, KstSeek::Failed, r, nowMs);
	s->attempts++;
	return KstSeek::Searching;
}

void kstSeekBegin(KstSeekState *s, kst_session_t *ses, uint32_t nowMs) {
	s->state = KstSeek::Searching;
	s->startMs = nowMs;
	s->endMs = nowMs;
	s->attempts = 0;
	s->result = KST_SES_BUSY;
	s->rx = KST_RX_NO_REPLY;
	s->faults = ses->stats.faults;
	s->stopping = false;
	s->noUi = false;
	attempt(s, ses, nowMs);
}

KstSeek kstSeekStep(KstSeekState *s, kst_session_t *ses, bool uiAlive, uint32_t nowMs) {
	if (s->state != KstSeek::Searching) return s->state;
	if (!uiAlive) s->noUi = true;

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
	if (s->stopping) return finish(s, KstSeek::Stopped, r, nowMs);
	if (r == KST_SES_OK) return finish(s, KstSeek::Found, r, nowMs);
	if (r != KST_SES_ERR_NO_SERVO) return finish(s, KstSeek::Failed, r, nowMs);
	if (s->noUi) return finish(s, KstSeek::NoUi, r, nowMs);
	if (nowMs - s->startMs >= KST_SEEK_TIMEOUT_MS) return finish(s, KstSeek::TimedOut, r, nowMs);
	return attempt(s, ses, nowMs);
}

void kstSeekStop(KstSeekState *s, kst_session_t *ses) {
	if (s->state != KstSeek::Searching) return;
	s->stopping = true;
	kst_session_abort(ses);
}

bool kstSeekLost(KstSeekState *s, kst_session_t *ses, kst_ses_t r, bool uiAlive,
                 uint32_t nowMs) {
	if (r != KST_SES_ERR_NO_SERVO || !ses->in_mode) return false;
	kst_session_power_cycled(ses);
	if (uiAlive) {
		kstSeekBegin(s, ses, nowMs);
	} else {
		s->startMs = nowMs;
		s->attempts = 0;
		s->rx = KST_RX_NO_REPLY;
		s->stopping = false;
		s->noUi = true;
		finish(s, KstSeek::NoUi, r, nowMs);
	}
	return true;
}

uint32_t kstSeekMs(const KstSeekState *s, uint32_t nowMs) {
	switch (s->state) {
		case KstSeek::Idle:      return 0;
		case KstSeek::Searching: return nowMs - s->startMs;
		default:                 return s->endMs - s->startMs;
	}
}
