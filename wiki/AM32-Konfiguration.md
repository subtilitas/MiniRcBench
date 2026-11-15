[English](AM32-Configuration) | **Deutsch**

# AM32-Konfiguration

Dieser Bildschirm liest, ändert und schreibt die Einstellungen eines AM32-ESC
(Electronic Speed Controller, Motorregler) über das Signalkabel, und er liest
und zeigt die eines Bluejay- oder BLHeli_S-ESC. Er braucht keinen Laptop und
keinen Konfigurator. Er aktualisiert keine ESC-Firmware.

<img src="img/am32-list.png" width="240" alt="AM32-Einstellungsliste">

> **Das schreibt in den Flash deines ESC.** Änderungen bleiben im Tester, bis
> du `WRITE...` antippst, die Seite mit der Liste der Änderungen prüfst und
> ihr `HOLD TO WRITE` 1 s lang hältst. Danach stehen sie im ESC.

---

## Verbinden

**MENU → ESC CONFIG** öffnen, dann **ESC SETTINGS** antippen.

<img src="img/esc-chooser.png" width="240" alt="ESC CONFIG: ESC SETTINGS und, grau, USB BRIDGE mit NOT BUILT">

`ESC CONFIG` ist eine Auswahl. Sie nimmt dem Signalpin nichts weg: DShot
läuft weiter, bis du `ESC SETTINGS` antippst. `USB BRIDGE` ist grau
gezeichnet, darunter `NOT BUILT` (nicht gebaut), und tut nichts. `BACK`
führt zu MENU zurück. Der Bildschirm heißt für jede ESC-Firmware
`ESC SETTINGS`; er liest und schreibt AM32 und liest Bluejay und
BLHeli_S, ohne sie zu schreiben: siehe [EFM8-ESCs](#efm8-escs).

Der Bildschirm übergibt den Signalpin vom DShot-Treiber an die
Bootloader-Verbindung (19200 Baud, ein Draht, halbduplex) und beginnt dann zu
suchen. Vorher sendet DShot den Gaswert null, bis 250 ms seit dem letzten
Gasbefehl vergangen sind. Ein laufender ESC sieht so einen Stopp, bevor die
Leitung still wird.

**Dann den ESC neu starten.** Zieh den ESC-Akku ab und steck ihn wieder an,
während dieser Bildschirm offen ist. Laut dem Quelltext des AM32-Bootloaders
entscheidet der Bootloader etwa 46 ms nach dem Einschalten und wartet dann,
solange die Leitung hoch liegt; fallen Bytes des Verbindungsaufbaus in diese
46 ms, startet stattdessen die ESC-Firmware, und die startet sich ohne DShot
nach 2 s selbst neu, sodass der Bootloader eine neue Gelegenheit bekommt; dann
startet nur ein Low in seinen letzten 5 ms die Firmware. Nicht gemessen.

Der Bildschirm sucht in einem Zyklus: Eine Sekunde lang sendet er nichts und
die Leitung liegt hoch, `LINE HIGH, NOTHING SENT` (Leitung hoch, nichts
gesendet); dann sendet er seinen Verbindungsaufbau 3 Mal und wartet je 30 ms
auf eine Antwort, `WAITING... (TRY n)` mit einem Versuchszähler; dann wieder
Ruhe. Die ruhige Sekunde lässt einen Bluejay- oder BLHeli_S-ESC, der lief,
von selbst in seinen Bootloader wechseln und hält einen AM32, der
währenddessen eingeschaltet wird, in seinem Bootloader. Nicht gemessen.

<img src="img/am32-quiet.png" width="240" alt="Die Verbindungsseite in ihrer ruhigen Sekunde: LINE HIGH, NOTHING SENT">

Der Bildschirm zeigt `POWER-CYCLE THE ESC` (den ESC aus- und wieder
einschalten) in Bernstein, den Schritt, der jetzt dran ist, den Stand der
Suche und einen Ladekringel. Bei Erfolg erscheint die Liste, und der
Streifen unter dem Titel zeigt den ESC-Namen, die MCU-Familie (Microcontroller)
und die Firmware mit ihrer Version, zum Beispiel
`NEOESC f051  F051  AM32 2.16`.

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
ist neuer als die Tabelle, also kann bei beiden ein Feld dort liegen, wo die
Tabelle ein anderes erwartet. Bei beiden öffnet der Bildschirm eine
**Nur-Lese-Ansicht**: die 48 Bytes hexadezimal, acht je Zeile, und kein Feld.
Die Leiste zeigt in Bernstein `LAYOUT 5 NOT KNOWN: READ-ONLY.` (mit dem
Layout, das der ESC speichert) und `NOT VERIFIED.`, gefolgt davon, wohin die
Sicherung ging: `BACKUP AM001`, `NO CARD: NO BACKUP.` ohne Karte oder mit
der Karte am USB, oder `BACKUP FAILED.`, wenn die Karte sie abweist. Unter den
Bytes: `THE BYTES AS READ. THE FIELD TABLE` /
`IS NOT CHECKED FOR THIS LAYOUT.`

<img src="img/am32-readonly.png" width="240" alt="Die Nur-Lese-Ansicht eines Blocks in Layout 5: LAYOUT 5 NOT KNOWN: READ-ONLY. und NOT VERIFIED. BACKUP AM001 in Bernstein, die 48 Bytes hexadezimal">

Die Sicherung geht beim Öffnen der Ansicht auf die Karte, da später nichts
geschrieben wird. Sie enthält die 48 Bytes, keine Feldzeile und die Zeile
`Layout not known: fields not read, not verified`. Die Ansicht hat ein
Bedienelement, `BACK`, das zum MENU führt: keine Zeile, kein `WRITE...`,
`REVERT` oder `HEX`, und der Bildschirm hat keinen Weg von dieser Ansicht zum
Schreiben. Der Titel bleibt `ESC SETTINGS`.
Das Schreiben selbst lehnt in einem solchen Layout gelesene Einstellungen
ebenfalls ab: `LAYOUT NOT KNOWN: NOT WRITTEN`. Um die Einstellungen eines
solchen ESC zu ändern, nimm den AM32-Konfigurator.

Welche Firmware ein ESC hat, der nicht antwortet, zeichnet die Firmware nicht
auf.

### EFM8-ESCs

Der Verbindungsaufbau ist auch für den Bootloader von Bluejay- und
BLHeli_S-ESCs ein gültiger Gruß, also kann ein solcher ESC antworten. Der
Bildschirm unterscheidet die Antwort von einer AM32-Antwort: `471` mit 0xE8
vier Bytes danach nennt den EFM8-Chip eines Bluejay- oder BLHeli_S-ESC
(`E8B1`, `E8B2` oder `E8B5`). Der Tester liest dann die Einstellungen dieses
ESC und zeigt sie; er ändert sie nicht. Laut dem Quelltext der Bootloader;
an keinem ESC gesehen.

Die Seite zeigt `EFM8 E8B2 ANSWERED: READING.` (geantwortet, wird gelesen)
und statt `POWER-CYCLE THE ESC` die Zeile `EFM8 BOOTLOADER`: Der ESC wartet
in seinem Bootloader auf Befehle, und seine Firmware läuft nicht, bis er ihn
verlässt. Der Tester hält die Signalleitung eine halbe Sekunde hoch, grüßt
den Bootloader erneut und liest seine Einstellungsseite, 112 Bytes bei
BLHeli_S und 255 bei Bluejay. Nichts wird gelöscht oder geschrieben. Mit
rcbenchs simuliertem ESC dauert das unter einer Sekunde; an keinem ESC
gemessen.

<img src="img/am32-efm8.png" width="240" alt="Die Verbindungsseite, während ein Bluejay-ESC gelesen wird: EFM8 BOOTLOADER, EFM8 E8B2 ANSWERED: READING.">

Dann öffnet sich die Liste. Der Streifen nennt die Firmware, ihre Version,
das Einstellungslayout und den Chip, zum Beispiel
`BLUEJAY 0.21  LAYOUT 208  E8B2`. Jede Zeile ist grau: angezeigt, nicht
änderbar. Namen und Werte sind die aus rcbenchs Tabellen, in Großbuchstaben;
die bidirektionalen Einstellungen von DIRECTION heißen `3D` und
`3D REVERSED`. Ein Wert, der keiner der Einstellung ist, steht zum Beispiel
als `RAW 17` in Bernstein. `WRITE...` und `REVERT` sind grau, und unter der
Liste steht `SHOWN ONLY: WRITING IS NOT BUILT.` (nur angezeigt, Schreiben
ist nicht gebaut) / `BACK SENDS LEAVE: THE ESC STARTS.` (BACK sendet LEAVE,
der ESC startet).

<img src="img/efm8-list.png" width="240" alt="Die Einstellungen eines Bluejay-0.21-ESC, jede Zeile grau, WRITE... und REVERT grau">

Tipp eine Zeile an, um sie zu wählen. Manche Zeilen haben einen Hinweis,
zwei Zeilen in Bernstein im Streifen:

| Zeile | Hinweis |
|---|---|
| DIRECTION auf `3D` oder `3D REVERSED` | `3D MODE: 50% THROTTLE IS FULL POWER,` / `ABOVE 50% THE OTHER WAY. PROP OFF.` |
| TEMPERATURE oder LOW-SPEED PROTECTION auf `OFF` | `PROTECTION OFF: THE ESC DOES NOT` / `CUT POWER ON THIS LIMIT.` |
| POWER RATING | `SETS HOW THE ESC READS ITS TEMP:` / `A WRONG VALUE MISREADS THE LIMIT.` |

<img src="img/efm8-note.png" width="240" alt="POWER RATING gewählt: SETS HOW THE ESC READS ITS TEMP: in Bernstein im Streifen">

DIRECTION auf `3D` oder `3D REVERSED` zeigt den 3D-Hinweis auch ohne
gewählte Zeile im Streifen und setzt im Tester
`3D MODE ESC: 0-50% REVERSED AT 2X`. `HEX` zeigt die Bytes wie gelesen,
8 pro Zeile; zieh, um durch die 255 Bytes einer Bluejay-Seite zu blättern.

<img src="img/efm8-hex.png" width="240" alt="ESC HEX: die Bytes der Bluejay-Seite, 8 pro Zeile">

Eine ältere Version und eine Seite, die die Firmware neu geschrieben hat,
erscheinen genauso, mit zwei Zeilen in Bernstein im Streifen:

| Seite | Streifen |
|---|---|
| Bluejay vor 0.21, BLHeli_S vor 16.3 | `BLUEJAY 0.19, LAYOUT 206: READ-ONLY.` / `SHOWN ONLY. NO BACKUP SAVED.` |
| Der ESC hat seine Tags verloren, die 48 Bytes, die seine Hardware nennen | `ESC LOST ITS TAGS: READ-ONLY.` / `REFLASH ON A COMPUTER. NO BACKUP.` |
| Die Einstellungsmarke ist falsch | `SETTINGS MARKER WRONG: READ-ONLY.` / `SHOWN ONLY. NO BACKUP SAVED.` |

<img src="img/efm8-readonly.png" width="240" alt="Bluejay 0.19: BLUEJAY 0.19, LAYOUT 206: READ-ONLY. in Bernstein">

Eine Seite, die der Tester nicht annimmt, bleibt auf der Verbindungsseite,
mit dem, was sie ist: `EFM8 E8B2: SETTINGS PAGE EMPTY` mit
`PAGE EMPTY, AS AFTER A CUT WRITE.` / `KEEP THE ESC POWERED: SEE MANUAL.`
(Seite leer wie nach einem abgebrochenen Schreiben; ESC eingeschaltet
lassen), oder `EFM8 E8B2: PAGE NOT KNOWN`, `BLUEJAY ON EFM8BB1: NOT TAKEN`
und ähnliche mit `NOT BLUEJAY OR BLHELI_S AS KNOWN:` /
`NOTHING IS WRITTEN.` Eine leere Seite hat ihre Tags schon verloren;
eingeschaltet bleibt der ESC in seinem Bootloader.

<img src="img/efm8-empty.png" width="240" alt="Eine leere Einstellungsseite: EFM8 E8B2: SETTINGS PAGE EMPTY und ihre zwei Zeilen">

**Die Liste verlassen.** `BACK` aus der Liste sendet dem Bootloader LEAVE,
das die ESC-Firmware startet: Die Seite zeigt `LEAVING` und in Bernstein
`KEEP THE PROPELLER OFF.` (Propeller ab lassen). Etwa eine Drittelsekunde
später kehrt DShot auf den Signalpin zurück und der Bildschirm zu MENU.
LEAVE hat keine Antwort: Ob der ESC gestartet ist, sieht der Tester nicht;
an keinem ESC geprüft.

<img src="img/efm8-leaving.png" width="240" alt="LEAVING: THE ESC STARTS ITS FIRMWARE ON LEAVE, THEN DSHOT RETURNS. KEEP THE PROPELLER OFF.">

Antwortet der ESC nicht mehr, bevor seine Seite gelesen ist, zeigt die
Seite `THE ESC ANSWERED, THEN STOPPED.` / `SEARCHING AGAIN.` (geantwortet,
dann verstummt; sucht erneut) und grüßt erneut.

Überall sonst nach einer solchen Antwort, auf der Verbindungsseite während
des Lesens, nach einer Seite, die nicht angenommen wird, oder nachdem die
Suche neu begonnen hat, kann der Bootloader noch auf Befehle warten, und
DShot-Frames könnten als einer durchgehen. Darum öffnet `BACK` eine
Verlassenseite, statt sofort zu MENU zurückzukehren:

1. `UNPLUG THE ESC BATTERY, THEN LEAVE.` (ESC-Akku abziehen, dann LEAVE) /
   `ITS BOOTLOADER WAITS FOR COMMANDS` / `AND MAY READ DSHOT AS ONE.`
2. `PLUG IT IN AFTER RESTART DSHOT.` (nach RESTART DSHOT wieder anstecken)

<img src="img/am32-leave.png" width="240" alt="Die Verlassenseite: Schritt 1 UNPLUG THE ESC BATTERY, THEN LEAVE., STAY und LEAVE">

`STAY` kehrt zum Bildschirm zurück; ein laufendes Lesen geht weiter. Zieh
den ESC-Akku ab, dann tippe `LEAVE`. `RESTART DSHOT` tritt an die Stelle von
LEAVE und wirkt 1 s später; es startet DShot auf dem Signalpin und kehrt zu
MENU zurück. Steck den Akku danach wieder an. `BACK` ist auf dieser Seite
grau. MENUs `POWER-CYCLE THE ESC TO RUN IT` verschwindet mit LEAVE aus der
Liste und mit RESTART DSHOT, da die Schritte den ESC neu gestartet haben.
Den Akku sieht der Tester nicht: RESTART DSHOT bei noch eingestecktem ESC,
oder ein Neustart des Testers, während der ESC wartet, schickt DShot in
seinen Bootloader.

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
speichert; der Streifen zeigt sie nach `AM32`. Ein Versionsbyte, das AM32
nicht geschrieben hat, zählt als die älteste.

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

**Auf dem Tester.** Hat ESC SETTINGS die Einstellungen des ESC mit 3D MODE an
gelesen, oder sie mit 3D MODE an geschrieben (vom ESC angenommen, ob das
Zurücklesen danach `WRITE VERIFIED` zeigt oder nicht), zeigt der Tester unter
der Drehzahl in Bernstein `3D MODE ESC: 0-50% REVERSED AT 2X` (3D-ESC:
0 bis 50 % rückwärts mit doppeltem Gas), an Stelle der Polzahl und des
Wisch-Hinweises, scharf oder nicht. Solange ein Wischer auf der Gasfläche
greift, steht `SWIPE ACTIVE` an seiner Stelle, und nach dem Loslassen kehrt
der Hinweis zurück.
Scharfschalten wird deswegen nicht verweigert. Nur ein bestätigtes Schreiben
oder ein Lesen, das die Liste öffnet, mit 3D MODE aus nimmt ihn weg, ebenso ein
Neustart: Er wird nicht gespeichert. Eine nicht geschriebene Änderung, ein vom
ESC abgelehntes Schreiben, ein nicht bestätigtes Schreiben mit 3D MODE aus, ein
gescheitertes Lesen und ein Lesen, das die Liste nicht öffnet, darunter die
Nur-Lese-Ansicht eines unbekannten Layouts, lassen ihn, wie er ist. Der
Tester kann nicht erkennen, dass der ESC am Pin gegen einen anderen getauscht
wurde: Der Hinweis beschreibt den letzten ESC, den ESC SETTINGS seit dem
Einschalten gelesen oder geschrieben hat. `3D ON` auf ESC COMMANDS setzt
denselben Hinweis, sein `3D OFF` lässt ihn stehen
([Die Bildschirme](Die-Bildschirme#esc-commands)). Teste einen 3D-ESC ohne
Propeller.

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

**Ein geänderter Wert ist cyan, mit einem cyanfarbenen Balken am linken Rand
seiner Zeile.** Die Bearbeitungsleiste zeigt ihn ebenfalls in Cyan, und
darüber den gelesenen Wert: zum Beispiel `PWM FREQ  24KHZ ->` über `25KHZ`.
Passen Name und alter Wert nicht zwischen `-` und `+`, zeigt die Leiste nur
`24KHZ ->`. An den ESC ist noch nichts gesendet. Ein Wert, wie er gelesen
wurde, steht in der Textfarbe, ausgewählt oder nicht: Die Auswahl hellt nur
die Zeile und ihren Namen auf.

`REVERT`, in Bernstein beschriftet, verwirft alle ungespeicherten Änderungen
und stellt die gelesenen Werte wieder her. `HEX` ist in der Textfarbe
beschriftet.

**BACK mit ungeschriebenen Änderungen fragt zuerst.** Das erste `BACK` lässt
die Liste stehen und setzt zwei bernsteinfarbene Zeilen auf den Streifen:
`2 CHANGES NOT WRITTEN.` und `BACK AGAIN DISCARDS THEM.`
(`1 CHANGE NOT WRITTEN.` und `BACK AGAIN DISCARDS IT.` bei einer). Ein
zweites `BACK` kehrt zum MENU zurück und verwirft die Änderungen; nichts geht
an den ESC. Jede andere Berührung nimmt die Zeilen weg und behält die
Änderungen. Es gibt keine Zeitgrenze. Ohne Änderungen kehrt `BACK` sofort
zum MENU zurück.

<img src="img/am32-discard.png" width="240" alt="Der Streifen nach BACK mit zwei Änderungen: 2 CHANGES NOT WRITTEN. und BACK AGAIN DISCARDS THEM. in Bernstein">

---

## Schreiben

Tippe `WRITE...` unter der Liste an. Es öffnet die Seite `ESC WRITE`;
geschrieben ist noch nichts. Ohne Änderungen ist `WRITE...` grau und tut
nichts.

<img src="img/am32-write.png" width="240" alt="ESC WRITE mit fünf Änderungen: vier Zeilen mit Name und alt -> neu, AND 1 MORE, BACKUP FIRST: BACKUP/AM001.TXT und HOLD TO WRITE">

Die Seite listet jede Einstellung, die sich ändert, mit Namen und
`alt -> neu`, den alten Wert grau und den neuen cyan: vier Zeilen, dann
`AND n MORE`. Der Streifen zählt sie, `5 CHANGES`. Die Zeile über der
Schaltfläche sagt, wohin die Sicherung geht: `BACKUP FIRST: BACKUP/AM001.TXT`
vor dem ersten Schreiben mit eingesteckter Karte,
`BACKUP: BACKUP/AM001.TXT`, sobald sie auf der Karte liegt. Ohne Karte zeigt
sie `NO CARD: OLD VALUES LOST ON LEAVING` in Bernstein, mit der Karte an USB
`USB DRIVE: OLD VALUES LOST ON LEAVING`.

`HOLD TO WRITE` 1 s lang halten. Währenddessen füllt sich ein
Fortschrittsbalken. Das Tippen, das die Seite geöffnet hat, zählt nicht zum
Halten. Während das Schreiben läuft, zeigt die Seite
`WRITING: DO NOT UNPLUG.` in Bernstein, und `BACK` ist grau und tut nichts.
`BACK` vor dem Halten oder unter dem Ergebnis kehrt zur Liste zurück.

<img src="img/am32-written.png" width="240" alt="Schreiben überprüft">

Solange die BOARD-Spannung auf dem Tester rot ist, unter 3,30 V und bis sie
wieder 3,35 V erreicht, schreibt das Halten nichts, weder die Sicherung noch
die Einstellungen. Das Ergebnis zeigt `BOARD VOLTAGE IS RED: NOT WRITTEN.`,
darunter `CHARGE THE CELL. TAP OK, WRITE AGAIN.` in Bernstein, und `OK` führt
mit den Änderungen zur Liste zurück; `WRITE...` und das Halten der Seite
schreiben erneut. Der Bildschirm fragt einmal, wenn das
Halten endet, vor der Sicherung. Bernstein und ein Wert unter 2,50 V (keine
Zelle zu beurteilen) lehnen kein Schreiben ab. Die Bereiche sind der übliche
Bereich einer Lithium-Polymer-Zelle, auf dieser Platine nicht gemessen. Was
die Platine an USB-Versorgung ohne Zelle liest, ist ebenfalls nicht
gemessen, also kann eine Platine nur an USB das Schreiben ablehnen.

<img src="img/am32-board-low.png" width="240" alt="BOARD VOLTAGE IS RED: NOT WRITTEN. mit CHARGE THE CELL. TAP OK, WRITE AGAIN. in Bernstein">

Nach dem Schreiben wartet die Firmware 300 ms, **liest die Einstellungen zurück
und vergleicht sie** Byte für Byte. Sie verlässt sich nicht auf die Bestätigung
des ESC. Bei Übereinstimmung erscheint `WRITE VERIFIED` in Grün, darunter
`POWER-CYCLE THE ESC TO APPLY.`. Bei einer Abweichung erscheint das erste
abweichende Byte als `DIFF @xx: GOT xx WANT xx`. Das Ergebnis steht in einer
Zeile in der kleinen Schrift unter der Liste der Änderungen. Ein Ergebnis,
das kein überprüftes Schreiben ist, steht in Bernstein, und darunter
(`WRITE FAILED: ...`, `VERIFY READ: ...`, `DIFF @..`) zeigt der Bildschirm
`NOT VERIFIED. TAP OK, WRITE AGAIN.` in Bernstein; siehe [Wenn das Schreiben
scheitert](#wenn-das-schreiben-scheitert).

<img src="img/am32-write-failed.png" width="240" alt="WRITE FAILED: REFUSED mit NOT VERIFIED. TAP OK, WRITE AGAIN.">

Vor dem ersten Schreiben einer Sitzung gehen die beim Eintritt gelesenen
Einstellungen als `BACKUP/AMnnn.TXT` auf die SD-Karte, in den Ring der
Sicherungen, den der KST- und der Hitec-Programmierer teilen
([KST-Programmierung](KST-Programmierung#files)). Das Ergebnis zeigt dann zum
Beispiel `WRITE VERIFIED. BACKUP AM018`. Eine Karte, die die Sicherung
verweigert, hält das Schreiben auf: `NO BACKUP ON THE CARD: NOT WRITTEN`,
darunter `CHECK THE CARD, THEN WRITE AGAIN.`. Ohne
Karte läuft das Schreiben, und die Sicherung bleibt im Speicher, bis ein
Schreiben mit Karte sie sichert oder der Editor schließt. Die Datei hält eine
Zeile je Feld, das die Firmware liest, die 48 Bytes in Hex und eine
Prüfsummenzeile; noch liest sie nichts zurück. Der ESC wird erst
geschrieben, wenn die Sicherung auf der Karte liegt. Eine Blackbox-Aufzeichnung, die beim Öffnen des Editors läuft,
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

`OK` tritt in der breiten Schaltfläche am unteren Rand an die Stelle von `HOLD
TO WRITE`. Sie nimmt nur ein neues Tippen an: Das Abheben des Fingers, der `HOLD
TO WRITE` gehalten hat, lässt das Ergebnis stehen.

Nur `WRITE VERIFIED` gilt als geschrieben. Nach jedem anderen Ergebnis führt
`OK` zur Liste zurück, die Änderungen bleiben erhalten, und `WRITE...` und `HOLD
TO WRITE` schreiben sie erneut. Die Sicherung entsteht vor dem ersten Schreiben
mit Karte, und erneut auf einer seitdem gewechselten Karte. Sie hält immer die
Einstellungen, wie sie beim Öffnen des Editors gelesen wurden, und ein erneutes
Halten auf derselben Karte legt keine zweite an.

Wenn sich ein Ergebnis wiederholt:

1. Prüfe Signal- und Masseader.
2. Verlasse den Bildschirm mit zweimal `BACK`: Das erste nennt die
   Änderungen, das zweite verwirft sie.
3. Öffne **MENU → ESC CONFIG → ESC SETTINGS** erneut und starte den ESC
   neu, während der Bildschirm offen ist, wie unter [Verbinden](#verbinden).
   Der Bildschirm liest dann, was im ESC steht.
4. Vergleiche es mit der Sicherung, der neuesten `BACKUP/AMnnn.TXT` auf der
   Karte. Sie hält die Einstellungen, wie sie beim Öffnen des Editors gelesen
   wurden, vor jedem Schreiben. Ihre Nummer steht in `BACKUP/LAST`, solange
   keine KST- oder Hitec-Sicherung danach kam. Ohne Karte lag die Sicherung
   nur im Speicher und ist weg.

---

## Die Hex-Ansicht

<img src="img/am32-hex.png" width="240" alt="Die rohen Einstellungsbytes">

`HEX` zeigt die 48 rohen Einstellungsbytes, nur lesbar, unter dem Titel
`ESC HEX`. Damit prüfst du ein Feld, das deine Layout-Version nicht
anbietet, oder siehst, was eine Änderung auf Byte-Ebene bewirkt hat.
`FIELDS` oder `BACK` kehrt zur Liste zurück.

---

## Verlassen

`BACK` auf der Liste gibt den Signalpin an den DShot-Treiber zurück und
kehrt zu `MENU` zurück; mit ungeschriebenen Änderungen fragt es zuerst, wie
unter [Ändern](#ändern). Der Tester ist entschärft.

Beim Verlassen schickt die Firmware einem AM32-Bootloader keinen Befehl, die
ESC-Firmware zu starten. Was der ESC nach dem Verlassen des Bildschirms
tut, ist nicht aufgezeichnet. **Starte den ESC nach dem Verlassen neu**,
bevor du einen Motor ansteuerst. Nach einer Antwort von Bluejay oder
BLHeli_S sendet `BACK` aus deren Liste LEAVE, und `BACK` von überall sonst
öffnet stattdessen die Verlassenseite ([EFM8-ESCs](#efm8-escs)).

Nach einer Sitzung, in der der Bootloader geantwortet hat, zeigt MENU
`POWER-CYCLE THE ESC TO RUN IT` in Bernstein unter der Zeile des Streifens.
Der Hinweis verschwindet, sobald ein eRPM-Frame eintrifft, den nur die
ESC-Firmware sendet, sobald LEAVE aus einer Bluejay- oder BLHeli_S-Liste
hinausging, mit RESTART DSHOT der Verlassenseite, oder wenn die Platine neu
startet.

<img src="img/menu-run-cue.png" width="240" alt="MENU mit POWER-CYCLE THE ESC TO RUN IT in Bernstein unter der Zeile des Streifens">

---

## Wenn keine Verbindung zustande kommt

- **Endlos `LINE HIGH, NOTHING SENT` und `WAITING...`.** Zieh den ESC-Akku
  ab und steck ihn wieder an, *während der Bildschirm offen ist*. Ob ein ESC, der schon
  vor dem Öffnen des Bildschirms lief, ohne Neustart erreicht wird, ist nicht
  gemessen.
- **`REPLY NOT UNDERSTOOD`.** Die Verbindung funktioniert, die Rahmenbildung
  nicht. Mögliche Ursachen: Der ESC läuft nicht mit AM32, oder sein Bootloader
  nutzt ein Protokoll, das dieser Transport nicht umsetzt.
- **`EFM8 E8B2 ANSWERED: READING.`, das stehen bleibt.** Ein Bluejay- oder
  BLHeli_S-ESC hat geantwortet, und seine Seite wird gelesen; das dauert
  etwa eine Sekunde. Siehe [EFM8-ESCs](#efm8-escs).
- **`THE ESC ANSWERED, THEN STOPPED.`** Der ESC hat auf den
  Verbindungsaufbau geantwortet, dann aber nicht auf den eigenen Gruß oder
  das Lesen des Testers. Die Suche läuft erneut. Prüfe das Signalkabel und
  lass den ESC eingeschaltet.
- **`NO VALID SETTINGS`.** Der ESC hat geantwortet, und der Einstellungsblock
  hat die Plausibilitätsprüfung nicht bestanden. Schreibe nichts.
- **`LAYOUT n NOT KNOWN: READ-ONLY.`** Der ESC speichert ein
  Einstellungslayout, das der Tester nicht kennt. Der Bildschirm zeigt die
  Bytes, sichert sie und schreibt nichts. Nimm zum Ändern dieses ESC den
  AM32-Konfigurator.
- **`READ FAILED (...)`.** Der Verbindungsaufbau hat geklappt, das Lesen der
  Einstellungen nicht. Prüfe Signal- und Masseader.

Mehr unter [Fehlersuche](Fehlersuche#am32-verbindet-nicht).
