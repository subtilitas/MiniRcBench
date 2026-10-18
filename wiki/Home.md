# MiniRcBench

A bench tester for RC (radio-controlled) model parts on the Waveshare
**RP2350-Touch-LCD-2.8**. It tests one ESC (electronic speed controller) over
bidirectional DShot, and drives servos. With two wires to the ESC, the board
is the throttle source, the telemetry display and the log recorder. It needs
no flight controller, laptop or Betaflight.

<img src="img/tester-armed.png" width="240" alt="The tester screen, armed">

---

## English

**Start here:** [First Run](First-Run): wiring, power, and the first motor
run.

| Page | What it covers |
|---|---|
| [First Run](First-Run) | Wiring an ESC, powering up, arming, first throttle input |
| [The Screens](The-Screens) | Every screen and every control, in order |
| [Telemetry](Telemetry) | What each tile means, where the value comes from, and why a tile reads `--` |
| [SD Logging](SD-Logging) | Recording to microSD and opening the result in [logwiju](https://subtilitas.github.io/logwiju/) |
| [AM32 Configuration](AM32-Configuration) | Reading, editing and writing ESC settings |
| [Servo Tester](Servo-Tester) | Driving one or two servos: start, stop, drag, sweep, profiles |
| [Troubleshooting](Troubleshooting) | Symptom first, cause second |

> **Before you power anything on:** a motor on a bench can injure. Read
> [First Run](First-Run#safety-first) before the first arm.

---

## Deutsch

**Hier anfangen:** [Erste Schritte](Erste-Schritte): Verkabelung, Strom und
der erste Motorlauf.

| Seite | Inhalt |
|---|---|
| [Erste Schritte](Erste-Schritte) | ESC anschließen, einschalten, scharfschalten, erster Gasbefehl |
| [Die Bildschirme](Die-Bildschirme) | Jeder Bildschirm und jedes Bedienelement, der Reihe nach |
| [Telemetrie](Telemetrie) | Was jede Kachel bedeutet, woher der Wert stammt und warum eine Kachel `--` zeigt |
| [SD-Aufzeichnung](SD-Aufzeichnung) | Aufzeichnen auf microSD und Auswerten in [logwiju](https://subtilitas.github.io/logwiju/) |
| [AM32-Konfiguration](AM32-Konfiguration) | ESC-Einstellungen lesen, ändern und schreiben |
| [Servotester](Servotester) | Ein oder zwei Servos ansteuern: starten, stoppen, ziehen, Sweep, Profile |
| [Fehlersuche](Fehlersuche) | Erst das Symptom, dann die Ursache |

> **Vor dem Einschalten:** Ein Motor auf der Werkbank kann verletzen. Lies
> [Erste Schritte](Erste-Schritte#sicherheit-zuerst) vor dem ersten
> Scharfschalten.

---

## Which board

MiniRcBench runs on the Waveshare RP2350-Touch-LCD-2.8 only: a 240×320
portrait touch panel, an RP2350, and JST-SH connectors instead of 2.54 mm
headers. The image from
[Releases](https://github.com/subtilitas/MiniRcBench/releases) is
`MiniRcBench-<version>-touch-lcd-2.8.uf2`. The splash screen and **MENU → SETTINGS** (the `BUILT FOR` row) name the
board the image is built for.

*Welche Platine?* MiniRcBench läuft nur auf der Waveshare RP2350-Touch-LCD-2.8:
ein 240×320-Touch-Display im Hochformat, ein RP2350 und JST-SH-Stecker statt
2,54-mm-Stiftleisten. Das Abbild aus den
[Releases](https://github.com/subtilitas/MiniRcBench/releases) heißt
`MiniRcBench-<Version>-touch-lcd-2.8.uf2`. Es prüft nicht, auf welcher Platine
es läuft: Auf eine andere Platine geflasht, treibt es Pins, die dort mit
anderen Bauteilen verbunden sind. Der Startbildschirm und **MENU → SETTINGS** (die Zeile `BUILT FOR`)
nennen die Platine, für die das Abbild gebaut ist.

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
