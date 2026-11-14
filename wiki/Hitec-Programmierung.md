[English](Hitec-Programming) | **Deutsch**

# Hitec-Programmierung

Dieser Bildschirm liest und schreibt die Einstellungen eines Hitec-Servos der
D-Serie oder der klassischen Familie über dessen Signalkabel. Ein klassisches
Servo braucht SERVO SUPPLY auf `GP28` oder `GP29`. Der Protokollcode stammt
von rcbench: Sitzung, Registermodell, Leitungsschicht und der Leitungstreiber
im PIO (Programmable I/O, programmierbare Ein-/Ausgabe). Frames der D-Serie
laufen mit 115.200 bit/s, halbduplex, auf dem ESC-Pin (Electronic Speed
Controller, Motorregler). Bei einem Servo der D-Serie liest der Bildschirm
dessen 21 Register, zeigt 20 Parameter mit ihren Einheiten, stellt die
änderbaren ein und schreibt sie zurück, zuvor mit einer Sicherung auf der
SD-Karte (Secure Digital).

<img src="img/hitec-list.png" width="240" alt="Die Hitec-Feldliste eines D485HW">

> **Nicht auf Hardware getestet.** Noch kein Byte des Leitungstreibers lag
> auf einem Pin dieser Platine, und noch kein Schreibplan von rcbench lief
> an einem Servo. Die Bildschirme in diesem Wiki entstehen aus dem Code der
> Bedienoberfläche (UI, User Interface) gegen ein simuliertes Servo.

> **Die klassische Familie braucht die geschaltete Servoversorgung.**
> `CLASSIC` (HS-5xxx, HS-7xxx) braucht den DC-DC-Wandler
> (Gleichspannungswandler) aus Teil C der [Verdrahtung](Verdrahtung) und
> SERVO SUPPLY auf `GP28` oder `GP29`. Unter `NONE` lässt sich nur die
> D-Serie öffnen. Siehe [Klassische Servos](#klassische-servos).

> **Zieh das Servo ab, bevor DShot läuft.** Außerhalb dieses Bildschirms liegt
> auf dem ESC-Pin DShot, das digitale Gasprotokoll. Ein Servo an diesem Pin
> liest das als Impuls von etwa 970 µs mit 1000 Hz und fährt Richtung eines
> Endanschlags. Das passiert, wenn die Platine mit einem Servo am ESC-Pin
> **startet**, und wenn du diesen Bildschirm verlässt. Der Bildschirm fordert
> dich vorher zum Abziehen auf.

Hitec ist ein Name oder eine Marke seines Inhabers. Er steht hier nur, um zu
sagen, mit welchen Servos dieser Bildschirm arbeitet. MiniRcBench ist nicht
mit Hitec verbunden.

---

## Verkabelung

Schließe das Servo an wie bei der
[KST-Programmierung](KST-Programmierung#verkabelung): das Servo an seiner
eigenen Versorgung, sein Signal an SIG, die beiden Massen einmal an der
Platine verbunden.

```
  Servoversorgung, 5 V
     +  ----/ Schalter /-------------------------------  Servo +   (rot, Mitte)
     -  ----------------+-----------------------------  Servo -   (braun oder schwarz)
                        |
  Platine GND  ---------+
  Platine SIG  ----------------------------------------  Servosignal (orange oder weiß)

  SIG GP0 = J4 Pin 9 oder J5 Pin 2, GND = J4 Pin 1 oder 5 oder J5 Pin 4
```

> **Ein Servokabel nie 1:1 an J5 anschließen.** J5 Pin 3 ist die
> 3,3-V-Versorgung der Platine, an der Stelle des Plus eines Servosteckers:
> siehe [Einrichtung](Einrichtung#das-adapterkabel).

GND ist die Masse der Platine.

- **Ein Servo an der Leitung.** Jeder Frame geht an die ID 0
  (Identifikationsnummer), und der Frame trägt keine andere Adresse. rcbench
  hält fest, dass zwei Servos der D-Serie an einem Anschluss beide einen
  Frame an die ID 0 annehmen. Ob ein Servo, dessen eigene ID nicht 0 ist, auf
  die ID 0 antwortet, ist nicht gemessen.
- **Kein Pull-up-Widerstand** am Signalkabel. Die Platine empfängt die
  Antwort des Servos durch Vorladen: Vor jeder Abtastung hebt der Pin die
  Leitung 100 ns lang an. Den Empfang von rcbench über einen
  Pull-up-Widerstand bietet sie nicht an.
- Das mittlere Kabel des Servos geht an seine Versorgung, nie an einen Pin
  der Platine. Der 5-V-Pin der Platine ist ihre USB-Versorgung (Universal
  Serial Bus).
- Schalte zuerst die Platine ein und dann die Servoversorgung, und beim
  Ausschalten zuerst die Servoversorgung.
- SIG ist der ESC-Pin, so wie unter **MENU → SETTINGS** eingestellt.
  Voreingestellt ist GP0.
- Die Läufe von rcbench nutzten eine Servoversorgung von 5,0 V. Andere
  Versorgungen sind nicht getestet, und auf dieser Platine ist nichts
  gelaufen.
- Zieh jeden ESC vom ESC-Pin ab. Der Bildschirm prüft den Pin auf einen ESC,
  erkennt aber nur einen ESC, der bidirektionales DShot beantwortet.

Mit **SETTINGS → SERVO SUPPLY** auf `GP28` oder `GP29` versorgt das Gerät das Servo selbst,
über den Wandler, den die
[KST-Programmierung](KST-Programmierung#geschaltete-versorgung) beschreibt.
Siehe [Geschaltete Versorgung](#geschaltete-versorgung). **Ein ESC an dieser
Leitung bekommt den Ausgang des Wandlers auf seine BEC-Leitung** (Battery
Eliminator Circuit), die Leitung seiner eigenen geregelten Versorgung.

---

## Die Familie wählen

**MENU → HITEC SERVO** öffnen. Der Bildschirm öffnet mit der
Familienauswahl und übernimmt SIG sofort vom ESC-Treiber, bevor eine Familie
gewählt ist: DShot sendet nach dem letzten Gas-Frame mindestens 250 ms lang
Gas null, hält an und lässt den Pin als Eingang, den sein Pull-up high hält.
Kein DShot-Frame erreicht ein Servo, das du in dieser Ansicht anschließt.

<img src="img/hitec-family.png" width="240" alt="Die Familienauswahl mit SERVO SUPPLY auf NONE: GP0 IS THE SERVO'S: DSHOT IS OFF. in Grün, CLASSIC abgeblendet mit NEEDS THE CONVERTER OF WIRING PART C, D SERIES mit POWER THE SERVO UP, THEN TAP.">

Die Leiste zeigt `RELEASING GP0 FROM DSHOT`, solange der Treiber anhält,
dann `GP0 IS THE SERVO'S: DSHOT IS OFF.` in Grün über
`CONNECT THE SERVO, CHOOSE ITS FAMILY`. Ab dann wirken die zwei
Schaltflächen:

- `CLASSIC`, darunter `HS-5XXX, HS-7XXX`. Es wirkt, solange SERVO SUPPLY
  auf `GP28` oder `GP29` steht. Steht SERVO SUPPLY auf `NONE`, ist es
  abgeblendet gezeichnet, und zwei bernsteinfarbene Zeilen nennen den Grund,
  zuerst die Hardware: `NEEDS THE CONVERTER OF WIRING PART C` und
  `AND SETTINGS > SERVO SUPPLY GP28/29.`
- `D SERIES`, darunter `D485HW, D645MW AND THE OTHERS` und
  `SUPPLY SWITCHED ON GP28` (oder `GP29`, je nach Einstellung) oder, unter
  `NONE`, `POWER THE SERVO UP, THEN TAP.`: An seiner eigenen
  Versorgung muss das Servo laufen, wenn der Bildschirm eintritt, denn der
  Eintritt liest sofort sein Modell.

**Schließe das Servo an, schalte es ein, dann tippe auf `D SERIES`.** Mit
`GP28` oder `GP29` bleibt der Wandler in dieser Ansicht aus, von seinem
Pull-down gehalten, und der Prüfstand schaltet das Servo nach dem Tippen ein.

Gehört der Pin dem Servo, führt `BACK` in dieser Ansicht zur Abziehseite,
wie `LEAVE`: DShot startet erst nach `UNPLUG THE SERVO.` und
`RESTART DSHOT` neu. `BACK` auf der Abziehseite führt zur Familienauswahl
zurück. Solange der Treiber noch anhält oder der Bildschirm den Pin nicht
übernommen hat, führt `BACK` zu `MENU` zurück.

<img src="img/hitec-family-esc.png" width="240" alt="Die Familienauswahl, während ein ESC auf GP0 antwortet: AN ESC ANSWERS ON GP0: UNPLUG IT. und DSHOT STAYS ON THE PIN UNTIL THEN. in Bernstein, beide Familien abgeblendet.">

- Antwortet ein ESC auf dem Pin, zeigt die Leiste in Bernstein
  `AN ESC ANSWERS ON GP0: UNPLUG IT.` und
  `DSHOT STAYS ON THE PIN UNTIL THEN.`, und beide Familien sind abgeblendet
  gezeichnet. Der Bildschirm übernimmt den Pin, sobald der ESC 500 ms lang
  still ist. Die Prüfung läuft nur, bevor der Pin übernommen ist: Ein ESC,
  der danach angeschlossen wird, wird nicht erkannt.
- Solange der Tester scharf ist, zeigt die Leiste `DISARM FIRST`. Solange die
  Pins noch belegt sind, zeigt sie `PINS BUSY`.

Das Tippen startet den Hitec-Job für diese Familie. Während er sich bindet,
zeigt die Leiste der nächsten Ansicht `TAKING GP0`.

- Kann die Platine die Leitung nicht einrichten, zeigt der Bildschirm
  `GP0 COULD NOT BE TAKEN (`, danach `REFUSED`, `PWM`, `PIO`, `DMA` oder
  `CLOCK` und eine schließende Klammer. PWM ist Pulsweitenmodulation, DMA
  direkter Speicherzugriff (Direct Memory Access). `RETRY` versucht es
  erneut.

> Ein Servo mit Strom an SIG bekommt keine Servoimpulse, solange der
> Bildschirm offen ist. Es kann in seine Failsafe-Stellung fahren.

---

## Eintritt und Lesen

Hält der Job die Leitung, beginnt der Eintritt sofort.

- **Mit SERVO SUPPLY auf `NONE`** liest der Bildschirm das Servo an der
  Versorgung, die es hat. Schalte das Servo ein, bevor du auf `D SERIES`
  tippst. Der Inhalt zeigt `READING THE MODEL`.
- **Mit SERVO SUPPLY auf `GP28` oder `GP29`** schaltet der Bildschirm die Versorgung
  2,0 s lang aus und wieder ein. Die Leiste zählt die Aus-Zeit,
  `GP0  SUPPLY OFF 1.4 S`, und der Inhalt zeigt `SUPPLY OFF`. Dann zeigt
  die Leiste `GP0  SUPPLY ON` und der Inhalt `SERVO STARTING`, während das
  Servo startet, etwa 1 s. Für diese Sekunde zeigt der Bildschirm keinen
  Zähler. Unter dem Text stehen zwei bernsteinfarbene Zeilen:
  `AN ESC ON THIS LEAD GETS THE` und `CONVERTER'S OUTPUT ON ITS BEC WIRE.`

`STOP` beendet den Eintritt. Der Bildschirm zeigt dann
`THE ENTRY WAS STOPPED.` mit `RETRY`.

Antwortet kein Servo, zeigt die Leiste `NO SERVO ANSWERS`. Eine
bernsteinfarbene Zeile nennt die letzte Antwort, zum Beispiel
`REPLY: SILENT`, und der Inhalt zeigt
`CHECK THE SERVO'S SUPPLY, THE SIGNAL AND GROUND.` Mit `GP28` oder `GP29` kommt
`THE SUPPLY IS OFF.` hinzu. `RETRY` tritt erneut ein. Die Antwortwörter sind
`SILENT`, `EARLY`, `LATE`, `NAK`, `LENGTH`, `FRAMING`, `GAP`, `NO ACK`,
`HEADER`, `ADDRESS`, `SUM`, `CUT`, `OVERFLOW` und `ARG`; `NONE` heißt: kein
Antwortcode.

Antwortet das Servo, zeigt die Leiste `GP0  IN SERIAL MODE`, und der Inhalt
zählt das Lesen: `READING 21 REGISTERS`, `7 OF 21`. Dieses erste vollständige
Lesen ist die Sicherung der Sitzung. Ein fehlgeschlagenes Lesen zeigt zum
Beispiel `READ FAILED AT 0x32` mit dem Antwortwort darunter und bietet
`READ AGAIN` an. Ein mit `STOP` abgebrochenes Lesen zeigt
`THE READ WAS STOPPED.` `BACK` während eines Lesens bricht es ab und öffnet
die Verlassenseite.

Ein abgeschlossenes Lesen öffnet die Feldliste.

Liegt ein Vermerk eines unterbrochenen Schreibens auf der Karte, zeigen die
Ansichten für Eintritt und Lesen in Bernstein zusätzlich
`A WRITE WAS INTERRUPTED. HITEC/` und
`HITEC.TXT HOLDS THE SERVO AS IT WAS.` Siehe [FILES](#files).

---

## Die Feldliste

Die Leiste zeigt das Modell, den Pin und die Sicherung:
`D485HW GP0 BACKUP IN MEMORY`, bis ein Schreibvorgang die Sicherung auf die
Karte gelegt hat, danach zum Beispiel `BACKUP HT001`. Ein Modell ohne Namen
in der Liste von rcbench zeigt `D SERIES` und seine Nummer. Rechts steht ein
grünes Feld `MODE`.

Jede Zeile zeigt die Gruppe in kleiner Schrift, den Parameternamen und den
Wert. Die Zeilen sind 40 px hoch mit 42 px Abstand. Ziehe die Liste nach oben
oder unten, um zu verschieben.

| Gruppe | Parameter |
|---|---|
| MOTION | `SPEED`, `DEAD BAND`, `SOFT START` |
| TRAVEL | `NEUTRAL`, `LEFT END`, `RIGHT END`, `DIRECTION` |
| PROTECT | `FAILSAFE`, `OVERLOAD`, `SMART SENSOR`, `SENSITIVITY` |
| INFO | `MODEL`, `ID`, `POSITION`, `R04`, `R22`, `R8A`, `R8C`, `RD4`, `RD6` |

- Werte tragen ihre Einheit und den gespeicherten Wert in Klammern:
  `90 % (18)`, `STEP 1 (1)`, `CW (0)`. `CW` heißt im Uhrzeigersinn, `CCW`
  gegen den Uhrzeigersinn. `NEUTRAL`, die Endpunkte und `POSITION` sind
  Zählwerte, zum Beispiel `8192`, ohne Klammer; ebenso `ID`, `SENSITIVITY`
  und die INFO-Zahlen `MODEL` und `R04` bis `RD6`. `FAILSAFE` zeigt `LIMP`,
  `OFF` oder eine Stellung in `US`. `US` steht für µs: Die Bildschirmschrift
  hat kein Mikro-Zeichen.
- Ein gespeicherter Wert, den keine Codierung annimmt, zeigt `RAW` und die
  Zahl in Bernstein, zum Beispiel `RAW 1234`. Ein Wert, der sich nicht
  decodieren lässt, zeigt `?`.
- Nur lesbare Zeilen sind gedämpft gezeichnet, und ein Tippen darauf wählt
  nichts aus: `SENSITIVITY` und jede INFO-Zeile außer `ID`.
- Ein geänderter Wert ist grün gezeichnet, und sein Name trägt ein `*`.

<img src="img/hitec-preview.png" width="240" alt="SPEED ausgewählt und von 100 % (4095) auf 90 % (18) gestellt">

### Einen Parameter ändern

1. Tippe auf eine änderbare Zeile. Die Editorleiste unter der Liste zeigt den
   Parameter, den gespeicherten und den vorgemerkten Wert, zum Beispiel
   `SPEED  100 % (4095) ->` über `90 % (18)`. Ist keine Zeile gewählt, zeigt
   die Leiste `TAP A ROW TO EDIT IT` und `SWIPE IT SIDEWAYS FOR COARSE`.
2. Tippe auf `-` oder `+` (44 × 44 px), um den Wert auf den nächsten Wert zu
   stellen, den seine Codierung annimmt. Gedrückt halten wiederholt den
   Schritt. `+` erhöht den angezeigten Wert, `-` senkt ihn.
3. Oder wische die Zeile zur Seite: ein grober Schritt je 16 px. Ein grober
   Schritt ist ein Elftel des Wertebereichs, aufgerundet, mindestens ein
   Schritt.
4. `REVERT` verwirft alle vorgemerkten Änderungen.

`FAILSAFE` geht durch `LIMP`, `OFF`, dann Stellungen von 900 µs bis
2100 µs. Ein `RAW`-Wert nimmt beim ersten Schritt den nächsten zulässigen
Wert an.

Das Umkehren von `DIRECTION` spiegelt `NEUTRAL` und beide Endpunkte, wie es
der Programmierer von Hitec tut.

Zwei Arten von Grenzen gelten:

- **Eine harte Grenze** wird nie angeboten. Der Schrittgeber überspringt Werte,
  die eine verletzen, daher halten `-` und `+` beim letzten zulässigen Wert an.
  Bereich und Schritte der Codierung sind harte Grenzen, ebenso die
  Reihenfolge `LEFT END` unter `NEUTRAL` unter `RIGHT END`. Ist kein weiterer
  Schritt zulässig, ist die Schaltfläche abgeblendet gezeichnet.
- **Eine weiche Regel** wird übernommen. Die Zeile bekommt an ihrem linken Rand
  eine 3 px breite bernsteinfarbene Markierung, und die Editorleiste zeigt den
  Text der Regel in Bernstein. Es sind die sieben Regeln von rcbench,
  verglichen mit dem zuletzt gelesenen Servo:

| Regel | Text |
|---|---|
| `SPEED` unter 30 % | `A SLOW SERVO CAN CAUSE LOSS OF CONTROL OF THE MODEL.` |
| `DEAD BAND` unter dem des Servos | `A NARROW DEAD BAND CAN CAUSE BACK-AND-FORTH MOVEMENT AND HEAT.` |
| `OVERLOAD` ausgeschaltet | `WITHOUT OVERLOAD PROTECTION A STALLED SERVO HEATS UP.` |
| Ein anderer Endpunkt als der des Servos | `END POINT CHANGED. THE SERVO TRAVEL PER STICK MOVEMENT CHANGES TOO.` |
| `NEUTRAL` anders als beim Servo | `THE CENTRE SHIFTS. ONE SIDE MOVES CLOSER TO THE MECHANICAL STOP.` |
| `DIRECTION` umgekehrt | `DIRECTION REVERSED. CHECK ALL CONTROL SURFACES BEFORE FLIGHT.` |
| Eine Failsafe-Stellung mehr als 300 µs von 1500 µs entfernt | `FAILSAFE POSITION IS FAR FROM THE CENTRE.` |

### Die ID

`ID` stellt die eigene ID des Servos ein, 0 bis 255. Die Platine spricht die
ID 0 an, gleich welche das Servo hält. Ob ein Servo mit einer anderen ID
weiter auf die ID 0 antwortet, ist nicht gemessen. Ein einziges Schreiben
einer anderen ID kann also ein Servo hinterlassen, das die Platine nicht
lesen und nicht aus einer Sicherung wiederherstellen kann. Solange `ID` einen
anderen Wert als 0 hält, zeigt die Editorleiste
`BENCH ADDRESSES ID 0. A WRITE MAY LOCK OUT READ AND RESTORE.` und die
Schreibseite
`THE BENCH ADDRESSES ID 0 AND MAY NOT READ OR RESTORE THIS SERVO AGAIN.`
(die Platine spricht die ID 0 an und kann dieses Servo danach vielleicht
nicht mehr lesen oder wiederherstellen).

<img src="img/hitec-id.png" width="240" alt="Die Schreibseite für ID 0 -> 1, mit dem bernsteinfarbenen ID-Hinweis">

Unter der Editorleiste liegen drei Schaltflächen, 46 px hoch: `HOLD TO WRITE`
(110 px), `REVERT` (52 px) und `FILES` (54 px). `HOLD TO WRITE` wirkt nur,
wenn etwas vorgemerkt ist und das Servo seit dem letzten fehlgeschlagenen
Schreiben gelesen wurde.

---

## Schreiben

**Halte `HOLD TO WRITE` 1 s lang.** Während du hältst, füllt sich ein Balken.
Die Schreibseite öffnet sich und schreibt noch nichts.

<img src="img/hitec-write.png" width="240" alt="Die Schreibseite: SPEED 100 % (4095) -> 90 % (18), KEEP THE SERVO HORN CLEAR.">

Die Schreibseite heißt `HITEC WRITE`. Die Leiste zählt die Werte, `1 VALUE`
oder `2 VALUES`. Jeder geänderte Parameter ist eine Zeile, `SPEED` und
`100 % (4095) -> 90 % (18)`, oder zwei, wenn der Wert nicht neben den Namen
passt. Eine geänderte `ID` steht zuerst. Danach zählt zum Beispiel
`AND 3 MORE` die Werte, die nicht aufgeführt sind.

Unter den Werten, von oben nach unten:

- `KEEP THE SERVO HORN CLEAR.` beim ersten Schreiben der Sitzung und bei
  jedem Schreiben, das `DIRECTION`, `NEUTRAL` oder einen Endpunkt ändert.
- `THE BENCH ADDRESSES ID 0 AND MAY NOT READ OR RESTORE THIS SERVO AGAIN.`,
  wenn die ID auf einen anderen Wert als 0 wechselt.
- Zum Beispiel `FILE OF ANOTHER MODEL: D645MW.` nach dem Laden einer Datei
  eines anderen Modells. Nur diese Seite sagt das.
- `MODEL NOT MEASURED: WRITTEN IN THE D LAYOUT.` für ein Modell, das rcbench
  nicht vermessen hat. Das vermessene Modell der D-Serie ist das D485HW.
- Zeilen mit einem `*` am Anfang: die weichen Regeln, die gelten.

Die ersten vier stehen immer, wenn sie gelten; die Werte bekommen den Platz,
der übrig bleibt, höchstens sechs. Jede weiche Regel steht ganz oder gar
nicht, und passen nicht alle, zählt die letzte Zeile den Rest:
`AND 2 MORE RULES: SEE EACH ROW.` Die Editorleiste jeder Zeile nennt ihre
Regel.

<img src="img/hitec-write-full.png" width="240" alt="Ein D645MW mit einer D485HW-Datei: ID zuerst, FAILSAFE unter dem Namen, AND 2 MORE, die Hornzeile, der ID-Hinweis, FILE OF ANOTHER MODEL, MODEL NOT MEASURED und AND 1 MORE RULE">

Die Zeile am unteren Rand sagt, wohin die Sicherung geht:

| Zeile | Wann |
|---|---|
| `BACKUP FIRST: BACKUP/HT001.TXT` | Eine Karte ist eingebunden, und die Sicherung liegt noch nicht auf ihr |
| `BACKUP: BACKUP/HT001.TXT` | Die Sicherung liegt auf der Karte |
| `NO CARD: OLD VALUES LOST AT LEAVE` | Keine Karte ist eingebunden |
| `CARD ON USB: OLD VALUES LOST AT LEAVE` | Ein USB-Host hält die Karte |
| `BACKUP FIRST: TO THE CARD` | Eine Karte ist eingebunden, und die nächste Sicherungsnummer ließ sich nicht von ihr lesen |
| `NO BACKUP ON THE CARD: LOST AT LEAVE` | Die Karte hat den Vermerk angenommen und die Sicherung abgewiesen |

`NO CARD`, `CARD ON USB` und `NO BACKUP ON THE CARD` stehen in Bernstein: Die
Werte des Servos vor dem ersten Schreiben liegen nur im Speicher, bis ein
späteres Schreiben mit eingebundener Karte sie sichert; LEAVE oder ein
Stromausfall verliert sie. Ohne Karte geht das Schreiben trotzdem durch.

**Ein zweites Halten von 1 s schreibt.** Zuerst kommen die Schritte auf der
Karte:

1. Vor dem ersten Schreiben der Sitzung, mit eingebundener Karte: Ein
   Vermerk, den ein unterbrochenes Schreiben auf der Karte gelassen hat,
   wandert in den Ring von `BACKUP/`, dann kommt die Sicherung als
   `BACKUP/HTnnn.TXT` auf die Karte.
2. Vor jedem Schreiben: Das Servo, wie es gerade ist, und das Ziel kommen als
   Vermerk `HITEC/HITEC.TXT` auf die Karte.

Mit einer Karte, die beim Öffnen des Bildschirms eingebunden war, gehen diese
Dateien zuerst auf die Karte, während der Bildschirm weiter zeichnet. Die
Seite zeigt `SAVING THE BACKUP TO THE CARD.` (bei einem späteren Schreiben
`SAVING THE FILES TO THE CARD.`) und zum Beispiel `BLOCKS LEFT: 12`. Das Servo
wird erst geschrieben, wenn die Karte die Dateien hält. Nimmt die Karte die
Daten nicht an, hält das Schreiben an. Beim ersten Schreiben der Sitzung
zeigt die Seite
`THE CARD DID NOT TAKE THE BACKUP (REMOVED OR SWAPPED?). NOTHING WENT TO THE SERVO. CHECK THE CARD, HOLD AGAIN.`,
bei
einem späteren Schreiben
`THE CARD DID NOT TAKE A FILE (REMOVED OR SWAPPED?). NOTHING WENT TO THE SERVO. CHECK THE CARD, HOLD AGAIN.`
Zum Servo ging nichts. Die Karte wird neu eingebunden, und `OK` führt zur
Feldliste zurück; das nächste `HOLD TO WRITE` sichert die Dateien erneut und
schreibt dann. Eine volle Karte endet nicht hier, sondern in
`THE CARD TOOK NO FILE`, bevor etwas eingereiht ist.

Das Schreiben sendet dann die geänderten Register im Abstand von 20 ms,
speichert sie im Servo, startet das Servo neu (1,0 s) und liest alle 21
Register zurück. Für eine Einstellung dauert das etwa 1,7 s, berechnet aus
den Zeiten von rcbench und nicht gemessen. Die Seite zeigt
`WRITING: DO NOT UNPLUG.` und darunter `WRITES SENT: 2 OF 4`, dann
`READING BACK`. `BACK` während des Schreibens wird vorgemerkt: Die Seite
zeigt `LEAVING AFTER THE WRITE ENDS.`, und die Verlassenseite folgt auf das
Ergebnis. Der Bildschirm bricht ein Schreiben nie ab.

Ein geprüftes Schreiben entfernt seinen Vermerk. Spätere Schreibvorgänge der
Sitzung sichern kein zweites Mal.

### Die BOARD-Spannung rot

Solange die BOARD-Spannung auf dem Tester rot ist, unter 3,30 V und bis sie
wieder 3,35 V erreicht, schreibt das zweite Halten nichts: Keine Datei geht
auf die Karte und kein Frame zum Servo. Die Seite zeigt in Bernstein
`BOARD VOLTAGE IS RED: NOT WRITTEN.` und
`CHARGE THE BOARD'S CELL, HOLD AGAIN.`, und `HOLD TO WRITE` bleibt. Der
Bildschirm fragt erneut, kurz bevor die Dateien des Schreibens und sein
erster Frame hinausgehen, nach dem Warten auf das Übertragen zur Karte. Eine
Spannung, die währenddessen rot wird, hält das Schreiben ebenfalls an: Die
Sicherung und der Vermerk des Schreibens können dann schon auf der Karte
sein, und kein Frame geht hinaus. Ein laufendes Schreiben wird nicht
angehalten: Abgebrochen ließe es das Servo teilweise geschrieben und
ungeprüft zurück, während eines, das zu Ende läuft, mit dem Rücklesen
endet. Bernstein und ein Wert unter 2,50 V (keine Zelle zu beurteilen)
lehnen kein Schreiben ab. `HOLD TO RESTORE` wird ebenso abgelehnt. Die
Bereiche sind der übliche Bereich einer Lithium-Polymer-Zelle, auf dieser
Platine nicht gemessen. Was die Platine an USB-Versorgung ohne Zelle liest,
ist ebenfalls nicht gemessen, also kann eine Platine nur an USB das
Schreiben ablehnen.

<img src="img/hitec-board-low.png" width="240" alt="Die Schreibseite mit roter BOARD-Spannung: BOARD VOLTAGE IS RED: NOT WRITTEN. und CHARGE THE BOARD'S CELL, HOLD AGAIN. in Bernstein">

### Das Ergebnis

Das Ergebnis ersetzt das Halten durch `OK`. `OK` führt zur Feldliste zurück.

<img src="img/hitec-write-error.png" width="240" alt="THE SERVO DOES NOT HOLD THE VALUES: 0x54. READ AND CHECK.">

| Ergebnis | Farbe | Bedeutung |
|---|---|---|
| `1 VALUE WRITTEN AND VERIFIED.` | grün | Das Servo hält die neuen Werte |
| `NOTHING WAS WRITTEN ...` | bernsteinfarben | Kein Frame hat das Servo erreicht |
| `THE SERVO DOES NOT HOLD THE VALUES: 0x54. READ AND CHECK.` | rot | Das Rücklesen weicht an den genannten Registern ab, höchstens vier, danach `...` |
| `REGISTER 0x54 GOT NO ANSWER. READ THE SERVO BEFORE A WRITE.` | rot | Ein Schreibvorgang bekam keine Antwort |
| `THE WRITE STOPPED AT STEP 2 OF 4. READ THE SERVO BEFORE A WRITE.` | rot | Ein Teil des Plans lief; der Zustand des Servos ist unbekannt |
| `ALL WRITES SENT; READ-BACK FAILED. READ THE SERVO BEFORE A WRITE.` | rot | Jeder Schreibvorgang ging hinaus, und das Rücklesen endete nicht |

Zu den bernsteinfarbenen Ergebnissen gehören:

- `NOTHING WAS WRITTEN: READ THE SERVO FIRST.`
- `NOTHING WAS WRITTEN: NO PLAN (CODE n).`, mit dem Code des Planers.
- `NOTHING WAS WRITTEN (CODE n).`, mit dem Code der Sitzung.
- `THE SCREEN STALLED. NOTHING WAS WRITTEN.`
- `THE CARD TOOK NO FILE (FULL OR REMOVED?). NOTHING WENT TO THE SERVO. CHECK THE CARD, HOLD AGAIN.`
- `THE CARD DID NOT TAKE A FILE (REMOVED OR SWAPPED?). NOTHING WENT TO THE SERVO. CHECK THE CARD, HOLD AGAIN.`

Nach einem Schreiben, das fehlschlug, nachdem Frames gesendet waren, hält
das Abbild die berührten Register nicht mehr: Die Schreib-Schaltfläche zeigt
`READ AGAIN`, bis ein Lesen sie füllt. Der Vermerk bleibt auf der Karte.

---

## FILES

`FILES` in der Feldliste öffnet die Hitec-Dateien der Karte.

<img src="img/hitec-files.png" width="240" alt="Die Seite HITEC FILES nach SAVE AS: SAVED AS HT001">

- Die Leiste zählt die `.TXT`-Dateien aus `HITEC/`, die die Liste zeigt:
  `CARD  12 FILES`. Vermerk und Sicherungen zählt sie nicht.
- Liegt der Vermerk `HITEC/HITEC.TXT` auf der Karte, ist er die erste Zeile,
  benannt `RECORD` mit `(HITEC.TXT)` in Bernstein rechts.
- Es folgen die Hitec-Sicherungen aus `BACKUP/`, die jüngste zuerst, jede mit
  `(BACKUP)` rechts.
- Dann die `.TXT`-Dateien aus `HITEC/`, nach Namen ohne Rücksicht auf Groß-
  und Kleinschreibung, höchstens 64. Ein Name länger als 8.3 (12 Zeichen)
  wird nicht gelistet. Eine Zeile zeigt den Namen ohne `.TXT`, `HITEC001`
  für eine Datei von rcbench.
- Tippe auf eine Zeile, um sie auszuwählen.

| Schaltfläche | Was sie tut |
|---|---|
| `SAVE AS HT002` | Schreibt das Servo, wie es zuletzt gelesen wurde, nach `HITEC/` unter dem nächsten freien Namen, eins über dem höchsten HTnnn, und nie über eine Datei. Die Leiste zeigt `SAVED AS HT002` in Grün. Hält `HITEC/` 50 `.TXT`-Dateien, den Vermerk nicht gezählt, wirkt die Schaltfläche nicht, und die Leiste zeigt `HITEC/ HOLDS 50 FILES. REMOVE SOME.` |
| `LOAD` | Übernimmt die änderbaren Werte der gewählten Datei als vorgemerkte Änderungen in die Feldliste. Geschrieben wird dann über `HOLD TO WRITE`. Ein Vermerk lädt das Servo, wie es vor seinem Schreiben war |
| `HOLD TO RESTORE` | 1 s halten. Öffnet die Wiederherstellungsseite `HITEC RESTORE`, die die Sicherung der Sitzung zurückschreibt. Wirkt, solange das Servo von der Sicherung abweicht |

Die Leiste der Wiederherstellungsseite zeigt zum Beispiel
`BACK TO THE BACKUP: 3 VALUES`. Ihr Halten, `HOLD TO RESTORE`, schreibt wie
`HOLD TO WRITE`.

Eine Datei der anderen Familie oder eines anderen Layouts wird abgelehnt:
`NOT A FILE FOR THIS SERVO: CLASSIC` oder
`NOT A FILE FOR THIS SERVO: LAYOUT`. Eine Datei eines anderen Modells mit
demselben Layout lädt, und die Schreibseite nennt sie. Weitere Hinweise in
der Leiste:

- `THE FILE DID NOT PARSE`: Der Text ist keine Servodatei, oder seine
  CRC-Zeile (Cyclic Redundancy Check, Prüfsumme) stimmt nicht.
- `THE FILE DID NOT READ`, `THE CARD IS FULL`, `THE CARD TOOK NO FILE`,
  `THE CARD'S LIST DID NOT READ`.
- `CARD MOUNTED AGAIN: CHOOSE AGAIN`: Ein Schreiben auf die Karte ist vorher
  fehlgeschlagen, daher hat LOAD die Karte neu eingebunden und die Liste neu
  gelesen. Nichts wird geladen, bis eine Zeile neu gewählt ist.

Die Dateien sind der Servodatei-Text von rcbench mit einer CRC-16-Zeile. Eine
Datei von der Karte von rcbench lädt, sobald sie nach `HITEC/` kopiert ist.
Eine von Hand bearbeitete Datei lädt nicht.

### Keine Karte, eine von USB gehaltene Karte, eine Karte ohne Datei

- **Keine Karte.** Die Leiste zeigt `NO CARD`. Die Seite zeigt
  `READ, EDIT AND WRITE STILL WORK.` und bietet `RETRY MOUNT` an. Sicherung
  und Vermerk bleiben im Speicher.
- **Eine von USB gehaltene Karte.** Die Leiste zeigt `CARD IN USE BY USB`.
  `RETRY MOUNT` ist abgeblendet gezeichnet. Die Sicherung bleibt im Speicher,
  und Lesen, Ändern und Schreiben funktionieren.
- **Eine Karte, die keine Datei annimmt.** Nimmt eine eingebundene Karte weder
  die Sicherung noch den Vermerk an, stoppt das Schreiben vor dem ersten
  Frame. Die Schreibseite zeigt
  `THE CARD TOOK NO FILE (FULL OR REMOVED?). NOTHING WENT TO THE SERVO. CHECK THE CARD, HOLD AGAIN.`

Die Sicherungen teilen sich einen Ring von 50 Nummern mit den KST- und
AM32-Sicherungen, `BACKUP/HTnnn.TXT`, `BACKUP/KSTnnn.TXT` und
`BACKUP/AMnnn.TXT`. Sind alle 50 vergeben, überschreibt die nächste
Sicherung die älteste, gleich von welchem Gerät. Den Ring beschreibt die
[KST-Programmierung](KST-Programmierung#files).

---

## Verlassen

`BACK` in der Feldliste öffnet die Verlassenseite. `BACK` während eines
Lesens öffnet sie ebenfalls.

Die Leiste nennt das Modell, den Pin und, mit `GP28` oder `GP29`, `SUPPLY ON` oder
`SUPPLY OFF`. Die Seite zeigt mit `GP28` oder `GP29` `LEAVE SWITCHES THE SERVO'S SUPPLY OFF.`,
mit `NONE` `LEAVE RESTARTS THE SERVO.` Mit vorgemerkten Änderungen kommt
`2 CHANGES NOT WRITTEN.` hinzu, nach einem fehlgeschlagenen Schreiben
`A WRITE DID NOT END VERIFIED.` in Bernstein.

| Schaltfläche | Was sie tut |
|---|---|
| `READ AGAIN` | Liest das Servo erneut und führt zur Feldliste zurück. Vorgemerkte Änderungen bleiben |
| `LEAVE` | Beendet die Sitzung: Mit `GP28` oder `GP29` schaltet es die Versorgung aus, mit `NONE` startet es das Servo mit seinen gespeicherten Werten neu |
| `BACK` | Führt zur Feldliste zurück |

Nach `LEAVE` mit `GP28` oder `GP29` zeigt die nächste Seite
`SUPPLY OFF FOR 2.0 S: 1.4 S LEFT.`, bis die Leitung geschlossen ist. Dann
zeigt sie `UNPLUG THE SERVO.` über `DSHOT RESTARTS ON GP0 AND READS AS` und
`A 970 US PULSE AT 1000 HZ.`

<img src="img/hitec-unplug.png" width="240" alt="Die Abziehseite: UNPLUG THE SERVO. und RESTART DSHOT">

**Zieh das Servo ab und tippe dann auf `RESTART DSHOT`.** Der ESC-Treiber
übernimmt den Pin wieder, und `MENU` erscheint. `BACK` auf dieser Seite
behält den Pin und tritt erneut in das Servo ein. Solange die Seite
`RELEASING THE PIN` zeigt oder die Versorgung herunterzählt, ist `BACK`
deaktiviert gezeichnet, als Umriss mit grauer Beschriftung: Es wirkt ab dem
Moment, in dem `UNPLUG THE SERVO.` erscheint.

`BACK` während des Eintritts, bevor das Servo geantwortet hat, bricht ihn ab
und zeigt die Abziehseite.

---

## Wenn es nicht funktioniert

- **`AN ESC ANSWERS ON GP0.`** Ein ESC hängt am ESC-Pin. Zieh ihn ab.
- **`NO SERVO ANSWERS` mit `REPLY: SILENT`.** Nichts hat geantwortet. Prüfe
  die Versorgung des Servos, das Signalkabel und die Masse. Mit `NONE`
  schalte das Servo vor dem Eintritt ein.
- **`NO SERVO ANSWERS` mit einem anderen Antwortwort.** Flanken kommen an,
  aber keine Antwort ist gültig. Prüfe Signalkabel und Masse, und dass kein
  Pull-up-Widerstand und kein zweites Servo an der Leitung hängt.
- **`READ FAILED AT 0x..`.** Tippe auf `READ AGAIN`.
- **`THE SCREEN STALLED. NOTHING WAS WRITTEN.`** Der Bildschirm hing länger
  als 250 ms, bevor der Schreibvorgang hinausging. Schreibe erneut.
- **Ein rotes Ergebnis.** Tippe auf `OK`, dann auf `READ AGAIN`, und prüfe
  die Werte. Zurück zum Servo, wie es war, führt `HOLD TO RESTORE` in FILES.
- **`THE CARD TOOK NO FILE (FULL OR REMOVED?). NOTHING WENT TO THE SERVO. CHECK THE CARD, HOLD AGAIN.`**
  Prüfe die Karte, oder nimm sie aus der USB-Nutzung, und schreibe dann
  erneut.

---

## Geschaltete Versorgung

Mit **SETTINGS → SERVO SUPPLY** auf `GP28` oder `GP29` versorgt das Gerät das Servo über
den DC-DC-Wandler (Gleichspannungswandler), dessen Enable-Eingang an GP28,
J4 Pin 11, oder GP29, J4 Pin 12, liegt. Seine Verkabelung steht in der
[KST-Programmierung](KST-Programmierung#geschaltete-versorgung). **Ein ESC an
dieser Leitung bekommt den Ausgang des Wandlers auf seine BEC-Leitung.**

<img src="img/hitec-stall.png" width="240" alt="Der hängende Bildschirm: GP0 SUPPLY OFF, RETRY">

- **Der Eintritt** schaltet das Servo 2,0 s aus, dann ein, und wartet etwa
  1 s auf seinen Start.
- **Die Versorgung ist nur an, solange der Bildschirm läuft.** Ein
  Bildschirm, der 250 ms hängt, schaltet sie ab: während des Eintritts
  sofort, sonst sobald ein laufendes Lesen oder Schreiben geendet hat. Die
  Seite zeigt dann
  `THE SCREEN STALLED, SO THE BENCH SWITCHED THE SERVO'S SUPPLY OFF.` und
  `RETRY POWERS IT UP AND READS IT AGAIN.` Ohne Karte, nach einem
  Schreiben, kommt `NO CARD: THE BACKUP IN MEMORY IS LOST.` hinzu.
- **LEAVE** schaltet die Versorgung ab. Der Pin wird 2,0 s nach dem
  Ausschalten freigegeben.

Mit `NONE` beendet ein Hängen die Sitzung nicht. Ein Hängen während des
Eintritts beendet den Eintritt: Die Seite zeigt `NO SERVO ANSWERS` mit
RETRY. Später endet ein laufendes Lesen oder Schreiben, das Servo bleibt im
Programmiermodus, und der nächste Vorgang wird abgewiesen, bis der
Bildschirm wieder läuft: Ein Schreiben zeigt dann
`THE SCREEN STALLED. NOTHING WAS WRITTEN.`

---

## Klassische Servos

`CLASSIC` (HS-5xxx, HS-7xxx) braucht SERVO SUPPLY auf `GP28` oder `GP29`:
Ein klassisches Servo geht nur beim Einschalten in den Programmiermodus,
und die Platine schaltet seine Versorgung über den Wandler aus Teil C der
[Verdrahtung](Verdrahtung). Unter `NONE` zeigt die Familienauswahl
`NEEDS THE CONVERTER OF WIRING PART C` und
`AND SETTINGS > SERVO SUPPLY GP28/29.`

Der Signalpegel:

- Ein klassisches Servo treibt die Signalleitung selbst auf High, über etwa
  1 kΩ, aus seiner Versorgung. Der Wandler liefert nominal 5 V.
- SIG, GP0 oder GP1, ist ein fehlertoleranter Pin. Er verträgt 5,5 V,
  solange die Platine eingeschaltet ist, und 3,63 V, solange sie aus ist.
  Schalte die Platine vor der Servoversorgung ein.
- Die Spannung auf der klassischen Leitung ist nicht gemessen.
- Zieht die Platine die Leitung gegen das 1 kΩ des Servos auf Low, nimmt
  der Pin bei 5 V etwa 5 mA auf. Der Low-Pegel bei diesem Strom ist nicht
  spezifiziert.

Die klassischen Bildschirme sind nur auf dem Host gebaut und getestet, gegen
ein simuliertes Servo. Noch kein klassischer Eintritt, kein Lesen und kein
Schreiben lief an einem Servo auf dieser Platine. Die Bilder unten zeigen
sie mit SERVO SUPPLY auf `GP28`.

<img src="img/hitec-family-classic.png" width="240" alt="Die Familienauswahl mit SERVO SUPPLY auf GP28: CLASSIC freigegeben"> <img src="img/hitec-repair.png" width="240" alt="Ein klassisches HS-5645MG: CHECKSUM BAD mit REPAIR"> <img src="img/hitec-not-measured.png" width="240" alt="Ein nicht vermessenes klassisches Modell: MODEL NOT MEASURED: WRITTEN IN THE CLASSIC LAYOUT.">

- **CHECKSUM.** Ein klassisches Servo, dessen Prüfsumme nicht aufgeht, zeigt
  `CHECKSUM` `BAD` in Bernstein. Die Zeile lässt sich auswählen, und die
  Editorleiste zeigt
  `CHECKSUM BAD. THE SERVO MAY IGNORE PWM AND DRAW HIGH CURRENT.` mit
  `REPAIR` anstelle von `-` und `+`. REPAIR wird dann über die Schreibseite
  geschrieben.
- **FACTORY VALUES.** FILES listet die Werkswerte des Servos als Zeile mit
  `(SERVO)`. LOAD übernimmt sie als vorgemerkte Änderungen in die Feldliste.
- **Modelle.** Die vermessenen klassischen Modelle sind das HS-5645MG und das
  HS-7955TG. Ein anderes Modell wird im Layout seiner Serie geschrieben, und
  die Schreibseite zeigt
  `MODEL NOT MEASURED: WRITTEN IN THE CLASSIC LAYOUT.`

---

## Einschränkungen

- Die Platine sendet jeden Frame an die ID 0. Ob ein Servo der D-Serie mit
  einer anderen ID darauf antwortet, ist nicht gemessen.
- Die Spannung auf der klassischen Leitung ist nicht gemessen, und der
  Low-Pegel des Pins gegen ein klassisches Servo ist nicht spezifiziert.
- Mit `GP28` oder `GP29` halten die Kartenzugriffe des Bildschirms die Versorgung
  höchstens 15 s lang an, während der Bildschirm nicht neu zeichnet, wie bei
  KST. Mit den vollen Zeiten der SD-Spezifikation dauert das erste
  Schreiben, das einen alten Vermerk aufbewahrt, 17,9 s auf einer
  exFAT-SDXC-Karte (Extended File Allocation Table, Secure Digital eXtended
  Capacity). Das liegt über der Grenze, und die Versorgung geht aus. Auf
  FAT32 (File Allocation Table, 32 Bit) dauert jede Kartenphase von
  Hitec höchstens 13,2 s, ohne Logs im Wurzelverzeichnis der Karte. Das sind
  FatFs-Blockzahlen mal Kartenzeiten, nicht auf einer Karte gemessen.
- Ein späteres Schreiben, dessen Vermerk die Karte nicht annimmt, läuft
  trotzdem, auf der Sicherung des ersten Schreibens. Die Schreibseite sagt
  das nicht.
- `READ AGAIN` verwirft ein vorgemerktes klassisches REPAIR. Das Lesen zeigt
  wieder `BAD`.
- Der Eintritt zeigt keinen Countdown für den Start des Servos von 1 s.

Vom Hitec-Programmierer ist nichts auf Hardware getestet.
