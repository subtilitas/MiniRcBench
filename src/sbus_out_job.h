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
 * | SAFE | the UART bound and idle: low, no frames | Start |
 * | RUNNING | a frame every 14 ms | Safe, a heartbeat lapse, a refused frame, the release |
 * | Tripped | SAFE, latched after a heartbeat lapse while RUNNING | Start, or a new bind |
 *
 * Every decision about the frames is in sbusOutCoreTake() and
 * sbusOutCoreStep(), pure functions the host suite runs as they ship; the
 * job around them copies the command block, loads the TX FIFO
 * (first-in, first-out buffer) and publishes the status.
 *
 * **The bind**, in this order: the pull-down on GP0, which holds the line at
 * its idle level; uart_init() at 100,000 baud, refused with
 * CORE1_JOB_CLOCK more than 1 % off (sbusBaudOk()); 8E2 and the receiver
 * off (UARTCR.RXE), since no pad carries UART0's RX during this claim; then
 * GP0 to the UART with its output inverted. gpio_set_function() writes the
 * whole control register and zeroes OUTOVER (SDK 2.1.1, gpio.c), so the
 * SDK's two calls, function then gpio_set_outover(), would put the UART's
 * high idle on the pad between them. The job does gpio_set_function()'s
 * three steps itself: input enable on and output disable off in the pad
 * register, the control register with FUNCSEL `GPIO_FUNC_UART` and OUTOVER
 * `GPIO_OVERRIDE_INVERT` in one write, then pad isolation cleared.
 *
 * **The release** loads no new frame and returns false while UARTFR.BUSY is
 * set, so the frame on the wire completes, at most 3,000 us after it was
 * loaded; @ref SBUS_OUT_RELEASE_MAX_MS is the backstop for a BUSY that never
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
 * `src/` sends a command. The bootloader probe that must run before any
 * START (docs/design/sbus-output.md, decision 1) and the screen come in
 * later slices of that note.
 *
 * Not run on hardware: the inversion on the pin, the level at the bind and
 * the release, and the frames on a scope are not measured.
 *
 * @see docs/design/sbus-output.md
 */

#pragma once

#include <stdint.h>

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
	uint16_t       ceilRaw;    /**< The throttle ceiling, raw, sbusOutCeilRaw(); taken by SbusOutCmdKind::Start. */
};

/** @brief What core1 publishes for core0. The release clears it. */
struct SbusOutStatus {
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
	uint16_t     ceilRaw;   /**< The throttle ceiling, raw, from Start, within 172 to 1811. */
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
 *   above 3. Otherwise it takes the throttle channel and the ceiling, clamped
 *   to raw 172 to 1811, sets the START frame (sbusOutStartFrame()) whatever
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
 * @return True when the command was new.
 */
bool sbusOutCoreTake(SbusOutCore *c, const SbusOutCmd *cmd, uint32_t nowUs,
                     bool uiAlive);

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
 * @param[out] out  The frame's bytes, written only when the result is true.
 * @return True when @p out holds a frame to load now.
 */
bool sbusOutCoreStep(SbusOutCore *c, uint32_t nowUs, bool uiAlive, bool txEmpty,
                     uint8_t out[SBUS_FRAME_BYTES]);

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
 * @brief Send a command. Core0.
 *
 * One command is held at a time, and a new one replaces a command core1 has
 * not taken, with one exception: a Safe not yet taken stays, and a command
 * sent after it is dropped. A STOP is never lost to a later command. Nothing
 * in `src/` calls it yet.
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

/** @brief The S.BUS output job's entry in the job table of core1_job.cpp. */
extern const Core1JobOps SBUS_OUT_JOB_OPS;
