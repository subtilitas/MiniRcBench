/**
 * @file clock_line.cpp
 * @brief The clock's line command: the parser, the answers and the line
 *        reader. @see clock_line.h
 */

#include "clock_line.h"

#include <stdio.h>
#include <string.h>

/** @brief Every line this file answers starts with this. */
static const char PREFIX[] = "CLOCK";
#define PREFIX_LEN (sizeof(PREFIX) - 1)

/** @brief The set's form after the prefix: a space, then `YYYY-MM-DD HH:MM:SS`. */
static const char SET_FORM[] = " DDDD-DD-DD DD:DD:DD";
#define SET_LEN (PREFIX_LEN + sizeof(SET_FORM) - 1)

/** @brief The number of the @p n digits at @p s. */
static unsigned digits(const char *s, int n) {
	unsigned v = 0;
	for (int i = 0; i < n; i++) v = v * 10u + (unsigned)(s[i] - '0');
	return v;
}

ClockCmd clockLineParse(const char *line, size_t len, bool over, ClockTime *t) {
	if (len < PREFIX_LEN || memcmp(line, PREFIX, PREFIX_LEN) != 0) return ClockCmd::Ignore;
	if (over) return ClockCmd::BadLine;
	if (len == PREFIX_LEN + 1 && line[PREFIX_LEN] == '?') return ClockCmd::Query;
	if (len != SET_LEN) return ClockCmd::BadLine;
	// Byte for byte: a digit where the form has D, else the form's own byte.
	for (size_t i = 0; i < sizeof(SET_FORM) - 1; i++) {
		char c = line[PREFIX_LEN + i], f = SET_FORM[i];
		if (f == 'D' ? (c < '0' || c > '9') : c != f) return ClockCmd::BadLine;
	}
	const char *d = line + PREFIX_LEN + 1;
	unsigned year = digits(d, 4);
	ClockTime v = {
		(uint16_t)year,           (uint8_t)digits(d + 5, 2),  (uint8_t)digits(d + 8, 2),
		(uint8_t)digits(d + 11, 2), (uint8_t)digits(d + 14, 2), (uint8_t)digits(d + 17, 2),
	};
	if (!clockValid(&v)) return ClockCmd::BadDate;
	*t = v;
	return ClockCmd::Set;
}

/** @brief @p text, then CR LF, into @p out. @return Its length. */
static size_t answer(char *out, const char *text) {
	return (size_t)snprintf(out, CLOCK_ANSWER_BYTES, "%s\r\n", text);
}

/** @brief `CLOCK `, @p head, the time, CR LF. @return Its length. */
static size_t answerTime(char *out, const char *head, const ClockTime *t) {
	char tt[CLOCK_TEXT_BYTES];
	clockText(t, tt);
	return (size_t)snprintf(out, CLOCK_ANSWER_BYTES, "CLOCK %s%s\r\n", head, tt);
}

size_t clockLineAnswer(ClockCmd cmd, const ClockTime *t, char *out) {
	switch (cmd) {
		case ClockCmd::Query: {
			ClockReading c;
			clockNow(&c);
			return c.set ? answerTime(out, "", &c.t) : answer(out, "CLOCK NOT SET");
		}
		case ClockCmd::Set:
			switch (clockSet(t)) {
				case ClockSetResult::Set:      return answerTime(out, "SET ", t);
				case ClockSetResult::BadDate:  return answer(out, "CLOCK REFUSED: BAD DATE");
				case ClockSetResult::ReadBack: return answer(out, "CLOCK REFUSED: RTC READ-BACK DIFFERS");
				case ClockSetResult::NoAnswer: break;
			}
			return answer(out, "CLOCK REFUSED: RTC NOT ANSWERING");
		case ClockCmd::BadLine: return answer(out, "CLOCK REFUSED: BAD LINE");
		case ClockCmd::BadDate: return answer(out, "CLOCK REFUSED: BAD DATE");
		case ClockCmd::Ignore:  break;
	}
	return 0;
}

size_t clockLinePoll(ClockLineIn *in, ClockByteFn get, char *out) {
	for (;;) {
		int c = get();
		if (c < 0) return 0;
		if (c == '\r' || c == '\n') {
			if (in->len == 0 && !in->over) continue;   // the LF of CR LF, or a blank line
			ClockTime t = {};
			ClockCmd cmd = clockLineParse(in->buf, in->len, in->over, &t);
			in->len = 0;
			in->over = false;
			return clockLineAnswer(cmd, &t, out);
		}
		if (in->len < CLOCK_LINE_MAX) in->buf[in->len++] = (char)c;
		else in->over = true;
	}
}
