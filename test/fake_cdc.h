/**
 * @file fake_cdc.h
 * @brief The host suite's USB serial port: TinyUSB's CDC calls with a PC on
 *        the other end, and the console switch of usb_dev.h.
 *
 * The model, from TinyUSB's CDC device with this board's tusb_config.h:
 *
 * - **PC to board.** A write of the PC goes out in packets of at most 64
 *   bytes; two writes never share a packet. The board's receive buffer is
 *   `CFG_TUD_CDC_RX_BUFSIZE`, 64 bytes, and takes the next packet only
 *   when it is empty: the PC is held off by flow control, nothing is lost.
 *   A packet moves in at `tud_task()`, one a call: TinyUSB takes the next
 *   OUT packet in a later USB frame, and core0's loop calls `tud_task()`
 *   once a pass, before usbBridgeTask().
 * - **Board to PC.** `tud_cdc_write()` fills a transmit buffer of
 *   `CFG_TUD_CDC_TX_BUFSIZE`, 256 bytes; `tud_cdc_write_flush()` moves it
 *   to the PC's side at once, unless the PC has stopped taking bytes
 *   (fakeCdcHostStall()), when it stays and the buffer fills.
 * - **DTR.** `tud_cdc_connected()` reads it; the PC sets and clears it.
 * - **The SDK.** `stdio_set_driver_enabled()` records the console's state,
 *   so the suite links the real usb_dev.cpp; `tusb_init()`, the board ID
 *   and the serial number string do nothing.
 *
 * Bytes are not timed: a test reads the PC's side when its model of a tool
 * would.
 */

#pragma once

#include <stdint.h>

/** @brief DTR clear, every buffer empty, the PC taking bytes, the console on. */
void fakeCdcReset();

/** @brief Set or clear DTR, as the PC opening or closing the port. */
void fakeCdcDtr(bool on);

/** @brief The PC writes @p n bytes: packets of at most 64 behind what it wrote before. */
void fakeCdcHostWrite(const uint8_t *d, int n);

/**
 * @brief The PC writes @p n bytes with a USB gap: the first @p split go as
 *        fakeCdcHostWrite() does, the rest only @p gapUs from now, as a host
 *        whose transfer stalls between two packets.
 */
void fakeCdcHostWriteGap(const uint8_t *d, int n, int split, uint32_t gapUs);

/** @brief Text the console wrote to TinyUSB's transmit buffer, not flushed: what printf() leaves. */
void fakeCdcConsoleText(const char *s);

/** @brief Bytes the PC wrote that the board has not read. */
int fakeCdcHostQueued();

/** @brief The PC reads what reached it, at most @p max bytes; the rest stays. */
int fakeCdcHostRead(uint8_t *buf, int max);

/** @brief Bytes on the PC's side not yet read. */
int fakeCdcHostPending();

/** @brief Every byte that reached the PC since the reset. */
long fakeCdcHostTotal();

/** @brief The PC stops taking bytes, or takes them again. */
void fakeCdcHostStall(bool on);

/** @brief Whether the console has the port: the stdio driver enabled (usbDevConsole() in the real usb_dev.cpp). */
bool fakeConsoleOn();

/** @brief Times usbDevConsole() changed it since the reset. */
int fakeConsoleSwitches();

/** @brief fakeCdcHostTotal() when the console last got the port back. */
long fakeConsoleBackAt();
