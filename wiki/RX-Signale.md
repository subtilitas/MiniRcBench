[English](RX-Signals) | **Deutsch**

# RX-Signale

`MENU → RX SIGNALS` enthält die Werkzeuge für die Signale eines Empfängers.
**MONITOR** zeigt, was ein Empfänger an seinem S.BUS-Ausgang sendet: die 16
Kanäle, die zwei digitalen Kanäle, ob die Verbindung steht, die Framerate
und die Fehler auf der Leitung. Er sendet nichts. **OUTPUT** sendet S.BUS
an den Empfängereingang eines Flugcontrollers: 16 Kanäle an Schiebereglern,
die zwei digitalen Kanäle und die Flags für Failsafe und verlorenen Frame.
S.BUS ist Futabas serieller Bus für Empfängerkanäle.

Nicht auf Hardware getestet: Noch kein Empfänger und kein Flugcontroller war
an einem Pin dieser Platine.

<img src="img/rx-chooser.png" width="240" alt="RX SIGNALS: MONITOR mit S.BUS FROM A RECEIVER, ON GP1., OUTPUT mit S.BUS TO A FLIGHT CONTROLLER. und ON GP0, NEVER TO AN ESC."> <img src="img/sbus-live.png" width="240" alt="S.BUS MONITOR mit Frames alle 14 ms: der Chip OK, 16 lindgrüne Balken mit ihren Werten in Mikrosekunden, 14.0 MS 71.4 HZ">

---

## Verdrahtung

Der Empfänger braucht eine eigene Versorgung. Die Platine nimmt nur Signal
und Masse.

```
  Empfängerversorgung, im Eingangsbereich des Empfängers
     +  ----------------------------------------------  Empfänger +
     -  ----------------+-----------------------------  Empfänger -
                        |
  Platine GND  ---------+
  Platine GP1  ----------------------------------------  Empfänger S.BUS-Ausgang

  GP1 = J5 Pin 1 oder J4 Pin 10, GND = J5 Pin 4 oder J4 Pin 1 oder 5
```

- GP1 ist AUX, der KISS-Pin, außer SETTINGS → SIG PIN steht auf GP1. Der
  Monitor liest GP1 in jeder Rolle: Es ist der einzige Pin, den der
  serielle Empfänger der Platine lesen kann.
- Versorge den Empfänger nie aus den 3,3-V- oder 5-V-Pins der Platine.
- Schalte zuerst die Platine ein, dann den Empfänger; schalte den Empfänger
  vor der Platine aus. Der Pin verträgt bis 5,5 V, solange die Platine
  läuft, nur 3,63 V, solange sie aus ist. S.BUS-Ausgänge haben meist
  3,3-V-Logik; gemessen wurde hier noch kein Empfänger, und die Platine
  prüft den Pegel nicht.
- Lass die Servo- und ESC-Ausgänge (ESC: Electronic Speed Controller,
  Motorregler) des Empfängers frei. Ein gebundener
  Empfänger steuert sie an, gleich, was dieser Bildschirm tut.

Siehe auch [Verdrahtung](Verdrahtung).

---

## Die Auswahl

`MENU → RX SIGNALS` öffnet eine Auswahl, eine Seite mit zwei Schaltflächen.
Die Leiste nennt beide Signalpins, `SIG GP0  AUX GP1`, so siehst du vor dem
Tippen, welche Rolle GP1 hat. Die Seite belegt keinen Pin: DShot läuft weiter.

- **MONITOR** öffnet S.BUS MONITOR.
- **OUTPUT** öffnet S.BUS OUTPUT. Die zwei Zeilen darunter sind seine
  Warnung: `S.BUS TO A FLIGHT CONTROLLER.` `ON GP0, NEVER TO AN ESC.`

`BACK` führt zu `MENU` zurück. Während DShot nach einem anderen Werkzeug
neu startet, öffnet RX SIGNALS wie immer, MONITOR und OUTPUT sind Umrisse
und tun nichts, und die Leiste zeigt `DSHOT RESTARTING ON SIG GP0` in
Bernstein.

---

## S.BUS MONITOR

Beim Öffnen nimmt der Bildschirm GP1 von DShot. Währenddessen steht dort
`RELEASING AUX GP1`, dann `TAKING AUX GP1`, dann erscheinen die Kanäle.

| Teil | Was er zeigt |
|---|---|
| Leiste | `AUX GP1  100 KBAUD  8E2`: der Pin, Geschwindigkeit und Format der Leitung. `SIG GP1`, wenn SIG auf GP1 liegt |
| Chip | die Verbindung: `NO SIGNAL`, `NO FRAMES`, `FAILSAFE`, `FRAME LOST` oder `OK` (unten) |
| 16 Zeilen | die Kanalnummer, ein Balken über den ganzen Rohwertbereich mit Marken bei 1000, 1500 und 2000 µs, und der Wert in µs |
| Zeile 1 | `CH17` und `CH18`, die zwei digitalen Kanäle, `ON` oder `OFF`; die Zeit zwischen den Frames und die Framerate, zum Beispiel `14.0 MS  71.4 HZ` |
| Zeile 2 | `FRAMES`: empfangene Frames, seit der Bildschirm den Pin nahm oder seit BACK TO MONITOR; `LOST` und `FS`: Frames mit dem Frame-Lost- und dem Failsafe-Flag des Empfängers |
| Zeile 3 | `PAR`, `FRM`, `BRK`, `OVR`: Bytes mit Paritäts-, Rahmen- oder Break-Fehler und Überläufe: Bytes gingen verloren, weil die Platine zu spät las. Ein Byte mit Fehler wird nicht verwendet |
| Zeile 4 | `RESYNC`: Bytes, die bei der Suche nach dem Anfang eines Frames verworfen wurden; `FOOTER`: Frames richtiger Länge, deren Endbyte der Decoder ablehnt |

| Chip | Farbe | Bedeutet |
|---|---|---|
| `NO SIGNAL` | Bernstein | seit 100 ms nichts empfangen, oder noch nichts |
| `NO FRAMES` | Bernstein | Bytes kommen, aber kein Frame: eine Leitung mit der richtigen Geschwindigkeit, die kein S.BUS ist, oder eine Variante, deren Endbyte der Decoder ablehnt. Zeile 4 zeigt, was zutrifft |
| `FAILSAFE` | Rot | der Empfänger meldet Failsafe: Der Sender ist aus oder außer Reichweite |
| `FRAME LOST` | Bernstein | ein Frame in den letzten 250 ms trug das Frame-Lost-Flag des Empfängers |
| `OK` | Grün | Frames kommen |

<img src="img/sbus-failsafe.png" width="240" alt="S.BUS MONITOR im Failsafe: der Chip FAILSAFE, rote Balken und Werte"> <img src="img/sbus-lost.png" width="240" alt="S.BUS MONITOR, nachdem der Empfänger aufgehört hat: der Chip NO SIGNAL, die letzten Werte in Grau">

**Die Balken.** Lindgrün, solange Frames kommen; rot, solange der Empfänger
Failsafe meldet, damit du seine Failsafe-Werte direkt abliest; grau, sobald
100 ms lang kein Frame kam, damit die letzten Werte lesbar bleiben. Vor dem
ersten Frame sind die Balken leer und die Werte zeigen `----`.

<img src="img/sbus-waiting.png" width="240" alt="S.BUS MONITOR vor dem ersten Frame: NO SIGNAL, leere Balken, Werte ----">

**Die Werte.** Der Rohwert eines Kanals reicht von 0 bis 2047. Der
Bildschirm zeigt ihn in µs so, wie die rcbench-Bibliothek umrechnet: 172 ist
1000 µs, 992 ist 1500 µs, 1811 ist 2000 µs. Ein Flugcontroller kann nahe den
Enden einige µs mehr oder weniger zeigen; die Mitte stimmt überein. Der
Balken ist über dem Rohwert gezeichnet und in beiden Fällen gleich.

**Was du damit prüfst.**

- Ob ein an deinen Sender gebundener Empfänger überhaupt S.BUS sendet, und
  mit welcher Framerate: etwa 71 Hz bei 14 ms, etwa 143 Hz bei 7 ms.
- Welcher Knüppel welchen Kanal bewegt, bevor du einen Flugcontroller
  einrichtest.
- Den Failsafe: Schalte den Sender aus. Der Empfänger sendet entweder weiter
  mit `FAILSAFE` und seinen Failsafe-Werten, oder er hört auf zu senden, was
  als `NO SIGNAL` erscheint.
- Die Verbindung auf der Werkbank: `LOST` zählt Frames, die der Empfänger
  als verloren markiert hat.

---

## Wenn der Monitor wartet

| Text | Warum | Was tun |
|---|---|---|
| `AN ESC ANSWERS ON SIG GP1.` `UNPLUG IT. PLUG THE RECEIVER IN` `ONCE THE LINK CHIP SHOWS.` | SIG liegt auf GP1, und dort antwortet ein ESC. Der Empfänger würde den Signaleingang des ESC treiben | Zieh den ESC ab. Der Bildschirm nimmt den Pin 500 ms, nachdem der ESC verstummt; steck den Empfänger erst an, wenn der Verbindungs-Chip erscheint. Der Chip erscheint erst, wenn der Pin belegt ist und der Job läuft; bis dahin treibt DShot GP1 weiter |
| `KISS WIRE ON AUX GP1 IS LIVE.` `UNPLUG IT. PLUG THE RECEIVER IN` `ONCE THE LINK CHIP SHOWS.` | auf GP1 kommt KISS-Telemetrie an | Zieh die Telemetrieleitung ab. Steck den Empfänger erst an, wenn der Verbindungs-Chip erscheint |
| `S.BUS NEEDS GP1.` `SET A PIN TO GP1 IN SETTINGS.` | weder SIG noch AUX liegt auf GP1 | Setze SIG PIN oder AUX PIN auf GP1. SETTINGS tauscht die beiden, daher kommt das auf dieser Platine nicht vor |
| `AUX GP1 COULD NOT BE TAKEN (CLOCK)` | der serielle Empfänger der Platine konnte 100.000 Baud nicht erreichen | `RETRY` |

Die ESC-Prüfung übersieht einen ESC, der nicht in bidirektionalem DShot
antwortet.

<img src="img/sbus-esc.png" width="240" alt="S.BUS MONITOR wartet: AN ESC ANSWERS ON SIG GP1. und UNPLUG IT. PLUG THE RECEIVER IN ONCE THE LINK CHIP SHOWS. in Bernstein"> <img src="img/sbus-no-gp1.png" width="240" alt="S.BUS MONITOR ohne Rolle auf GP1: S.BUS NEEDS GP1. und SET A PIN TO GP1 IN SETTINGS. in Bernstein">

---

## Verlassen

`BACK` öffnet die Verlassenseite, wie beim [Servotester](Servotester). Die
Platine gibt GP1 sofort frei. Schritt 1 lautet `UNPLUG THE RECEIVER.` und
sagt, wo DShot neu startet: `DSHOT RESTARTS ON SIG GP0.`, oder, wenn SIG auf
GP1 liegt, `DSHOT RESTARTS ON SIG GP1, WHERE THE RECEIVER DRIVES THE LINE.`
Dort würden der Ausgang von DShot und der des Empfängers eine Leitung
gegeneinander treiben.

- `BACK TO MONITOR` behält den Pin und kehrt zum Monitor zurück, jeder
  Zähler auf 0.
- `RESTART DSHOT` führt zu RX SIGNALS und gibt die Pins an DShot zurück. Es
  wartet 1 s, nachdem der Pin freigegeben ist.

`BACK` auf der Seite ist ein Umriss und tut nichts.

<img src="img/sbus-leave.png" width="240" alt="Die S.BUS-Verlassenseite: 1 UNPLUG THE RECEIVER., DSHOT RESTARTS ON SIG GP0., BACK TO MONITOR, RESTART DSHOT noch grau"> <img src="img/sbus-leave-sig.png" width="240" alt="Die S.BUS-Verlassenseite mit SIG auf GP1: DSHOT RESTARTS ON SIG GP1, WHERE THE RECEIVER DRIVES THE LINE.">

---

## S.BUS OUTPUT

**Zuerst lesen.** GP0 kommt nur an einen Flugcontroller (FC), nie an einen
ESC (Drehzahlsteller). Nimm die Propeller ab. Der Ausgang ist ein Gassignal
für jeden, der zuhört: Ein scharfgeschalteter FC gibt den Gaskanal an seine
Motoren weiter, und ein Scharfschalter ist einer der Kanäle. Vor START prüft
die Platine GP0 auf den Bootloader eines ESC, findet einen BLHeli_S-ESC aber
nur, solange dieser während der Prüfung Strom hat und in seinem Bootloader
ist. Ein ESC ohne Strom während der Prüfung, oder einer, der danach
angesteckt wird, wird nicht gefunden. Nach seinem Quelltext, gelesen und
nicht an einem ESC ausprobiert, kann ein BLHeli_S-ESC die S.BUS-Leitung als
Gassignal nehmen, seinen Gasweg kalibrieren, scharf schalten und seinen
Motor starten, ohne dass du etwas tust.

Wofür: die Kanalzuordnung, Endpunkte und Schalter eines FC in seinem
Konfigurator prüfen, ohne Empfänger und Sender; sehen, was der FC mit den
Flags für Failsafe und verlorenen Frame macht und wenn die Frames ausbleiben;
feste, wiederholbare Werte senden.

<img src="img/sbus-out-safe.png" width="240" alt="S.BUS OUTPUT SAFE: der Chip SAFE, GP0 GOES TO A FLIGHT CONTROLLER. und NEVER TO AN ESC. PROPS OFF. in Bernstein, acht graue Schieberegler, CH3 THR mit der Marke seiner Obergrenze, HOLD TO START"> <img src="img/sbus-out-running.png" width="240" alt="S.BUS OUTPUT läuft: der Chip RUNNING, THROTTLE CH3: 1000 TO 1200 US (20%)., die Zeile der Flags, Schieberegler mit lindgrünen Werten, STOP">

### Verdrahtung

```
  Platine GP0  ------------------------------------  FC S.BUS-Eingang
  Platine GND  ------------------------------------  FC Masse

  GP0 = J5 Pin 2 oder J4 Pin 9, GND = J5 Pin 4 oder J4 Pin 1 oder 5
```

- GP0 ist SIG, der ESC-Pin, außer SETTINGS → SIG PIN steht auf GP1. In der
  Standardverdrahtung kommt die FC-Leitung dorthin, wo der ESC war: Zieh
  zuerst den ESC ab.
- Die Platine liefert nur Signal und Masse. Der FC bekommt Strom über seinen
  eigenen USB-Anschluss oder seine eigene Versorgung; an seinem Akku sind
  seine ESCs und Motoren aktiv.
- Schalte die Platine vor dem FC ein und den FC vor der Platine aus. Was der
  S.BUS-Eingang eines FC an GP0 macht, solange die Platine aus ist, und ob
  jeder FC das 3,3-V-Signal der Platine liest, ist nicht geprüft.

### Vor START

Der Bildschirm nimmt GP0 von DShot und prüft den Pin dann etwa 1,1 s lang
auf den Bootloader eines ESC: eine stille Sekunde, dann drei Begrüßungen, wie
ESC SETTINGS. Er wiederholt die Prüfung, bis eine Runde keine Antwort
bekommt. Erst dann zeigt der Chip `SAFE`, und **HOLD TO START** wird
verfügbar.

| Text | Warum | Was tun |
|---|---|---|
| `CHECKING SIG GP0 FOR AN ESC.` | die Prüfung läuft | etwa eine Sekunde warten |
| `AN ESC BOOTLOADER ANSWERS ON SIG GP0.` `UNPLUG THE ESC BATTERY AND LEAD.` `START WAITS UNTIL NONE ANSWERS.` | ein Bootloader von Bluejay, BLHeli_S oder AM32 hat geantwortet: Ein ESC hängt an GP0 | Zieh den Akku des ESC und seine Leitung ab. START bleibt aus, bis eine Runde der Prüfung keine Antwort bekommt. `MENU` zeigt `POWER-CYCLE THE ESC TO RUN IT` |
| `AN ESC ANSWERS ON SIG GP0.` `UNPLUG IT: NO ESC STAYS ON` `THE BENCH WHILE S.BUS RUNS.` | ein ESC antwortet auf SIG mit DShot | Zieh ihn ab. Der Bildschirm nimmt den Pin 500 ms, nachdem er verstummt |
| `KISS WIRE ON AUX GP0 IS LIVE.` `UNPLUG IT. PLUG THE FLIGHT` `CONTROLLER IN ONCE SAFE SHOWS.` | SIG ist GP1, GP0 ist AUX, und dort kommt KISS-Telemetrie an | Zieh die Telemetrieleitung ab. Steck den FC an, sobald der Chip `SAFE` erscheint |
| `S.BUS NEEDS GP0.` `SET A PIN TO GP0 IN SETTINGS.` | weder SIG noch AUX ist GP0 | Stell SIG PIN oder AUX PIN auf GP0. SETTINGS tauscht die beiden, daher kommt das auf dieser Platine nicht vor |
| `SIG GP0 COULD NOT BE TAKEN (CLOCK)` | die serielle Schnittstelle erreicht keine 100.000 Baud, oder die Leitung der Prüfung bekam ihren Takt nicht | `RETRY` |
| `SIG GP0 COULD NOT BE TAKEN (PIO)`, `(DMA)` oder `(REFUSED)` | die Leitung der Prüfung fand keine freie PIO-Zustandsmaschine oder keinen freien DMA-Kanal, oder ihr Treiber nahm den Pin nicht | `RETRY` |

`SIG GP0` heißt `AUX GP0`, wenn SIG auf GP1 liegt.

<img src="img/sbus-out-esc.png" width="240" alt="S.BUS OUTPUT wartet: AN ESC ANSWERS ON SIG GP0. UNPLUG IT: NO ESC STAYS ON THE BENCH WHILE S.BUS RUNS. in Bernstein"> <img src="img/sbus-out-kiss.png" width="240" alt="S.BUS OUTPUT mit GP0 als AUX: KISS WIRE ON AUX GP0 IS LIVE. UNPLUG IT. PLUG THE FLIGHT CONTROLLER IN ONCE SAFE SHOWS. in Bernstein">

<img src="img/sbus-out-checking.png" width="240" alt="S.BUS OUTPUT prüft: CHECKING SIG GP0 FOR AN ESC., HOLD TO START als Umriss"> <img src="img/sbus-out-bootloader.png" width="240" alt="S.BUS OUTPUT: AN ESC BOOTLOADER ANSWERS ON SIG GP0. UNPLUG THE ESC BATTERY AND LEAD. START WAITS UNTIL NONE ANSWERS. in Bernstein">

### Die Hauptansicht

| Teil | Was es zeigt |
|---|---|
| Leiste | `SIG GP0  100 KBAUD  8E2`: der Pin, Geschwindigkeit und Format der Leitung; der Chip `SAFE` (grün), `RUNNING` (rot) oder `STALLED` (magenta) |
| Hinweise | im Zustand SAFE: die zwei Warnungen in Bernstein, `GP0 GOES TO A FLIGHT CONTROLLER.` `NEVER TO AN ESC. PROPS OFF.` Laufend: `THROTTLE CH3: 1000 TO 1200 US (20%).`, der Gaskanal und seine Obergrenze; und `CH17 OFF  CH18 OFF  FS OFF  LOST OFF`, in Bernstein, solange eines davon an ist. Nach einem Stopp der Grund, und wieder die Warnung `NEVER TO AN ESC. PROPS OFF.`: START geht sofort, ohne neue Prüfung auf einen ESC |
| 8 Zeilen | ein Kanal, sein Schieberegler von 1000 bis 2000 µs und der Wert, den er sendet, lindgrün, solange Frames laufen. Der Gaskanal heißt `CH3 THR` in Bernstein, mit einer Marke in Bernstein an seiner Obergrenze |
| Schaltflächen | `HOLD TO START`, oder `STOP`, solange Frames laufen; `CH 9-16` oder `CH 1-8` für die anderen acht Kanäle; `SET` |

- **HOLD TO START**, 1 s, sendet einen festen ersten Frame, egal was die
  Regler zeigten: den Gaskanal auf 1000 µs, die anderen drei Knüppel auf
  1500 µs, die Kanäle 5 bis 16 auf 1000 µs, alles andere aus. Die Regler
  springen dorthin.
- **Die Regler** bewegen sich nur, solange Frames laufen, durch Ziehen, in
  Schritten von 5 µs. Ein Tippen allein ändert nichts.
- **Der Gaskanal** hält an der Obergrenze von SETTINGS → MAX THROTTLE: bei
  den voreingestellten 20 % sind das 1200 µs. Die Grenze begrenzt den Kanal,
  nicht die Motoren eines scharfen FC, der Leerlauf und Stabilisierung
  dazurechnet.
- **STOP** hält die Frames sofort an, beim Berühren. Der FC geht dann in
  seinen eigenen Failsafe.
- **Der Bildschirm hängt** eine Viertelsekunde: Die Frames hören auf, der
  Chip zeigt `STALLED` und der Hinweis `STOPPED: SCREEN STALLED`.
- **30 s ohne Berührung**, solange der Gaskanal über 1000 µs steht oder die
  Kanäle 5 bis 16, CH17 oder CH18 nicht auf ihren ersten Werten sind: Die
  Frames hören mit `STOPPED: NO TOUCH FOR 30 S.` auf. Ein über einen Schalter
  scharfgeschalteter FC dreht seine Motoren auch bei Gas null im Leerlauf.
  Steht alles auf dem ersten Frame, laufen die Frames weiter, so kann die
  Empfängerseite des FC offen bleiben.
- **Der Touch-Controller antwortet nicht**: Vier Lesungen in Folge
  schlagen über 100 ms oder länger fehl, `STOP` lässt sich also nicht
  drücken. Die Frames hören auf wie bei `STOP`, in der Hauptansicht und
  unter SET, der Hinweis zeigt `STOPPED: TOUCH DID NOT ANSWER` bis zum
  nächsten START, und `HOLD TO START` ist als Umriss gezeichnet, bis der
  Controller wieder antwortet. Der Tester und der Servotester halten nach
  derselben Regel an (siehe [Die Bildschirme](Die-Bildschirme#der-tester)).

Nach jedem Stopp halte START erneut; der erste Frame ist wieder der feste.

<img src="img/sbus-out-touch-lost.png" width="240" alt="S.BUS OUTPUT, nachdem der Touch-Controller nicht mehr antwortete: der Chip SAFE, STOPPED: TOUCH DID NOT ANSWER und NEVER TO AN ESC. PROPS OFF. in Bernstein, HOLD TO START als Umriss">

<img src="img/sbus-out-stalled.png" width="240" alt="S.BUS OUTPUT, nachdem der Bildschirm hing: der Chip STALLED, STOPPED: SCREEN STALLED in Magenta, NEVER TO AN ESC. PROPS OFF. in Bernstein">

### SET

| Zeile | Werte | Wann sie sich ändert |
|---|---|---|
| THROTTLE CHANNEL | `CH1` bis `CH4`, voreingestellt `CH3` | nur gestoppt: `STOP TO CHANGE THE CHANNEL.` |
| DIGITAL CH17, CH18 | `OFF`, `ON` | nur, solange Frames laufen: `START TO SET THE FLAGS.` |
| FLAGS FAILSAFE BIT, FRAME LOST BIT | `OFF`, `ON` | wie CH17 |

Tippe eine Zeile an, dann `-` oder `+`; eine Änderung gilt sofort. Solange
Frames laufen, steht `STOP` unten rechts; gestoppt steht unten
`NEVER TO AN ESC. PROPS OFF.` `BACK` führt zur Hauptansicht zurück.

<img src="img/sbus-out-set.png" width="240" alt="OUTPUT SET gestoppt: START TO SET THE FLAGS., die Zeilen CHANNEL CH3, CH17 OFF, CH18 OFF, FAILSAFE BIT OFF, und unten NEVER TO AN ESC. PROPS OFF.">

### Verlassen des Ausgangs

`BACK` hält die Frames an und öffnet die Verlassenseite. Schritt 1 lautet
`UNPLUG THE FLIGHT CONTROLLER.` und `DSHOT RESTARTS ON SIG GP0.`: Das
ESC-Signal kommt auf diesem Pin zurück.

- `BACK TO OUTPUT` behält den Pin und führt zum Bildschirm zurück; vor START
  läuft die Bootloader-Prüfung erneut.
- `RESTART DSHOT` führt zu RX SIGNALS zurück und gibt den Pin an DShot
  zurück.
  Es wartet 1 s, nachdem der Pin freigegeben ist.

Hat ein Bootloader von Bluejay oder BLHeli_S auf die Prüfung geantwortet,
zeigt `BACK` zuerst den Schritt von ESC SETTINGS:
`UNPLUG THE ESC BATTERY, THEN LEAVE.` Der ESC wartet in seinem Bootloader und
könnte DShot als Befehl lesen. `STAY` führt zum Bildschirm zurück, `LEAVE`
gibt den Pin frei; der letzte Schritt erinnert daran, den ESC nach
`RESTART DSHOT` wieder anzustecken.

<img src="img/sbus-out-leave.png" width="240" alt="Die Verlassenseite von S.BUS OUTPUT: 1 UNPLUG THE FLIGHT CONTROLLER., DSHOT RESTARTS ON SIG GP0., BACK TO OUTPUT, RESTART DSHOT"> <img src="img/sbus-out-efm8-leave.png" width="240" alt="Die Verlassenseite, nachdem ein Bootloader von Bluejay oder BLHeli_S geantwortet hat: 1 UNPLUG THE ESC BATTERY, THEN LEAVE., STAY und LEAVE">

---

## Noch nicht geprüft

- Der Ausgangspegel eines Empfängers an GP1, und dass die Platine ihn mit
  Reserve liest.
- Frames eines echten Empfängers, seine Framerate und eine Leitung ohne
  Fehler über längere Zeit.
- Was GP1 liest, nachdem ein Empfänger abgezogen wurde: Nach dem Erratum
  RP2350-E9 des Chips kann der Pin bei etwa 2,1 V stehen bleiben. Der
  Bildschirm zeigt dann `NO SIGNAL`, mit `BRK` um eins höher.
- Der Ausgang: sein Signal an GP0, die Frameperiode von 14 ms auf einem
  Oszilloskop, und ein Flugcontroller, der seine Frames, Flags und Stopps
  liest.
- Dass der Bootloader eines ESC auf die Prüfung der Platine antwortet, und
  was Bluejay, BLHeli_S und AM32 mit S.BUS auf ihrer Signalleitung machen:
  nur aus ihren Quelltexten gelesen.
