/**
 * @file test_esc_cmd.cpp
 * @brief Host tests for the command slot on core1, slice 1 of
 *        `docs/design/esc-commands.md`, held against model ESCs.
 *
 * The bench side is the firmware's own pure functions, in escTaskPoll()'s
 * order: escPumpBind(), escFoldReply(), escPumpSlot(), escEdtForget(),
 * escFrameWire() and escPumpFrameSent(). What is modelled is the PIO driver
 * (one frame a millisecond, its reply read on the next slot), core0's request
 * and the ESC.
 *
 * The model ESCs follow the command paths the design note reads from source
 * (AM32 2.22 `Src/dshot.c`, Bluejay 0.21 `src/Modules/Isrs.asm` and
 * `DShot.asm`, BLHeli_S 16.7 `BLHeli_S.asm`), cut down to what decides
 * whether a command is taken:
 *
 * - AM32 acts on the sixth equal frame, a beacon on the first, only after more
 *   than 1000 frames of zero input; a 0 frame clears the count; a lost or
 *   damaged frame leaves it; it answers every frame it captures, before
 *   decoding it, so a damaged frame is answered too.
 * - Bluejay acts on the seventh equal frame, a beacon on the first; a 0 frame
 *   or a command frame without the telemetry bit clears the command and its
 *   count; a lost or damaged frame leaves it, and a damaged one is not
 *   answered.
 * - BLHeli_S acts on the seventh, and a 0 frame leaves the count. BLHeli_S
 *   16.7 sends no eRPM; the model answers anyway, as a stand-in, so that its
 *   count rule meets the bench's sequence. One case keeps it silent.
 *
 * Every model clears its command after acting. How long a model stays silent
 * after a save (20 frames) or a beacon (Bluejay 200, BLHeli_S 100, the
 * interrupts-off times the note reads) is the model's, not measured.
 */

#include "check.h"

#include "../src/esc_task.h"

#include <stdio.h>
#include <string.h>

// ---------------------------------------------------------- the ESC models

/** @brief The firmware a model ESC runs. */
enum class Fw : uint8_t { Am32, Bluejay, BlheliS };

/** @brief What happens to the frame a slot sends. Set per slot by a test. */
enum class Fault : uint8_t {
	None,
	Lost,     /**< The ESC does not capture it: no count, no reply. */
	Damaged,  /**< It fails the ESC's checksum: no count; AM32 still answers. */
	NoReply,  /**< The ESC takes it; its reply does not reach the bench. */
	BadReply, /**< The ESC takes it; its reply arrives damaged. */
	EdtReply, /**< The ESC takes it and answers with an EDT voltage frame. */
};

/** @brief Frames a model stays silent after a save: its flash write. */
#define MODEL_SAVE_SILENT 20u

struct ModelEsc {
	Fw       fw;
	bool     present;       /**< Powered and wired. */
	bool     answers;       /**< Sends replies at all. */
	uint32_t periodUs;      /**< Electrical period; 0 is a stopped motor. */
	unsigned zeroFrames;    /**< AM32: frames of zero input taken. */
	uint16_t cmd;           /**< The command being counted. */
	unsigned count;         /**< Its count, in the firmware's own terms. */
	unsigned run;           /**< Equal command frames taken in a row. */
	unsigned silent;        /**< Frames left with nothing captured. */
	bool     edt;           /**< EDT on. */
	unsigned replies;       /**< Replies sent. */
	bool     reversed;      /**< Direction in RAM. */
	bool     savedReversed; /**< Direction in flash. */
	bool     bidir;         /**< 3D in RAM. */
	bool     savedBidir;    /**< 3D in flash. */
	unsigned saves;         /**< Saves done. */
	unsigned tones[6];      /**< Tones played, by beacon number; a save's tone too. */
	unsigned acts;          /**< Commands 7 to 10 and 12 acted on. */
	unsigned actRun;        /**< @ref run when it last acted. */
};

static void modelPowerUp(ModelEsc *m, Fw fw) {
	memset(m, 0, sizeof(*m));
	m->fw = fw;
	m->present = m->answers = true;
}

static void modelAct(ModelEsc *m, uint16_t v) {
	if (v >= 1 && v <= 5) {
		m->tones[v]++;
		if (m->fw == Fw::Bluejay) m->silent = 200;
		if (m->fw == Fw::BlheliS) m->silent = 100;
		return;
	}
	switch (v) {
		case DSHOT_CMD_SPIN_DIRECTION_1: m->reversed = false; break;
		case DSHOT_CMD_SPIN_DIRECTION_2: m->reversed = true; break;
		case DSHOT_CMD_3D_MODE_OFF:      m->bidir = false; break;
		case DSHOT_CMD_3D_MODE_ON:       m->bidir = true; break;
		case DSHOT_CMD_SAVE_SETTINGS:
			m->savedReversed = m->reversed;
			m->savedBidir = m->bidir;
			m->saves++;
			if (m->fw == Fw::Am32) m->tones[m->reversed ? 2 : 1]++;
			m->silent = MODEL_SAVE_SILENT;
			break;
		case DSHOT_CMD_EDT_ENABLE:
			if (m->fw != Fw::BlheliS) m->edt = true;
			return;
		default: return;
	}
	m->acts++;
	m->actRun = m->run;
}

/** @brief AM32's command path for one decoded frame. */
static void am32Take(ModelEsc *m, uint16_t v) {
	if (v <= DSHOT_CMD_MAX) {
		if (++m->zeroFrames > 1000) m->zeroFrames = 1001;
	} else {
		m->zeroFrames = 0;
	}
	if (v == 0) m->count = 0;
	bool armed = m->zeroFrames > 1000;
	if (v >= 1 && v <= DSHOT_CMD_MAX && armed && m->periodUs == 0) {
		if (v != m->cmd) {
			m->cmd = v;
			m->count = 0;
		}
		if (v <= 5) m->count = 6;
		if (++m->count >= 6) {
			m->count = 0;
			modelAct(m, v);
		}
	}
}

/** @brief Bluejay's and BLHeli_S's command path for one decoded frame. */
static void bluejayTake(ModelEsc *m, uint16_t v, bool telem) {
	if (v == 0) {
		if (m->fw == Fw::Bluejay) m->cmd = m->count = 0;
		return;
	}
	if (v > DSHOT_CMD_MAX) return;
	if (!telem) {
		m->cmd = m->count = 0;
		return;
	}
	if (v != m->cmd) {
		m->cmd = v;
		m->count = 0;
	} else {
		m->count++;
	}
	if (m->periodUs != 0) return;
	if (v <= 5 || m->count >= 6) {
		m->cmd = m->count = 0;
		modelAct(m, v);
	}
}

/** @brief An electrical period as an ESC encodes it; 0x0FFF is stopped. */
static uint16_t erpmPayload(uint32_t periodUs) {
	if (periodUs == 0) return 0x0FFFu;
	unsigned e = 0;
	while ((periodUs >> e) > 0x1FFu) e++;
	return (uint16_t)((e << 9) | (periodUs >> e));
}

/** @brief One frame in; the reply the bench reads on the next slot out. */
static dshot_reply_t modelFrame(ModelEsc *m, uint16_t v, bool telem, Fault fault,
                                uint16_t *value) {
	if (!m->present) return DSHOT_REPLY_NONE;
	if (m->silent) {
		m->silent--;
		return DSHOT_REPLY_NONE;
	}
	if (fault == Fault::Lost) return DSHOT_REPLY_NONE;
	bool damaged = fault == Fault::Damaged;
	if (damaged && m->fw != Fw::Am32) return DSHOT_REPLY_NONE;

	// AM32 answers what it captured before it decodes it.
	bool answer = m->answers;
	if (!damaged) {
		uint16_t run = (v >= 1 && v <= DSHOT_CMD_MAX && v == m->cmd) ? m->run + 1 : 1;
		if (v == 0 || v > DSHOT_CMD_MAX) run = 0;
		m->run = run;
		if (m->fw == Fw::Am32) am32Take(m, v);
		else                   bluejayTake(m, v, telem);
	}
	if (!answer || fault == Fault::NoReply) return DSHOT_REPLY_NONE;
	if (fault == Fault::BadReply) return DSHOT_REPLY_BAD;
	m->replies++;
	if (fault == Fault::EdtReply || (m->edt && m->replies % 30 == 0))
		*value = dshot_telem_value(DSHOT_TELEM_VOLTAGE, 66);
	else
		*value = dshot_telem_value(DSHOT_TELEM_ERPM, erpmPayload(m->periodUs));
	return DSHOT_REPLY_GOOD;
}

// ---------------------------------------------------------------- the bench

/** @brief Slots in the trace: 9 s at 1 kHz. */
#define BENCH_SLOTS 9000u

struct Bench {
	EscPump       pump;
	EscTelemetry  tel;
	ModelEsc      esc;
	bool          armed;          /**< core0's s_armed. */
	bool          uiAlive;        /**< Core0's heartbeat is fresh. */
	uint16_t      throttle;       /**< core0's s_throttle. */
	EscCmdRequest req;            /**< core0's s_cmdReq. */
	bool          kissRx;         /**< A KISS receiver runs. */
	uint32_t      ms;             /**< millis() of the slot about to run. */
	dshot_reply_t prev;           /**< The reply to the last frame. */
	uint16_t      prevValue;      /**< Its value, when good. */
	unsigned      slot;           /**< Slots run, the trace index. */
	uint16_t      sent[BENCH_SLOTS];   /**< Each slot's DShot value. */
	bool          telem[BENCH_SLOTS];  /**< Each slot's telemetry bit. */
	bool          cmdFrame[BENCH_SLOTS]; /**< The slot sent a command frame. */
	EscCmdReady   ready[BENCH_SLOTS];  /**< EscPump::ready after the slot. */
	EscCmdState   state[BENCH_SLOTS];  /**< EscCmdSeq::state after the slot. */
	Fault         fault[BENCH_SLOTS];  /**< The fate of the slot's frame. */
};

static Bench s_bench;   // most of it trace: static, not on the stack

/** @brief A pump bound at @p ms to a model ESC of @p fw powered up with it. */
static Bench *benchBoot(Fw fw, uint32_t ms) {
	Bench *b = &s_bench;
	memset(b, 0, sizeof(*b));
	b->ms = ms;
	b->uiAlive = true;
	modelPowerUp(&b->esc, fw);
	escPumpBind(&b->pump, &b->armed, &b->throttle);
	return b;
}

/** @brief One frame slot, as escTaskPoll() runs it. */
static void benchSlot(Bench *b) {
	dshot_telem_t t;
	memset(&t, 0, sizeof(t));
	dshot_reply_t r = b->prev;
	if (r == DSHOT_REPLY_GOOD &&
	    !dshot_telem_from_value(b->prevValue, escPumpDecodeEdt(&b->pump), &t))
		r = DSHOT_REPLY_BAD;
	escFoldReply(&b->tel, r, &t, 14, b->ms);

	EscSlot s = escPumpSlot(&b->pump, &b->tel, b->armed, b->uiAlive, b->throttle,
	                        &b->req, r, b->kissRx, b->ms);
	if (s.forgetEdt) escEdtForget(&b->tel);
	if (s.cmdTaken) b->req.cmd = 0;
	bool telem = false;
	uint16_t v = escFrameWire(&s.frame, &telem);
	escPumpFrameSent(&b->pump, &s.frame, b->ms);

	Fault f = Fault::None;
	if (b->slot < BENCH_SLOTS) {
		b->sent[b->slot] = v;
		b->telem[b->slot] = telem;
		b->cmdFrame[b->slot] = s.frame.sendCommand;
		b->ready[b->slot] = b->pump.ready;
		b->state[b->slot] = b->pump.seq.state;
		f = b->fault[b->slot];
	}
	b->prevValue = 0;
	b->prev = modelFrame(&b->esc, v, telem, f, &b->prevValue);
	b->slot++;
	b->ms++;
}

static void benchRun(Bench *b, unsigned n) {
	for (unsigned i = 0; i < n; i++) benchSlot(b);
}

/** @brief Run until the pump reads Ready; false if it does not within 5 s. */
static bool benchReady(Bench *b) {
	for (unsigned i = 0; i < 5000; i++) {
		if (b->slot && b->ready[b->slot - 1] == EscCmdReady::Ready) return true;
		benchSlot(b);
	}
	return false;
}

/** @brief Core0's request, stamped with this slot's clock. */
static void benchRequest(Bench *b, uint8_t cmd, bool save) {
	b->req.cmd = cmd;
	b->req.save = save;
	b->req.atMs = b->ms;
}

/**
 * @brief Run until no request waits and no sequence runs, at most 5 s, then
 *        10 slots more.
 */
static void benchIdle(Bench *b) {
	for (unsigned i = 0; i < 5000 && (b->req.cmd || b->pump.seq.cmd); i++)
		benchSlot(b);
	benchRun(b, 10);
}

/** @brief Slot @p from starts a run of exactly @p n frames of @p value. */
static bool runOf(const Bench *b, unsigned from, unsigned n, uint16_t value) {
	if (from > 0 && b->sent[from - 1] == value) return false;
	for (unsigned i = 0; i < n; i++)
		if (b->sent[from + i] != value) return false;
	return b->sent[from + n] != value;
}

/** @brief The @p n slots from @p from sent value 0. */
static bool zeros(const Bench *b, unsigned from, unsigned n) {
	for (unsigned i = 0; i < n; i++)
		if (b->sent[from + i] != 0) return false;
	return true;
}

/** @brief The first slot from @p from that sent @p value; BENCH_SLOTS if none. */
static unsigned firstOf(const Bench *b, unsigned from, uint16_t value) {
	for (unsigned i = from; i < b->slot && i < BENCH_SLOTS; i++)
		if (b->sent[i] == value) return i;
	return BENCH_SLOTS;
}

/** @brief Command frames between two slots. */
static unsigned commandFrames(const Bench *b, unsigned from, unsigned to) {
	unsigned n = 0;
	for (unsigned i = from; i < to && i < BENCH_SLOTS; i++) n += b->cmdFrame[i];
	return n;
}

/** @brief A direction hold, 8 then 12, from a settled ESC; its first slot. */
static unsigned benchHold(Bench *b, uint8_t cmd) {
	benchRequest(b, cmd, true);
	unsigned at = b->slot;
	benchIdle(b);
	return at;
}

static const char *fwName(Fw fw) {
	return fw == Fw::Am32 ? "AM32" : fw == Fw::Bluejay ? "Bluejay" : "BLHeli_S";
}

// ------------------------------------------------------------------- rules

/** @brief Which sequences the slot takes, and when core0 hands one over. */
static void testCmdAccept() {
	section("ESC commands: the requests the slot takes");

	int bad = 0;
	for (unsigned c = 0; c <= 47; c++) {
		bool alone = (c >= 1 && c <= 5) || c == 12;
		bool saved = c >= 7 && c <= 10;
		if (escCmdValid((uint8_t)c, false) != alone) bad++;
		if (escCmdValid((uint8_t)c, true) != saved) bad++;
	}
	checkInt("valid: beacons and 12 alone, 7 to 10 saved", bad, 0);

	const EscCmdReady all[] = {EscCmdReady::NoAnswer, EscCmdReady::Turning,
	                           EscCmdReady::Settling, EscCmdReady::Busy,
	                           EscCmdReady::Ready};
	const bool takes[] = {false, false, false, true, true};
	for (unsigned i = 0; i < 5; i++) {
		char what[64];
		snprintf(what, sizeof(what), "disarmed, readiness %u: %s", i,
		         takes[i] ? "taken" : "refused");
		checkTrue(what, escCmdAccept(8, true, false, all[i]) == takes[i]);
		snprintf(what, sizeof(what), "armed, readiness %u: refused", i);
		checkTrue(what, !escCmdAccept(8, true, true, all[i]));
	}
	checkTrue("a beacon at Ready is taken", escCmdAccept(1, false, false, EscCmdReady::Ready));
	checkTrue("7 without its save is refused",
	          !escCmdAccept(7, false, false, EscCmdReady::Ready));
	checkTrue("6 is refused", !escCmdAccept(6, false, false, EscCmdReady::Ready));
	checkTrue("13 is the enable's own", !escCmdAccept(13, false, false, EscCmdReady::Ready));
}

/** @brief The readiness rule at each boundary. */
static void testCmdReadiness() {
	section("ESC commands: the readiness at its boundaries");

	checkTrue("no answer: NoAnswer, before everything",
	          escCmdReadiness(false, 5000, 0, 9000, true) == EscCmdReady::NoAnswer);
	checkTrue("eRPM 1: Turning",
	          escCmdReadiness(true, 1, 1000, 9000, false) == EscCmdReady::Turning);
	checkTrue("Turning before Settling",
	          escCmdReadiness(true, 1, 8999, 9000, false) == EscCmdReady::Turning);
	checkTrue("1499 ms after link-up: Settling",
	          escCmdReadiness(true, 0, 1000, 2499, false) == EscCmdReady::Settling);
	checkTrue("1500 ms after: Ready",
	          escCmdReadiness(true, 0, 1000, 2500, false) == EscCmdReady::Ready);
	checkTrue("no link-up time: Settling",
	          escCmdReadiness(true, 0, 0, 9000, false) == EscCmdReady::Settling);
	checkTrue("Settling before Busy",
	          escCmdReadiness(true, 0, 1000, 2000, true) == EscCmdReady::Settling);
	checkTrue("settled and busy: Busy",
	          escCmdReadiness(true, 0, 1000, 2500, true) == EscCmdReady::Busy);
	checkTrue("link-up across the wrap: 1499 ms Settling",
	          escCmdReadiness(true, 0, 0xFFFFFF00u, 1243, false) == EscCmdReady::Settling);
	checkTrue("1500 ms Ready",
	          escCmdReadiness(true, 0, 0xFFFFFF00u, 1244, false) == EscCmdReady::Ready);

	// On the pump: 500 ms after the last eRPM answer NoAnswer, not at 499.
	Bench *b = benchBoot(Fw::Am32, 5000);
	checkTrue("AM32 settles to Ready", benchReady(b));
	benchRun(b, 400);
	b->esc.present = false;
	benchRun(b, 2);
	uint32_t last = b->tel.lastRpmMs;
	benchRun(b, last + 499 - b->ms);
	benchSlot(b);
	checkTrue("499 ms after the last answer: not NoAnswer",
	          b->ready[b->slot - 1] != EscCmdReady::NoAnswer);
	benchSlot(b);
	checkTrue("500 ms after: NoAnswer", b->ready[b->slot - 1] == EscCmdReady::NoAnswer);
	escPumpRelease(&b->pump);
	checkTrue("a teardown reads NoAnswer", b->pump.ready == EscCmdReady::NoAnswer);
}

/** @brief A direction hold's frames on the wire. */
static void testCmdFrames() {
	section("ESC commands: a direction hold's frames");

	Bench *b = benchBoot(Fw::Am32, 5000);
	b->kissRx = true;
	checkTrue("settled", benchReady(b));
	unsigned at = benchHold(b, DSHOT_CMD_SPIN_DIRECTION_2);
	const unsigned gap = ESC_CMD_GAP_MS;
	checkTrue("10 frames of 8 on the request's slot", runOf(b, at, 10, 8));
	checkTrue("then 300 frames of 0", runOf(b, at + 10, gap, 0));
	checkTrue("then 10 frames of 12", runOf(b, at + 10 + gap, 10, 12));
	checkTrue("then 300 frames of 0, 620 in all", zeros(b, at + 20 + gap, gap));
	bool bits = true;
	for (unsigned i = at; i < at + 20 + 2 * gap; i++)
		if (b->cmdFrame[i] && !b->telem[i]) bits = false;
	checkTrue("every command frame carries the telemetry bit", bits);
	checkInt("command frames in the sequence", (long)commandFrames(b, at, at + 620), 20);
	bool kissInBlock = false, kissInGap = false;
	for (unsigned i = at; i < at + 620; i++) {
		bool block = i < at + 10 || (i >= at + 310 && i < at + 320);
		if (!b->cmdFrame[i] && b->telem[i]) (block ? kissInBlock : kissInGap) = true;
	}
	checkTrue("no KISS request while a command is owed", !kissInBlock);
	checkTrue("KISS requests in the gaps", kissInGap);
	checkTrue("Busy from the first frame to the last gap's end",
	          b->ready[at] == EscCmdReady::Busy && b->ready[at + 619] == EscCmdReady::Busy);
	checkTrue("Ready after it", b->ready[at + 620] == EscCmdReady::Ready);
	checkTrue("Sending until the save's last frame",
	          b->state[at] == EscCmdState::Sending &&
	          b->state[at + 318] == EscCmdState::Sending);
	checkTrue("Sent from it", b->state[at + 319] == EscCmdState::Sent);
	checkInt("the command is 8", b->pump.seq.stateCmd, 8);
	checkTrue("AM32 reversed and saved", b->esc.reversed && b->esc.savedReversed);
	checkInt("AM32 acted on the sixth equal frame", (long)b->esc.actRun, 6);
	checkInt("and played beacon 2's tone on the save", (long)b->esc.tones[2], 1);

	// The I/O split: the wire value and the bit of each frame kind.
	EscFrame f = {true, 8, 0, false};
	bool telem = false;
	checkInt("a command frame's value", escFrameWire(&f, &telem), 8);
	checkTrue("with the telemetry bit", telem);
	f = {false, 0, 100, false};
	checkInt("a throttle frame's value", escFrameWire(&f, &telem), 147);
	checkTrue("without it", !telem);
	f = {false, 0, 0, true};
	checkInt("a KISS request at 0", escFrameWire(&f, &telem), 0);
	checkTrue("with it", telem);
}

/** @brief Each model takes the bench's sequences; six repeats would not do. */
static void testCmdModels() {
	section("ESC commands: AM32, Bluejay and BLHeli_S take the sequences");

	const Fw fws[] = {Fw::Am32, Fw::Bluejay, Fw::BlheliS};
	for (Fw fw : fws) {
		char what[80];
		Bench *b = benchBoot(fw, 5000);
		snprintf(what, sizeof(what), "%s: settles", fwName(fw));
		checkTrue(what, benchReady(b));
		benchHold(b, DSHOT_CMD_SPIN_DIRECTION_2);
		snprintf(what, sizeof(what), "%s: REVERSED sent", fwName(fw));
		checkTrue(what, b->pump.seq.state == EscCmdState::Sent);
		snprintf(what, sizeof(what), "%s: reversed and saved", fwName(fw));
		checkTrue(what, b->esc.reversed && b->esc.savedReversed && b->esc.saves == 1);
		snprintf(what, sizeof(what), "%s: acted on equal frame", fwName(fw));
		checkInt(what, (long)b->esc.actRun, fw == Fw::Am32 ? 6 : 7);
		benchHold(b, DSHOT_CMD_3D_MODE_ON);
		snprintf(what, sizeof(what), "%s: 3D on and saved", fwName(fw));
		checkTrue(what, b->esc.bidir && b->esc.savedBidir && b->esc.saves == 2);
		benchHold(b, DSHOT_CMD_SPIN_DIRECTION_1);
		benchHold(b, DSHOT_CMD_3D_MODE_OFF);
		snprintf(what, sizeof(what), "%s: NORMAL and 3D off, saved", fwName(fw));
		checkTrue(what, !b->esc.savedReversed && !b->esc.savedBidir && b->esc.saves == 4);
		benchRequest(b, DSHOT_CMD_BEEP3, false);
		benchIdle(b);
		snprintf(what, sizeof(what), "%s: BEEP 3 plays", fwName(fw));
		checkTrue(what, b->esc.tones[3] >= 1);
	}

	// Six repeats: AM32 acts, Bluejay and BLHeli_S do not. The bench sends
	// ten, so a slot cut to six fails the cases above.
	for (Fw fw : fws) {
		ModelEsc m;
		modelPowerUp(&m, fw);
		uint16_t v;
		for (int i = 0; i < 1100; i++) modelFrame(&m, 0, false, Fault::None, &v);
		for (int i = 0; i < 6; i++) modelFrame(&m, 8, true, Fault::None, &v);
		for (int i = 0; i < 10; i++) modelFrame(&m, 0, false, Fault::None, &v);
		char what[80];
		snprintf(what, sizeof(what), "6 frames of 8: %s %s", fwName(fw),
		         fw == Fw::Am32 ? "acts" : "does not");
		checkTrue(what, m.reversed == (fw == Fw::Am32));
	}

	// Bluejay drops a command frame without the telemetry bit.
	ModelEsc m;
	modelPowerUp(&m, Fw::Bluejay);
	uint16_t v;
	for (int i = 0; i < 10; i++) modelFrame(&m, 8, i != 5, Fault::None, &v);
	checkTrue("Bluejay: a frame without the bit restarts the count", !m.reversed);

	// A BLHeli_S 16.7 sends no eRPM: nothing is sent to it.
	Bench *b = benchBoot(Fw::BlheliS, 5000);
	b->esc.answers = false;
	benchRun(b, 3000);
	checkTrue("silent BLHeli_S: NoAnswer", b->pump.ready == EscCmdReady::NoAnswer);
	benchRequest(b, DSHOT_CMD_SPIN_DIRECTION_2, true);
	unsigned at = b->slot;
	benchIdle(b);
	checkTrue("silent BLHeli_S: the request expires",
	          b->pump.seq.state == EscCmdState::Expired);
	checkInt("and no command frame went out", (long)commandFrames(b, at, b->slot), 0);
}

/** @brief A lost or damaged frame leaves the count; one missing reply passes. */
static void testCmdLostFrames() {
	section("ESC commands: a lost or damaged frame, one missing reply");

	const Fw fws[] = {Fw::Am32, Fw::Bluejay, Fw::BlheliS};
	const Fault faults[] = {Fault::Lost, Fault::Damaged};
	int wrong = 0;
	for (Fw fw : fws)
		for (Fault fault : faults)
			for (unsigned blk = 0; blk < 2; blk++)
				for (unsigned k = 1; k <= 5; k++) {
					Bench *b = benchBoot(fw, 5000);
					benchReady(b);
					unsigned at = b->slot;
					b->fault[at + blk * (10 + ESC_CMD_GAP_MS) + k - 1] = fault;
					benchHold(b, DSHOT_CMD_SPIN_DIRECTION_2);
					bool ok = b->pump.seq.state == EscCmdState::Sent &&
					          b->esc.savedReversed;
					if (!ok) wrong++;
					if (!ok || (blk == 0 && k == 3)) {
						char what[96];
						snprintf(what, sizeof(what), "%s, frame %u of block %u %s: sent, saved",
						         fwName(fw), k, blk + 1,
						         fault == Fault::Lost ? "lost" : "damaged");
						checkTrue(what, ok);
					}
				}
	checkInt("lost or damaged frames 1 to 5, either block: wrong", wrong, 0);

	// The counts behind it, for frame 3 of the first block.
	Bench *b = benchBoot(Fw::Am32, 5000);
	benchReady(b);
	b->fault[b->slot + 2] = Fault::Damaged;
	benchRequest(b, DSHOT_CMD_SPIN_DIRECTION_2, true);
	benchRun(b, 7);
	checkInt("AM32, frame 3 damaged: it answered, no miss", b->pump.seq.misses, 0);
	b = benchBoot(Fw::Bluejay, 5000);
	benchReady(b);
	b->fault[b->slot + 2] = Fault::Damaged;
	benchRequest(b, DSHOT_CMD_SPIN_DIRECTION_2, true);
	benchRun(b, 7);
	checkInt("Bluejay, frame 3 damaged: no answer, one miss", b->pump.seq.misses, 1);
	b = benchBoot(Fw::Bluejay, 5000);
	benchReady(b);
	b->fault[b->slot + 2] = Fault::Lost;
	unsigned at = b->slot;
	benchHold(b, DSHOT_CMD_SPIN_DIRECTION_2);
	checkInt("Bluejay, frame 3 lost: acted on its seventh frame",
	         (long)b->esc.actRun, 7);
	checkTrue("all 10 frames went out", runOf(b, at, 10, 8));
}

/** @brief The check of each block's replies, frame by frame. */
static void testCmdReplyCheck() {
	section("ESC commands: the reply check, frame by frame");

	const unsigned blockAt[2] = {0, 10 + ESC_CMD_GAP_MS};
	int wrong = 0;
	for (unsigned blk = 0; blk < 2; blk++) {
		for (unsigned k = 1; k <= 10; k++) {
			for (int twice = 0; twice < 2; twice++) {
				if (twice && (k < 2 || k > 6)) continue;
				Bench *b = benchBoot(Fw::Am32, 5000);
				benchReady(b);
				unsigned at = b->slot;
				b->fault[at + blockAt[blk] + k - 1] = k % 2 ? Fault::NoReply : Fault::BadReply;
				if (twice) b->fault[at + blockAt[blk]] = Fault::NoReply;
				benchHold(b, DSHOT_CMD_SPIN_DIRECTION_2);
				bool stops = k == 6 || (twice && k <= 5);
				EscCmdState want = !stops ? EscCmdState::Sent
				                 : blk == 0 && k <= 5 ? EscCmdState::NotSent
				                 : EscCmdState::SaveLost;
				unsigned frames = !stops ? 20 : blk == 0 ? k : 10 + k;
				unsigned blockFrames = commandFrames(b, at + blockAt[blk],
				                                     at + blockAt[blk] + 10);
				bool ok = b->pump.seq.state == want &&
				          commandFrames(b, at, at + 2000) == frames &&
				          blockFrames == (stops ? k : 10);
				// A stopped block is followed by its gap, then nothing.
				if (stops && blk == 0)
					ok = ok && b->ready[at + k - 1 + ESC_CMD_GAP_MS] == EscCmdReady::Busy &&
					     b->ready[at + k + ESC_CMD_GAP_MS] == EscCmdReady::Ready;
				// NotSent: no ESC acted. SaveLost at frame 6: AM32 acted, unsaved.
				if (want == EscCmdState::NotSent) ok = ok && !b->esc.reversed;
				if (stops && blk == 0 && k == 6)
					ok = ok && b->esc.reversed && !b->esc.savedReversed;
				char what[96];
				snprintf(what, sizeof(what), "block %u, frame %u%s: %s", blk + 1, k,
				         twice ? " and frame 1" : "",
				         want == EscCmdState::Sent ? "sent"
				         : want == EscCmdState::NotSent ? "not sent" : "save lost");
				if (!ok) wrong++;
				if (!ok || k == 5 || k == 6) checkTrue(what, ok);
			}
		}
	}
	checkInt("cases with a wrong outcome", wrong, 0);

	// The allowance is per block: one miss in each passes.
	Bench *b = benchBoot(Fw::Am32, 5000);
	benchReady(b);
	unsigned at = b->slot;
	b->fault[at + 2] = b->fault[at + blockAt[1] + 3] = Fault::NoReply;
	benchHold(b, DSHOT_CMD_SPIN_DIRECTION_2);
	checkTrue("one miss in each block: sent", b->pump.seq.state == EscCmdState::Sent);
	// A reply before a block, to a 0 frame, is not the block's.
	b->fault[b->slot] = Fault::NoReply;
	benchSlot(b);
	at = b->slot;
	b->fault[at + blockAt[1] - 1] = Fault::NoReply;
	b->fault[at + 1] = b->fault[at + blockAt[1] + 1] = Fault::NoReply;
	benchHold(b, DSHOT_CMD_SPIN_DIRECTION_1);
	checkTrue("the reply to the frame before each block is not counted",
	          b->pump.seq.state == EscCmdState::Sent && !b->esc.savedReversed);
	// A sequence after a stopped one starts whole, its allowance whole.
	at = b->slot;
	b->fault[at] = b->fault[at + 1] = Fault::NoReply;
	benchHold(b, DSHOT_CMD_SPIN_DIRECTION_2);
	checkTrue("stopped at frame 2: not sent", b->pump.seq.state == EscCmdState::NotSent);
	at = b->slot;
	b->fault[at + 4] = Fault::NoReply;
	benchHold(b, DSHOT_CMD_SPIN_DIRECTION_2);
	checkTrue("the next, with one miss: sent, 20 frames",
	          b->pump.seq.state == EscCmdState::Sent &&
	          commandFrames(b, at, b->slot) == 20 && b->esc.savedReversed);

	// EDT replies are replies, eRPM or EDT alike.
	b = benchBoot(Fw::Am32, 5000);
	benchReady(b);
	at = b->slot;
	for (unsigned k = 0; k < 6; k++) {
		b->fault[at + k] = Fault::EdtReply;
		b->fault[at + blockAt[1] + k] = Fault::EdtReply;
	}
	uint32_t volts = b->tel.edtVoltsMs;
	benchHold(b, DSHOT_CMD_SPIN_DIRECTION_2);
	checkTrue("EDT replies to frames 1 to 6 of both blocks: sent",
	          b->pump.seq.state == EscCmdState::Sent);
	checkTrue("and they were read as EDT", b->tel.edtVoltsMs != volts);

	// Nothing is read after frame 6: an ESC silent for frames 7 to 10 of
	// both blocks is sent.
	b = benchBoot(Fw::Am32, 5000);
	benchReady(b);
	at = b->slot;
	for (unsigned k = 6; k < 10; k++) {
		b->fault[at + k] = Fault::NoReply;
		b->fault[at + blockAt[1] + k] = Fault::NoReply;
	}
	benchHold(b, DSHOT_CMD_SPIN_DIRECTION_2);
	checkTrue("no replies to frames 7 to 10: sent", b->pump.seq.state == EscCmdState::Sent);

	// 12 alone: one block, NotSent before frame 6, SaveLost at it.
	b = benchBoot(Fw::Am32, 5000);
	benchReady(b);
	at = b->slot;
	b->fault[at + 5] = Fault::NoReply;
	benchRequest(b, DSHOT_CMD_SAVE_SETTINGS, false);
	benchIdle(b);
	checkTrue("12 alone, frame 6 unanswered: save lost",
	          b->pump.seq.state == EscCmdState::SaveLost && b->pump.seq.stateCmd == 12);
	b = benchBoot(Fw::Am32, 5000);
	benchReady(b);
	at = b->slot;
	b->fault[at + 1] = b->fault[at + 3] = Fault::NoReply;
	benchRequest(b, DSHOT_CMD_SAVE_SETTINGS, false);
	benchIdle(b);
	checkTrue("12 alone, frames 2 and 4 unanswered: not sent",
	          b->pump.seq.state == EscCmdState::NotSent && b->esc.saves == 0);

	// A beacon's block is not checked. Bluejay is silent for 200 ms after
	// the tone it plays on the first frame.
	b = benchBoot(Fw::Bluejay, 5000);
	benchReady(b);
	at = b->slot;
	benchRequest(b, DSHOT_CMD_BEEP1, false);
	benchSlot(b);
	checkTrue("a beacon is sent from its first frame", b->state[at] == EscCmdState::Sent);
	benchIdle(b);
	checkTrue("all 6 frames, unanswered", runOf(b, at, BEEP_REPEATS, 1));
	checkTrue("still sent", b->pump.seq.state == EscCmdState::Sent);
	checkInt("one tone", (long)b->esc.tones[1], 1);
}

/** @brief The guards: arm, motor turning, the settle, the wait, a cancel. */
static void testCmdGuards() {
	section("ESC commands: arm, motor, settle, wait");

	// An arm drops the sequence wherever it stands.
	struct ArmCase { unsigned at; EscCmdState want; const char *what; };
	const ArmCase cases[] = {
		{2, EscCmdState::NotSent, "arm at frame 3 of the first block: not sent"},
		{6, EscCmdState::SaveLost, "arm after its frame 6: save lost"},
		{100, EscCmdState::SaveLost, "arm in the first gap: save lost"},
		{312, EscCmdState::SaveLost, "arm in the save block: save lost"},
		{400, EscCmdState::Sent, "arm in the last gap: sent stays"},
	};
	for (const ArmCase &c : cases) {
		Bench *b = benchBoot(Fw::Am32, 5000);
		benchReady(b);
		unsigned at = b->slot;
		benchRequest(b, DSHOT_CMD_SPIN_DIRECTION_2, true);
		benchRun(b, c.at);
		b->armed = true;
		b->throttle = 100;
		benchRun(b, 700);
		bool ok = b->pump.seq.state == c.want && b->pump.seq.cmd == 0 &&
		          commandFrames(b, at + c.at, b->slot) == 0 &&
		          b->sent[at + c.at] == 147;
		checkTrue(c.what, ok);
	}

	// An arm drops a waiting request; core0 refuses one while armed.
	Bench *b = benchBoot(Fw::Am32, 5000);
	benchRun(b, 100);
	benchRequest(b, DSHOT_CMD_BEEP1, false);
	benchRun(b, 10);
	checkTrue("settling: the request waits", b->req.cmd == 1);
	b->armed = true;
	benchSlot(b);
	checkTrue("an arm drops it: expired",
	          b->req.cmd == 0 && b->pump.seq.state == EscCmdState::Expired &&
	          b->pump.seq.stateCmd == 1);
	b->armed = false;
	unsigned at = b->slot;
	benchRun(b, 3000);
	checkTrue("no beacon after the disarm", firstOf(b, at, 1) == BENCH_SLOTS);

	// eRPM not 0: held, Turning; it goes once the motor stops.
	b = benchBoot(Fw::Am32, 5000);
	benchReady(b);
	b->esc.edt = false;     // every reply eRPM: the stop is read on one slot
	b->esc.periodUs = 2000;
	benchRun(b, 2);
	checkTrue("motor turning: Turning", b->pump.ready == EscCmdReady::Turning);
	benchRequest(b, DSHOT_CMD_SPIN_DIRECTION_2, true);
	benchRun(b, 500);
	checkTrue("the request is held", b->req.cmd != 0 && b->pump.seq.cmd == 0);
	b->esc.periodUs = 0;
	at = b->slot;
	benchRun(b, 2);
	checkTrue("motor stopped: it starts on the slot that reads eRPM 0",
	          b->sent[at + 1] == 8 && b->sent[at] != 8);
	benchIdle(b);
	checkTrue("and is sent", b->pump.seq.state == EscCmdState::Sent && b->esc.savedReversed);

	// The settle: an AM32 plugged in with a request waiting starts 1500 ms
	// after it first answers, not 1499 ms; AM32 has armed by then.
	b = benchBoot(Fw::Am32, 5000);
	b->esc.present = false;
	benchRun(b, 3000);
	modelPowerUp(&b->esc, Fw::Am32);
	unsigned plug = b->slot;
	benchSlot(b);
	benchSlot(b);
	uint32_t up = b->pump.edt.linkUpSinceMs;
	checkInt("link-up on the first slot that reads its answer", (long)(up - 5000 - plug), 1);
	benchRequest(b, DSHOT_CMD_SPIN_DIRECTION_2, true);
	benchRun(b, up + 1500 - b->ms);
	checkTrue("1499 ms after link-up: Settling, waiting",
	          b->ready[b->slot - 1] == EscCmdReady::Settling && b->req.cmd != 0);
	at = b->slot;
	benchSlot(b);
	checkTrue("1500 ms: the first frame of 8", b->sent[at] == 8);
	benchIdle(b);
	checkTrue("AM32 armed in time and saved", b->esc.savedReversed);

	// The wait: a request taken at Busy, during beacon 5's gap, whose ESC
	// then goes; it expires at 2000 ms, not 1999 ms.
	b = benchBoot(Fw::Am32, 5000);
	benchReady(b);
	benchRequest(b, DSHOT_CMD_BEEP5, false);
	benchRun(b, 20);
	checkTrue("beacon 5's gap: Busy", b->pump.ready == EscCmdReady::Busy);
	benchRequest(b, DSHOT_CMD_BEEP1, false);
	uint32_t asked = b->ms;
	b->esc.present = false;
	benchRun(b, asked + 1999 - b->ms);
	benchSlot(b);
	checkTrue("1999 ms: still waiting", b->req.cmd == 1);
	benchSlot(b);
	checkTrue("2000 ms: expired",
	          b->req.cmd == 0 && b->pump.seq.state == EscCmdState::Expired &&
	          b->pump.seq.stateCmd == 1);
	checkTrue("beacon 1 never went out", firstOf(b, 0, 1) == BENCH_SLOTS);

	// A request stamped after the slot's clock is new, not 2^32 - 1 ms old.
	b = benchBoot(Fw::Am32, 5000);
	benchRun(b, 10);
	b->req = {DSHOT_CMD_BEEP1, false, b->ms + 1};
	benchSlot(b);
	checkTrue("a stamp 1 ms ahead: waiting", b->req.cmd == 1);

	// A cancel removes the request before it starts: nothing goes out.
	b = benchBoot(Fw::Am32, 5000);
	benchRun(b, 100);
	benchRequest(b, DSHOT_CMD_SPIN_DIRECTION_2, true);
	benchRun(b, 500);
	b->req.cmd = 0;
	at = b->slot;
	benchRun(b, 3000);
	checkTrue("cancelled: no command frame but the enable's",
	          firstOf(b, at, 8) == BENCH_SLOTS && firstOf(b, at, 12) == BENCH_SLOTS &&
	          b->pump.seq.state == EscCmdState::Idle);

	// Core0 hangs: the sequence finishes.
	b = benchBoot(Fw::Am32, 5000);
	benchReady(b);
	benchRequest(b, DSHOT_CMD_3D_MODE_ON, true);
	at = b->slot;
	benchSlot(b);
	b->uiAlive = false;
	benchIdle(b);
	checkTrue("heartbeat lapsed: the sequence finishes",
	          runOf(b, at, 10, 10) && runOf(b, at + 310, 10, 12) &&
	          b->pump.seq.state == EscCmdState::Sent && b->esc.savedBidir);
}

/** @brief The EDT enable and the sequence share the wire in turn. */
static void testCmdEdtOrder() {
	section("ESC commands: the EDT enable first, none inside a sequence");

	// An ESC that does not take the enable: it is retried every second.
	Bench *b = benchBoot(Fw::BlheliS, 5000);
	benchReady(b);
	while (b->pump.edt.reps == 0) benchSlot(b);
	unsigned burst = b->slot - 1;
	benchRequest(b, DSHOT_CMD_SPIN_DIRECTION_2, true);
	benchSlot(b);
	checkTrue("during an enable: Busy, the request waits",
	          b->ready[b->slot - 1] == EscCmdReady::Busy && b->req.cmd != 0);
	benchIdle(b);
	checkTrue("the enable whole, 10 frames", runOf(b, burst, EDT_ENABLE_REPEATS, 13));
	checkTrue("the sequence on the next frame",
	          runOf(b, burst + EDT_ENABLE_REPEATS, 10, 8));
	unsigned seqAt = burst + EDT_ENABLE_REPEATS;
	// The next retry falls due 1000 ms after the burst's last frame, after
	// the sequence's 620 ms.
	benchRun(b, 1500);
	checkInt("the next enable at its own time, after the sequence",
	         (long)firstOf(b, seqAt, 13),
	         (long)(burst + EDT_RETRY_MS + EDT_ENABLE_REPEATS - 1));

	// A sequence at the retry's due slot: the enable follows the last gap.
	b = benchBoot(Fw::BlheliS, 5000);
	benchReady(b);
	while (b->pump.edt.reps == 0) benchSlot(b);
	unsigned due = b->slot - 1 + EDT_RETRY_MS + EDT_ENABLE_REPEATS - 1;
	benchRun(b, due - 1 - b->slot);
	benchRequest(b, DSHOT_CMD_3D_MODE_ON, true);
	benchRun(b, 1000);
	checkTrue("a sequence one slot before the retry", runOf(b, due - 1, 10, 10));
	checkInt("the enable after its last gap", (long)firstOf(b, due - 1, 13),
	         (long)(due - 1 + 620));
}

/** @brief A beacon's gap, and the sequence across a millis() wrap. */
static void testCmdGapsAndWrap() {
	section("ESC commands: a beacon's gap, across a millis() wrap");

	const unsigned gaps[5] = {260, 260, 280, 280, 1020};
	for (unsigned n = 1; n <= 5; n++) {
		Bench *b = benchBoot(Fw::Am32, 5000);
		benchReady(b);
		unsigned at = b->slot;
		benchRequest(b, (uint8_t)n, false);
		benchIdle(b);
		char what[64];
		snprintf(what, sizeof(what), "beacon %u: 6 frames, %u of 0, Busy", n, gaps[n - 1]);
		checkTrue(what, runOf(b, at, BEEP_REPEATS, (uint16_t)n) &&
		                zeros(b, at + BEEP_REPEATS, gaps[n - 1]) &&
		                b->ready[at + BEEP_REPEATS + gaps[n - 1] - 1] == EscCmdReady::Busy &&
		                b->ready[at + BEEP_REPEATS + gaps[n - 1]] == EscCmdReady::Ready);
	}

	// A hold whose last gap crosses millis() 0.
	Bench *b = benchBoot(Fw::Am32, 0u - 2000u);
	checkTrue("wrap: settles", benchReady(b));
	unsigned at = benchHold(b, DSHOT_CMD_SPIN_DIRECTION_2);
	checkTrue("wrap: the sequence crossed 0", (uint32_t)(b->ms - 1) < 1000u);
	checkTrue("wrap: 10 of 8, 300 of 0, 10 of 12",
	          runOf(b, at, 10, 8) && runOf(b, at + 310, 10, 12));
	checkTrue("wrap: sent and saved",
	          b->pump.seq.state == EscCmdState::Sent && b->esc.savedReversed);

	// A wait across it: expired 2000 ms after the stamp, not before.
	b = benchBoot(Fw::Am32, 0u - 1000u);
	b->esc.present = false;
	benchRequest(b, DSHOT_CMD_BEEP1, false);
	benchRun(b, 2000);
	checkTrue("wrap: 1999 ms waiting", b->req.cmd == 1);
	benchSlot(b);
	checkTrue("wrap: 2000 ms expired", b->req.cmd == 0 &&
	                                   b->pump.seq.state == EscCmdState::Expired);
}

/** @brief A bind or a teardown, and a drain frame, stop a sequence. */
static void testCmdBindAndDrain() {
	section("ESC commands: a bind, a teardown and a drain");

	Bench *b = benchBoot(Fw::Am32, 5000);
	benchReady(b);
	benchRequest(b, DSHOT_CMD_SPIN_DIRECTION_2, true);
	benchRun(b, 3);
	escPumpBind(&b->pump, &b->armed, &b->throttle);
	checkTrue("a bind at frame 3: not sent, nothing ready",
	          b->pump.seq.state == EscCmdState::NotSent && b->pump.seq.cmd == 0 &&
	          b->pump.ready == EscCmdReady::NoAnswer);

	b = benchBoot(Fw::Am32, 5000);
	benchReady(b);
	benchRequest(b, DSHOT_CMD_SPIN_DIRECTION_2, true);
	benchRun(b, 200);
	escPumpRelease(&b->pump);
	checkTrue("a teardown in the first gap: save lost",
	          b->pump.seq.state == EscCmdState::SaveLost && b->pump.seq.cmd == 0);

	// A drain frame inside a block breaks it.
	b = benchBoot(Fw::Am32, 5000);
	benchReady(b);
	benchRequest(b, DSHOT_CMD_SPIN_DIRECTION_2, true);
	benchRun(b, 4);
	EscSlot d = escDrainSlot();
	escPumpFrameSent(&b->pump, &d.frame, b->ms);
	checkTrue("a drain frame at frame 5: not sent",
	          b->pump.seq.state == EscCmdState::NotSent && b->pump.seq.cmd == 0);
}

void runEscCmdTests() {
	testCmdAccept();
	testCmdReadiness();
	testCmdFrames();
	testCmdModels();
	testCmdLostFrames();
	testCmdReplyCheck();
	testCmdGuards();
	testCmdEdtOrder();
	testCmdGapsAndWrap();
	testCmdBindAndDrain();
}
