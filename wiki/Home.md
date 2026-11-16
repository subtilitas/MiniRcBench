# MiniRcBench

A bench tester for RC (radio-controlled) model parts on the Waveshare
**RP2350-Touch-LCD-2.8**. It tests one ESC (electronic speed controller) over
bidirectional DShot, and drives servos. With two wires to the ESC, the board
is the throttle source, the telemetry display and the log recorder. It needs
no flight controller, laptop or Betaflight.

<img src="img/tester-armed.png" width="240" alt="The tester screen, armed">

---

## English

**Start here:** [Setup](Setup): parts, the cable and installing the
firmware. Then [First Run](First-Run): wiring, power, and the first motor
run.

| Page | What it covers |
|---|---|
| [Setup](Setup) | What you need, the adapter cable, installing the firmware |
| [First Run](First-Run) | Wiring an ESC, powering up, arming, first throttle input |
| [Wiring](Wiring) | Every connection on one sheet: the board's connectors, an ESC, a servo with a hand switch or on the switched supply |
| [The Screens](The-Screens) | Every screen and every control, in order, among them [ESC COMMANDS](The-Screens#esc-commands) and [SETUP](The-Screens#setup) with its DIAGNOSTICS section |
| [Telemetry](Telemetry) | What each tile means, where the value comes from, and why a tile reads `--` |
| [SD Logging](SD-Logging) | LOGS: recording to microSD, [the run summary](SD-Logging#the-run-summary) beside each log, and opening the result in [logwiju](https://subtilitas.github.io/logwiju/) |
| [AM32 Configuration](AM32-Configuration) | ESC SETTINGS: reading, editing and writing the settings of an AM32, Bluejay or BLHeli_S ESC |
| [Servo Tester](Servo-Tester) | Driving one or two servos: start, stop, drag, sweep, profiles, and [the servo log](Servo-Tester#the-log-on-the-card) of each run |
| [KST Programming](KST-Programming) | Reading and writing the settings of a KST or Chaservo servo, with backup and restore |
| [Hitec Programming](Hitec-Programming) | Reading and writing the settings of a Hitec D series or classic servo, with backup and restore. Not tested on hardware |
| [Receiver](Receiver) | S.BUS MONITOR, reading a receiver's S.BUS output: channels, failsafe, frame rate, errors; and S.BUS OUTPUT, sending S.BUS to a flight controller. Not tested on hardware |
| [USB Bridge](USB-Bridge) | ESC PROGRAM's second entry: a desktop ESC tool reaching the ESC's bootloader through the board's USB port. Not tested on hardware |
| [Angle Meter](Angle-Meter) | SERVO → ANGLE METER: incidence, decalage, dihedral and control-surface throw from the board's own sensor. Not tested on hardware |
| [Troubleshooting](Troubleshooting) | Symptom first, cause second |

> **Before you power anything on:** a motor on a bench can injure. Read
> [First Run](First-Run#safety-first) before the first arm.

**Hardware testing.** No image of this firmware has run on hardware yet. The
firmware's tests run on a host computer.

**Bench plan.** The ordered hardware tests of the bring-up:
[docs/bench-plan.md](https://github.com/subtilitas/MiniRcBench/blob/main/docs/bench-plan.md).

**Licence.** The software, meaning firmware, tests, other tools and
documentation, is MIT. The hardware design files are
[CC BY-NC-SA 4.0](https://creativecommons.org/licenses/by-nc-sa/4.0/):
non-commercial use, credit, and the same licence for what you share. They are
the models and CAD files in `hardware/cad/` and `hardware/mechanical/` (the
board's 3D model among them), and the drawings: the wiring drawing
`wiring.svg` on the Wiring page and `tools/make_wiring.py`, the script that
draws it. The legal code is
[hardware/LICENSE-CAD.txt](https://github.com/subtilitas/MiniRcBench/blob/main/hardware/LICENSE-CAD.txt).
The Waveshare name and logo are Waveshare's trademarks.

---

## Deutsch

**Hier anfangen:** [Einrichtung](Einrichtung): Teile, das Kabel und das
Aufspielen der Firmware. Dann [Erste Schritte](Erste-Schritte): Verkabelung,
Strom und der erste Motorlauf.

| Seite | Inhalt |
|---|---|
| [Einrichtung](Einrichtung) | Was du brauchst, das Adapterkabel, Firmware aufspielen |
| [Erste Schritte](Erste-Schritte) | ESC anschließen, einschalten, scharfschalten, erster Gasbefehl |
| [Verdrahtung](Verdrahtung) | Jede Verbindung auf einem Blatt: die Steckverbinder der Platine, ein ESC, ein Servo mit Handschalter oder an der geschalteten Versorgung |
| [Die Bildschirme](Die-Bildschirme) | Jeder Bildschirm und jedes Bedienelement, der Reihe nach, darunter [ESC COMMANDS](Die-Bildschirme#esc-commands) und [SETUP](Die-Bildschirme#setup) mit dem Abschnitt DIAGNOSTICS |
| [Telemetrie](Telemetrie) | Was jede Kachel bedeutet, woher der Wert stammt und warum eine Kachel `--` zeigt |
| [SD-Aufzeichnung](SD-Aufzeichnung) | LOGS: Aufzeichnen auf microSD, [die Zusammenfassung des Laufs](SD-Aufzeichnung#die-zusammenfassung-des-laufs) neben jeder Aufzeichnung und Auswerten in [logwiju](https://subtilitas.github.io/logwiju/) |
| [AM32-Konfiguration](AM32-Konfiguration) | ESC SETTINGS: die Einstellungen eines AM32-, Bluejay- oder BLHeli_S-ESC lesen, ändern und schreiben |
| [Servotester](Servotester) | Ein oder zwei Servos ansteuern: starten, stoppen, ziehen, Sweep, Profile, und [die Servo-Aufzeichnung](Servotester#die-aufzeichnung-auf-der-karte) jedes Laufs |
| [KST-Programmierung](KST-Programmierung) | Einstellungen eines KST- oder Chaservo-Servos lesen und schreiben, mit Sicherung und Wiederherstellung |
| [Hitec-Programmierung](Hitec-Programmierung) | Einstellungen eines Hitec-Servos der D-Serie oder der klassischen Familie lesen und schreiben, mit Sicherung und Wiederherstellung. Nicht auf Hardware getestet |
| [Empfänger](Empfaenger) | S.BUS MONITOR, den S.BUS-Ausgang eines Empfängers lesen: Kanäle, Failsafe, Framerate, Fehler; und S.BUS OUTPUT, S.BUS an einen Flugcontroller senden. Nicht auf Hardware getestet |
| [USB-Brücke](USB-Bruecke) | Zweiter Eintrag von ESC PROGRAM: ein ESC-Programm auf dem Computer erreicht den Bootloader des ESC über den USB-Anschluss der Platine. Nicht auf Hardware getestet |
| [Winkelmesser](Winkelmesser) | SERVO → ANGLE METER: Einstellwinkel, EWD, V-Form und Ruderausschlag mit dem Sensor der Platine. Nicht auf Hardware getestet |
| [Fehlersuche](Fehlersuche) | Erst das Symptom, dann die Ursache |

> **Vor dem Einschalten:** Ein Motor auf der Werkbank kann verletzen. Lies
> [Erste Schritte](Erste-Schritte#sicherheit-zuerst) vor dem ersten
> Scharfschalten.

**Hardware-Tests.** Kein Image dieser Firmware lief bisher auf Hardware. Die
Tests der Firmware laufen auf einem Host-Computer.

**Prüfplan.** Die geordneten Hardware-Tests der Inbetriebnahme:
[docs/bench-plan.md](https://github.com/subtilitas/MiniRcBench/blob/main/docs/bench-plan.md).

**Lizenz.** Die Software, also Firmware, Tests, übrige Werkzeuge und
Dokumentation, steht unter MIT. Die Hardware-Entwurfsdateien stehen unter
[CC BY-NC-SA 4.0](https://creativecommons.org/licenses/by-nc-sa/4.0/deed.de):
nur nicht kommerzielle Nutzung, Namensnennung, und was du weitergibst, unter
derselben Lizenz. Es sind die Modelle und CAD-Dateien in `hardware/cad/` und
`hardware/mechanical/` (darunter das 3D-Modell der Platine) und die Zeichnungen:
der Verdrahtungsplan `wiring.svg` auf der Seite Verdrahtung und
`tools/make_wiring.py`, das Skript, das ihn zeichnet. Der Lizenztext ist
[hardware/LICENSE-CAD.txt](https://github.com/subtilitas/MiniRcBench/blob/main/hardware/LICENSE-CAD.txt).
Name und Logo von Waveshare sind Marken von Waveshare.

---

## Which board

MiniRcBench runs on the Waveshare RP2350-Touch-LCD-2.8 only: a 240×320
portrait touch panel, an RP2350, and JST-SH connectors instead of 2.54 mm
headers. The image from
[Releases](https://github.com/subtilitas/MiniRcBench/releases) is
`MiniRcBench-<version>-touch-lcd-2.8.uf2`. It does not check which board it
runs on: flashed onto another board, it can drive pins that are wired to other
parts there. The splash screen and **MENU → SETUP** (the `BUILT FOR` row)
name the board the image is built for.

*Welche Platine?* MiniRcBench läuft nur auf der Waveshare RP2350-Touch-LCD-2.8:
ein 240×320-Touch-Display im Hochformat, ein RP2350 und JST-SH-Stecker statt
2,54-mm-Stiftleisten. Das Abbild aus den
[Releases](https://github.com/subtilitas/MiniRcBench/releases) heißt
`MiniRcBench-<Version>-touch-lcd-2.8.uf2`. Es prüft nicht, auf welcher Platine
es läuft: Auf eine andere Platine geflasht, kann es Pins treiben, die dort mit
anderen Bauteilen verbunden sind. Der Startbildschirm und **MENU → SETUP**
(die Zeile `BUILT FOR`) nennen die Platine, für die das Abbild gebaut ist.

<img src="img/splash.png" width="240" alt="The splash screen naming the board">

---

*The project's test suite renders the screenshots on this wiki from the UI
(user interface) code. After each release the Docs workflow (`docs.yml`)
renders them at the release tag and publishes them with the wiki, so they
match the released firmware.*

*Die Testsuite des Projekts erzeugt die Screenshots in diesem Wiki aus dem
Code der Bedienoberfläche (UI, User Interface). Nach jedem Release erzeugt der
Docs-Workflow (`docs.yml`) sie am Release-Tag neu und veröffentlicht sie mit
dem Wiki, daher entsprechen sie der veröffentlichten Firmware.*
