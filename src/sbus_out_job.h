/**
 * @file sbus_out_job.h
 * @brief The S.BUS output's core1 job: UART0 sends frames on GP0, the pad's
 *        output inverted, while the pin claim holds it.
 *
 * S.BUS is Futaba's serial receiver bus: an inverted UART (universal
 * asynchronous receiver-transmitter) at 100,000 baud, 8 data bits, even
 * parity and 2 stop bits (8E2), 16 channels of 11 bits and two digital ones
 * in a 25-byte frame. The job sends one frame every
 * @ref SBUS_OUT_PERIOD_US, 14 ms, from UART0's TX on GP0 to a flight
 * controller (FC). rcbench's sbus_encode() (lib/rcbench/protocols/sbus/)
 * packs each frame.
 *
 * | State | GP0 | Leaves it |
 * |---|---|---|
 * | Probe | rcbench's escwire line: on the pull-up, a burst of 3 greetings each second | a cycle with no answer: SAFE; the release |
 * | SAFE | the UART bound and idle: low, no frames | Start |
 * | RUNNING | a frame every 14 ms | Safe, a heartbeat lapse, a refused frame, the release |
 * | Tripped | SAFE, latched after a heartbeat lapse while RUNNING | Start, or a new bind |
 *
 * Every decision about the frames is in sbusOutCoreTake() and
 * sbusOutCoreStep(), pure functions the host suite runs as they ship; the
 * job around them copies the command block, loads the TX FIFO
 * (first-in, first-out buffer) and publishes the status.
 *
 * **The bootloader probe** (docs/design/sbus-output.md, decision 1) runs on
 * every bind, before GP0 reaches the UART: ESC SETTINGS' connect cycle
 * (greet_cycle.h), @ref ESC_GREET_QUIET_MS on the pull-up, then
 * @ref ESC_GREET_BURST greetings, on the line driver of ESC SETTINGS' job
 * (escwire_port.h), rcbench's first contact for a greeting, each answer
 * read by blAnswerClassify(). An EFM8 bootloader's answer (Bluejay,
 * BLHeli_S), a piece of one, or an AM32 bootloader's answer is an ESC on
 * GP0: the cycle repeats, and only a cycle whose greetings all went out and
 * came back silent, or with bytes that are no bootloader's answer, lets the
 * job go on to the UART. No command is taken during the probe, and the
 * switch takes the command block's seq as acknowledged, so a START sent
 * before it is never taken: START is impossible until a probe cycle with no
 * answer has passed in this bind, and each bind probes again.
 *
 * **The bind**, in this order: SIO's output enable for GP0 off and its output
 * low, so whatever an earlier owner left driving, SIO drives nothing;
 * uart_init() at 100,000 baud, refused with CORE1_JOB_CLOCK more than 1 % off
 * (sbusBaudOk()); 8E2 and the receiver off (UARTCR.RXE), since no pad carries
 * UART0's RX during this claim; then the escwire line on GP0, which parks the
 * pad on its pull-down, and the pull-up at once (`EW_LINE_UP`). The UART runs
 * from the bind with no pad on it, so a clock it cannot use refuses the bind
 * before the probe. A refused bind leaves GP0's function as it was, SIO
 * driving nothing, and the pull-up on: the level the pump's teardown and the
 * probe hold, so nothing pulls the line low.
 *
 * **The switch**, once a probe cycle passes: the escwire line closed, which
 * leaves GP0 an SIO input, low, on the pull-up (escwirePortClose()); the
 * pull-down, which holds the line at the UART's idle level from here on; then
 * GP0 to the UART with its output inverted. gpio_set_function() writes the
 * whole control register and zeroes OUTOVER (SDK 2.1.1, gpio.c), so the
 * SDK's two calls, function then gpio_set_outover(), would put the UART's
 * high idle on the pad between them. The job does gpio_set_function()'s three
 * steps itself: input enable on and output disable off in the pad register,
 * the control register with FUNCSEL `GPIO_FUNC_UART` and OUTOVER
 * `GPIO_OVERRIDE_INVERT` in one write, then pad isolation cleared.
 *
 * **The release** during the probe cuts a greeting on the wire and closes
 * the escwire line: GP0 stays an SIO input on the pull-up, the probe's level,
 * and never reaches the UART; then uart_deinit(). After the switch it loads
 * no new frame and returns false while UARTFR.BUSY is set, so the frame on
 * the wire completes, at most 3,000 us after it was loaded;
 * @ref SBUS_OUT_RELEASE_MAX_MS is the backstop for a BUSY that never
 * clears. Then SIO's output enable for GP0 off and its output low, the pad
 * to SIO, which zeroes OUTOVER, the pull-down, and only then uart_deinit():
 * the pad leaves the UART before the UART stops, so the line never shows
 * the UART's own level. The status is cleared.
 *
 * The job binds GP0 only: SIG or AUX, whichever is GP0. GP1 keeps its
 * function. With neither role on GP0 the bind refuses with
 * CORE1_JOB_NO_GP0 and takes nothing.
 *
 * Nothing opens the job: no screen claims PinUser::SbusOut and nothing in
 * `src/` sends a command. The screen, a later slice of the note, offers
 * START only while sbusOutProbePassed() holds.
 *
 * Not run on hardware: the inversion on the pin, the level at the bind, the
 * switch and the release, the probe's greetings and an ESC's answer to them,
 * and the frames on a scope are not measured.
 *
 * @see docs/design/sbus-output.md
 */

#pragma once

#include <stdint.h>

#include "bl_answer.h"
#include "core1_job.h"
#include "sbus.h"

/** @brief UART0's transmit pin, the one GPIO the job binds. */
#define SBUS_OUT_GPIO 0u
/** @brief One frame start to the next, us. */
#define SBUS_OUT_PERIOD_US 14000u
/**
 * @brief The shortest time from one frame start to the next, us: 7 ms, the
 *        other period rcbench names (sbus.h), so a gap of 4,000 us at least.
 *
 * A pass late by less than a period sends at once and keeps the deadlines
 * on their 14,000 us grid, which brings the next frame closer; this bound
 * keeps it 7,000 us away, as it does for a START soon after a STOP.
 */
#define SBUS_OUT_MIN_US 7000u
/** @brief One frame on the wire: 25 bytes of 12 bits at 10 us, us. */
#define SBUS_OUT_FRAME_US 3000u
/**
 * @brief The release lets GP0 go this long after its first pass even while
 *        UARTFR.BUSY stays set, ms: the backstop for a UART that never
 *        finishes. The last frame takes at most @ref SBUS_OUT_FRAME_US.
 */
#define SBUS_OUT_RELEASE_MAX_MS 10u
/** @brief The lowest raw value the output sends: 1000 us (sbus.h). */
#define SBUS_OUT_RAW_LO SBUS_RAW_1000US
/** @brief The highest raw value the output sends: 2000 us (sbus.h). */
#define SBUS_OUT_RAW_HI SBUS_RAW_2000US
/** @brief The centre of the sticks in the START frame: 1500 us, sbus_from_us(1500). */
#define SBUS_OUT_RAW_MID 992u
/** @brief The throttle channel's index by default: channel 3, Betaflight's map `AETR1234`. */
#define SBUS_OUT_THR_DEFAULT 2u
/**
 * @brief Bytes the probe takes after a greeting, at most: the AM32
 *        transport's handshake reads as many (am32_bl.cpp). An answer is 9.
 */
#define SBUS_OUT_PROBE_RX_MAX 40u

/** @brief Where the job is in a bind. */
enum class SbusOutPhase : uint8_t {
	Off,    /**< Not bound. */
	Probe,  /**< The bootloader probe runs on GP0; no command is taken. */
	Uart,   /**< A probe cycle passed with no answer: GP0 is UART0's, SAFE or RUNNING. */
};

/** @brief What a command asks of the job. */
enum class SbusOutCmdKind : uint8_t {
	Safe,   /**< STOP: no frame after the one on the wire. */
	Start,  /**< HOLD TO START: the START frame from the next pass, whatever was sent before. */
	Frame,  /**< New channels and flags while RUNNING; taken and ignored while SAFE. */
};

/** @brief One command from core0. */
struct SbusOutCmd {
	uint16_t       seq;        /**< Set by sbusOutJobCommand(); core1 echoes it. */
	SbusOutCmdKind kind;       /**< What to do. */
	uint16_t       channel[SBUS_CHANNELS];  /**< Raw, for SbusOutCmdKind::Frame; core1 clamps them. */
	bool           ch17;       /**< Digital channel 17, for SbusOutCmdKind::Frame. */
	bool           ch18;       /**< Digital channel 18, for SbusOutCmdKind::Frame. */
	bool           failsafe;   /**< The failsafe bit, for SbusOutCmdKind::Frame. */
	bool           frameLost;  /**< The frame-lost bit, for SbusOutCmdKind::Frame. */
	uint8_t        thrCh;      /**< The throttle channel's index, 0 to 3; taken by SbusOutCmdKind::Start. */
	uint16_t       ceilRaw;    /**< The throttle ceiling, raw, sbusOutCeilRaw(); taken by SbusOutCmdKind::Start, never above MAX THROTTLE's. */
};

/**
 * @brief What core1 publishes for core0. The release clears it, but for the
 *        answers' counts and their time, which run from the board's start.
 */
struct SbusOutStatus {
	SbusOutPhase phase;        /**< Off, the probe, or the UART. */
	bool         answered;     /**< A bootloader answered the probe in this bind: START waits for a cycle with none. */
	bool         efm8;         /**< That answer, or one of them, was an EFM8 bootloader's or a piece of one. */
	uint16_t     signature;    /**< The last EFM8 answer's signature, 0xE8B2 and its kin; 0 for a piece or none. */
	uint32_t     cycles;       /**< Probe cycles ended in this bind. */
	uint16_t     answerSeq;    /**< Answers of either kind since the board's start; never cleared. */
	uint16_t     efm8Seq;      /**< EFM8 answers and pieces since the board's start; never cleared. */
	uint32_t     answerMs;     /**< millis() of the pass that read the last answer; never cleared. */
	uint16_t     ackSeq;       /**< Seq of the last command taken. */
	bool         running;      /**< RUNNING: frames go out. */
	bool         tripped;      /**< A heartbeat lapse stopped the frames; cleared by START and by a bind. */
	bool         refused;      /**< sbus_encode() refused a frame and the frames stopped; cleared by START and by a bind. */
	uint32_t     frames;       /**< Frames loaded since START. */
	uint32_t     lastFrameMs;  /**< millis() of the pass that loaded the last frame; 0 before the first. */
	sbus_frame_t sent;         /**< The frame last loaded: what the line carries. */
};

/**
 * @brief Core1's state for the frames. Plain data, so the host suite drives
 *        sbusOutCoreTake() and sbusOutCoreStep() directly.
 */
struct SbusOutCore {
	sbus_frame_t frame;     /**< What the next frame carries, clamped. */
	bool         running;   /**< RUNNING. */
	bool         tripped;   /**< Latched by a heartbeat lapse while RUNNING. */
	bool         refused;   /**< Latched by a frame sbus_encode() refused. */
	uint8_t      thrCh;     /**< The throttle channel's index, from Start. */
	uint16_t     ceilRaw;   /**< The throttle ceiling, raw, from Start: 172 to MAX THROTTLE's ceiling. */
	uint16_t     ackSeq;    /**< Seq of the last command taken. */
	uint32_t     nextUs;    /**< When the next frame is due. */
	bool         sentAny;   /**< A frame was loaded since the reset. */
	uint32_t     lastUs;    /**< When the last frame was loaded. */
	uint32_t     frames;    /**< Frames loaded since Start. */
};

/**
 * @brief Put @p c into SAFE: no frames, no trip, no refusal, commands up to
 *        @p ackSeq treated as taken. Pure.
 * @param[out] c  The state.
 * @param ackSeq  Seq of the command block at the bind, so no command sent
 *                before it is taken.
 */
void sbusOutCoreReset(SbusOutCore *c, uint16_t ackSeq);

/**
 * @brief The START frame: the throttle channel at raw 172, the other three
 *        of channels 1 to 4 at raw 992, channels 5 to 16 at raw 172, CH17,
 *        CH18 and both flags off. Pure.
 * @param[out] f  The frame.
 * @param thrCh   The throttle channel's index, 0 to 3.
 */
void sbusOutStartFrame(sbus_frame_t *f, uint8_t thrCh);

/**
 * @brief Take a command. Pure.
 *
 * A command whose seq equals the last one taken is not new and changes
 * nothing. A refused command is still acknowledged.
 *
 * - **Safe** ends the frames; the frame on the wire completes.
 * - **Start** is refused while @p uiAlive is false and for a throttle index
 *   above 3. Otherwise it takes the throttle channel and the ceiling: the
 *   command's, clamped to raw 172 to @p ceilMax, so a wrong ceiling from the
 *   screen never passes MAX THROTTLE's; it sets the START frame (sbusOutStartFrame()) whatever
 *   was sent before, clears the trip, the refusal and the frame count, and
 *   makes a frame due at @p nowUs.
 * - **Frame** while RUNNING replaces the channels and flags, every channel
 *   clamped to raw 172 to 1811 and the throttle channel taken at Start to the
 *   ceiling. The throttle channel and the ceiling of a Frame are ignored.
 *   While SAFE it is taken and moves nothing.
 *
 * @param[in,out] c The state.
 * @param cmd       The command block.
 * @param nowUs     micros() of the pass.
 * @param uiAlive   core1UiAlive().
 * @param ceilMax   The highest ceiling Start takes, raw:
 *                  sbusOutCeilRaw(`settings()->maxThrottle`), which the job
 *                  reads on core1. Above 1811 reads as 1811.
 * @return True when the command was new.
 */
bool sbusOutCoreTake(SbusOutCore *c, const SbusOutCmd *cmd, uint32_t nowUs,
                     bool uiAlive, uint16_t ceilMax);

/**
 * @brief One pass: the heartbeat and the frame schedule. Pure.
 *
 * A lapsed heartbeat while RUNNING ends the frames and latches the trip. While
 * RUNNING a frame is due at the deadline: the first on the pass that took
 * Start, each next one @ref SBUS_OUT_PERIOD_US after the one before, and none
 * less than @ref SBUS_OUT_MIN_US after the last one loaded. A due frame waits
 * while @p txEmpty is false. A pass a period or more late sends at once and
 * sets the next deadline a period from that pass, so a stall never makes a
 * burst. A due frame is encoded with sbus_encode() into @p out; a frame the
 * encoder refuses ends the frames and latches SbusOutCore::refused.
 *
 * @param[in,out] c The state.
 * @param nowUs     micros() of the pass. The clock may wrap.
 * @param uiAlive   core1UiAlive().
 * @param txEmpty   UARTFR.TXFE: the TX FIFO is empty.
 * @param[out] out  25 bytes, `SBUS_FRAME_BYTES` of sbus.h: the frame,
 *                  written only when the result is true.
 * @return True when @p out holds a frame to load now.
 */
bool sbusOutCoreStep(SbusOutCore *c, uint32_t nowUs, bool uiAlive, bool txEmpty,
                     uint8_t *out);

/**
 * @brief The throttle channel's ceiling for MAX THROTTLE, raw:
 *        172 + 1639 x @p maxThrottle / 2000, rounded down. Pure.
 * @param maxThrottle `settings()->maxThrottle`, 0 to 2000; above 2000 reads
 *                    as 2000.
 * @return 253 at 100, 499 at 400, 991 at 1000, 1811 at 2000.
 */
uint16_t sbusOutCeilRaw(uint16_t maxThrottle);

/**
 * @brief The roles an S.BUS output claim asks for: the role that is GP0. Pure.
 *
 * | Wiring | Roles |
 * |---|---|
 * | SIG GP0, AUX GP1 | PIN_SIG |
 * | SIG GP0, AUX GP0 (KISS off) | PIN_SIG |
 * | SIG GP1, AUX GP0 | PIN_SIG \| PIN_AUX |
 * | SIG GP1, AUX GP1 (KISS off) | 0: no claim; S.BUS needs GP0 |
 *
 * @param sigGpio escTaskDshotPin().
 * @param auxGpio escTaskKissPin().
 * @return PIN_SIG, PIN_SIG | PIN_AUX, or 0 (pin_claim.h).
 */
uint8_t sbusOutClaimRoles(uint8_t sigGpio, uint8_t auxGpio);

/**
 * @brief Read one greeting's answer. Pure.
 *
 * An answer is a bootloader on GP0: an EFM8 bootloader's (@ref
 * BlAnswerKind::Efm8), a reply that holds a piece of one
 * (@ref BlAnswer::fragment), or an AM32 bootloader's. Bytes that are none
 * of these are no answer, as ESC SETTINGS reads them.
 *
 * @param a        blAnswerClassify() of the bytes.
 * @param[out] efm8 Set when the answer is an EFM8 bootloader's or a piece of one.
 * @return True for an answer.
 */
bool sbusOutProbeAnswer(const BlAnswer &a, bool *efm8);

/**
 * @brief Send a command. Core0.
 *
 * One command is held at a time, and a new one replaces a command core1 has
 * not taken, with one exception: a Safe not yet taken stays, and a command
 * sent after it is dropped. A STOP is never lost to a later command. Nothing
 * in `src/` calls it yet. Core1 takes no command until the probe has passed
 * (@ref SbusOutPhase::Uart), and none sent before that.
 *
 * @param c The command; its seq is ignored.
 * @return The seq of the command held, which SbusOutStatus::ackSeq echoes
 *         once core1 has taken it.
 */
uint16_t sbusOutJobCommand(const SbusOutCmd *c);

/**
 * @brief Read the job's status. Core0, under the job's critical section.
 * @param[out] out The status as core1 last published it.
 */
void sbusOutJobStatus(SbusOutStatus *out);

/** @brief The probe as the screen shows it. */
enum class SbusOutProbeState : uint8_t {
	Off,       /**< The job is not bound. */
	Checking,  /**< The probe runs and no bootloader has answered in this bind. */
	Answered,  /**< A bootloader answered in this bind; the probe repeats. */
	Passed,    /**< A cycle passed with no answer: GP0 is UART0's, START may go. */
};

/**
 * @brief The gate: true once a probe cycle with no answer has passed in this
 *        bind and GP0 is UART0's. Core0. The screen offers START only while
 *        it holds; core1 takes no command before it either.
 * @return @ref SbusOutPhase::Uart in the published status.
 */
bool sbusOutProbePassed();

/**
 * @brief Where the probe is, for the screen. Core0, once per UI frame by the
 *        screen that holds the claim.
 *
 * Once an EFM8 bootloader, or a piece of its answer, has answered in this
 * claim (sbusOutProbeEfm8()), it calls pinClaimWantLeavePage(): SIG returns
 * to DShot only after the leave page, whose first step asks for the ESC's
 * battery to come off (docs/design/bluejay-blheli-s.md, decision 8).
 *
 * @return Off, Checking, Answered or Passed.
 */
SbusOutProbeState sbusOutProbePoll();

/**
 * @brief An EFM8 bootloader, or a piece of its answer, answered the probe
 *        since the last sbusOutProbeRestart(): the leave page takes decision
 *        8's first step, `1 UNPLUG THE ESC BATTERY, THEN LEAVE.` Core0.
 * @return True after such an answer.
 */
bool sbusOutProbeEfm8();

/**
 * @brief MENU's run cue, `POWER-CYCLE THE ESC TO RUN IT`, ORed with ESC
 *        SETTINGS' and the USB bridge's (ui_menu.cpp). Core0.
 *
 * Set by an answer of either kind to the probe: the ESC waits in its
 * bootloader. Cleared by an eRPM frame stamped after the last answer, which
 * only the ESC's firmware sends, and by sbusOutProbeRestart() after an EFM8
 * answer. Kept after the claim ends.
 *
 * @param lastRpmMs EscTelemetry::lastRpmMs.
 * @return True while the cue stands.
 */
bool sbusOutRunCue(uint32_t lastRpmMs);

/**
 * @brief RESTART DSHOT on the leave page. Core0. After an EFM8 answer the
 *        page's first step took the ESC's battery off: the EFM8 mark and
 *        the run cue go. After an AM32 answer alone nothing goes: the page
 *        does not ask for the ESC's battery, and the cue stays until an eRPM
 *        frame.
 */
void sbusOutProbeRestart();

/** @brief The S.BUS output job's entry in the job table of core1_job.cpp. */
extern const Core1JobOps SBUS_OUT_JOB_OPS;
