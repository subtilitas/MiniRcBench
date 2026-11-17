/**
 * @file usb_bridge.h
 * @brief The USB half of the USB bridge: the board's serial port to the
 *        frame slot and back, on core0 (docs/design/usb-bridge.md, slices 2
 *        and 3).
 *
 * A desktop ESC tool on the board's USB serial port reaches the ESC's
 * bootloader on SIG at 19200 baud 8N1. Core1's job owns the pin
 * (bridge_wire.h); this file reads the port with `tud_cdc_available()` and
 * `tud_cdc_read()`, hands each frame from the PC to the slot, and writes the
 * ring to the port with `tud_cdc_write()`. It ports DshotDisplay's
 * `usb_bridge.*` at 6693084: the counters and the transparent rule, with the
 * two waits on `delay(1)` turned into states advanced once a pass.
 *
 * **When it forwards.** usbBridgeTask() runs every pass of core0's loop,
 * after usbDevTask(). The bridge is open exactly while the pin claim is
 * Running for PinUser::Bridge and the line is open (bridgeWireOpen()): only
 * then does it read the port or put a byte in the slot. At the open it
 * takes the port from the console (usbDevConsole(false)), drops what the
 * PC queued before, zeroes its counts and waits for a first byte. From the
 * claim's BACK on it reads nothing more, drops a frame it was gathering
 * and sends nothing of its own, no RUN among it (decision 6); the console
 * gets the port back once the claim is Free, after the log's dump when
 * @ref USB_BRIDGE_LOG is 1.
 *
 * **Modes** (decision 3). Waiting until the PC's first byte, which chooses:
 *
 * | First byte | Mode | Ends on |
 * |---|---|---|
 * | `$`, an MSP frame | Msp: the board answers MSP (bridge_msp.h) | @ref BRIDGE_IDLE_MS with no byte either way, the port closed |
 * | 0x2F, a 4-way frame | FourWay at once: the tools' recovery sends 4-way frames with no MSP before them | `cmd_InterfaceExit` once answered, the port closed; not a pause |
 * | any other | Passthrough | @ref BRIDGE_IDLE_MS with no byte either way, the port closed |
 *
 * Msp becomes FourWay at `MSP_SET_PASSTHROUGH` answered 1, or at a whole
 * 4-way frame. In both, an MSP frame is answered and a 4-way frame run
 * (bridge_4way.h); a byte between frames other than `$` and 0x2F is dropped,
 * and a frame with no byte for @ref BRIDGE_PROTO_GAP_US is dropped unsent.
 * The PC's next frame is read only once the last is answered: the PC is held
 * off by USB flow control meanwhile. The 4-way device is kept across pauses
 * and dropped when the mode ends. Later bytes never switch a mode: a raw
 * bootloader session holds 0x24 and 0x2F in its data. The port closes when
 * DTR (`tud_cdc_connected()`) falls after it was set; a tool that never sets
 * DTR keeps its mode until the idle time, `cmd_InterfaceExit` or BACK. The
 * bridge forwards whether DTR is set or not.
 *
 * **The gather.** A PC frame arrives in 64-byte USB packets, one per core0
 * pass at most (`CFG_TUD_CDC_RX_BUFSIZE` 64). Each pass first reads what
 * TinyUSB holds, then looks at the clock: the frame goes to the slot once
 * @ref BRIDGE_GATHER_US pass with no byte, or once it holds
 * @ref BRIDGE_FRAME_MAX bytes. A long pass, a repaint, in the middle of a
 * frame thus ends in the next packet read, not in half a frame sent; the PC
 * is held off by USB flow control meanwhile. While the slot refuses, the
 * frame waits and the port is not read.
 *
 * **Transparent.** The bridge never waits for the ESC before it forwards
 * the next frame: SET_BUFFER has no answer of its own, and a bridge that
 * waits for one corrupts the payload exchange that follows. The echo is the
 * wire's, at wire pace (bridgeWireKeepEcho(true)).
 *
 * **Write-class frames.** For the screen and for BACK, each PC frame's first
 * byte is named as an AM32 and BLHeli bootloader command
 * (usbBridgeCmdOf()); a frame after SET BUFFER is its payload. PROGRAM,
 * ERASE and a payload are write-class: from the handover until the
 * frame's answer, a byte received beyond its echo, or
 * @ref BRIDGE_WRITE_WAIT_MS, BACK is drawn disabled (decision 7); and from
 * then until a RUN or the leave page's last button, the leave page carries
 * `FLASH THE ESC AGAIN BEFORE USE.`, after BRIDGE's reopen too. A RUN counts
 * once core1 has taken it from the slot: one handed over shortly before BACK
 * is never sent, and the note stays.
 * BACK on a Running bridge always opens the leave page (decision 10 as
 * revised, ui_bridge.h): an ESC can sit in its bootloader with no frame of
 * the PC's answered. MENU shows the run cue from the claim's begin and
 * the open until the leave page's last button or an eRPM frame. The frames
 * themselves are forwarded unchanged.
 * In the 4-way interface the same rules hold for its write-class bootloader
 * frames, the data after SET BUFFER, PROGRAM and ERASE: BACK disabled from
 * the handover until the 4-way command's answer, at most
 * @ref BRIDGE_WRITE_WAIT_MS after the last of them, and the note until
 * `cmd_DeviceReset`'s RUN. `cmd_InterfaceExit` does not end an unfinished
 * write.
 *
 * **The 3D note** (decision 12). Passthrough reads byte 0x12 of each AM32
 * settings page it forwards: the greeting's answer names the AM32 family,
 * and with it the page's address (blAnswerAm32Settings()); SET ADDRESS gives
 * the address of what follows. A READ answered with a good CRC and the ACK,
 * from that address, of 0x30 bytes or more of plausible settings, sets or
 * clears the tester's note; a payload to that address with 3D on, whose
 * PROGRAM is answered 0x30, sets it (bridgeAm32Page3d(), esc_3d.h). The
 * answers are read past each frame's echo. The 4-way interface does the
 * same from its device's signature (bridge_4way.h).
 *
 * Core0 only.
 *
 * @see docs/design/usb-bridge.md, "USB side", "Passthrough", "MSP and the
 *      4-way interface", "Safety"
 */

#pragma once

#include <stdint.h>

/** @brief Gap with no byte from the PC that ends a frame, µs (DshotDisplay's BRIDGE_GATHER_MS, 2). */
#define BRIDGE_GATHER_US 2000u

/** @brief Passthrough ends after this long with no byte in either direction, ms. */
#define BRIDGE_IDLE_MS 2000u

/** @brief BACK stays disabled after a write-class frame this long at most, ms (decision 7). */
#define BRIDGE_WRITE_WAIT_MS 3000u

/** @brief A frame from the PC in MSP or the 4-way interface is dropped after this long with no byte, µs. */
#define BRIDGE_PROTO_GAP_US 50000u

/** @brief The screen says a motor command was refused for this long, ms. */
#define BRIDGE_MOTOR_NOTE_MS 5000u

/** @brief The bridge's mode. */
enum class BridgeMode : uint8_t {
	Closed,       /**< Not open: the claim is not Running for the bridge. */
	Waiting,      /**< Open, waiting for the PC's first byte. */
	Passthrough,  /**< Bytes copied both ways. */
	Msp,          /**< MSP answered, before `MSP_SET_PASSTHROUGH`. */
	FourWay,      /**< The 4-way interface. */
};

/** @brief A PC frame named as a bootloader command, by its first byte. */
enum class BridgeCmd : uint8_t {
	None,        /**< No frame since the open. */
	Init,        /**< The greeting: `BLHeli` and its CRC at its end. */
	SetAddress,  /**< 0xFF. */
	SetBuffer,   /**< 0xFE. */
	Payload,     /**< The frame after a SET BUFFER. */
	Read,        /**< 0x03. */
	Program,     /**< 0x01. */
	Erase,       /**< 0x02. */
	Run,         /**< 0x00, not the greeting. */
	KeepAlive,   /**< 0xFD. */
	Other,       /**< Any other first byte. */
	Msp,         /**< An MSP frame: BridgeLast::first is its command. */
	FourWay,     /**< A 4-way frame: BridgeLast::first is its command, BridgeLast::addr its address. */
};

/** @brief The bridge's counts since its open. */
struct UsbBridgeStats {
	uint32_t toEsc;       /**< Bytes from the PC handed to the wire. */
	uint32_t toHost;      /**< Bytes from the wire written to the PC, echoes included. */
	uint32_t frames;      /**< PC frames handed to the wire. */
	uint32_t lastMs;      /**< millis() of the last byte either way; the open's time before one. */
	uint32_t lost;        /**< The wire's lost bytes (BridgeWireStats::lost). */
	uint32_t collisions;  /**< Frames whose echo differed or came short: the ESC drove the line while the bench did. */
};

/** @brief The last PC frame, named. */
struct BridgeLast {
	BridgeCmd cmd;    /**< Its command. */
	uint8_t   first;  /**< Its first byte; for Msp and FourWay the MSP or 4-way command. */
	uint16_t  addr;   /**< SET ADDRESS and FourWay: bytes 2 and 3, high first. */
	uint16_t  len;    /**< Its length, bytes. */
};

/**
 * @brief Name a PC frame. Pure; host-testable.
 *
 * The greeting is the frame whose last 9 bytes are 0x0D, `BLHeli` and two
 * more, its CRC: AM32's 21 bytes and the 17 of Betaflight's other builds.
 * A frame after SET BUFFER is its payload whatever it holds. Otherwise the
 * first byte: 0xFF SET ADDRESS, 0xFE SET BUFFER, 0x03 READ, 0x01 PROGRAM,
 * 0x02 ERASE, 0xFD KEEP ALIVE (am32_bl.h; Betaflight's
 * `serial_4way_avrootloader.c` lines 49 to 64), and 0x00 RUN only for a
 * frame of 4 bytes whose last two are the CRC of the first two: a payload's
 * tail after a USB gap of 2 ms can start with 0x00 as well.
 *
 * @param f              The frame.
 * @param n              Its length; 0 gives None.
 * @param afterSetBuffer The frame before it was a SET BUFFER.
 * @return Its command.
 */
BridgeCmd usbBridgeCmdOf(const uint8_t *f, uint16_t n, bool afterSetBuffer);

/**
 * @brief The bootloader frames' CRC, CRC-16/ARC: polynomial 0xA001
 *        reflected, initial value 0, low byte first on the wire. Pure.
 * @param d The bytes.
 * @param n How many.
 * @return The CRC.
 */
uint16_t usbBridgeCrc16(const uint8_t *d, uint16_t n);

/** @brief PROGRAM, ERASE and a SET BUFFER payload. Pure. @param c A command. @return True for those three. */
bool usbBridgeCmdWrites(BridgeCmd c);

/**
 * @brief The screen's name of a command: `SET ADDRESS`, `PAYLOAD`, `INIT`.
 * @param c A command.
 * @return Its name; `""` for None, `FRAME` for Other, `MSP` and `4-WAY`
 *         for those.
 */
const char *usbBridgeCmdName(BridgeCmd c);

/**
 * @brief One pass of the bridge's USB side. Core0, every pass of the loop,
 *        after usbDevTask().
 *
 * Opens, forwards and closes as this file's description says. Does nothing
 * but look at the claim while the bridge does not hold it.
 */
void usbBridgeTask();

/** @brief The mode. @return Closed while the bridge is not open. */
BridgeMode usbBridgeMode();

/** @brief The counts since the open. @param out Where they go. */
void usbBridgeStats(UsbBridgeStats *out);

/** @brief The last PC frame since the open. @return cmd None before one. */
BridgeLast usbBridgeLast();

/**
 * @brief A write-class frame waits for its answer: BACK is drawn disabled.
 *        True from its handover until a byte beyond its echo comes, or
 *        @ref BRIDGE_WRITE_WAIT_MS pass, whether or not the bridge is
 *        still open. False at @ref BRIDGE_WRITE_WAIT_MS, true 1 ms before.
 * @return True while it waits.
 */
bool usbBridgeWriteWaiting();

/**
 * @brief A write-class frame went to the wire, and no RUN and no leave
 *        page's last button after it: the leave page carries its note to
 *        flash the ESC again (ui_bridge.h). Kept through the close, the
 *        leave page and BRIDGE's reopen, which leave the ESC's flash as cut
 *        as it was.
 *        While the board runs it is cleared only by a RUN that core1 took
 *        from the slot, `cmd_DeviceReset`'s among them, and by
 *        usbBridgeWriteUnfinishedClear() at the leave page's last button,
 *        which ends the claim: the next claim starts with none. A RUN
 *        handed over and not taken when BACK comes is never sent and clears
 *        nothing; a write after a RUN still pending keeps it set.
 *
 * It is held in RAM only: a board reset or a power loss in the middle of a
 * flash restarts the board with SIG parked low and no note.
 * @return True after such a frame.
 */
bool usbBridgeWriteUnfinished();

/**
 * @brief The leave page's last button, after which the user was told to
 *        flash the ESC again: the claim ends, and its unfinished write with
 *        it.
 */
void usbBridgeWriteUnfinishedClear();

/**
 * @brief An `MSP_SET_MOTOR` from the PC was refused in the last
 *        @ref BRIDGE_MOTOR_NOTE_MS, and the screen says so. Until the next
 *        open.
 * @return True for those 5 s.
 */
bool usbBridgeMotorRefused();

/**
 * @brief MENU's run cue, `POWER-CYCLE THE ESC TO RUN IT`, ORed with ESC
 *        SETTINGS' and the S.BUS output's probe's (ui_menu.cpp).
 *
 * Set when the claim begins, usbBridgeRunCueSet(), and at every open: from
 * then on the ESC may wait in its bootloader, whatever the tool sends, or
 * nothing. Cleared by usbBridgeRunCueClear() at the leave page's last
 * button and at START DSHOT, by usbBridgeRunCueSettle() at a claim's end
 * with no leave page that no byte from the ESC reached, and by an eRPM frame
 * stamped after the claim's begin or the open: the firmware runs again. A
 * RUN does not clear it, as a garbled frame could read as one. Kept across
 * the close.
 * @param lastRpmMs EscTelemetry::lastRpmMs.
 * @return True while the cue shows.
 */
bool usbBridgeRunCue(uint32_t lastRpmMs);

/**
 * @brief Set MENU's run cue when the screen's claim begins.
 *
 * From the claim's begin DShot is off SIG, and the line may meet an ESC
 * whose bootloader waits, entered at a power-up with SIG high or by an AM32
 * ESC after about 2 s without a signal. The cue stands until an eRPM frame
 * stamped after this call, usbBridgeRunCueClear(), or usbBridgeRunCueSettle()
 * at a claim's end with no leave page.
 *
 * It notes bridgeWireBinds() and sets the claim's "ESC byte seen" flag to
 * whether the cue already stood: a cue an earlier claim left standing is
 * never cleared by this claim's settle. From then on every usbBridgeTask()
 * pass reads the counts once bridgeWireBinds() has moved, so they are this
 * claim's, counted from 0 at the line's last bind; more received than sent,
 * or `lost` or `collisions` above 0, sets the flag
 * (docs/design/dshot-lifetime.md, decision 13 and "D3 as built").
 */
void usbBridgeRunCueSet();

struct BridgeWireStats;

/**
 * @brief Whether a claim's counts, read since the line's last bind, hold a
 *        byte from the ESC: more received than sent, or a byte lost, or a
 *        collision. The echo gives one byte back per byte sent, so any byte
 *        more came from the ESC (bridge_wire.h). Pure.
 * @param w bridgeWireStats() read after bridgeWireBinds().
 * @return True when a byte came from the ESC.
 */
bool usbBridgeEscByteSeen(const BridgeWireStats *w);

/**
 * @brief A claim ended with no leave page: BACK while Stopping, while Held
 *        before a bind, after binds that failed, or in the race where core1's
 *        bind lands after the frame's pinClaimPoll(). Such a claim never
 *        reached Running, so the bridge sent nothing. Reads the counts once
 *        more under usbBridgeTask()'s condition and clears the cue only if no
 *        byte from the ESC was seen on the claim's binds. Ends the watch, so a
 *        second call changes nothing.
 */
void usbBridgeRunCueSettle();

/**
 * @brief The leave page's last button, whose steps power the ESC off, and
 *        START DSHOT on the start step of ESC TESTER and ESC COMMANDS
 *        (docs/design/dshot-lifetime.md, decision 8): the cue goes.
 */
void usbBridgeRunCueClear();

/** @brief DTR is set: `PORT OPEN` on the screen. @return tud_cdc_connected(). */
bool usbBridgePortOpen();

/**
 * @brief The port closed after traffic: DTR fell after it was set, with a
 *        byte forwarded either way since the open. Cleared by the PC's
 *        next byte and by the next open.
 * @return True after such a close.
 */
bool usbBridgePortClosed();
