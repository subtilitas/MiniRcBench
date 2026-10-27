# Hitec servo reading

Status: planned, nothing built. The owner decided its open questions on
2026-10-23 ([decisions of the owner](#decisions-of-the-owner)). The classic
family depends on the switched servo supply, step 5 of
[kst-programming.md](kst-programming.md#work-breakdown), which is not built.
rcbench's Hitec module, the source, is a wire layer: frames, transactions,
reply decoders and an RP2350 pin driver on the PIO block ([terms](#terms)
below). It has no session and no register model. What upstream records as run
on a wire is in [what has run upstream](#what-has-run-upstream); no write of
any kind is among it. The design is a user of the pin claim of
[pin-claim.md](pin-claim.md), and is written for the system the KST work of
[kst-programming.md](kst-programming.md) leaves after its step 5: MENU with a
KST SERVO row, the switched servo supply on GP0, the shared list `ui_list`,
the shared hold `ui_hold` and the 36 px header.

## Terms

Hitec is a servo brand; the name is used only to say which servos
this works with. A classic servo is one of Hitec's HS-5xxx and HS-7xxx
series, a D series servo one of its D series. Both program over the
servo's signal wire, half duplex, as UART-style characters: a start bit,
8 data bits least significant first, a stop bit (8N1). UART is a universal
asynchronous receiver-transmitter. PIO is the RP2350's programmable
input/output block, SM a PIO state machine, DMA direct memory access, RAM
random-access memory, ESC electronic speed controller, DShot the digital
ESC protocol, KISS the serial telemetry of the KISS ESC line, SD Secure
Digital, SDIO Secure Digital input/output, FAT File Allocation Table, USB
Universal Serial Bus, UI user interface, CI continuous integration, EEPROM
electrically erasable programmable read-only memory, GPIO general-purpose
input/output, ADC analogue-to-digital converter, ID identifier. IOVDD is
the RP2350's GPIO supply, 3.3 V on this board. SIG and AUX are the ESC and
KISS signal pins as [pin-claim.md](pin-claim.md) defines them. Open drain
means the pin drives low or lets go and never drives high; push-pull means
it drives both levels. PULL-UP and PRECHARGE are rcbench's two ways to
receive a D series answer: a resistor that lifts the released line, or the
pin itself lifting it before each sample ([electrical](#electrical)).

Source: rcbench, commit 4a437488, MIT, same holder: the core in
`protocols/hitec` (`hitec_wire.c`, `hitec_pio.c`, `hitec_pre.c` and their
3 headers, 2,029 lines), the RP2350 driver
`protocols/hitec/rp2350/hitec_line.{h,c,pio}` (997 lines), the module's
`README.md`, the HITEC link page `shared/outputs/hitec_port.c` as a pattern
only, and `docs/LinkHitec.md` and `docs/BenchConsole.md` for the measured
behaviour. The KST files in `lib/rcbench/` stay at commit 453cf334. At
4a437488 one of them differs in code: `kst_store.h` names its directory
`CARD_KST_CONF` from `card_tree.h` in place of `"KST"`.

## What it does

On SIG, with DShot released, for the family the user chose before the
claim:

1. classic: brings the servo into serial mode with the switched servo
   supply: off, on, and a listen for `+` from the switch-on
   ([decision 2](#decisions-of-the-owner)); then sends the first command
   of a session, `0x67`, which the servo answers with a value and `+`;
2. reads one address: classic EEPROM (`0x61` with an address of 0x00 to
   0xFF, answered with the value and `+`), or a D series register (a 5-byte
   read request; the answer carries a 16-bit value);
3. classic: reads the position (`0x65`, answered with its high and low
   byte);
4. dumps a range of addresses, one read each, and shows the values;
5. saves a dump to the SD card as a text file, `HITEC/HTnnn.TXT`.

Not in this design ([decision 1](#decisions-of-the-owner)): any write, EEPROM,
register, data memory or move. There is no register map of either family in
rcbench, so the screen shows addresses and raw values, not named settings. A
dump before and after a change made with Hitec's own programmer is the way to
learn what an address holds.

Why no write: no write frame of this code has been on a wire, and a byte
written to an address whose meaning is not known can leave a servo in a
state nobody can name. A read changes nothing that is known; whether a read
of an address the servo does not use has an effect is not known.

### What has run upstream

rcbench's records do not agree with each other:

- The module README says D series reads on a D485HW, with a pull-up, are
  the only part of the module that has run on a wire, "nothing else of it
  has".
- The same README reports the answer delay of 153 D series reads by PRECHARGE
  on GP13 of rcbench's IOmcu, its input/output microcontroller, at the
  module's precharge values (2026-10-20), and 133 of 133 reads of 19 registers
  by PRECHARGE at other values (20 ns precharge, 80 ns sample). It lists "a
  long run at the values this module chose" as not measured.
- `hitec_line.h` says no byte of the driver has been on a pin.
- The module README says an HS-7955TG, an HS-5955TG and an HS-5645MG "enter
  serial mode and answer every read on an open-drain port" at a 5.0 V supply;
  `docs/LinkHitec.md` says the same in other words. They do not say how serial
  mode was entered, or whether those reads ran on this driver.

None of it ran on GP29 or on this board.

## Wire

From rcbench's module README. The character formats, frames and command
bytes are measured on an HS-5645MG, an HS-7955TG and a D485HW, with the
HPP-21 Plus and HFP-30 programmers, at a 1.4 V threshold.

| | Classic | D series |
|---|---|---|
| Bit rate | 19,200 bit/s, 52.08 µs a bit | 115,200 bit/s, 8.68 µs a bit |
| Polarity | the line idles high, a start bit is low | inverted: inside a frame the line rests low, a start bit is high |
| High level | made by the servo, through about 1 kΩ | made by a pull-up, or by the master's precharge ([electrical](#electrical)) |
| Request | 4 bytes `cmd p1 p2 sum`, the 4 bytes sum to 0 modulo 256 | read: 5 bytes `96 ID addr 00 sum`; `sum` is the low byte of the sum of every byte after `96` |
| Answer | 2 bytes, starting 508 µs to 517 µs after the start of the master's last byte after reads, up to 614 µs after writes and after the first read of a session | 7 bytes `69 ID addr 02 lo hi sum`, starting 5.9 ms to 6.5 ms after the end of the request with the HFP-30; this module measured 5.93 ms to 6.52 ms over 153 reads by PRECHARGE |

The ID byte of a D series request is set on the screen, 0x00 to 0xFF, 0x00 by
default, the value the bench reads on a D485HW ([decision
5](#decisions-of-the-owner)). The sum counts it, as every byte after `96`. The
answer's ID byte is shown and not checked: rcbench reads 0x00 from a D485HW,
and 0xFF and 0xFE are reported from a servo with ID 0, not measured. Whether a
servo answers a request with another ID than its own is not known.

Time per read, computed from the table and the driver's windows, not
measured:

- classic: about 3.6 ms. The PIO stream's front padding, 6.5 bit times or
  338.5 µs, the 4 request bytes at 572.9 µs start to start, and the
  2 answer bytes to the stop bit of the second. A dump of 256 addresses
  takes about 0.9 s. An address that does not answer waits out the 1.5 ms
  window.
- D series: 7.2 ms to 7.8 ms. A 117.2 µs lead, 5 request bytes, the
  measured delay of 5.93 ms to 6.52 ms and 8 answer bytes. A dump of
  256 addresses takes about 1.9 s. An address that does not answer waits out
  the 25 ms window, so a dump of 256 silent addresses takes about 6.5 s.
- The spacing a servo needs between two reads is not measured upstream; a
  dump sends the next request at the first core1 pass after the last read
  ends.

The core1 loop polls the line every pass; rcbench asks for a poll about
every 1 ms. The time of a pass with a PRECHARGE decode in it is not
measured on the RP2350; on the host one decode of a full 30 ms window
takes 3.4 µs.

## Electrical

| | Classic | D series |
|---|---|---|
| Pin | SIG only, GP29 by default | SIG only |
| Drive | open drain; the driver refuses push-pull | push-pull for the request, which PRECHARGE needs (below) |
| Receive | the line released; the servo drives the high level | PRECHARGE: the pin drives high for 100.0 ns of every 1086.7 ns and samples 113.3 ns after the release; PULL-UP is not offered ([decision 4](#decisions-of-the-owner)) |
| Pull at open and after close | the pad's pull-down, 50 kΩ to 80 kΩ | the pad's pull-up, its input inverted while open |
| Servo supply | the converter on GP0, 5 V nominal (KST decision 16); 5.0 V in rcbench's runs, other supplies not tested | the converter with `SERVO SUPPLY` at `GP0`, else the user's |

**Classic high level.** A classic servo drives the high level itself,
through about 1 kΩ, from a supply of 5.0 V in rcbench's runs. The line
voltage is not measured. If the servo drives its signal wire to its
supply, a 5 V servo puts 5 V on GP29. MiniRcBench's README allows 3.6 V on
every GPIO, and GP29 is an ADC pin whose 5.5 V rating is not checked
([pin-claim.md](pin-claim.md#signal-level)); the module README states
5.5 V for rcbench's own pins. The module README also names a classic servo
driving the line while the master is unpowered, against the 3.63 V rating
of an unpowered pin. When the master drives low against the servo's 1 kΩ,
the pad sinks about 5 mA at 5 V. On a classic line the driver sets no drive
strength, so the pad keeps its reset value, 4 mA; the low level at 5 mA is
not specified. This is
[decision 3](#decisions-of-the-owner): until the level is measured, the
classic family does not open on a bench pin.

**D series high level.** The servo pulls the line low and leaves it
released for a high level; it has a 3 kΩ pull-down of its own, reported
with another programmer and not measured. Which side drives each level of
an answer is not measured. The
pad's pull-up alone leaves the line below the 1.4 V threshold, and the
servo answers no read. rcbench offers two ways to receive an answer:

- PULL-UP: a resistor from the signal wire to a supply. 2.2 kΩ to the
  servo supply is tested on a D485HW. Against the 3 kΩ pull-down that is
  58 % of the servo supply: 2.9 V at 5.0 V, 4.8 V at 8.4 V, above 3.6 V
  from a supply of 6.24 V. A resistor to 3.3 V needs 1.95 kΩ or less to
  give the RP2350 its 2.0 V input high; that value is computed, not
  tested.
- PRECHARGE: no resistor. A second SM drives the pin high for 100.0 ns,
  releases it, waits, and samples it, every 1086.7 ns at 150 MHz. A servo
  pulling low has pulled the line down by the sample. The evidence and its
  contradictions are under [what has run upstream](#what-has-run-upstream).
  Not measured: the pad current while the precharge meets a servo pulling
  low, a precharge into a servo whose supply is off, and any clock but
  150 MHz. This board runs at 150 MHz. rcbench's port cuts a PRECHARGE
  receive when it sees the servo supply go off. With `SERVO SUPPLY` at
  `GP0` the bench switches the supply itself and starts no receive while it
  is off. With `NONE` this board cannot see the user's supply, so a receive
  runs to its end: up to 25 ms for one read, about 6.5 s for a dump of
  silent addresses; the dump view then reads `KEEP THE SERVO POWERED` while
  a dump runs.

PRECHARGE needs nothing on the lead and never lifts the line above 3.3 V.
A request on a PRECHARGE line goes out with no sampler, so its high bits
come from the pin itself: the request is push-pull, about 1.1 mA into the
servo's 3 kΩ.

**Ground and supply.** As for the servo tester
([pin-claim.md](pin-claim.md#wiring)): the servo on its own supply, the
grounds joined once at the board, the board powered before the servo supply.
How a classic servo enters serial mode is not recorded upstream; this design
powers it up through the switched supply of
[kst-programming.md](kst-programming.md#switched-servo-supply) ([decision
2](#decisions-of-the-owner)), so the classic family needs `SERVO SUPPLY` at
`GP0` and its converter. A D series read needs no power-up in rcbench's runs.
With `SERVO SUPPLY` at `GP0` the bench switches a D series servo's supply on
at the session's first READ or START and off at the release; with
`NONE` the user's supply stays as it is.

**After the claim.** The claim's unplug page applies as for every servo
screen: DShot restarts only on RESTART DSHOT. A D series line closes with
the pad's pull-up on, not the pull-down the other jobs leave
([pin-claim.md](pin-claim.md#restoring-dshot)). The DShot pump puts its
own pull-up on SIG when it binds again, so the difference lasts only until
RESTART DSHOT.

## Resources

During a claim the DShot pump and the KISS receiver have released their
SMs, so pio2 is empty, pio0 is empty, and pio1 holds the SDIO driver's
32 words ([pin-claim.md](pin-claim.md#pio-and-dma-budget)).

| | Words | SMs | DMA channels | Block |
|---|---|---|---|---|
| Hitec line | 29 of 32, one copy per open line | 1 | 2 | pio2 or pio0, the first with room that reaches the pin |
| PRECHARGE sampler | 7 at 150 MHz, 12 at most | 1 | 1 | the other of pio2 and pio0 |
| SDIO, from the first card initialisation | 32 | 2 | 2 | pio1 |
| Panel | 0 | 0 | 1 | |

With PRECHARGE the line takes 2 SMs and 3 DMA channels. With the SDIO
driver initialised and the panel's channel that is 4 of 12 SMs and
6 of 16 DMA channels. No other core1 job runs during the claim. RAM: one
`hitec_line_t`, 4,100 bytes with arm-none-eabi-gcc 14.2, 3,712 of them the
sampler's buffer, a static of the job. No interrupt and no PIO interrupt
flag is used. `clk_sys` stays at 150 MHz while a line is open, as rcbench
requires: the classic divider is exact (488 + 72/256), the D series one
0.002 % short (81 + 97/256), and rcbench accepts 0.05 %.

## Where it runs

Core1, as the KST job does:

- `PinUser::Hitec`, a row of `PIN_USERS` with `Core1Job::Hitec`, the
  unplug page and the ESC check on, and a row of `s_ops[]` in
  `src/core1_job.cpp`.
- The release aborts what runs and takes the abort's capture first; then,
  with `GP0`, the claim end of [supply](#supply), and the line closes;
  GP0 is released last.
- The family is chosen before the claim. `hitecJobSetFamily(family)` writes it
  under the job's critical section, on core0, and only while no core1 job
  runs; the screen calls it, then begins the claim, and the bind reads it and
  opens the line for that family. RETRY after a bind error
  (`pinClaimRetry()`) and BACK on the unplug page (`pinClaimKeep()`) bind
  again with the family it holds. An open failure is a
  bind error, reported by `pinClaimJobError()` as
  `GP29 COULD NOT BE TAKEN (PIO)`, `(DMA)` or `(CLOCK)` with RETRY, as on the
  other servo screens.
  Another family means leaving through the unplug page and choosing again.
- `src/hitec_job.{h,cpp}`: one command block, one status block, both under one
  critical section, as `src/kst_job.h`. Commands: `Enter` (classic: the entry
  of [supply](#supply), then `0x67`; D series with `GP0`: the switch-on of
  [supply](#supply); D series with `NONE`: nothing), `Read(addr)`, `Position`
  (classic `0x65`), `Dump(from, to)`, `Stop`; a D series `Read` and `Dump`
  carry the ID. BACK leaves through `pinClaimEnd()` and the release; with
  `SERVO SUPPLY` at `NONE` there is no leave sequence for either family, with
  `GP0` the claim end of [supply](#supply). The screen sends `Enter` itself
  before the first read of a session and after the supply went off. The status
  carries the state, the last read's value and decoder result, the dump's
  progress with a value and a decoder result for each address, and the supply
  fields of [supply](#supply).
- `src/hitec_line_port.{h,cpp}`: opens and closes the line on the claimed
  GPIO, maps `hitec_line_err_t` to `CORE1_JOB_PIO`, `_DMA` and `_CLOCK`,
  and is the one file the host suite replaces with a fake.
- A heartbeat lapse (250 ms) lets the running read end and starts no
  other. A dump stops between two reads and keeps the values read; START
  repeats it from its first address. The screen reads
  `DUMP STOPPED: THE SCREEN STALLED`, and, with `SERVO SUPPLY` at `GP0`,
  `THE SUPPLY WENT OFF` ([supply](#supply)).

### Supply

With `SERVO SUPPLY` at `GP0`, the job drives GP0 through `servo_supply.h`
of [kst-programming.md](kst-programming.md#firmware), under the rules
KST's job follows there. With `NONE` it never touches GP0.

- **Bind.** `servoSupplyBind()` after the claim's ESC check, as for KST:
  GP0 driven low, the supply off. GP0 is not driven before the bind.
- **Classic entry.** The supply off for `KST_SUPPLY_OFF_MS`, 2000 ms, from
  the switch-off command; rcbench's console example holds it off 1000 ms.
  Then on, and a LISTEN for the first byte from the switch-on in a window
  of 2,000 ms, the console's value, decoded by `hitec_listen_decode()` as
  `+`. Whether the servo needs an answer byte, and which, is not recorded:
  the first build sends none, and bring-up tries the programmers' timing
  of 0.52 ms to 0.55 ms after the servo's byte. Then `0x67`.
- **D series.** `Enter`: the supply off for the same 2000 ms, then on,
  then `KST_SUPPLY_WAIT_MS`, 1000 ms from the switch-on command, before the
  first request, KST's wait after a switch-on. The converter's start-up
  time and the servo's boot time are not known until bring-up; 1000 ms is
  KST's value, not a measurement. No listen.
- **Off.** The supply goes off at STOP during an `Enter`, in any view, after a
  listen that gets no `+`, after a `0x67` that is not answered with a value
  and `+`, and at every claim end. At a heartbeat lapse it
  goes off once the running read has ended; a lapse during the entry's listen
  or the D series wait cuts it and switches the supply off at once, as KST
  ends its time between switch-on and entry. It goes on only while the
  heartbeat is trusted. After it went off, a session needs `Enter` again
  before READ, POSITION or START; the screen sends it at its next READ,
  POSITION or START.
- **Claim end.** BACK, from any view, after the supply was on: the supply
  off, `KST_SUPPLY_OFF_MS` from the last switch-off command, then the line
  closed, then `servoSupplyRelease()`, then the unplug page; KST decision
  18 sets the same wait before DShot can reach the servo. The screen reads
  `SUPPLY OFF FOR 2.0 S` and BACK does nothing during the wait.
- **Status.** The job's status carries the level it drives,
  `servoSupplyIsOn()`, and the milliseconds since the last switch command,
  for the views' `SUPPLY OFF 2.0 S`, the D series wait and the listen's
  progress.
- **ESC on the lead.** The converter's output is the lead's middle wire, so an
  ESC on the lead gets it on its BEC (battery eliminator circuit) wire, and
  the claim's ESC check misses an ESC without bidirectional DShot. The entry
  view, and the D series views with `GP0`, read in amber:
  `AN ESC ON THIS LEAD GETS THE CONVERTER'S OUTPUT ON ITS BEC WIRE.`
- GP0 has the Hitec job as another core1 user beside KST's job and, once
  [servo-tester.md](servo-tester.md) decision 20 is built, the servo
  tester.

The decoders run on core1 in the job:
`hitec_c_decode()` with the reply
shape of the command, `hitec_d_decode()` with the address asked for. The D
decoder checks the first 7 bytes and ignores the 0x00 the D485HW sends
after its answer; it reports the ID byte and does not check it.

## Screens

Entry: MENU → HITEC SERVO, one line in `ui_registry.cpp`. `HITEC SERVO` is
11 characters, 130 px at scale 2, and ends at x 145, before `PINS BUSY` at
x 169. The row follows KST SERVO ([decision 6](#decisions-of-the-owner)).
As the fifth row it makes MENU seven rows, which scroll 0 px to 36 px, as
[menu.md](menu.md) designs, so SETTINGS, the last row, shows 6 px at
scroll 0 and takes no tap until MENU is scrolled.

Views:

| View | What it shows | Controls |
|---|---|---|
| Family | CLASSIC (HS-5XXX, HS-7XXX) and D SERIES, with the wiring note of each; no pin is claimed yet | CLASSIC, D SERIES, BACK |
| Entry, classic | `SUPPLY OFF 2.0 S`, then `LISTENING FOR THE SERVO` with the window's progress; a result word and RETRY when no `+` comes or `0x67` gets no valid answer; the amber ESC line of [supply](#supply) | STOP, RETRY, BACK |
| Read | the address, the last value read, the answer's ID byte (D series), the decoder's result as a word (`SILENT`, `LATE`, `SUM` and the rest of rcbench's 14 codes); D series with `GP0`, `SUPPLY OFF 2.0 S` and the wait during `Enter`, and the amber ESC line of [supply](#supply) | − and + for the address, − and + for the ID (D series), READ, DUMP, POSITION (classic), BACK; STOP while a D series `Enter` runs |
| Dump | the range, the progress, and the values on `ui_list`, 40 px rows at a 42 px pitch. Classic: 8 addresses a row, `0x30` and 8 two-digit values, 28 characters, 167 px; 256 addresses are 32 rows, 1,344 px of content. D series: 4 addresses a row, `0x30` and 4 four-digit values, 24 characters, 143 px; 256 addresses are 64 rows, 2,688 px. An address without a value shows `--` or `----`; a tap on a row shows the decoder words of that row's empty addresses under the list, each in its address's column: one line for a D series row (4 words, 35 characters at most), two lines of 4 for a classic row. D series with `GP0`, as the Read view: the `Enter` texts and the amber ESC line | FROM and TO steppers, START, STOP, SAVE, BACK |
| Leaving | with `GP0`, after the supply was on: `SUPPLY OFF FOR 2.0 S` while the claim end waits | none; BACK does nothing |
| Unplug | the claim's unplug page | RESTART DSHOT, BACK |

A classic `Enter` sent from the Read or Dump view, after the supply went off,
opens the Entry view, which returns to the view it came from once `0x67` is
answered; with no `+`, or no valid answer to `0x67`, it stays, with the
decoder's word and RETRY.

The claim refusals, the bind errors and RETRY are those of the KST screen:
`AN ESC ANSWERS ON GP29.`, `GP29 COULD NOT BE TAKEN (PIO)`, `PINS BUSY`.
CLASSIC is drawn disabled, with `LINE LEVEL NOT MEASURED` under it, until
bring-up has measured the line ([decision 3](#decisions-of-the-owner)),
and with `NEEDS SERVO SUPPLY GP0` under it while `SERVO SUPPLY` is `NONE`.

A classic entry whose answer is not a value and `+` reads
`NO SERVO IN SERIAL MODE` with the decoder's word. A D series read that
gets no answer reads `SILENT`.

## Storage

SAVE writes the dump that is up as `HITEC/HTnnn.TXT`, the next free number
from 001 to 999, in a folder at the card's root as the KST files' `KST/`.
rcbench reserves `BENCH/HITEC/CONF` for its Hitec programmer's files
(`shared/logfile/include/card_tree.h`), and nothing reads it yet; this
board's card holds no BENCH
tree. The file goes through FatFs on core0, under the mount rule of
`src/card_dir.cpp` (`cardMounted()`): the card mounted by the logger and not held by a USB
host. The file is plain text, one line per address. A D series value has
four hex digits, a classic one two. A D series file names the request's ID
in its header and each answer's ID byte on its line; a classic file has no
ID:

```text
Hitec dump, format 1
Family: D series
Request ID: 0x00
0x32 = 0x01F4 (answer ID 0x00)
0x33 = SILENT
```

```text
Hitec dump, format 1
Family: classic
0x12 = 0x5A
```

There is no load: with no write there is nothing to load a file into.
SAVE is offered only while no dump runs. A card call longer than 250 ms on
core0 counts as a heartbeat lapse: SAVE's own walk of `HITEC/` and its
write, and a blackbox log, which keeps writing to the card during a claim.
The lapse stops a running dump as above and, with `GP0`, switches the
supply off. The dump is in RAM and stays; the session needs `Enter` again
(classic) or a new switch-on (D series). The Hitec job has no card phase
like KST's step 5, so it does not inherit KST's open question on the
15 s cap.

## Porting

- Copy the 9 source files unchanged from rcbench commit 4a437488 into
  `lib/rcbench/protocols/hitec/`: `hitec_wire.c`, `hitec_pio.c`,
  `hitec_pre.c`, `include/hitec_wire.h`, `include/hitec_pio.h`,
  `include/hitec_pre.h`, `rp2350/hitec_line.h`, `rp2350/hitec_line.c` and
  `rp2350/hitec_line.pio`. The module's `README.md`, whose links point into
  rcbench, and its two `CMakeLists.txt` stay out, as for KST.
  `lib/rcbench/README.md` gains a table of the Hitec files at 4a437488,
  and its opening sentence names both commits; the existing rows keep
  453cf334.
- Copy rcbench's portable tests unchanged into `test/rcbench/`:
  `test_hitec_wire.c` (11 cases), `test_hitec_pre.c` (18) and
  `test_hitec_pio.c` (22) with `hitec_pio_model.h` and the fixture
  `fixtures/hitec_line_words.txt`, and `hitec_headers.cpp`. `greatest.h`
  is already in `test/rcbench/`; rcbench's copy is byte for byte the
  same at 4a437488. `hitec_headers.cpp` is C++, so `test/Makefile` gains a
  rule for a C++ test of that kind beside its C tests. The port suites
  (`test_hitec_port*.c`) need rcbench's link pages and supply model and
  are not copied.
- `src/hitec.cmake` and `test/hitec.mk`, as `src/kst.cmake` and
  `test/kst.mk`: the sources, the include folders, the PIO header, the C
  tests and the two path defines `test_hitec_pio.c` reads.
- CI: the two include folders join the config-permutation job's include
  lists, in both places that list them.
- Licence: every copied file but the fixture carries the licence line
  `SPDX-License-Identifier: MIT`. The module's 4 headers carry rcbench's
  sentence on the Hitec name; the C files, the PIO file, the tests and
  `hitec_pio_model.h` do not. New and adapted files that hold Hitec code
  carry that sentence, and adapted ones also start with
  `Adapted from rcbench <path>, commit 4a437488, MIT.`, as
  [kst-programming.md](kst-programming.md#licence-headers) requires of the
  KST files; the built KST files in `src/` do not carry them yet. README.md's
  "Legal notice and disclaimer" names Hitec beside KST and Chaservo and states
  the origin ([decision 7](#decisions-of-the-owner)).
- `hitec_line.c` is not in rcbench's host build either. Here it is
  compiled by the firmware build only, under `-Werror`, as KST's driver
  is; `make syntax` typechecks `hitec_line_port.cpp`, with the driver's
  header.

## Testing

- rcbench's four tests above, run by `make` and CI.
- `test_hitec_job.cpp`: the job from the job table on a fake line
  (`test/fake_hitec_line.cpp`, the `hitec_driver_t` with scripted captures).
  The answers are the bench captures in rcbench's `test_hitec_wire.c` tables.
  Cases: the bind with each family and its three refusals; each command in
  each state; a classic entry, `0x67` and a read; a D series read at ID 0x00
  and at ID 0x2A, the request's sum counting the ID; each decoder result
  reaching the status; a dump of 256 addresses with one `SILENT` address in
  it; STOP mid-dump; a heartbeat lapse mid-dump; the release aborting a
  running read; the classic entry on the fake supply: 2000 ms off, on, a `+`
  within the 2,000 ms window, `0x67`; no `+`, then the supply off and RETRY;
  STOP in the entry switching the supply off; a lapse in the entry's off
  phase, with the supply never switched on; a lapse during the entry's listen
  and during the D series wait, each cutting it and switching the supply off
  at once; a D series `Enter` waiting 1000 ms after the switch-on before its
  first request; a `+`, then a `0x67` that gets `SILENT`: the supply off and
  RETRY; STOP in the Read view during a D series `Enter`, switching the supply
  off; a classic re-entry from the Read view after a lapse, through the Entry
  view, with no `+`, then RETRY; STOP in the Dump view during a D series
  `Enter`, switching the supply off; a lapse mid-dump with `GP0`, the supply
  off, then `Enter` required before START; a D series session with `GP0`
  switching the supply on at its first READ, off at a lapse and on again at
  the next READ; every claim end after the supply was on waiting 2000 ms off
  before the line closes and GP0 is released, with BACK doing nothing during
  it; with `NONE`, GP0 untouched through a whole claim; GP0 not driven before
  the bind; no transaction that is a write, counted over every test.
- `test_hitec_ui.cpp`, by control id ([menu.md](menu.md)): MENU opens the
  screen, the family choice before the claim, the claim, READ, POSITION,
  DUMP and STOP, SAVE with a card, no card, a card held by USB, BACK in
  every view, RESTART DSHOT; `checkControlTable()` in each view; a
  preview, `docs/hitec-preview.png`.
- The changed MENU tests: the row count in `test_menu.cpp` and `test_ui.cpp`,
  `openSettings()`, which scrolls MENU before it taps SETTINGS, and
  `docs/menu-preview.png`.
- Coverage holds `tools/coverage.py`'s floors, 93 % in total and 85 % per
  file, with the README sentence and the STATUS.md table written by it.

## Work breakdown

1. Copy the module and the four tests; `src/hitec.cmake` with its `include()`
   in `CMakeLists.txt`, `test/hitec.mk` with its `include`, `FEATURE_*`,
   `INCLUDES +=` and `SRCS` lines in `test/Makefile`, its include folders in
   the two C test rules and in the coverage rule, the C++ test rule, CI
   include lists, `lib/rcbench/README.md`. The firmware links the driver;
   nothing opens it.
2. `PinUser::Hitec`, `Core1Job::Hitec`, `src/hitec_job.*`,
   `src/hitec_line_port.*` with `$(HITEC_DEVICE_SRCS)` on the `DEVICE_SRCS`
   line of `test/Makefile`, the fake line and `test_hitec_job.cpp`.
   [pin-claim.md](pin-claim.md) gains Hitec in the enums of its
   application programming interface section, its screen and job table,
   its core1 table, its heartbeat table, its PIO and DMA budget and every
   sentence that names the servo and KST screens as the claim's users, its
   Leaving and Restoring text names the D series pull-up, and its wiring
   section names Hitec beside KST for the switched supply.
   [kst-programming.md](kst-programming.md)'s supply sections and the
   comments of `src/servo_supply.h` name the Hitec job as a GP0 user beside
   KST's.
3. `src/ui_hitec.*`, its registry line, the dump file
   (`src/hitec_files.*`), `test_hitec_ui.cpp`, the MENU test changes, the
   preview and its entry in `tools/make_previews.py`.
4. README section, wiring with the warning that an ESC on the lead gets the
   converter's output on its BEC wire, legal notice and MENU table row; wiki
   pages `Hitec-Reading.md` and its German counterpart `Hitec-Auslesen.md`,
   the pair in `tools/check_wiki.py`, their entries in `_Sidebar.md`,
   `Home.md` and the screens pages; STATUS.md and CHANGELOG.md.
5. Bring-up; the first item before a classic servo is put on GP29:
   - the classic line's high level on a scope, the servo's signal lead on a
     68 kΩ resistor to ground, near the pad's pull-down, and not on the
     board, while something makes the servo drive the line: a session run
     by Hitec's programmer on the same lead, or a D SERIES session with
     `SERVO SUPPLY` at `GP0`, which switches the converter on at its
     first READ. A line that never goes high gives no result, and CLASSIC stays
     off;
   - the classic entry of [decision 2](#decisions-of-the-owner) on the
     owner's servos: whether a `+` comes after the switch-on, and whether
     the servo needs an answer byte;
   - the converter's start-up and the D series servo's first answer after
     a switch-on, against the 1000 ms wait;
   - a D series read by PRECHARGE on the owner's servo, with the answer
     delay compared against 5.93 ms to 6.52 ms;
   - a dump of each family compared with what Hitec's programmer shows.

About 5 to 7 days for 1 to 4, not measured. Bring-up depends on the
servos and is not estimated. The design comes after KST step 5, the
switched supply, which the classic entry uses; KST step 4 adds MENU's
sixth row before it.

## Decisions of the owner

Decisions of the owner, 2026-10-23, questions 1 to 7.

1. **Writes.** None in this design: reads, position and dumps only. A
   write comes once a register map is known for at least one servo and
   one write with read-back has run at bring-up.
2. **Classic entry.** By the switched servo supply of
   [kst-programming.md](kst-programming.md#switched-servo-supply): off,
   on, and a listen for `+` from the switch-on. Not chosen:
   `0x67` alone with the supply on, and the user powering the servo up by
   hand during the listen. How a classic servo enters serial mode is not
   recorded upstream: rcbench's listen decoder's comment names "the
   classic entry's `+`", and its bench console's classic session includes
   a supply cycle with a 2 s listen for `+`, run after `0x67` and a read.
   The programmers answer a servo's first byte 0.52 ms to 0.55 ms after
   it, with a byte whose value is not recorded.
3. **Classic line level.** Measured at bring-up before CLASSIC is enabled;
   a protecting lead, if the measurement asks for one, is designed from
   that measurement. A lead has limits: a series resistor R makes the
   master's low reach the servo as V × R / (R + 1 kΩ), 2.5 V at R = 1 kΩ
   and 5 V, which may read as high; a clamp to 3.3 V feeds an unpowered
   board through the clamp.
4. **D series receive.** PRECHARGE only. PULL-UP is not offered: 2.2 kΩ to
   the servo supply puts more than 3.6 V on the line from a 6.24 V supply,
   and a value to 3.3 V is not tested.
5. **D series ID.** A stepper, 0x00 to 0xFF, 0x00 by default. It is not
   stored: every opening of the screen starts at 0x00.
6. **MENU row.** HITEC SERVO follows KST SERVO; SETTINGS becomes the
   seventh row, which takes a tap after a scroll.
7. **Origin wording.** The legal notice states rcbench's origin claim for
   the Hitec code: wire captures of the project's own hardware and Hitec's
   published D series manual; no vendor code and no vendor document is
   included. Confirmed by the owner.

## Choices made in this design

1. **SIG only.** As KST: the wire a servo is plugged into for the servo
   tester. AUX is not offered.
2. **One family per claim.** The family is chosen before the claim, so a
   bind failure is reported as a bind failure. Another family leaves
   through the unplug page.
3. **No session in rcbench, none invented here.** The job runs single
   transactions and dumps. A register map, once known, adds named fields
   in a later change.
4. **The switched supply for D series too, when it is set.** With
   `SERVO SUPPLY` at `GP0` the converter is the servo's only supply, so a D
   series session switches it on at its first READ or START, and
   again after it went off ([supply](#supply)); a D series read needs no power
   cycle of its own in rcbench's runs.
5. **No card phase.** SAVE's card calls may lapse the heartbeat and switch
   the supply off; the dump stays in RAM and the session is entered again.
   A card phase, as KST's step 5 designs, would carry KST's open question
   on the 15 s cap into this design.
