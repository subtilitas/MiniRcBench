[English](KST-Programming) | **Deutsch**

# KST-Programmierung

Dieser Bildschirm liest und schreibt die Einstellungen eines KST- oder
Chaservo-Servos über dessen Signalkabel. Beide Marken teilen sich ein
Programmierprotokoll und einen Satz von 32 Registern. Das Protokoll ist
Manchester-codiert, halbduplex, und läuft im PIO des RP2350 (Programmable I/O,
programmierbare Ein-/Ausgabe). Der Bildschirm zeigt die Register als 27
benannte Felder mit ihren Einheiten, stellt die änderbaren Felder ein und
schreibt sie zurück, zuvor mit einer Sicherung auf der SD-Karte (Secure
Digital).

<img src="img/kst-list.png" width="240" alt="Die KST-Feldliste">

> **Nicht auf Hardware getestet.** Noch kein Frame des Leitungstreibers lag
> auf einem Pin. Die Bildschirme in diesem Wiki entstehen aus dem Code der
> Bedienoberfläche (UI, User Interface) gegen ein simuliertes Servo.

> **Zieh das Servo ab, bevor DShot läuft.** Außerhalb dieses Bildschirms liegt
> auf dem ESC-Pin (Electronic Speed Controller, Motorregler) DShot, das
> digitale Gasprotokoll. Ein Servo an diesem Pin liest das als Impuls von etwa
> 970 µs mit 1000 Hz und fährt Richtung eines Endanschlags. Das passiert, wenn
> die Platine mit einem Servo am ESC-Pin **startet**, und wenn du diesen
> Bildschirm verlässt. Der Bildschirm fordert dich vorher zum Abziehen auf.

Der Bildschirm schaltet die Versorgung des Servos nicht. Du schaltest das
Servo von Hand ein und aus, mit einem Stecker oder einem Schalter. Siehe
[Einschränkungen](#einschränkungen).

---

## Verkabelung

Das Servo braucht eine eigene Stromversorgung. Die Platine liefert nur Signal
und Masse. Das Servo antwortet nur in den ersten Momenten, nachdem seine
Versorgung eingeschaltet wurde. Die Versorgung muss sich daher von Hand
schalten lassen: mit einem Stecker oder einem Schalter im Plusleiter.

```
  Servoversorgung, 5 V
     +  ----/ Schalter /-------------------------------  Servo +   (rot, Mitte)
     -  ----------------+-----------------------------  Servo -   (braun oder schwarz)
                        |
  Platine GND  ---------+
  Platine SIG  ----------------------------------------  Servosignal (orange oder weiß)

  SIG GP29 = J4 Pin 12, GND = J4 Pin 1 oder 5
```

- Der Besitzer programmiert jedes KST-Servo, auch HV-Servos (Hochvolt), mit
  5 V. Andere Spannungen sind nicht getestet.
- Das mittlere Kabel des Servos geht an seine Versorgung, nie an einen Pin
  der Platine. Der 5-V-Pin der Platine ist ihre USB-Versorgung (Universal
  Serial Bus), und ein Servo kann mehr als 1 A ziehen.
- Schalte zuerst die Platine ein und dann die Servoversorgung, und beim
  Ausschalten zuerst die Servoversorgung.
- Verbinde die beiden Massen einmal, an der Platine.
- SIG ist der ESC-Pin, so wie unter **MENU → SETTINGS** eingestellt.
  Voreingestellt ist GP29.
- Zieh jeden ESC vom ESC-Pin ab. Der Bildschirm wartet, solange ein ESC
  darauf antwortet, erkennt aber nicht jeden ESC.

---

## Öffnen

**MENU → KST SERVO** öffnen.

Der Bildschirm übernimmt SIG vom ESC-Treiber. Dabei zeigt die Leiste
`RELEASING GP29`, dann `TAKING GP29`. Solange ein ESC auf dem Pin antwortet,
zeigt sie stattdessen `CHECKING GP29 FOR AN ESC`.

- Antwortet ein ESC auf dem Pin, zeigt der Bildschirm `AN ESC ANSWERS ON GP29.`
  und danach `UNPLUG IT: KST FRAMES MAY READ` und `AS THROTTLE.` Er übernimmt
  den Pin, sobald der ESC weg ist.
- Solange der ESC scharf ist, zeigt der Bildschirm `DISARM FIRST`. Solange die
  Pins noch belegt sind, zeigt er `PINS BUSY`.
- Kann die Platine die Leitung nicht einrichten, zeigt der Bildschirm
  `GP29 COULD NOT BE TAKEN (`, danach `REFUSED`, `PWM`, `PIO`, `DMA` oder
  `CLOCK` und eine schließende Klammer. PWM ist Pulsweitenmodulation, DMA
  direkter Speicherzugriff (Direct Memory Access). `RETRY` versucht es
  erneut.

Ist der Pin übernommen, beginnt die Suche sofort. Der Bildschirm sendet keine
Servoimpulse.

> Ein Servo mit Strom an SIG bekommt keine Impulse, solange der Bildschirm
> offen ist. Es kann in seine Failsafe-Stellung fahren.

---

## Suchen

<img src="img/kst-search.png" width="240" alt="Die Suche: POWER UP THE SERVO NOW">

Der Bildschirm wiederholt die Eintrittsfolge des KST-Codes von rcbench, einen
Versuch alle 116,6 ms bis 118,8 ms, 30 s lang. Die Leiste zählt Versuche und
Zeit: `GP29  TRY 12  1.4 S`. Solange die Suche läuft, dreht sich ein Kreis aus
Punkten.

Der Inhalt zeigt `POWER UP` und `THE SERVO NOW`, dann
`PLUG IT IN OR SWITCH ITS SUPPLY ON.` und
`IT ANSWERS ONLY RIGHT AFTER POWER-ON.`

**Schalte das Servo ein, während der Bildschirm sucht.** Das Servo geht in den
Programmiermodus und antwortet. Kommen Flanken auf der Leitung an, aber keine
Antwort ist gültig, nennt eine bernsteinfarbene Zeile den Fehler, zum Beispiel
`REPLY NOT VALID: LATE`. Die Wörter sind `EARLY`, `LINE HIGH`, `LATE`,
`CELL`, `LENGTH`, `RATE`, `FLAGS`, `LONG LAST EDGE`, `WINDOW`, `OVERFLOW` und
`CAPTURE`.

| Leiste | Was geschehen ist | Schaltfläche |
|---|---|---|
| `GP29  TRY 12  1.4 S` | Die Suche läuft | `STOP SEARCH` |
| `NO SERVO IN 30 S` | Kein Servo hat in 30 s geantwortet. Der Inhalt zeigt `CHECK SIGNAL AND GROUND, THEN POWER THE SERVO UP AGAIN.` | `RETRY` |
| `SEARCH STOPPED` | `STOP SEARCH` hat sie beendet, oder der Bildschirm hat gehangen und die Suche hat sich selbst gestoppt | `RETRY` |
| `SEARCH FAILED` | Die Sitzung endete mit einem Ergebnis, das der Inhalt in Bernstein nennt | `RETRY` |

Antwortet ein Servo, zeigt die Leiste `IN PROGRAMMING MODE` und der Inhalt
`READING 32 REGISTERS`. Der Bildschirm liest jedes Register, bis 3 von 5
Lesungen übereinstimmen. Beruhigt sich ein Register nicht, zeigt der Inhalt
`READ FAILED: NO 3 EQUAL READS` und listet die Register, zum Beispiel
`REGISTERS 03 0A`. `READ AGAIN` wiederholt das Lesen.

<img src="img/kst-readfail.png" width="240" alt="READ FAILED: NO 3 EQUAL READS, Register 05">

Ist ein früherer Schreibvorgang nicht zu Ende gelaufen, liegt auf der Karte
der Vermerk `KST/KST.TXT`. Der Bildschirm zeigt dann in Bernstein
`A WRITE WAS INTERRUPTED. KST/KST.TXT` und `HOLDS THE SERVO AS IT WAS.`
Siehe [FILES](#files).

Antwortet das Servo später nicht mehr, kehrt der Bildschirm hierher zurück
und sucht erneut, aus jeder Ansicht außer der Verlassenseite und der
Abziehseite. Er bemerkt es nur, während ein Lesen oder Schreiben läuft: Ein
Servo, das unter einer ruhenden Feldliste ausgeschaltet wird, lässt die
Liste stehen bis zum nächsten Lesen oder Schreiben. `BACK` während dieser
Suche beendet sie und öffnet die Verlassenseite.

---

## Die Feldliste

Ein abgeschlossenes Lesen öffnet die Feldliste.

Die Leiste zeigt den Servotyp, `V6` oder `V8`, den Pin und die Sicherung:
`BACKUP IN MEMORY`, bis ein Schreibvorgang die Sicherung auf die Karte gelegt
hat, danach `BACKUP KST001`. Rechts steht ein Feld: `MODE` (grün), oder
`IN DOUBT` (bernsteinfarben), wenn ein Schreibvorgang das Servo in einem
unbekannten Zustand zurückgelassen hat.

Jede Zeile zeigt die Gruppe in kleiner Schrift, den Feldnamen und den Wert.
Die Zeilen sind 40 px hoch mit 42 px Abstand. Ziehe die Liste nach oben oder
unten, um zu verschieben.

| Gruppe | Felder |
|---|---|
| TUNE | `MAX. DUTY`, `BOOST`, `STRETCH`, `SOFT START`, `SPEED LIMIT`, `SPEED`, `DEAD BAND` |
| PULSE | `PULSE LOWER`, `PULSE UPPER`, `NEUTRAL`, `REVERSION`, `LEFT ANGLE`, `RIGHT ANGLE` |
| FAILSAFE | `ALLOW UNCONT`, `UNCONTROLLED POS`, `UNCONTROLLED TIME` |
| LOCKED | `20K SEL`, `LOCK`, `PROT1 CANCLE`, `PROT1 EN`, `PROT1 TIME`, `PROT1 PWM`, `PROT POT ERR`, `POT SAM TIMES`, `PROT PWM ERR`, `NODE ADDR`, `SERVO / GYRO` |

- Werte tragen ihre Einheit: `96 %`, `300.0 US`, `1.0 S`, `X1`, `ON`, `OFF`
  oder eine Anzahl der Form `n OF m`. `US` steht für µs: Die Bildschirmschrift
  hat kein Mikro-Zeichen. Ein Rohwert, dessen Anzeige nicht feststeht, zeigt
  `RAW` und die Zahl.
- LOCKED-Felder sind nur lesbar. Sie sind grau gezeichnet, und ein Tippen
  darauf wählt nichts aus.
- Ein geänderter Wert ist grün gezeichnet.

<img src="img/kst-edit.png" width="240" alt="DEAD BAND ausgewählt und von 2.0 US auf 2.2 US gestellt">

### Ein Feld ändern

1. Tippe auf eine änderbare Zeile. Die Editorleiste unter der Liste zeigt das
   Feld, den gespeicherten und den vorgemerkten Wert, zum Beispiel
   `DEAD BAND  2.0 US ->`. Ist keine Zeile gewählt, zeigt die Leiste
   `TAP A ROW TO EDIT IT` und `SWIPE IT SIDEWAYS FOR COARSE`.
2. Tippe auf `-` oder `+` (44 × 44 px), um den Wert zu verstellen. Gedrückt
   halten wiederholt den Schritt.
3. Oder wische die Zeile zur Seite. Ein Schritt ist ein Elftel des
   Wertebereichs des Feldes, aufgerundet, mindestens ein Rohschritt.
4. `REVERT` verwirft alle vorgemerkten Änderungen.

Ein Rohschritt ändert nicht immer den Text: `MAX. DUTY` zeigt `96 %` bei
Rohwert 245 und bei Rohwert 244. Die Editorleiste und die Schreibseite
ergänzen dann die Rohwerte: `96 % (245) -> 96 % (244)`.

Zwei Arten von Grenzen gelten:

- **Eine harte Grenze** wird nie angeboten. Der Schrittgeber überspringt Werte,
  die eine verletzen, daher halten `-` und `+` beim letzten zulässigen Wert an.
  `MAX. DUTY` hält zum Beispiel bei 98 % an. Ist kein weiterer Schritt
  zulässig, ist die Schaltfläche abgeblendet gezeichnet.
- **Eine weiche Regel** wird übernommen. Die Zeile bekommt an ihrem linken Rand
  eine 3 px breite bernsteinfarbene Markierung, und die Editorleiste zeigt den
  Text der Regel in Bernstein, zum Beispiel
  `WITHOUT SOFT START THE SERVO JUMPS AT POWER-ON.`

Unter der Editorleiste liegen drei Schaltflächen, 46 px hoch: `HOLD TO WRITE`
(110 px), `REVERT` (52 px) und `FILES` (54 px). `HOLD TO WRITE` wirkt nur,
wenn etwas vorgemerkt ist, keine harte Grenze verletzt ist und das Servo nicht
im Zweifel ist.

---

## Schreiben

**Halte `HOLD TO WRITE` 1 s lang.** Während du hältst, füllt sich ein Balken.
Die Schreibseite öffnet sich und schreibt noch nichts.

<img src="img/kst-write.png" width="240" alt="Die Schreibseite, ein Feld"> <img src="img/kst-write-many.png" width="240" alt="Die Schreibseite, fünf Felder, AND 1 MORE">

Die Schreibseite heißt `KST WRITE`. Die Leiste zählt die Felder, `1 FIELD`
oder `2 FIELDS`. Jedes geänderte Feld ist eine Zeile, `DEAD BAND` und
`2.0 US -> 2.2 US`, bis zu vier, danach `AND 3 MORE`. Bernsteinfarbene Zeilen
mit einem `*` am Anfang nennen die weichen Regeln, die gelten. Die Zeile am
unteren Rand sagt, wohin die Sicherung geht:

| Zeile | Wann |
|---|---|
| `BACKUP FIRST: KST/KST001.TXT` | Eine Karte ist eingebunden, und in dieser Sitzung ist noch nichts geschrieben worden |
| `BACKUP: KST/KST001.TXT` | Die Sicherung liegt auf der Karte |
| `NO CARD: BACKUP IN MEMORY` | Keine Karte ist eingebunden |
| `CARD IN USE BY USB: BACKUP IN MEMORY` | Ein USB-Host hält die Karte |

**Ein zweites Halten von 1 s schreibt.** Bevor der erste Frame zum Servo geht,
kommt das beim Eintritt gelesene Abbild als `KST/KSTnnn.TXT` auf die Karte,
und das Servo, wie es gerade ist, als Vermerk `KST/KST.TXT`. Das Schreiben
läuft dann als Einzelregister-Schreibvorgänge mit Rücklesen, gefolgt von
einer vollständigen Prüfung. Ein geprüftes Schreiben entfernt den Vermerk.

Während des Schreibens zeigt die Seite `WRITING: DO NOT UNPLUG.` und darunter
`PLANNING THE ORDER OF THE WRITES` oder `FRAMES SENT: 94`. `BACK` während des
Schreibens wird vorgemerkt: Die Seite zeigt `LEAVING AFTER THE WRITE ENDS.`,
und die Verlassenseite folgt auf das Ergebnis.

### Das Ergebnis

Das Ergebnis ersetzt die Schaltflächen durch `OK`. `OK` führt zur Feldliste
zurück.

<img src="img/kst-result.png" width="240" alt="WRITE VERIFIED">

| Ergebnis | Farbe | Bedeutung |
|---|---|---|
| `WRITE VERIFIED` | grün | Das Servo hält die eingestellten Werte |
| `... NOTHING WAS WRITTEN.` | bernsteinfarben | Kein Register hat sich geändert |
| `ROLLED BACK: THE SERVO HOLDS ITS OLD VALUES` | bernsteinfarben | Ein Schreibvorgang hat nicht gegriffen und wurde zurückgenommen |
| `IN DOUBT: ... RESTORE FROM FILES.` | rot | Der Zustand des Servos ist unbekannt; nur eine Wiederherstellung wird angenommen |

Zu den bernsteinfarbenen Ergebnissen, die auf `NOTHING WAS WRITTEN.` enden,
gehören:

- `THE SERVO DIFFERS FROM THE READ. NOTHING WAS WRITTEN.`
- `A WRITE DID NOT TAKE. NOTHING WAS WRITTEN.`
- `THE VALUES BREAK A LIMIT. NOTHING WAS WRITTEN.`
- `NO SAFE ORDER OF WRITES LEADS THERE. NOTHING WAS WRITTEN.`
- `TOO MANY CHANGES AT ONCE: WRITE FEWER. NOTHING WAS WRITTEN.`

Auch `NO SERVO ANSWERS. CHECK THE PLUG AND THE SUPPLY.` ist bernsteinfarben:
Das Servo hat nicht geantwortet, und nichts wurde geschrieben.

Zu den roten Ergebnissen gehören:

- `IN DOUBT: THE ROLL-BACK FAILED. RESTORE FROM FILES.`
- `IN DOUBT: A REGISTER CHANGED THAT WAS NOT WRITTEN. RESTORE FROM FILES.`
- `IN DOUBT: THE SERVO DOES NOT HOLD THE VALUES. RESTORE FROM FILES.`
- `THE LINE FAILED. READ THE SERVO AGAIN.`
- `ABORTED. READ THE SERVO AGAIN.`

### Im Zweifel

<img src="img/kst-doubt.png" width="240" alt="IN DOUBT: THE SERVO DOES NOT HOLD THE VALUES">

Nach einem Ergebnis `IN DOUBT` zeigt das Feld der Feldliste `IN DOUBT`, und
die Schreib-Schaltfläche zeigt `IN DOUBT` und tut nichts. Der Bildschirm
nimmt kein Schreiben an, bis eine Wiederherstellung gelaufen ist. Stelle
über [FILES](#files) wieder her.

---

## FILES

`FILES` in der Feldliste öffnet die Dateien `KST/*.TXT` der Karte.

<img src="img/kst-files.png" width="240" alt="Die Seite KST FILES">

- Die Leiste zählt die Dateien: `CARD  12 FILES`. Die Seite listet die ersten
  64 Dateien, danach zeigt die Leiste zum Beispiel
  `CARD  FIRST 64 OF 100 FILES`.
- Die jüngste KSTnnn steht vorn. Liegt der Vermerk `KST/KST.TXT` auf der
  Karte, ist er die allererste Zeile, benannt `RECORD` mit `(KST.TXT)` rechts.
  Die Sicherung dieser Sitzung trägt `(BACKUP)`.
- Tippe auf eine Zeile, um sie auszuwählen.

| Schaltfläche | Was sie tut |
|---|---|
| `SAVE AS KST003` | Schreibt das Servo, wie es gerade ist, unter dem nächsten freien Namen, KST001 bis KST999. Die Leiste zeigt `SAVED AS KST002` in Grün. Es gibt keine Tastatur, daher keinen anderen Namen |
| `LOAD` | Hält die gewählte Datei gegen das Servo (Typ, Zugriffsklassen, harte Grenzen) und übernimmt ihre Werte als vorgemerkte Änderungen in die Feldliste. Geschrieben wird dann über `HOLD TO WRITE` |
| `HOLD TO RESTORE` | 1 s halten. Schreibt das beim Eintritt gelesene Abbild über die Wiederherstellungsseite ins Servo zurück |

Eine Ablehnung nennt ihre Zeile, zum Beispiel
`LINE 14: NOT A VALUE OF DEAD BAND`. Weitere Hinweise in der Leiste:

- `THE FILE IS FOR A V8 SERVO`: Der Typ der Datei widerspricht dem des Servos.
- `LINE 7: PULSE UPPER BREAKS A LIMIT`: Ein Wert in der Datei verletzt eine
  harte Grenze.
- `THE FILE DID NOT READ`, `THE CARD IS FULL`, `THE CARD TOOK NO FILE`.

`HOLD TO RESTORE` wirkt, wenn das Servo von der Sicherung abweicht oder im
Zweifel ist. Die Wiederherstellungsseite heißt `KST RESTORE`. Sie zeigt zum
Beispiel `BACK TO THE SESSION'S BACKUP` und
`4 REGISTERS DIFFER FROM THE BACKUP`. Sie zählt Register, gesperrte und
unbenannte eingeschlossen, denn die Wiederherstellung schreibt auch sie.
Eine Wiederherstellung, die der Planer nicht geprüft hat, braucht ein
zweites Halten. Die Seite zeigt dann
`THE PLANNER DID NOT CHECK THIS` und `RESTORE. HOLD AGAIN TO RUN IT.`

### Keine Karte, eine von USB gehaltene Karte, eine Karte ohne Datei

- **Keine Karte.** Die Leiste zeigt `NO CARD`. Die Seite zeigt
  `READ, EDIT AND WRITE STILL WORK.` und bietet `RETRY MOUNT` an. Die
  Sicherung bleibt im Speicher.
- **Eine von USB gehaltene Karte.** Die Leiste zeigt `CARD IN USE BY USB`.
  `RETRY MOUNT` ist abgeblendet gezeichnet. Die Sicherung bleibt im Speicher,
  und Lesen, Ändern und Schreiben funktionieren.
- **Eine Karte, die keine Datei annimmt.** Nimmt eine eingebundene Karte weder
  die Sicherung noch den Vermerk an, stoppt das Schreiben vor dem ersten
  Frame. Die Schreibseite zeigt
  `THE CARD TOOK NO FILE. NOTHING WAS WRITTEN TO THE SERVO.`

---

## Unbekanntes Layout: READ ONLY

Der Bildschirm prüft die 32 Register gegen den Layout-Fingerabdruck von
rcbench. Ein Abbild, das die Prüfung nicht besteht, wird gezeigt und nicht
geschrieben. Ohne gewählte Zeile zeigt die Editorleiste
`READ ONLY: UNKNOWN LAYOUT` in Bernstein und
`NOTHING IS WRITTEN TO THIS SERVO`, und die Schreib-Schaltfläche zeigt
`READ ONLY` und tut nichts.

---

## Verlassen

`BACK` in der Feldliste öffnet die Verlassenseite. `BACK` in der Suche öffnet
sie ebenfalls, sobald ein Servo geantwortet hat.

<img src="img/kst-leave.png" width="240" alt="Die Verlassenseite: POWER THE SERVO OFF."> <img src="img/kst-leave-pending.png" width="240" alt="Die Verlassenseite mit 5 CHANGES NOT WRITTEN.">

Die Seite zeigt `POWER THE SERVO OFF.` und
`IT STAYS IN PROGRAMMING MODE UNTIL ITS SUPPLY IS OFF.` Mit vorgemerkten
Änderungen kommt `2 CHANGES NOT WRITTEN.` hinzu, im Zweifel
`THE SERVO IS IN DOUBT: RESTORE FIRST.` `BACK` führt zur Feldliste zurück,
nach einem verlorenen Servo zur Suche.

**Schalte das Servo aus. Tippe dann auf `LEAVE`.** Der Bildschirm hält die
Leitung 100 ms lang auf Low, lässt den Pin los und zeigt `UNPLUG THE SERVO.`
über `DSHOT RESTARTS ON GP29 AND READS AS` und `A 970 US PULSE AT 1000 HZ.`
`LEAVE` verlässt auch eine Sitzung, die im Zweifel ist.

**Zieh das Servo ab und tippe dann auf `RESTART DSHOT`.** Der ESC-Treiber
übernimmt den Pin wieder, und `MENU` erscheint. `BACK` auf dieser Seite führt
stattdessen zur Suche zurück und behält den Pin.

`BACK` in der Suche, bevor ein Servo geantwortet hat, gibt den Pin frei und
zeigt die Abziehseite unmittelbar.

---

## Wenn es nicht funktioniert

- **`AN ESC ANSWERS ON GP29.`** Ein ESC hängt am ESC-Pin. Zieh ihn ab.
- **`NO SERVO IN 30 S`.** Prüfe Signal und Masse. Schalte das Servo erneut
  ein, während der Bildschirm sucht: Es antwortet nur direkt nach dem
  Einschalten.
- **`REPLY NOT VALID: ...`.** Flanken kommen an, aber keine Antwort
  ist gültig. Prüfe Signalkabel, Masse und Versorgung. Der Bildschirm nennt
  nur das Wort des Fehlers, nicht seine Ursache.
- **`READ FAILED: NO 3 EQUAL READS`.** Tippe auf `READ AGAIN`.
- **Der Bildschirm kehrt zur Suche zurück.** Das Servo antwortet nicht mehr.
  Schalte es erneut ein.
- **`IN DOUBT`.** Stelle über FILES mit `HOLD TO RESTORE` wieder her.
- **`THE CARD TOOK NO FILE. NOTHING WAS WRITTEN TO THE SERVO.`** Prüfe die
  Karte, oder nimm sie aus der USB-Nutzung, und schreibe dann erneut.

---

## Einschränkungen

- Keine geschaltete Versorgung: Das Servo wird von Hand ein- und ausgeschaltet.
  Schritt 5 der Entwurfsnotiz (`docs/design/kst-programming.md`) fügt einen
  Wandler an GP0 hinzu.
- Die Kartenaufrufe des ersten Schreibens laufen auf core0, und der
  Bildschirm wird erst neu gezeichnet, wenn sie enden. Mit 997 KSTnnn-Dateien
  auf der Karte sind das unter 7,3 s für eine Karte mit Zugriffs- und
  Busy-Zeiten von wenigen ms und bis zu 82,2 s für eine SDXC-Karte (Secure
  Digital eXtended Capacity) mit den vollen Zeiten der Spezifikation. Beide
  Werte sind aus FatFs-Blockzahlen berechnet, nicht auf einer Karte gemessen.

Vom KST-Programmierer ist nichts auf Hardware getestet.
