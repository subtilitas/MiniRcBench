[English](AM32-Configuration) | **Deutsch**

# AM32-Konfiguration

Dieser Bildschirm liest, ändert und schreibt die Einstellungen eines AM32-ESC
(Electronic Speed Controller, Motorregler) über das Signalkabel. Er braucht
keinen Laptop und keinen Konfigurator. Er aktualisiert keine ESC-Firmware.

<img src="img/am32-list.png" width="240" alt="AM32-Einstellungsliste">

> **Das schreibt in den Flash deines ESC.** Änderungen bleiben im Tester, bis
> du `HOLD TO WRITE` 1 s lang hältst. Danach stehen sie im ESC.

---

## Verbinden

**MENU → AM32 CONFIG** öffnen.

Der Bildschirm übergibt den Signalpin vom DShot-Treiber an die
Bootloader-Verbindung (19200 Baud, ein Draht, halbduplex) und beginnt dann zu
suchen. Vorher sendet DShot den Gaswert null, bis 250 ms seit dem letzten
Gasbefehl vergangen sind. Ein laufender ESC sieht so einen Stopp, bevor die
Leitung still wird.

**Dann den ESC neu starten.** Der AM32-Bootloader lauscht nur in einem kurzen
Fenster nach dem Einschalten. Zieh den ESC-Akku ab und steck ihn wieder an,
während dieser Bildschirm offen ist. Die Firmware wiederholt ihren
Verbindungsaufbau und wartet je 30 ms auf eine Antwort, damit sie sendet, wenn
das Fenster aufgeht. Die Länge des Fensters ist nicht gemessen.

Der Bildschirm zeigt `SEARCHING...`, dann `WAITING... (TRY n)` mit einem
Versuchszähler, und einen Ladekringel. Bei Erfolg erscheint die Liste, und der
Streifen unter dem Titel zeigt den ESC-Namen, die MCU-Familie (Microcontroller)
und die Firmware-Version.

<img src="img/am32-connect.png" width="240" alt="Die Verbindungsseite: POWER-CYCLE THE ESC, WAITING... (TRY 3) und der Ladekringel">

Kommen Bytes an, die sich nicht deuten lassen, zeigt der Bildschirm die
Byteanzahl, die ersten drei Bytes und die Baudrate, darunter
`REPLY NOT UNDERSTOOD:` und `IS THE ESC RUNNING AM32?`. Das Kabel
funktioniert; die Antwort ist nicht die des AM32-Bootloaders. Der ESC läuft
vielleicht mit anderer Firmware, oder sein Bootloader nutzt ein Protokoll,
das dieser Transport nicht umsetzt.

<img src="img/am32-connect-garbled.png" width="240" alt="Eine Antwort, die sich nicht deuten lässt: die Bytes, REPLY NOT UNDERSTOOD: und IS THE ESC RUNNING AM32?">

Antwortet der Bootloader und besteht sein Einstellungsblock die
Plausibilitätsprüfung nicht (erstes Byte 0x00 oder 0xFF, Layout-Version 0
oder über 8, oder 0 Motorpole), zeigt der Bildschirm `NO VALID SETTINGS` mit
`NOT AN AM32 ESC, OR BLANK SETTINGS.` und `NOTHING WILL BE WRITTEN.` Der
Bildschirm bleibt auf dieser Seite, die keine Schreib-Schaltfläche hat, und
liest weiter.

Die Feldtabelle ist gegen die Layout-Versionen 2 bis 4 geprüft: Jede
Version von am32-firmware/AM32 schreibt eine davon (2.22 schreibt 4), und
die Firmware von AlkaMotors ab 1.78 schreibt 2. Layout 1, die Firmware von
AlkaMotors vor 1.78, liest nichts über Byte 0x29, und eine Version über 4
ist neuer als die Tabelle. Bei beiden zeigt der Bildschirm
`LAYOUT n NOT KNOWN` mit `THE TESTER KNOWS LAYOUTS 2 TO 4.` und
`NOTHING WILL BE WRITTEN.` und bleibt auf der Verbindungsseite.

<img src="img/am32-layout.png" width="240" alt="LAYOUT 5 NOT KNOWN, mit THE TESTER KNOWS LAYOUTS 2 TO 4. und NOTHING WILL BE WRITTEN.">

Welche Firmware ein ESC hat, der nicht antwortet, zeichnet die Firmware nicht
auf.

---

## Blättern

Hoch oder runter ziehen scrollt. Die Liste folgt dem Finger 1:1, sobald die
Bewegung erkannt ist. Ziehen wählt nie eine Zeile aus.

<img src="img/am32-scrolled.png" width="240" alt="Die Liste mitten im Scrollen">

Ein **Tippen ohne Bewegung** wählt die Zeile darunter aus. Die Editorleiste am
unteren Rand zeigt dann Namen und Wert des Feldes.

Die Zeilen sind 40 px hoch, es sind also drei auf einmal sichtbar. Sie sind
gruppiert, mit dem Gruppennamen in Grau über jeder Gruppe.
Welche Felder es gibt, hängt von der Layout-Version der ESC-Einstellungen ab.
Angezeigt werden nur die Felder, die zur gelesenen Version passen.

### Felder mit Hinweis

Manche Werte ändern, was der Tester mit dem ESC tun kann, oder schalten einen
Schutz ab. Für diese zeigt der Streifen unter dem Titel zwei bernsteinfarbene
Zeilen statt des ESC-Namens. Der Hinweis der ausgewählten Zeile kommt zuerst,
dann 3D MODE, dann PROTOCOL, dann Firmware unter 1.80. Was AM32 mit jedem Wert
tut, stammt aus dem AM32-Quellcode: den Versionen v2.05, v2.16 und v2.18 bis
v2.22 und dem Zweig `main` bei 077b6a4 (November 2026), mit dem
AM32-Konfigurator bei 06955fe und den Tags v1.77 bis v2.01 der Firmware von
AlkaMotors und ihrer Geschichte für Firmware unter 2.00. Am Prüfstand gemessen
ist es nicht. Die Firmware-Version ist die, die der ESC in seinen Einstellungen
speichert; der Streifen zeigt sie als `FW`. Ein Versionsbyte, das AM32 nicht
geschrieben hat, zählt als die älteste.

<img src="img/am32-3d.png" width="240" alt="3D MODE an: zwei bernsteinfarbene Zeilen auf dem Streifen statt des ESC-Namens"> <img src="img/am32-protection-off.png" width="240" alt="TEMP LIMIT ausgewählt bei OFF: PROTECTION OFF: THE ESC DOES NOT CUT POWER ON THIS LIMIT.">

| Feld | Werte | Hinweis auf dem Streifen |
|---|---|---|
| `3D MODE` | `OFF`, `ON`. Die AM32-Einstellung `bi_direction`: 3D-Modus, umkehrbar. Das ist nicht bidirektionales DShot, die Telemetrie, die der Tester nutzt | Solange an, gelesen oder geändert: `3D MODE: 50% THROTTLE IS FULL POWER,` / `ABOVE 50% THE OTHER WAY. PROP OFF.` |
| `TEMP LIMIT` | 70 bis 140 °C; `OFF` bei 141. AM32 nimmt jeden Wert außerhalb 70 bis 140 als keine Grenze | Ausgewählt und `OFF`: `PROTECTION OFF: THE ESC DOES NOT` / `CUT POWER ON THIS LIMIT.` |
| `CURRENT LIMIT` | 2 bis 200 A in 2-A-Schritten; `OFF` bei 0 und darüber. AM32 ab 2.21 begrenzt den Strom bei gespeicherten Werten 1 bis 100, ältere AM32 bei 1 bis 99; dort zeigt 200 A `OFF` | wie TEMP LIMIT |
| `LOW VOLT CUTOFF` | `OFF`, `CELL` (je Zelle, bei CELL CUTOFF), `ABSOLUTE` (bei ABS VOLT CUT, ab 2.21) | wie TEMP LIMIT, bei `OFF` und bei `? (2)` |
| `PROTOCOL` | `AUTO`, `DSHOT`, `SERVO`, `SERIAL`, `EDT ARM`, gespeichert als 0 bis 4; eine gespeicherte 5 zeigt `DRONECAN` | Bei jedem Wert außer `AUTO`, `DSHOT` und 10 oder mehr: `THE TESTER SENDS DSHOT ONLY:` / `THE ESC WILL NOT RUN HERE.` |

Im 3D-Modus teilt AM32 den DShot-Gasbereich: Werte 48 bis 1047 drehen den
Motor in die eine Richtung, 1048 bis 2047 in die andere, jeweils von wenig
bis voller Leistung. Der Tester sendet 47 plus sein Gas von 1 bis 2000, also
ist 50 % volle Leistung in die eine Richtung, und knapp über 50 % dreht der
Motor mit wenig Leistung in die andere. Mit der voreingestellten Gasgrenze
von 20 % erreicht der Tester etwa 40 % des AM32-Gasbereichs in eine
Richtung.

Mit `SERVO` sucht AM32 nur Servopulse. Mit `SERIAL`, `DRONECAN` und den
gespeicherten Werten 6 bis 9 erkennt AM32 DShot, aber sein Startcode lässt das
Gas gesperrt (`EDT_ARMED` bei 0), sodass es jeden Gasbefehl verwirft; ein Wert
ab 10 wirkt wie `AUTO`. Der Editor bietet 0 bis 4 an, wie der Konfigurator:
`DRONECAN` ist für DroneCAN-Builds, und eine gespeicherte 5 behält ihren Namen.
`EDT ARM` nimmt DShot erst nach dem EDT-Einschaltbefehl (Extended DShot
Telemetry), und AM32 sperrt das Gas bei jedem Frame mit Gas null wieder. Der
Tester sendet den Einschaltbefehl nur entschärft und danach Frames mit Gas
null, vor jedem Gas; ein ESC auf `EDT ARM` verwirft hier also jeden Gasbefehl.

### Werte, die von der Firmware abhängen

| Feld | Werte | Auf älterer Firmware |
|---|---|---|
| `LOW VOLT CUTOFF` | `OFF`, `CELL`, `ABSOLUTE` | `ABSOLUTE` ab 2.21. Der AM32-Konfigurator bietet es ab 2.19 an, aber 2.19 und 2.20 vergleichen die Akkuspannung in 10 mV mit dem gespeicherten Byte, sodass die Abschaltung nie auslöst. Eine gespeicherte 2 unter 2.21 zeigt `? (2)` mit dem Hinweis PROTECTION OFF |
| `PWM TYPE` | `FIXED`, `VARIABLE`, `BY RPM` | `BY RPM` ab 2.18. 2.16 und älter variieren die PWM nur bei `VARIABLE` |
| `BRAKE ON STOP` | `OFF`, `BRAKE`, `ACTIVE` | `ACTIVE` ab 2.19. 2.18 bremst bei jedem Wert außer 0; 2.16 und älter nur bei `BRAKE` |
| `PWM FREQ` | 8 bis 144 kHz | 8 bis 48 kHz vor 2.19 |
| `ADVANCE` | 0,0 bis 30,0 Grad in 0,9375-Grad-Schritten; ein gespeichertes 0 bis 3 (die alte Kodierung, 7,5-Grad-Schritte) wird auch gelesen, und ein Druck schreibt die neue Kodierung | Vor 2.19 und auf unbekannter Firmware 0,0 bis 22,5 Grad in 7,5-Grad-Schritten, gespeichert als 0 bis 3: Diese Firmware nimmt jedes andere Byte als 15 Grad, ein gespeichertes 10 bis 42 zeigt also `? (n)` |
| `ABS VOLT CUT` | 0,5 bis 50,0 V in 0,5-V-Schritten | Vor 2.21 das gespeicherte Byte ohne Einheit: 2.19 und 2.20 lesen es nicht als Volt |
| `ACTIVE BRAKE` | 0,0 bis 2,5 % der PWM-Periode in 0,5-%-Schritten | Der Konfigurator zeigt die gespeicherten 0 bis 5 als Prozent; AM32 nutzt 0,5 % je Schritt |

Auf älterer Firmware hält der Editor beim letzten Wert an, den diese Firmware
annimmt. Ein gespeicherter Wert außerhalb des Bereichs, den der Tester auf
dieser Firmware anbietet, zeigt `? (n)`, auch eine Zahl: Auf 2.18 zeigt ein
PWM-FREQ-Byte von 100 `? (100)`, denn 2.18 übergeht es. Ein An/Aus-Feld,
das 2 oder mehr hält, zeigt ebenfalls `? (n)`: Der Konfigurator schreibt nur
0 und 1, und AM32 vor 2.18 wirkt bei mehreren dieser Bytes nur bei 1. Der
Tester schreibt jedes Byte, das du nicht geändert hast, so zurück, wie er es
gelesen hat: Ein `? (n)` bleibt, bis du in dieser Zeile `-` oder `+`
drückst; das holt den Wert in den Bereich. Die Namen sind die des
Konfigurators, gekürzt: `CELL` ist „Cell based“, `BRAKE` ist „Brake on
stop“, `ACTIVE` ist „Active brake“.

<img src="img/am32-fw222.png" width="240" alt="Firmware 2.22: ABS VOLT CUT zeigt 10.0V">

### Firmware, die nicht jedes Byte liest

Ältere Firmware liest weniger Bytes. Ein Feld, dessen Byte die Firmware des
ESC nicht liest, steht nicht in der Liste, und sein Byte wird so
zurückgeschrieben, wie es gelesen wurde:

| Bytes | Felder | Angezeigt ab |
|---|---|---|
| 0x2A bis 0x2D | RUNNING LEVEL, TEMP LIMIT, CURRENT LIMIT, SINE POWER | 1.80 |
| 0x2E | PROTOCOL | 1.94 |
| 0x2F | AUTO ADVANCE | 2.16 |

In der Geschichte der Firmware von AlkaMotors fügt Commit 9909ea7 das Lesen
von 0x2A bis 0x2D hinzu und hebt die Version von 1.79 auf 1.80, und Commit
aa590e8 fügt das Lesen von 0x2E hinzu und hebt sie von 1.93 auf 1.94. Eine
Version kann eine niedrigere Nummer speichern als ihr Code (der Tag v1.99
speichert 1.95), nie eine höhere; eine gespeicherte Version ab der Grenze
liest das Byte also. Die Versionen v2.05 bis v2.12 von am32-firmware/AM32
lesen 0x2F nicht, v2.16 liest es; die Versionen ohne Tag dazwischen gelten
als nicht lesend. Bei 1.80 wirkt CURRENT LIMIT bei 2 bis 99 (4 bis 198 A),
ab 1.81 (Commit f917c01) bei 1 bis 99.

Unter 1.80 und auf einem ESC, dessen Versionsbyte AM32 nicht geschrieben hat,
zeigt der Streifen `FIRMWARE BELOW 1.80 OR UNKNOWN:` /
`FIELDS IT MAY NOT READ ARE HIDDEN.`

<img src="img/am32-oldfw.png" width="240" alt="Firmware 1.35: FIRMWARE BELOW 1.80 OR UNKNOWN: FIELDS IT MAY NOT READ ARE HIDDEN.">

---

## Ändern

<img src="img/am32-edit.png" width="240" alt="Ein Feld wird geändert">

| Geste | Wirkung |
|---|---|
| Zeile antippen | Auswählen |
| `-` / `+` in der Editorleiste | **Fein**: ein Schritt je Betätigung. Halten wiederholt, zunehmend schneller |
| Zeile **seitwärts** wischen | **Grob**: ein Grobschritt je 16 px Weg; 176 px decken den Bereich jedes Feldes ab |
| **Hoch / runter** ziehen | Scrollen |

Eine Geste ist entweder Scrollen oder Ändern. Die Achse steht nach 6 px
Bewegung fest.

Ein Grobschritt ist ein ganzzahliges Vielfaches der Schrittweite des Feldes.
Ein Wert, der auf der Schrittweite liegt, bleibt darauf; eine gerade Polzahl
bleibt zum Beispiel gerade. Ein Wert, den der ESC außerhalb der Schrittweite
gespeichert hat, ändert sich von dort aus um die Schrittweite. Ein
Grobschritt, der über das Ende des Bereichs hinausginge, hält am Ende an. Weg
über das Ende hinaus zählt nicht, sodass schon der erste Grobschritt zurück
den Wert vom Ende löst.

**Geänderte Zeilen sind am linken Rand bernsteinfarben markiert**, und ihre
Werte sind bernsteinfarben. An den ESC ist noch nichts gesendet.

`REVERT` verwirft alle ungespeicherten Änderungen und stellt die gelesenen
Werte wieder her.

---

## Schreiben

<img src="img/am32-written.png" width="240" alt="Schreiben überprüft">

`HOLD TO WRITE` 1 s lang halten. Währenddessen füllt sich ein
Fortschrittsbalken. Ohne Änderungen zeigt die Schaltfläche `NO CHANGES`.

Nach dem Schreiben wartet die Firmware 300 ms, **liest die Einstellungen
zurück und vergleicht sie** Byte für Byte. Sie verlässt sich nicht auf die
Bestätigung des ESC. Bei Übereinstimmung erscheint `WRITE VERIFIED`, darunter
`POWER-CYCLE THE ESC TO APPLY.`. Bei einer Abweichung erscheint das erste
abweichende Byte als `DIFF @xx: GOT xx WANT xx`. Ein Ergebnis mit mehr als
18 Zeichen steht in der kleinen Schrift, damit es auf dem Bildschirm
bleibt. Unter jedem Ergebnis, das kein überprüftes Schreiben ist
(`WRITE FAILED: ...`, `VERIFY READ: ...`, `DIFF @..`), zeigt der Bildschirm
`NOT VERIFIED. TAP OK, WRITE AGAIN.` in Bernstein; siehe
[Wenn das Schreiben scheitert](#wenn-das-schreiben-scheitert).

<img src="img/am32-write-failed.png" width="240" alt="WRITE FAILED: REFUSED mit NOT VERIFIED. TAP OK, WRITE AGAIN.">

Vor dem ersten Schreiben einer Sitzung gehen die beim Eintritt gelesenen
Einstellungen als `BACKUP/AMnnn.TXT` auf die SD-Karte, in den Ring der
Sicherungen, den der KST-Programmierer teilt
([KST-Programmierung](KST-Programmierung)). Das Ergebnis zeigt dann zum
Beispiel `WRITE VERIFIED. BACKUP AM018`. Eine Karte, die die Sicherung
verweigert, hält das Schreiben auf: `NO BACKUP ON THE CARD: NOT WRITTEN`,
darunter `CHECK THE CARD, THEN WRITE AGAIN.`. Ohne
Karte läuft das Schreiben, und die Sicherung bleibt im Speicher, bis ein
Schreiben mit Karte sie sichert oder der Editor schließt. Die Datei hält eine
Zeile je Feld, das die Firmware liest, die 48 Bytes in Hex und eine Prüfsummenzeile; noch liest sie
nichts zurück. Der ESC wird erst geschrieben, wenn die Sicherung auf der
Karte liegt. Eine Blackbox-Aufzeichnung, die beim Öffnen des Editors läuft,
endet.

<img src="img/am32-written-backup.png" width="240" alt="Schreiben überprüft, mit genannter Sicherung und der Zeile POWER-CYCLE THE ESC TO APPLY.">

Die meisten AM32-Einstellungen wirken ab dem nächsten Einschalten des ESC,
daher die Zeile unter dem Ergebnis. Das ist Verhalten von AM32; der Tester
prüft es nicht.

### Wenn das Schreiben scheitert

Das Schreiben schickt die Einstellungen in drei Schritten: die Adresse, die
48 Bytes und den Befehl, sie zu programmieren. Jeder Schritt wartet bis zu
250 ms auf die Bestätigung des ESC. Danach läuft das Zurücklesen, bis zu
dreimal, jedes nach 300 ms Wartezeit. Der Bildschirm zeigt eines dieser
Ergebnisse:

| Ergebnis | Was der Code gesehen hat |
|---|---|
| `NO BACKUP ON THE CARD: NOT WRITTEN` | Die Karte hat die Sicherung verweigert, oder das Zurückschreiben auf die Karte scheiterte. Nichts ging an den ESC |
| `WRITE FAILED: NO REPLY` | Ein Schritt bekam binnen 250 ms keine Bestätigung |
| `WRITE FAILED: REFUSED` | Ein Schritt bekam ein Antwortbyte, das keine Bestätigung ist |
| `WRITE FAILED: CRC FAIL` | Beim Schreiben nicht erreicht: Seine Antworten tragen keine Prüfsumme (CRC, Cyclic Redundancy Check) |
| `WRITE FAILED: NOT READY` | Die Bootloader-Verbindung hat keinen Pin. Nicht zu erwarten: Die Liste öffnet erst nach einem Lesen über den Pin |
| `VERIFY READ: NO REPLY`, `REFUSED`, `CRC FAIL` oder `NOT READY` | Der ESC hat das Schreiben bestätigt, und alle drei Zurücklesungen scheiterten: keine Antwort, eine Antwort, die keine Bestätigung ist, eine Prüfsumme, die nicht passt, oder kein Pin. Was im ESC steht, ist nicht bekannt |
| `DIFF @xx: GOT yy WANT zz` | Das Zurückgelesene weicht ab. `xx` ist die Stelle des ersten abweichenden Bytes, `yy` der Wert im ESC, `zz` der geschriebene Wert, alle in Hex |

Nur `WRITE VERIFIED` gilt als geschrieben. Nach jedem anderen Ergebnis führt
`OK` zur Liste zurück, die Änderungen bleiben erhalten, und `HOLD TO WRITE`
schreibt sie erneut. Die Sicherung entsteht vor dem ersten Schreiben mit
Karte, und erneut auf einer seitdem gewechselten Karte. Sie hält immer die
Einstellungen, wie sie beim Öffnen des Editors gelesen wurden, und ein
erneutes Halten auf derselben Karte legt keine zweite an.

Wenn sich ein Ergebnis wiederholt:

1. Prüfe Signal- und Masseader.
2. Verlasse den Bildschirm mit `BACK`; die Änderungen gehen verloren.
3. Öffne **MENU → AM32 CONFIG** erneut und starte den ESC neu, während der
   Bildschirm offen ist, wie unter [Verbinden](#verbinden). Der Bildschirm
   liest dann, was im ESC steht.
4. Vergleiche es mit der Sicherung, der neuesten `BACKUP/AMnnn.TXT` auf der
   Karte. Sie hält die Einstellungen, wie sie beim Öffnen des Editors gelesen
   wurden, vor jedem Schreiben. Ihre Nummer steht in `BACKUP/LAST`, solange
   keine KST- oder Hitec-Sicherung danach kam. Ohne Karte lag die Sicherung
   nur im Speicher und ist weg.

---

## Die Hex-Ansicht

<img src="img/am32-hex.png" width="240" alt="Die rohen Einstellungsbytes">

`HEX` zeigt die 48 rohen Einstellungsbytes, nur lesbar. Damit prüfst du ein
Feld, das deine Layout-Version nicht anbietet, oder siehst, was eine Änderung
auf Byte-Ebene bewirkt hat. `FIELDS` kehrt zur Liste zurück.

---

## Verlassen

`BACK` gibt den Signalpin an den DShot-Treiber zurück und kehrt zu
`MENU` zurück. Der Tester ist entschärft.

Beim Verlassen schickt die Firmware dem Bootloader keinen Befehl, die
ESC-Firmware zu starten. Was der ESC nach dem Verlassen des Bildschirms
tut, ist nicht aufgezeichnet. **Starte den ESC nach dem Verlassen neu**,
bevor du einen Motor ansteuerst.

Nach einer Sitzung, in der der Bootloader geantwortet hat, zeigt MENU
`POWER-CYCLE THE ESC TO RUN IT` in Bernstein unter der Zeile des Streifens.
Der Hinweis verschwindet, sobald ein eRPM-Frame eintrifft, den nur die
ESC-Firmware sendet, oder wenn die Platine neu startet.

<img src="img/menu-run-cue.png" width="240" alt="MENU mit POWER-CYCLE THE ESC TO RUN IT in Bernstein unter der Zeile des Streifens">

---

## Wenn keine Verbindung zustande kommt

- **Endlos `SEARCHING...` oder `WAITING...`.** Zieh den ESC-Akku ab und steck
  ihn wieder an, *während der Bildschirm offen ist*. Ein Neustart vor dem
  Öffnen des Bildschirms funktioniert nicht.
- **`REPLY NOT UNDERSTOOD`.** Die Verbindung funktioniert, die Rahmenbildung
  nicht. Mögliche Ursachen: Der ESC läuft nicht mit AM32, oder sein Bootloader
  nutzt ein Protokoll, das dieser Transport nicht umsetzt.
- **`NO VALID SETTINGS`.** Der ESC hat geantwortet, und der Einstellungsblock
  hat die Plausibilitätsprüfung nicht bestanden. Schreibe nichts.

Mehr unter [Fehlersuche](Fehlersuche#am32-verbindet-nicht).
