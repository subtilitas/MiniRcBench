[English](RX-Signals) | **Deutsch**

# RX-Signale

`MENU → RX SIGNALS` enthält die Werkzeuge für die Signale eines Empfängers.
**MONITOR** zeigt, was ein Empfänger an seinem S.BUS-Ausgang sendet: die 16
Kanäle, die zwei digitalen Kanäle, ob die Verbindung steht, die Framerate
und die Fehler auf der Leitung. Er sendet nichts. **OUTPUT**, ein
S.BUS-Ausgang zu einem Flugcontroller, ist noch nicht gebaut. S.BUS ist
Futabas serieller Bus für Empfängerkanäle.

Nicht auf Hardware getestet: Noch kein Empfänger war an einem Pin dieser
Platine.

<img src="img/rx-chooser.png" width="240" alt="RX SIGNALS: MONITOR mit S.BUS FROM A RECEIVER, ON GP1., OUTPUT als Umriss mit S.BUS TO A FLIGHT CONTROLLER. und NOT BUILT"> <img src="img/sbus-live.png" width="240" alt="S.BUS MONITOR mit Frames alle 14 ms: der Chip OK, 16 lindgrüne Balken mit ihren Werten in Mikrosekunden, 14.0 MS 71.4 HZ">

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
- Lass die Servo- und ESC-Ausgänge des Empfängers frei. Ein gebundener
  Empfänger steuert sie an, gleich, was dieser Bildschirm tut.

Siehe auch [Verdrahtung](Verdrahtung).

---

## Die Auswahl

`MENU → RX SIGNALS` öffnet eine Seite mit zwei Schaltflächen. Die Leiste
nennt beide Signalpins, `SIG GP0  AUX GP1`, so siehst du vor dem Tippen,
welche Rolle GP1 hat. Die Seite belegt keinen Pin: DShot läuft weiter.

- **MONITOR** öffnet S.BUS MONITOR.
- **OUTPUT** ist als Umriss gezeichnet, darunter `NOT BUILT`, und tut
  nichts.

`BACK` führt zu `MENU` zurück. Während DShot nach einem anderen Werkzeug
neu startet, ist die Zeile RX SIGNALS in `MENU` ein Umriss und tut nichts,
wie die anderen Zeilen, die zu einem Pin führen.

---

## S.BUS MONITOR

Der Bildschirm nimmt GP1 beim Öffnen von DShot. Währenddessen steht dort
`RELEASING AUX GP1`, dann `TAKING AUX GP1`, dann erscheinen die Kanäle.

| Teil | Was er zeigt |
|---|---|
| Leiste | `AUX GP1  100 KBAUD  8E2`: der Pin, Geschwindigkeit und Format der Leitung. `SIG GP1`, wenn SIG auf GP1 liegt |
| Chip | die Verbindung: `NO SIGNAL`, `NO FRAMES`, `FAILSAFE`, `FRAME LOST` oder `OK` (unten) |
| 16 Zeilen | die Kanalnummer, ein Balken über den ganzen Rohwertbereich mit Marken bei 1000, 1500 und 2000 µs, und der Wert in µs |
| Zeile 1 | `CH17` und `CH18`, die zwei digitalen Kanäle, `ON` oder `OFF`; die Zeit zwischen den Frames und die Framerate, zum Beispiel `14.0 MS  71.4 HZ` |
| Zeile 2 | `FRAMES`: empfangene Frames, seit der Bildschirm offen ist; `LOST` und `FS`: Frames mit dem Frame-Lost- und dem Failsafe-Flag des Empfängers |
| Zeile 3 | `PAR`, `FRM`, `BRK`, `OVR`: Bytes mit Paritäts-, Rahmen- oder Break-Fehler und Bytes, die verloren gingen, weil die Platine zu spät las. Ein Byte mit Fehler wird nicht verwendet |
| Zeile 4 | `RESYNC`: Bytes, die bei der Suche nach dem Anfang eines Frames verworfen wurden; `FOOTER`: Frames richtiger Länge, deren Endbyte der Decoder ablehnt |

| Chip | Farbe | Bedeutet |
|---|---|---|
| `NO SIGNAL` | Bernstein | seit 100 ms nichts empfangen, oder noch nichts |
| `NO FRAMES` | Bernstein | Bytes kommen, aber kein Frame: eine Leitung mit der richtigen Geschwindigkeit, die kein S.BUS ist, oder eine Variante, deren Endbyte der Decoder ablehnt. Zeile 4 sagt, welches |
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
| `AN ESC ANSWERS ON SIG GP1.` `UNPLUG IT: THE RECEIVER GOES THERE.` | SIG liegt auf GP1, und dort antwortet ein ESC. Der Empfänger würde den Signaleingang des ESC treiben | Zieh den ESC ab. Der Bildschirm nimmt den Pin 500 ms, nachdem der ESC verstummt |
| `KISS WIRE ON AUX GP1 IS LIVE.` `UNPLUG IT: THE RECEIVER GOES THERE.` | auf GP1 kommt KISS-Telemetrie an | Zieh die Telemetrieleitung ab |
| `S.BUS NEEDS GP1.` `SET A PIN TO GP1 IN SETTINGS.` | weder SIG noch AUX liegt auf GP1 | Setze SIG PIN oder AUX PIN auf GP1. SETTINGS tauscht die beiden, daher kommt das auf dieser Platine nicht vor |
| `AUX GP1 COULD NOT BE TAKEN (CLOCK)` | der serielle Empfänger der Platine konnte 100.000 Baud nicht erreichen | `RETRY` |

Die ESC-Prüfung übersieht einen ESC, der nicht in bidirektionalem DShot
antwortet.

<img src="img/sbus-esc.png" width="240" alt="S.BUS MONITOR wartet: AN ESC ANSWERS ON SIG GP1. und UNPLUG IT: THE RECEIVER GOES THERE. in Bernstein">

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
- `RESTART DSHOT` führt zu `MENU` und gibt die Pins an DShot zurück. Es
  wartet 1 s, nachdem der Pin freigegeben ist.

`BACK` auf der Seite ist ein Umriss und tut nichts.

<img src="img/sbus-leave.png" width="240" alt="Die S.BUS-Verlassenseite: 1 UNPLUG THE RECEIVER., DSHOT RESTARTS ON SIG GP0., BACK TO MONITOR, RESTART DSHOT noch grau">

---

## Noch nicht geprüft

- Der Ausgangspegel eines Empfängers an GP1, und dass die Platine ihn mit
  Reserve liest.
- Frames eines echten Empfängers, seine Framerate und eine Leitung ohne
  Fehler über längere Zeit.
- Was GP1 liest, nachdem ein Empfänger abgezogen wurde: Nach dem Erratum
  RP2350-E9 des Chips kann der Pin bei etwa 2,1 V stehen bleiben. Der
  Bildschirm zeigt dann `NO SIGNAL`, mit `BRK` um eins höher.
