**English** | [Deutsch](USB-Bruecke)

# USB Bridge

`MENU → ESC CONFIG → USB BRIDGE` turns the board into a USB-serial adapter
on the ESC's signal wire. A desktop ESC tool on your computer opens the
board's serial port and talks to the ESC's bootloader through it, at
19200 baud, as through an adapter whose TX and RX are joined: every byte
the tool sends goes to the ESC, and every byte on the wire, the echo of
the tool's own included, comes back to the tool.

Not tested on hardware: no byte of the bridge has gone to an ESC from
this board, and no desktop tool has been run against it.

<img src="img/bridge-waiting.png" width="240" alt="USB BRIDGE waiting: SIG GP0 19200 8N1, PORT CLOSED, MODE WAITING, both counters at 0 B, the six lines"> <img src="img/bridge-passthrough.png" width="240" alt="USB BRIDGE after a frame: PORT OPEN, PASSTHROUGH, 6 B and 7 B, FRAMES 1 LOST 0, LAST SET ADDRESS 7E00">

---

## Which tools

- **Works, by design:** a tool that speaks the bootloader's frames itself
  and expects the echo: the AM32 configurator with **Direct** ticked.
- **Not yet:** ESC-Configurator, and the AM32 configurator without Direct.
  They speak MSP (the MultiWii Serial Protocol) and the 4-way interface of
  flight controllers, which the board does not answer yet. They get their
  own bytes back and no answer.
- **Not checked:** BLHeliSuite.

---

## Use it

1. Connect the ESC to SIG and GND as for DShot, on its own battery. Take
   the propeller off.
2. Connect the board to the computer by USB.
3. Open `MENU → ESC CONFIG` and tap `USB BRIDGE`. DShot stops; the strip
   reads `RELEASING SIG GP0` for a moment, then `SIG GP0  19200 8N1`.
4. In the tool, pick the board's serial port, the same one the board's
   console uses, and connect. The chip reads `PORT OPEN` once the tool
   opens the port; some tools do not signal that, and the bridge works
   either way.
5. Read, write or flash in the tool. Power-cycle the ESC if the tool finds
   no ESC, and after a flash.
6. Tap `BACK` when done.

While the bridge is open the board's console is off: no telemetry line
reaches the port. It comes back once DShot runs again.

If `USB DRIVE` on SD LOG holds the card, the bridge does not start and
reads `EJECT THE USB DRIVE FIRST.`. Eject the drive and it starts.

---

## What the screen shows

| Line | Meaning |
|---|---|
| `PORT OPEN`, `PORT CLOSED` | Whether the tool has the port open |
| `WAITING`, `PASSTHROUGH` | Waiting for the tool's first byte, or passing bytes. 2 s with no byte, or the tool closing the port, returns to `WAITING` |
| `PC > ESC`, `ESC > PC` | Bytes each way; `ESC > PC` includes the echo |
| `FRAMES`, `LOST` | Frames from the tool; bytes the board could not keep, amber above 0 |
| `LAST SET ADDRESS 7E00` | The last frame's command, as the bootloader names it |

`THE PC CLOSED THE PORT.` shows after the tool closed the port.
`BYTES LOST: THE BRIDGE FELL BEHIND.` shows once a byte was lost; read the
ESC again in the tool.

---

## Writing and leaving

<img src="img/bridge-writing.png" width="240" alt="USB BRIDGE during a write: WRITING: DO NOT UNPLUG ESC OR USB. in amber, BACK grey"> <img src="img/bridge-leave.png" width="240" alt="The leave page: 1 POWER THE ESC OFF., 2 RESTART DSHOT., FLASH THE ESC AGAIN BEFORE USE., BRIDGE, RESTART DSHOT">

While the ESC writes or erases its flash, the first line reads
`WRITING: DO NOT UNPLUG ESC OR USB.` and `BACK` is grey, until the ESC
answers or 3 s pass.

`BACK` sends nothing to the ESC. What happens next depends on the session:

- **Nothing written**, or the tool started the ESC's firmware after its
  last write: `BACK` returns to MENU, and DShot starts again.
- **A write the tool did not finish** with a start of the firmware: a
  leave page. Power the ESC off, then tap `RESTART DSHOT`, live 1 s after
  the page opens. Flash the ESC again before you use it. `BRIDGE` returns
  to the bridge instead.

If the USB cable is pulled during a write, the board keeps the signal pin
and sends nothing. Plug the cable in again and flash again.

---

## Not checked yet

- That any desktop tool works through the board.
- That each tool signals an open port (`PORT OPEN`).
- That the board hears the ESC's answer on the signal wire, and the echo's
  timing.
- What an ESC does with DShot after a flash cut part-way.
