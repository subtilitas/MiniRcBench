/**
 * @file fakes.h
 * @brief Controls for the host-test fakes. @see fakes.cpp
 */

#pragma once

#include "esc_task.h"
#include "usb_msc.h"
#include "hardware/clocks.h"

#include <stdint.h>

struct EscTelemetry;

/** @brief Dirty bands the last st7789FlushDirty() found: 0 when the frame repainted nothing. */
int fakeLastFlushBands();

/** @brief Advance the virtual clock. Time only moves when a test says so. */
void fakeAdvance(uint32_t ms);

/**
 * @brief Advance the virtual clock by microseconds. millis() is the
 *        microsecond count divided by 1000, so it moves once 1000 us have
 *        added up.
 */
void fakeAdvanceUs(uint32_t us);

/**
 * @brief Move the clock forward to the next whole millisecond at which
 *        millis() is a multiple of @p periodMs; the clock keeps no
 *        sub-millisecond part afterwards.
 *
 * A screen that animates from millis() (a 12-dot spinner steps every 150 ms,
 * so a period of 1800 ms) draws the same picture whatever ran before. Call it
 * before the frames of the shot. The move is under @p periodMs; a period of
 * 0 moves nothing.
 */
void fakeClockAlign(uint32_t periodMs);

/** @brief One turn of a 12-dot spinner that steps every 150 ms, in ms. */
constexpr uint32_t FAKE_SPINNER_TURN_MS = 12 * 150;

/**
 * @brief The time since boot, in ms, at which the shot of SETTINGS' UPTIME is
 *        taken: 10000 h, so UPTIME reads 10000:00:00. It is later than any
 *        test reaches before that shot.
 */
constexpr uint64_t FAKE_SHOT_UPTIME_MS = 10000ull * 3600u * 1000u;

/**
 * @brief Move the clock forward to the absolute time @p ms since boot.
 *
 * A screen that prints the time since boot (SETTINGS' UPTIME) reads the same
 * text whatever ran before. The clock never moves back, so a time already
 * past is refused and the call returns false, which the caller checks. Call
 * it before the frames of the shot.
 */
__attribute__((warn_unused_result)) bool fakeClockSetMs(uint64_t ms);

/*
 * Deviation from the SDK: sleep_us() moves the clock by whole milliseconds
 * only, and drops the rest, so delayMicroseconds() of a few microseconds
 * moves no time. sleep_ms() and fakeAdvanceUs() are exact.
 */

/**
 * @brief Uses of a critical section that critical_section_init() never set up.
 *
 * Deviation from the SDK: the fake critical section excludes nothing, but
 * critical_section_init() marks the section and critical_section_enter_blocking()
 * and critical_section_exit() count each call on an unmarked one. On the chip
 * such a call uses a NULL spin lock pointer. main() in test_main.cpp fails the
 * run when the count is not 0, so a core1 job without its init call fails the
 * suite. A section is unmarked until its init call, as a zeroed static is.
 */
unsigned fakeCriticalSectionMisuse();

struct SdLogStatus;

/**
 * @brief Force the fake logger into a given state. @param st What to report.
 */
void fakeSdLogSet(const SdLogStatus *st);
struct RunSum;
/**
 * @brief The run summary the fake logger's next starts begin, and their
 *        stops leave in @ref SdLogStatus::summary, with MOTOR POLES at the
 *        stop. As sd_log.cpp: none while a log runs, a failed start keeps
 *        the last, a mount and USB DRIVE's handover forget it. nullptr, the
 *        default, gives none.
 */
void fakeSdLogSetSummary(const RunSum *s, uint8_t poles);
/** @brief The highest `LOGnnnnn.BFL` on the fake card, 0 for none: the
 *         next fake start takes the number after it (sdLogNextNumber()). */
void fakeSdLogSetHighest(uint32_t n);

enum class CardCache : uint8_t;

/** @brief Calls the screens made of the fake write cache. */
struct FakeCacheCalls {
	unsigned begins, ends, flushes, recovers;
};

/**
 * @brief What one step of kstCardWaitPoll() runs: @p pass, or by default a
 *        millisecond and one core1JobPoll().
 */
void fakeCardWaitPass(void (*pass)());

/** @brief The fake cache off, Idle, no calls counted. */
void fakeCacheReset();

/** @brief Put the fake cache, while on, into @p st with @p left blocks to go. */
void fakeCacheSet(CardCache st, uint16_t left);

/** @brief What cardCacheFlush() ends in: Idle, or Failed. */
void fakeCacheFlushTo(CardCache st);

/** @brief The calls counted since fakeCacheReset(). */
FakeCacheCalls fakeCacheCalls();

/**
 * @brief What cardVolumeSerial() reads while a card is mounted: @p serial, or
 *        no answer when @p answers is false. fakeCacheReset() sets 1, answering.
 */
void fakeVolumeSet(uint32_t serial, bool answers = true);

/** @brief The next @p n reads of cardVolumeSerial() fail, whatever fakeVolumeSet() said. */
void fakeVolumeFailNext(unsigned n);

struct ServoLogStatus;

/** @brief Calls SERVO TESTER made of the fake servo log (servo_log.h). */
struct FakeServoLogCalls {
	unsigned starts;    /**< servoLogStart(). */
	unsigned follows;   /**< servoLogFollow(). */
	unsigned backs;     /**< servoLogBack(). */
	uint16_t seq;       /**< The seq of the last servoLogFollow(). */
	bool     ranAtStart; /**< The job reported RUNNING at the last servoLogStart(). */
	uint16_t ackAtStart; /**< The job's acknowledged seq at the last servoLogStart(). */
	unsigned startsAtFollow; /**< servoLogStart() calls before the last servoLogFollow(). */
};

/**
 * @brief Whether the fake servo log would open (servoLogWanted()), and how
 *        long its open holds core0, ms, advancing the fake clock. Both off,
 *        0, by default.
 */
void fakeServoLogOpen(bool wanted, uint32_t openMs);

/**
 * @brief The fake servo log: what servoLogStatus() reports, no calls
 *        counted. servo_log.cpp itself runs over FatFs in `make lognum`
 *        (test/servo_log_fatfs.cpp).
 */
void fakeServoLogSet(const ServoLogStatus *st);

/** @brief The calls counted since fakeServoLogSet(). */
FakeServoLogCalls fakeServoLogCalls();

/** @brief Begin a touch at (@p x, @p y); sets the one-poll `pressed` edge. */
void fakePress(int x, int y);

/** @brief Continue a touch at (@p x, @p y) without a new `pressed` edge. */
void fakeHold(int x, int y);

/** @brief End the current touch; sets the one-poll `released` edge. */
void fakeRelease();

/**
 * @brief Whether the fake touch controller answers. While it does not,
 *        touchAnsweredMs() keeps the last answer's time and
 *        touchFailedReads() counts; touchInit() starts the count at 0, as
 *        the driver does, returns @p on and, with
 *        false, leaves no answer on record (touchAnswered()). The scripted
 *        finger is still reported, presses included, so the refusals of a
 *        lost touch are tested against a press that reaches them.
 * @param on Reads complete.
 */
void fakeTouchAnswers(bool on);

/**
 * @brief Set touchFailedReads() to @p n, for a test that needs the touch lost
 *        within fewer frames than the 4 failed reads take.
 */
void fakeTouchFails(uint8_t n);

/**
 * @brief Stamp touchAnsweredMs() at the clock now, as a completed read
 *        does, for tests that drive core1 without UI frames.
 */
void fakeTouchStamp();

/** @brief Read a byte of the fake ESC's settings page, as actually stored. */
uint8_t fakeEscByte(int offset);
/** @brief Store @p v at @p offset of the fake ESC's settings page, as another tool would. */
void fakeEscSet(int offset, uint8_t v);

/** @brief Throttle most recently commanded to the fake DShot task, 0..2000. */
uint16_t fakeThrottle();

/** @brief Arm state of the fake DShot task. */
bool fakeArmed();

/**
 * @brief True while Run is asked for: DShot has the pin or binds on the next
 *        pass.
 */
bool fakePinReturned();

/** @brief The wiring escTaskConfigure() last set: what a rebuild binds. */
struct FakeWiring {
	uint8_t  dshotPin;    /**< ESC signal GPIO. */
	uint16_t dshotKbaud;  /**< DShot bitrate. */
	bool     kissEnable;  /**< KISS wire expected. */
	uint8_t  kissPin;     /**< KISS GPIO. */
};

/** @brief The fake pump's wiring at the time of the call. */
FakeWiring fakeWiring();

/**
 * @brief One pass of the fake DShot pump on core1: escTaskPoll()'s handover
 *        and one frame decision on the shipped rules, and the pads of a park.
 *
 * Not modelled: the PIO and its pad writes, the reply fold, the frame timing,
 * the heartbeat (uiAlive is true), the beacon request and the KISS receiver.
 */
void fakePumpPoll();

/** @brief EscPump::lastThrottleMs of the fake pump. */
uint32_t fakeLastThrottleMs();

/** @brief Frames the fake pump has sent, drain frames included. */
int fakeFrameCount();

/** @brief The last frame the fake pump sent. */
EscFrame fakeLastFrame();

/** @brief How many times escTaskStart() has been called. */
int fakeResumeCount();

/** @brief How many times the fake pump has bound. */
int fakeRebuildCount();

/** @brief The wiring the fake pump's last bind used. */
FakeWiring fakeResumeWiring();

/**
 * @brief Call @p hook inside each fake bind, where out_dshot_bind() runs:
 *        after the wiring is read, before the bind is reported.
 * @param hook The hook; nullptr for none.
 */
void fakeSetBindHook(void (*hook)());

/**
 * @brief Put core1's side of the fake pump back to its power-on state, as
 *        esc_task.cpp's static initialisers leave it: no bind, Run wanted
 *        and reached, the bind stamp 0, nothing held, no rebuild pending.
 *        The wiring is left as configured.
 */
void fakePumpPowerOn();

/** @brief Whether the core1 job was idle at the last escTaskStart(). */
bool fakeResumeJobIdle();

/** @brief GPIO the AM32 transport was last handed by am32BlBegin(). */
uint8_t fakeAm32Pin();
/** @brief Forget the last am32BlBegin(): fakeAm32Pin() reads 255 until the next one. */
void fakeAm32Reset();

/** @brief The next @p n am32BlWrite() calls fail with a NAK and store nothing. */
void fakeAm32FailWrites(int n);
/** @brief The next @p n am32BlRead() calls time out. */
void fakeAm32FailReads(int n);
/** @brief The next @p n am32BlWrite() calls store byte 0x1B with bit 6 flipped. */
void fakeAm32SkewWrites(int n);
/** @brief am32BlWrite() calls since the start, failed ones among them. */
int fakeAm32Writes();
/**
 * @brief Handshakes answer with @p n bytes, at most 16, classified by
 *        blAnswerClassify(): bytes that are no bootloader's answer, or an
 *        EFM8 bootloader's; @p n 0 lets them succeed again with an F051's.
 */
void fakeAm32Garble(const uint8_t *b, uint8_t n);
/** @brief am32BlHandshake() calls since the start: the greetings sent. */
int fakeAm32Handshakes();
/** @brief Reads return an erased page, all 0xFF, while @p on. */
void fakeAm32Blank(bool on);
/** @brief Handshakes time out with no reply while @p on: the ESC is not in its bootloader. */
void fakeAm32Silent(bool on);

/** @brief Whether a suspend was still requested at the last am32BlEnd(). */
bool fakeAm32EndBeforeResume();
/** @brief am32BlEnd() calls since the start: the transport let go of SIG. */
int fakeAm32Ends();

/** @brief Set the telemetry the fake ESC reports. */
void fakeSetTelemetry(const EscTelemetry *t);

/**
 * @brief FNV-1a over a rectangle of the framebuffer.
 *
 * For asserting that a region changed, or that two renders match, without
 * depending on where individual glyphs land.
 */
uint32_t fakeRegionHash(int x, int y, int w, int h);

/**
 * @brief Whether @p text, drawn at (@p x, @p y) in @p col at @p scale over a
 *        @p bg fill, is what the framebuffer shows there.
 *
 * The expected text is drawn over the one on screen and the two hashes are
 * compared, so the check is on the rendered glyphs and on where they start.
 * The pixels under the probe are put back afterwards. In high contrast the
 * probe is one row taller, as the glyphs are (themeBold()).
 */
bool fakeTextIs(int x, int y, const char *text, uint16_t col, uint16_t bg, int scale);

/** @brief As fakeTextIs(), with the text centred on the panel as gfxTextCenter() does. */
bool fakeTextCentredIs(int y, const char *text, uint16_t col, uint16_t bg, int scale);

/**
 * @brief Whether a write page's change `old -> now` ends at x 232 on the line
 *        at @p y, at scale 1: `old -> ` in `C_DIM`, `now` in `C_CYAN`, as
 *        uiChangeText() draws it.
 */
bool fakeChangeIs(int y, const char *old, const char *now, uint16_t bg);

/**
 * @brief The leftmost and rightmost columns in rows @p y to @p y + @p h - 1
 *        that hold a pixel other than @p bg.
 * @return false when every pixel there is @p bg.
 */
bool fakeInkExtent(int y, int h, uint16_t bg, int *x0, int *x1);

/**
 * @brief Set what the fake ADC reads, 0 to 4095: the board's battery through
 *        its divider, `raw * 3.3 / 4095 * 3` V.
 */
void fakeAdcSet(uint16_t raw);

/** @brief The fake ADC's reading at start, about 3.92 V. */
uint16_t fakeAdcDefault();

/**
 * @brief How many command requests escRequestCommand() took, the UI's beeps
 *        among them.
 */
int fakeBeepRequests();

/**
 * @brief The last command escRequestCommand() took.
 * @param[out] save Its save flag; may be null.
 * @return The command; 0 before the first.
 */
uint8_t fakeLastCommand(bool *save);

/** @brief How many times escCancelCommand() was called. */
int fakeCommandCancels();

/**
 * @brief Set what escCommandReady() reports, core1's readiness in the fake.
 *        EscCmdReady::Ready by default.
 */
void fakeSetCmdReady(EscCmdReady r);

/**
 * @brief Set what escCommandState() reports for the last sequence, as core1
 *        publishes it, and end a request that waits: the request was read.
 *        A request escRequestCommand() takes reads EscCmdState::Waiting until
 *        this or escCancelCommand(). EscCmdState::Idle by default.
 * @param st  The sequence's state.
 * @param cmd Its command.
 */
void fakeSetCmdState(EscCmdState st, uint8_t cmd);

/**
 * @brief Count framebuffer pixels of an exact colour.
 *
 * Used to prove a screen left nothing of the previous one behind: the cyan of
 * the SETTINGS section captions is distinctive, so more than a trace of it
 * surviving on the tester screen is a leftover.
 */
int fakeCountColour(uint16_t c);

/** @brief Write the current framebuffer to a binary PPM, for eyeballing. */
void fakeDumpFrame(const char *name);

/** @brief Pole count most recently pushed to the fake pump. */
uint8_t fakePoles();

/** @brief Set what escEdtRequested() reports. */
void fakeSetEdtRequested(bool on);

/** @brief Backlight level most recently driven. */
uint8_t fakeBacklight();

/** @brief Lowest backlight level seen since fakeBacklightResetMin(). */
uint8_t fakeBacklightMin();

/** @brief Start watching for a new backlight minimum. */
void fakeBacklightResetMin();

/** @brief Erase the fake settings flash to 0xFF, as a blank part reads. */
void fakeFlashClear();

/** @brief Make the fake settings flash refuse writes, as a full image would. */
void fakeFlashSetWritable(bool on);

/** @brief Raw bytes of the fake settings flash, for corrupting on purpose. */
uint8_t *fakeFlashBytes();

/** @brief How many storage writes have been *attempted* since process start. */
int fakeFlashWrites();

/** @brief Calls of watchdog_reboot(). Asserted to stay put. */
int fakeRebootCount();
/** @brief What powman_hw->chip_reset reads. */
void fakeSetChipReset(uint32_t v);
/** @brief What watchdog_hw->reason reads and watchdog_enable_caused_reboot() returns. */
void fakeSetWatchdog(uint32_t reason, bool enableCaused);
/** @brief What rom_get_last_boot_type() returns. */
void fakeSetBootType(int type);

/** @brief How many times the UI has rebuilt the DShot pump's wiring. */
int fakeConfigureCount();

/** @brief DShot bitrate most recently pushed to the fake pump. */
uint16_t fakeDshotKbaud();

/** @brief GPIO most recently pushed to the fake pump. @see escTaskDshotPin() */
uint8_t escTaskDshotPin();

/** @brief GPIOs the PWM and GPIO fake records. */
#define FAKE_GPIOS  30
/** @brief Slices the PWM fake records, for GP0 to GP29. */
#define FAKE_SLICES 8

/** @brief Which pull a pin has, as the GPIO fake last set it. */
enum class FakePull : uint8_t {
	Untouched,  /**< No pull written since fakePwmReset(). */
	None,       /**< PUE and PDE clear. */
	Down,       /**< PDE alone. */
	Up,         /**< PUE alone. */
	Keep,       /**< PUE and PDE both: the RP2350's bus keeper. */
};

/** @brief What the fake recorded for one GPIO. */
struct FakePin {
	int      function;     /**< gpio_function last set; 0 for none. */
	uint16_t level;        /**< Compare level last set, or 0 after pwm_init(). */
	int      levelWrites;  /**< pwm_set_gpio_level() calls. */
	bool     output;       /**< gpio_set_dir() last set an output. */
	FakePull pull;         /**< The pad register's PUE and PDE, once a pull was written; read by fakePin(). */
	bool     high;         /**< gpio_put() last drove it high. */
	bool     inputOff;     /**< gpio_set_input_enabled() last disabled its input. */
	int      puts;         /**< gpio_put() calls. */
	int      highEnables;  /**< gpio_set_dir() made it an output while gpio_put() held it high. */
	int      cutPulses;    /**< Pulses cut short or left high by a stop or a mux change. */
	uint32_t cutAtUs;      /**< How far into the pulse the last cut came, us. */
	uint8_t  inover;       /**< Input override, `GPIO_OVERRIDE_*`; gpio_set_function() zeroes it, as the SDK does. */
	uint8_t  outover;      /**< Output override, `GPIO_OVERRIDE_*`, from the last control-register write. */
	uint32_t ctrl;         /**< The control register's last value: FUNCSEL, OUTOVER and INOVER. */
	/**
	 * @defgroup fake_pin_order Order of the writes
	 * @brief Each the value of one counter, shared by every pin and never
	 *        reset, at the pin's last write of that kind: a test compares
	 *        them to say which write came first. 0 for none since
	 *        fakePwmReset().
	 * @{
	 */
	uint32_t putAt;        /**< The last gpio_put(). */
	uint32_t levelAt;      /**< The last pwm_set_gpio_level(). */
	uint32_t functionAt;   /**< The last gpio_set_function(). */
	uint32_t dirAt;        /**< The last gpio_set_dir(). */
	uint32_t pullAt;       /**< The last gpio_pull_down(), gpio_pull_up() or gpio_disable_pulls(). */
	uint32_t padMaskAt;    /**< The last hw_write_masked() on the pad register. */
	uint32_t padIsoAt;     /**< The last hw_clear_bits() that cleared the pad's ISO bit. */
	/** @} */
	int      inoverWrites; /**< gpio_set_inover() calls. */
};

/** @brief What the fake recorded for one PWM slice. */
struct FakeSlice {
	bool     enabled;  /**< Counting. */
	uint16_t top;      /**< Wrap register. */
	uint32_t div16;    /**< Clock divider, in 1/16ths. */
	int      inits;    /**< pwm_init() calls. */
	uint64_t startUs;  /**< time_us_64() when pwm_init() started its counter. */
};

/**
 * @brief The compare level a pin's slice latched for the frame under way at
 *        the virtual time: the last level written before that frame began.
 *
 * The counter runs at 1 MHz from pwm_init(), one frame of top + 1 counts. A
 * level written during a frame takes effect at the wrap that ends it, and a
 * level written exactly at a frame's start takes effect a frame later. The
 * pin is high while the counter is below the latched level.
 *
 * @param gpio GPIO.
 * @return The latched level; 0 for a pin whose slice is not counting.
 */
uint16_t fakePwmLatched(unsigned gpio);

/**
 * @brief Clear every pin and slice record, the control-register record and
 *        the broken map. Every pad register reads its reset value, 0x116:
 *        ISO and PDE set, IE and OD clear.
 */
void fakePwmReset();

/** @brief One write of a GPIO's control register. */
struct FakeCtrlWrite {
	uint32_t value;  /**< FUNCSEL in bits 0 to 4, OUTOVER in 12 and 13, INOVER in 16 and 17. */
	uint32_t at;     /**< The pin write counter at the write (@ref fake_pin_order). */
	uint64_t us;     /**< time_us_64() at the write. */
};

/**
 * @brief How many times a GPIO's control register was written since
 *        fakePwmReset(): gpio_set_function(), gpio_set_inover() and a direct
 *        write of `io_bank0_hw->io[n].ctrl` each count one.
 * @param gpio GPIO.
 */
int fakeGpioCtrlWrites(unsigned gpio);

/**
 * @brief One write of a GPIO's control register, in order.
 * @param gpio GPIO.
 * @param i    0 for the first since fakePwmReset().
 * @return The write; all zero past the last.
 */
FakeCtrlWrite fakeGpioCtrlWrite(unsigned gpio, int i);

/**
 * @brief A GPIO's pad register as the fake holds it: IE, OD, ISO, PUE and
 *        PDE (hardware/structs/pads_bank0.h). gpio_set_function() sets IE and
 *        clears OD and ISO, and gpio_pull_down() and its kin set the pulls,
 *        as the SDK does.
 * @param gpio GPIO.
 */
uint32_t fakePad(unsigned gpio);

/**
 * @brief Make pwm_gpio_to_slice_num() name the wrong slice, as an SDK whose
 *        mapping disagreed with out_pwm_map.h would.
 * @param on True to break it.
 */
void fakePwmBreakSliceMap(bool on);

/** @brief The record of one GPIO. @param gpio GPIO. @return The record. */
FakePin fakePin(unsigned gpio);

/**
 * @brief Call @p hook after each GPIO write of the fake: gpio_put(),
 *        gpio_set_dir(), gpio_set_function(), a pull, the input buffer and
 *        the drive. Off while the hook runs.
 * @param hook The hook, given the GPIO written; nullptr for none.
 */
void fakeSetGpioHook(void (*hook)(uint32_t gpio));

/** @brief The record of one slice. @param slice Slice. @return The record. */
FakeSlice fakeSlice(unsigned slice);

/** @brief Put the fake USB card reader into a state. @see usb_msc.h */
void fakeMscSet(MscState s, uint32_t blocksRead);

/** @brief Set what clock_get_hz() reads for clk_sys or clk_peri; both start at 150 MHz. */
void fakeClockSet(enum clock_index i, uint32_t hz);

/** @brief Entries of the PL011 receive FIFO. */
#define FAKE_UART_FIFO 32u

/** @brief What the UART0 fake recorded. */
struct FakeUart {
	bool     enabled;   /**< Between uart_init() and uart_deinit(). */
	unsigned baud;      /**< What the last uart_init() returned. */
	uint32_t ibrd;      /**< Integer baud divisor of the last uart_init(). */
	uint32_t fbrd;      /**< Fractional baud divisor of the last uart_init(). */
	unsigned dataBits;  /**< Format: data bits. */
	unsigned stopBits;  /**< Format: stop bits. */
	int      parity;    /**< Format: `UART_PARITY_*`. */
	int      inits;     /**< uart_init() calls. */
	int      deinits;   /**< uart_deinit() calls. */
	unsigned fifo;      /**< Words in the receive FIFO. */
	unsigned queued;    /**< Words on the line, not yet due. */
	unsigned dropped;   /**< Words due while the receiver did not hear the line. */
	unsigned lost;      /**< Words due while the FIFO was full. */
	unsigned drReads;   /**< UARTDR reads. */
	unsigned caughtMid; /**< Words caught in the middle of their character. */
	uint32_t formatWaitUs; /**< The wait of the last uart_set_format() on a running UART, us. */
	bool     txe;       /**< UARTCR.TXE: uart_init() sets it. */
	bool     rxe;       /**< UARTCR.RXE: uart_init() sets it; the receiver hears nothing without it. */
	/**
	 * @defgroup fake_uart_order Order of the UART's calls
	 * @brief Values of the pin write counter (@ref fake_pin_order) at the
	 *        last call of each kind; 0 for none since fakeUartReset().
	 * @{
	 */
	uint32_t initAt;    /**< The last uart_init(). */
	uint32_t deinitAt;  /**< The last uart_deinit(). */
	uint32_t formatAt;  /**< The last uart_set_format(). */
	uint32_t crAt;      /**< The last write of UARTCR. */
	/** @} */
	unsigned txWrites;  /**< UARTDR writes. */
	unsigned txLost;    /**< UARTDR writes with the UART or TXE off, or the TX FIFO full. */
	unsigned txCut;     /**< Characters cut short: the UART stopped, or GP0 left the inverted UART function, before their last bit. */
};

/** @brief One character the UART0 fake transmitted. */
struct FakeTxByte {
	uint8_t  value;    /**< The byte. */
	uint64_t writeUs;  /**< time_us_64() of its UARTDR write. */
	uint64_t startUs;  /**< Its start bit: the write, or the end of the character before it. */
	uint64_t endUs;    /**< The end of its last stop bit. */
	bool     onPad;    /**< GP0 was on the UART function with its output inverted from its start to its end. */
	bool     cut;      /**< Cut short (FakeUart::txCut). */
	uint32_t bits;     /**< Bits on the wire: start, data, parity and stop. */
};

/**
 * @brief UART0 off and reset, nothing on the line, clk_peri at 150 MHz,
 *        every count 0.
 */
void fakeUartReset();

/**
 * @brief Put one UARTDR word on the line: the byte in bits 0 to 7 and FE,
 *        PE or BE as a receiver at 8E2 would flag it. It reaches the FIFO
 *        once the clock passes @p dueUs, at the next register read, read in
 *        the format the UART is set to (fakes.cpp).
 * @param dueUs time_us_64() at which the character is in: 11 bit times
 *              after its start bit.
 * @param word  The word; OE is the fake's, set by a full FIFO.
 */
void fakeUartLine(uint64_t dueUs, uint16_t word);

/**
 * @brief uart_init() leaves the receiver on at 8N1 for @p us before it
 *        returns, as a slow core between it and uart_set_format() would.
 *        fakeUartReset() sets 0.
 */
void fakeUartInitTakes(uint32_t us);

/** @brief The UART0 fake's record, after moving every due word. */
FakeUart fakeUart();

/**
 * @brief The UART0 fake's transmitter: each UARTDR write goes into a
 *        32-entry TX FIFO and out at the format and rate set, back to back,
 *        12 bits of 10 us a character at 100,000 baud 8E2. UARTFR.TXFE is
 *        set while no character waits in the FIFO, BUSY until the last stop
 *        bit is out. Not modelled: the bits' levels; a character is on the
 *        pad when GP0 is on the UART function with OUTOVER inverted.
 * @return How many characters were written since fakeUartReset().
 */
int fakeUartTxCount();

/** @brief One transmitted character, in order. @param i 0 for the first. @return It; all zero past the last. */
FakeTxByte fakeUartTxByte(int i);

/**
 * @brief A transmitter that never finishes: UARTFR.BUSY stays set, and a
 *        character written while it is on never leaves the TX FIFO, so TXFE
 *        stays clear behind it.
 */
void fakeUartTxStuck(bool on);
