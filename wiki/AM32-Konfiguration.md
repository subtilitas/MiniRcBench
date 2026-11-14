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

Kommen Bytes an, die sich nicht deuten lassen, zeigt der Bildschirm
`REPLY NOT UNDERSTOOD:`, die Byteanzahl, die ersten drei Bytes und die
Baudrate sowie `IS THE ESC RUNNING AM32?`. Das Kabel funktioniert; die
Antwort ist nicht die des AM32-Bootloaders. Der ESC läuft vielleicht mit
anderer Firmware, oder sein Bootloader nutzt ein Protokoll, das dieser
Transport nicht umsetzt.

Antwortet der Bootloader und besteht sein Einstellungsblock die
Plausibilitätsprüfung nicht (erstes Byte 0x00 oder 0xFF, Layout-Version 0
oder über 8, oder 0 Motorpole), zeigt der Bildschirm `NO VALID SETTINGS` mit
`NOT AN AM32 ESC, OR BLANK SETTINGS.` und `NOTHING WILL BE WRITTEN.` Der
Bildschirm bleibt auf dieser Seite, die keine Schreib-Schaltfläche hat, und
liest weiter.

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
dann 3D MODE, dann PROTOCOL. Was AM32 mit jedem Wert tut, stammt aus dem
AM32-Quellcode (Zweig `main` von am32-firmware/AM32, November 2026); am
Prüfstand gemessen ist es nicht.

| Feld | Werte | Hinweis auf dem Streifen |
|---|---|---|
| `3D MODE` | `OFF`, `ON`. Die AM32-Einstellung `bi_direction`: 3D-Modus, umkehrbar. Das ist nicht bidirektionales DShot, die Telemetrie, die der Tester nutzt | Solange an, gelesen oder geändert: `3D MODE: 50% THROTTLE IS FULL POWER,` / `ABOVE 50% THE OTHER WAY. PROP OFF.` |
| `TEMP LIMIT` | 70 bis 140 °C; `OFF` bei 141. AM32 nimmt jeden Wert außerhalb 70 bis 140 als keine Grenze | Ausgewählt und `OFF`: `PROTECTION OFF: THE ESC DOES NOT` / `CUT POWER ON THIS LIMIT.` |
| `CURRENT LIMIT` | 2 bis 200 A in 2-A-Schritten; `OFF` bei 0 und über 200 A. AM32 begrenzt den Strom nur bei gespeicherten Werten 1 bis 100 | wie TEMP LIMIT |
| `LOW VOLT CUTOFF` | `OFF`, `ON` | wie TEMP LIMIT, bei `OFF` |
| `PROTOCOL` | `AUTO`, `DSHOT`, `SERVO`, `SERIAL`, `EDT ARM`, `DRONECAN`: gespeichert als 0 bis 5 | Bei `SERVO` oder `DRONECAN`: `THE TESTER SENDS DSHOT ONLY:` / `THE ESC WILL NOT RUN HERE.` |

Im 3D-Modus teilt AM32 den DShot-Gasbereich: Werte 48 bis 1047 drehen den
Motor in die eine Richtung, 1048 bis 2047 in die andere, jeweils von wenig
bis voller Leistung. Der Tester sendet 47 plus sein Gas von 1 bis 2000, also
ist 50 % volle Leistung in die eine Richtung, und knapp über 50 % dreht der
Motor mit wenig Leistung in die andere. Mit der voreingestellten Gasgrenze
von 20 % erreicht der Tester etwa 40 % des AM32-Gasbereichs in eine
Richtung.

Mit `SERVO` sucht AM32 nur Servopulse, und ein DroneCAN-Build mit
`DRONECAN` schaltet den DShot-Eingang ab. Mit `SERIAL` erkennt AM32 den
Eingang wie mit `AUTO`. `EDT ARM` nimmt DShot und schärft, sobald der ESC den
EDT-Einschaltbefehl (Extended DShot Telemetry) empfängt, den der Tester
sendet; ob ein solcher ESC am Tester läuft, ist nicht getestet.

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
`NOT VERIFIED. TAP OK, WRITE AGAIN.` in Bernstein: `OK` kehrt mit den
Änderungen zur Liste zurück, und erneutes Halten von `HOLD TO WRITE` schreibt
noch einmal.

Vor dem ersten Schreiben einer Sitzung gehen die beim Eintritt gelesenen
Einstellungen als `BACKUP/AMnnn.TXT` auf die SD-Karte, in den Ring der
Sicherungen, den der KST-Programmierer teilt
([KST-Programmierung](KST-Programmierung)). Das Ergebnis zeigt dann zum
Beispiel `WRITE VERIFIED. BACKUP AM018`. Eine Karte, die die Sicherung
verweigert, hält das Schreiben auf: `NO BACKUP ON THE CARD: NOT WRITTEN`,
darunter `CHECK THE CARD, THEN WRITE AGAIN.`. Ohne
Karte läuft das Schreiben, und die Sicherung bleibt im Speicher, bis ein
Schreiben mit Karte sie sichert oder der Editor schließt. Die Datei hält eine
Zeile je Feld, die 48 Bytes in Hex und eine Prüfsummenzeile; noch liest sie
nichts zurück. Der ESC wird erst geschrieben, wenn die Sicherung auf der
Karte liegt. Eine Blackbox-Aufzeichnung, die beim Öffnen des Editors läuft,
endet.

<img src="img/am32-written-backup.png" width="240" alt="Schreiben überprüft, mit genannter Sicherung und der Zeile POWER-CYCLE THE ESC TO APPLY.">

Die meisten AM32-Einstellungen wirken ab dem nächsten Einschalten des ESC,
daher die Zeile unter dem Ergebnis. Das ist Verhalten von AM32; der Tester
prüft es nicht.

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
