**English** | [Deutsch](USB-Bruecke)

# USB Bridge

`MENU → ESC CONFIG → USB BRIDGE` lets a desktop ESC tool on your computer
reach the ESC's bootloader on the signal wire, through the board's serial
port. The board works in one of two ways, chosen by the tool's first byte:

- **Passthrough:** a USB-serial adapter whose TX and RX are joined, at
  19200 baud. Every byte the tool sends goes to the ESC, and every byte on
  the wire, the echo of the tool's own included, comes back to the tool.
- **4-way interface:** the board answers as a flight controller with one
  ESC. It answers the tool's MSP (MultiWii Serial Protocol) questions, then
  runs BLHeli's 4-way interface: it turns each of the tool's requests into
  the bootloader's commands on the signal wire and sends back the answer.

Not tested on hardware: no byte of the bridge has gone to an ESC from
this board, and no desktop tool has been run against it.

<img src="img/bridge-waiting.png" width="240" alt="USB BRIDGE waiting: SIG GP0 19200 8N1, PORT CLOSED, MODE WAITING, both counters at 0 B, the six lines"> <img src="img/bridge-passthrough.png" width="240" alt="USB BRIDGE after a frame: PORT OPEN, PASSTHROUGH, 6 B and 7 B, FRAMES 1 LOST 0, LAST SET ADDRESS 7E00"> <img src="img/bridge-4way.png" width="240" alt="USB BRIDGE in the 4-way interface: 4-WAY INTERFACE, LAST MSP SET MOTOR, MOTOR COMMAND FROM THE PC REFUSED. in amber">

---

## Which tools

- **Passthrough, by design:** a tool that speaks the bootloader's frames
  itself and expects the echo: the AM32 configurator with **Direct**
  ticked.
- **4-way interface, by design:** ESC-Configurator, for Bluejay, BLHeli_S
  and AM32 ESCs, and the AM32 configurator without Direct.
- **Not checked:** BLHeliSuite.

The board knows which from the tool's first byte; you choose nothing.

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

<img src="img/bridge-stopping.png" width="240" alt="USB BRIDGE while DShot stops: RELEASING SIG GP0 on the strip, no chip, the instruction lines"> <img src="img/bridge-failed.png" width="240" alt="USB BRIDGE after a failed bind: SIG GP0 COULD NOT BE TAKEN (PIO) in amber, RETRY in the footer">

While DShot stops, the bridge is not open yet. If the board cannot take
SIG, one amber line reads `SIG GP0 COULD NOT BE TAKEN (PIO)`, with `REFUSED`
or `CLOCK` in place of `PIO`, and `RETRY` appears:

- `PIO` (programmable I/O): no PIO block has room for the line's transmitter
  and receiver;
- `CLOCK`: the system clock (`clk_sys`) is too slow for the line's rate;
- `REFUSED`: the board's second core did not take the job.

`RETRY` tries again. `BACK` goes straight to ESC CONFIG, with no leave page, while
DShot stops and after a failed bind: the bridge never took the line.

While the bridge is open the board's console is off: no telemetry line
reaches the port. It comes back once DShot runs again.

If `USB DRIVE` on SD LOG holds the card, the bridge does not start and
reads `EJECT THE USB DRIVE FIRST.`. Eject the drive and it starts.

<img src="img/bridge-eject.png" width="240" alt="USB BRIDGE with the card held by USB DRIVE: EJECT THE USB DRIVE FIRST. in amber">

---

## What the screen shows

| Line | Meaning |
|---|---|
| `PORT OPEN`, `PORT CLOSED` | Whether the tool has the port open |
| `WAITING`, `PASSTHROUGH`, `4-WAY INTERFACE` | Waiting for the tool's first byte, passing bytes, or answering as a flight controller. In passthrough, and while the tool still asks its first MSP questions, which already show `4-WAY INTERFACE`, 2 s with no byte, or the tool closing the port, returns to `WAITING`; once the tool has started the 4-way interface it stays until the tool disconnects or closes the port |
| `PC > ESC`, `ESC > PC` | Bytes each way; in passthrough `ESC > PC` includes the echo, in the 4-way interface it counts the board's answers |
| `FRAMES`, `LOST` | Frames to the ESC; bytes the board could not keep, amber above 0 |
| `LAST SET ADDRESS 7E00` | The last command: as the bootloader names it, `LAST MSP UID` for an MSP question, `LAST READ 7C00` for a 4-way request |

`THE PC CLOSED THE PORT.` shows after the tool closed the port.
`BYTES LOST: THE BRIDGE FELL BEHIND.` shows once a byte was lost; read the
ESC again in the tool. `MOTOR COMMAND FROM THE PC REFUSED.` shows for 5 s
when the tool's motor tab tries to spin the motor: the board never does.

<img src="img/bridge-lost.png" width="240" alt="USB BRIDGE in passthrough with LOST 932 in amber and BYTES LOST: THE BRIDGE FELL BEHIND. in amber">

A settings page of an AM32 ESC that the tool reads or writes sets the
tester's `3D MODE ESC` note as ESC SETTINGS does: on after reading or
writing 3D mode on, off after reading it off.

---

## Writing and leaving

<img src="img/bridge-writing.png" width="240" alt="USB BRIDGE during a write: WRITING: DO NOT UNPLUG ESC OR USB. in amber, BACK grey"> <img src="img/bridge-leave.png" width="240" alt="The leave page: 1 POWER THE ESC OFF., 2 RESTART DSHOT., FLASH THE ESC AGAIN BEFORE USE., BRIDGE, RESTART DSHOT and BACK grey">

While the ESC writes or erases its flash, the first line reads
`WRITING: DO NOT UNPLUG ESC OR USB.` and `BACK` is grey, until the ESC
answers or 3 s pass. This holds in both ways of working.

`BACK` sends nothing to the ESC. What happens next depends on the session:

On a running bridge, `BACK` opens a leave page every time, whatever the
tool did: the ESC may sit in its bootloader, entered at power-up or by
itself, and DShot frames can pass as its commands. Power the ESC off, then
tap `RESTART DSHOT`, live 1 s after the page opens. `BRIDGE` returns to
the bridge instead. After a write not followed by a start of the firmware
the page adds `FLASH THE ESC AGAIN BEFORE USE.`: flash the ESC again
before you use it.
In the 4-way interface only the tool's reset of the ESC counts as that
start, not its disconnect. The note stays after `BRIDGE`: every later
`BACK` brings it back until the tool starts the firmware. `RESTART DSHOT`
ends the session, and the next `USB BRIDGE` starts without the note.

<img src="img/bridge-leave-read.png" width="240" alt="The leave page after a session that only read: 1 POWER THE ESC OFF., 2 RESTART DSHOT., no FLASH note, BRIDGE, RESTART DSHOT and BACK grey">

Without such a write, as after a session that only read or a write followed
by a start, the page shows the two steps and no note.

MENU shows `POWER-CYCLE THE ESC TO RUN IT` once the screen has taken the
signal pin, also after a bind that failed, until DShot restarts from the
leave page or the ESC answers DShot again.

If the USB cable is pulled during a write while the board runs on its own
cell, the board keeps the signal pin and sends nothing. On USB power alone
the board stops with the cable, and plugging it in again restarts the
board with DShot on SIG. Either way: power the ESC off before you plug the
cable in again, then flash again.

---

## Not checked yet

- That any desktop tool works through the board.
- That each tool signals an open port (`PORT OPEN`).
- That ESC-Configurator and the AM32 configurator accept the board's
  answers as a flight controller's.
- That the board hears the ESC's answer on the signal wire, and the echo's
  timing.
- What an ESC does with DShot after a flash cut part-way.
