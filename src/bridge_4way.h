/**
 * @file bridge_4way.h
 * @brief The USB bridge's 4-way interface: BLHeli's protocol in which a
 *        flight controller speaks to an ESC's bootloader on the PC's behalf
 *        (docs/design/usb-bridge.md, "MSP and the 4-way interface", slice 3).
 *
 * ESC-Configurator and the AM32 configurator send it after
 * `MSP_SET_PASSTHROUGH` (bridge_msp.h), or with no MSP before it to recover
 * an interface left open. The board runs it on core0: each PC frame becomes
 * the bootloader frames of rcbench's escwire (`ew_a.h`), sent through the
 * frame slot (bridge_wire.h), and the bootloader's answer, read from the
 * ring, becomes the PC's answer.
 *
 * **Frames.** From the PC: 0x2F, the command, the address's high and low
 * byte, the parameters' length (0 meaning 256), the parameters and a
 * CRC-16/XMODEM (cyclic redundancy check, polynomial 0x1021, initial 0) of
 * all of it, high byte first. The answer: 0x2E, the same header with the
 * answer's parameters, at least one, an ACK (acknowledgement) code and the
 * CRC. A frame is acted on only once it is whole and its CRC fits; a wrong
 * CRC is answered `B4W_ACK_I_INVALID_CRC` and sends nothing on the wire.
 *
 * **One ESC.** The ESC on SIG is index 0. `cmd_DeviceInitFlash` and
 * `cmd_DeviceReset` name an index in their first parameter; any other answers
 * `B4W_ACK_I_INVALID_CHANNEL` and sends nothing.
 *
 * | Command | The board does |
 * |---|---|
 * | 0x30 TestAlive | the bootloader's keep-alive, 0xFD 0x00, while a device is connected; its answer 0xC1 is the good one |
 * | 0x31, 0x32, 0x33 | 108, `MRCB4W`, the firmware version as two bytes |
 * | 0x34 Exit | answers, then the bridge leaves the 4-way interface |
 * | 0x35 DeviceReset | RUN, 0x00 0x00 and its CRC; the device is dropped |
 * | 0x37 DeviceInitFlash | the 21-byte greeting, its 9-byte answer, up to 3 times; the interface mode from the signature |
 * | 0x39 PageErase | SET ADDRESS of the page, ERASE, its ACK within 3000 ms |
 * | 0x3A Read | SET ADDRESS, READ, the data, their CRC and the ACK |
 * | 0x3B Write | SET ADDRESS, SET BUFFER, 4 ms of silence, the data, their ACK, PROGRAM, its ACK within 500 ms |
 * | 0x40 Verify | ARM only: SET ADDRESS, SET BUFFER, the data, VERIFY |
 * | 0x3F SetMode | SiLabs (1) and ARM (4); any other `B4W_ACK_I_INVALID_PARAM` |
 * | 0x38, 0x3C, 0x3D, 0x3E, others | `B4W_ACK_I_INVALID_CMD`, as no Atmel or C2 interface is offered |
 *
 * PageErase, Read, Write and Verify before a device is connected answer
 * `B4W_ACK_D_GENERAL_ERROR` and send nothing. SET ADDRESS 0xFFFF is
 * skipped, as Betaflight skips it. The interface mode: SiLabs for a signature
 * 0xE801 to 0xF8FF, ARM for a high byte 0x01 to 0x8F with the low byte 0x06;
 * any other signature is no device.
 *
 * **The wire.** Each bootloader frame goes to the slot whole, after the ring
 * is emptied of anything before it. It is out once the wire has received as
 * many bytes as it holds, its echo, or, should the echo be lost, its length
 * at 534 µs a byte and 20 ms more after the handover. The wait for its answer
 * starts there, with escwire's limits (`ew_a.h`): SET ADDRESS 4 ms, READ
 * 80 ms and 2 ms a byte, the data after SET BUFFER 80 ms, PROGRAM 500 ms,
 * ERASE 3000 ms, the greeting 100 ms; the keep-alive 4 ms and VERIFY 40 ms.
 * The answer is read from the ring before the clock, so a long pass of core0
 * ends with the answer read, not with a timeout. The data after SET BUFFER
 * go 4 ms after the header is out, as Betaflight sends them: a bootloader
 * ends a message after 260 µs of idle line, and data closer behind the
 * header would join it.
 *
 * **Writes.** The data after SET BUFFER, PROGRAM and ERASE are write-class,
 * as in passthrough (usb_bridge.h): the step that hands one over reports it,
 * and the bridge holds BACK disabled until the command's answer, at most
 * 3000 ms after the last such frame, and gives the leave page its note to
 * flash the ESC again until a RUN.
 *
 * **The 3D note** (decision 12). With an ARM device whose signature's high
 * byte is an AM32 family (bl_answer.h), a Read or a Write that starts at its
 * settings page and covers its first 0x30 bytes, of a page that looks like
 * AM32 settings (am32Plausible()), sets the tester's 3D note from byte 0x12:
 * a read sets or clears it, an acknowledged write only sets it, as item 24's
 * rule has it (esc_3d.h).
 *
 * Core0 only.
 *
 * @see docs/design/usb-bridge.md, "MSP and the 4-way interface"
 */

#pragma once

#include <stdint.h>

/** @brief A PC frame's first byte. */
#define B4W_PC_START 0x2Fu
/** @brief An answer's first byte. */
#define B4W_IF_START 0x2Eu

/** @brief The 4-way commands. */
enum Bridge4wayCmd : uint8_t {
	B4W_TEST_ALIVE       = 0x30, /**< `cmd_InterfaceTestAlive`. */
	B4W_PROTOCOL_VERSION = 0x31, /**< `cmd_ProtocolGetVersion`. */
	B4W_GET_NAME         = 0x32, /**< `cmd_InterfaceGetName`. */
	B4W_GET_VERSION      = 0x33, /**< `cmd_InterfaceGetVersion`. */
	B4W_EXIT             = 0x34, /**< `cmd_InterfaceExit`. */
	B4W_RESET            = 0x35, /**< `cmd_DeviceReset`. */
	B4W_INIT_FLASH       = 0x37, /**< `cmd_DeviceInitFlash`. */
	B4W_ERASE_ALL        = 0x38, /**< `cmd_DeviceEraseAll`. */
	B4W_PAGE_ERASE       = 0x39, /**< `cmd_DevicePageErase`. */
	B4W_READ             = 0x3A, /**< `cmd_DeviceRead`. */
	B4W_WRITE            = 0x3B, /**< `cmd_DeviceWrite`. */
	B4W_C2CK_LOW         = 0x3C, /**< `cmd_DeviceC2CK_LOW`. */
	B4W_READ_EEPROM      = 0x3D, /**< `cmd_DeviceReadEEprom`. */
	B4W_WRITE_EEPROM     = 0x3E, /**< `cmd_DeviceWriteEEprom`. */
	B4W_SET_MODE         = 0x3F, /**< `cmd_InterfaceSetMode`. */
	B4W_VERIFY           = 0x40, /**< `cmd_DeviceVerify`. */
};

/** @brief The ACK codes of an answer. */
enum Bridge4wayAck : uint8_t {
	B4W_ACK_OK                = 0x00, /**< Done. */
	B4W_ACK_I_INVALID_CMD     = 0x02, /**< A command the board does not run. */
	B4W_ACK_I_INVALID_CRC     = 0x03, /**< The frame's CRC does not fit. */
	B4W_ACK_I_VERIFY_ERROR    = 0x04, /**< VERIFY found other bytes. */
	B4W_ACK_I_INVALID_CHANNEL = 0x08, /**< An ESC other than 0. */
	B4W_ACK_I_INVALID_PARAM   = 0x09, /**< A mode the board does not offer. */
	B4W_ACK_D_GENERAL_ERROR   = 0x0F, /**< The bootloader did not answer as it should, or no device. */
};

/** @brief The interface modes the board offers. */
#define B4W_MODE_SILABS 1u
/** @brief ARM: AM32 and BLHeli_32. */
#define B4W_MODE_ARM    4u

/** @brief The protocol version, Betaflight's. */
#define B4W_PROTOCOL 108u
/** @brief The interface's name (decision 5). */
#define B4W_NAME "MRCB4W"

/** @brief The longest PC frame: header 5, 256 parameters, CRC 2. */
#define B4W_FRAME_MAX 263u
/** @brief The longest answer: header 5, 256 parameters, ACK, CRC 2. */
#define B4W_ANSWER_MAX 264u

/** @brief A wire byte as the line sends it within a frame, 82 cycles at 8 a bit, µs, rounded up. */
#define B4W_BYTE_US 534u
/** @brief A frame whose echo never completes counts as out this long after its wire time, µs. */
#define B4W_OUT_SLACK_US 20000u
/** @brief The silence after SET BUFFER's header before the data go, µs (Betaflight's two 2 ms waits). */
#define B4W_BUFFER_PAUSE_US 4000u
/** @brief The wait for the answer to VERIFY, µs (Betaflight's 40 ms). */
#define B4W_VERIFY_US 40000u

/** @brief A 4-way frame being read from the PC, byte by byte. */
struct Bridge4wayReader {
	uint16_t got;                 /**< Bytes read, 0 outside a frame. */
	uint16_t need;                /**< Bytes the frame has, once its header is in. */
	uint8_t  buf[B4W_FRAME_MAX];  /**< The frame. */
};

/** @brief The result of one byte. */
enum class Bridge4wayFeed : uint8_t {
	More,     /**< Inside a frame. */
	Frame,    /**< A whole frame whose CRC fits. */
	BadCrc,   /**< A whole frame whose CRC does not fit: answer bridge4wayCrcAnswer(). */
	Dropped,  /**< A byte other than 0x2F outside a frame. */
};

/** @brief What the connected device is. */
struct Bridge4wayDevice {
	bool     connected;  /**< A greeting was answered since the last reset, RESET or failed command. */
	uint8_t  mode;       /**< The interface mode, `B4W_MODE_SILABS` or `B4W_MODE_ARM`; 0 before a SetMode or a greeting. */
	uint16_t signature;  /**< High byte, then low byte, as the bootloader names them. */
	uint16_t settings;   /**< The AM32 settings page's address; 0 for none known. */
};

/** @brief What one step did. */
struct Bridge4wayEvents {
	bool     write;   /**< A write-class bootloader frame was handed to the wire. */
	bool     run;     /**< RUN was handed to the wire. */
	bool     done;    /**< The command ended: its answer waits in bridge4wayTakeAnswer(). */
	bool     exit;    /**< That command was Exit: leave the 4-way interface once its answer is out. */
	uint16_t bytes;   /**< Bytes handed to the wire. */
	uint8_t  frames;  /**< Frames handed to the wire. */
};

/**
 * @brief CRC-16/XMODEM: polynomial 0x1021, initial value @p crc, not
 *        reflected. Pure.
 * @param d   The bytes.
 * @param n   How many.
 * @param crc The CRC so far; 0 to start.
 * @return The CRC.
 */
uint16_t bridge4wayCrc(const uint8_t *d, uint16_t n, uint16_t crc = 0);

/** @brief Start reading a new frame. @param r The reader. */
void bridge4wayReaderReset(Bridge4wayReader *r);

/** @brief Whether a frame is being read. @param r The reader. @return True from its 0x2F to its end. */
bool bridge4wayInFrame(const Bridge4wayReader *r);

/**
 * @brief Take one byte from the PC.
 * @param r The reader.
 * @param b The byte.
 * @return More, Frame, BadCrc or Dropped; after Frame or BadCrc the reader
 *         holds the frame until the next byte.
 */
Bridge4wayFeed bridge4wayFeed(Bridge4wayReader *r, uint8_t b);

/**
 * @brief The answer to a frame whose CRC does not fit: its command and
 *        address, one parameter 0, `B4W_ACK_I_INVALID_CRC`.
 * @param r   The reader, after BadCrc.
 * @param out Room for 9 bytes.
 * @return 9.
 */
uint16_t bridge4wayCrcAnswer(const Bridge4wayReader *r, uint8_t *out);

/**
 * @brief No device, no command, the interface mode 0: the 4-way interface
 *        left or not yet entered. A running command stops where it is:
 *        nothing more is handed to the wire and nothing is answered; a
 *        frame already in the slot still goes out.
 */
void bridge4wayReset();

/**
 * @brief Start the command of a whole frame. Nothing reaches the wire until
 *        bridge4wayStep().
 * @param r The reader, after Frame.
 */
void bridge4wayStart(const Bridge4wayReader *r);

/** @brief A command runs: started and not yet answered. @return True while it runs. */
bool bridge4wayBusy();

/**
 * @brief One pass of the running command: hands its next bootloader frame
 *        to the slot, reads the ring, judges the answer, and once the
 *        command ends builds the PC's answer.
 * @param nowUs micros().
 * @return What it did.
 */
Bridge4wayEvents bridge4wayStep(uint32_t nowUs);

/**
 * @brief The answer of the command that ended.
 * @param out Room for @ref B4W_ANSWER_MAX bytes.
 * @return Its length; 0 when none waits. Taken once.
 */
uint16_t bridge4wayTakeAnswer(uint8_t *out);

/** @brief The connected device. @return Its state. */
Bridge4wayDevice bridge4wayDevice();

/**
 * @brief Byte 0x12 of an AM32 settings page in some bytes at an address.
 *        Pure.
 * @param addr     Where the bytes start.
 * @param settings The settings page's address; 0 for none known.
 * @param d        The bytes.
 * @param n        How many.
 * @return 1 for 3D mode on, 0 for off, -1 when the bytes are not that page:
 *         another address, fewer than 0x30 bytes, or no plausible AM32
 *         settings (am32Plausible()).
 */
int bridgeAm32Page3d(uint16_t addr, uint16_t settings, const uint8_t *d, uint16_t n);

/**
 * @brief The screen's name of a 4-way command: `READ`, `WRITE`, `INIT FLASH`.
 * @param cmd The command.
 * @return Its name; null for one with none.
 */
const char *bridge4wayName(uint8_t cmd);

/** @brief Whether the screen shows a 4-way command with its address: the memory commands. @param cmd The command. @return True for them. */
bool bridge4wayNameHasAddress(uint8_t cmd);
