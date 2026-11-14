[English](Troubleshooting) | **Deutsch**

# Fehlersuche

Erst das Symptom, dann die Ursache. ESC steht für Electronic Speed Controller
(Motorregler).

---

## Der Bildschirm bleibt dunkel

MiniRcBench läuft nur auf der Waveshare RP2350-Touch-LCD-2.8. Prüfe die
Platine: Die 2,8" hat JST-SH-Stecker und keine 2,54-mm-Stiftleisten. Für die
RP2350-Touch-LCD-2 gibt es
[DshotDisplay](https://github.com/subtilitas/DshotDisplay).

Neu flashen: **BOOT** halten, **RESET** antippen, **BOOT** loslassen. Die
Platine erscheint als USB-Laufwerk (USB: Universal Serial Bus), gleich welche
Firmware sie trägt. Kopiere die `.uf2`-Datei darauf:
[Einrichtung](Einrichtung#firmware-aufspielen).

Auf einer RP2350-Touch-LCD-2.8 hat ein dunkler Bildschirm keine bekannte
Ursache in der Firmware. Melde ihn wie unter
[Immer noch nicht weiter](#immer-noch-nicht-weiter) beschrieben.

---

## Keine Telemetrie

<img src="img/tester-stale.png" width="240" alt="Keine Telemetrie">

Auf dem Signalkabel antwortet nichts, und das Feld unter dem Titel des
Bildschirms `MENU` zeigt `NO ESC`. Prüfe in dieser Reihenfolge:

1. **Der ESC ist seit AM32 CONFIG nicht neu gestartet.** Beim Verlassen
   dieses Bildschirms schickt die Firmware dem Bootloader keinen Befehl, die
   ESC-Firmware zu starten, und was der ESC dann tut, ist nicht
   aufgezeichnet. Starte den ESC neu, bevor du ihn testest:
   [AM32-Konfiguration](AM32-Konfiguration#verlassen).
2. **Falscher Pin.** Vergleiche **MENU → SETTINGS → ESC PIN** damit, wo das Kabel
   steckt. Die `LINK`-Zeile auf diesem Bildschirm ist live. Eine Paketrate
   über null heißt, der Pin stimmt.
3. **Keine gemeinsame Masse.** Die Masse des ESC und die der Platine müssen
   verbunden sein.
4. **Der ESC hat keinen Strom.** Er läuft an seinem eigenen Akku, nicht an der
   Platine.
5. **Der ESC beherrscht kein bidirektionales DShot**, oder es ist in seinen
   Einstellungen abgeschaltet. Unidirektionales DShot sendet nichts zurück.
6. **Das Kabel ist zu lang oder fängt Störungen ein.** Stelle **DSHOT KBAUD**
   auf 300 und beobachte `LINK`.

---

## Drehzahl läuft, jede Kachel zeigt `--`

Der ESC sendet eRPM (elektrische Drehzahl) und kein EDT (Extended DShot
Telemetry). Entweder beherrscht er kein EDT, oder EDT ist noch nicht
freigegeben.

Sieh auf das Feld unter dem Titel des Bildschirms `MENU`. Steht dort
`EDT OFF`, lies den nächsten Abschnitt. Beherrscht die ESC-Firmware kein EDT,
liefern zwei Kabel nur die Drehzahl. Ein
[KISS-Telemetriekabel](Telemetrie#das-optionale-dritte-kabel) liefert
Spannung, Strom, Temperatur und mAh.

---

## EDT bleibt aus

Die Firmware sendet die Freigabe jede Sekunde, solange eRPM eintrifft und EDT
nicht. Bleibt das Feld über mehrere Versuche rot, nimmt der ESC sie nicht an.

- **Steht der Motor?** Die Freigabe geht nur bei eRPM null hinaus. Nach dem
  Entschärfen läuft ein Motor sekundenlang aus.
- **Ist der Tester entschärft?** Im scharfen Zustand geht keine Freigabe
  hinaus.
- **Beherrscht die ESC-Firmware EDT?** ESC-Firmware ohne EDT ignoriert die
  Freigabe und sieht so aus.
- **Starte den ESC neu.** Nach 500 ms ohne eRPM setzt die Firmware ihren
  EDT-Zustand zurück und beginnt neu.

Bleibt EDT bei einem ESC aus, dessen Firmware EDT beherrscht, öffne ein Issue
(siehe [Immer noch nicht weiter](#immer-noch-nicht-weiter)).

---

## Bekannte Einschränkung: BLHeli_32 32.6 auf RAZOR32 und EXUAV 70A

Von einem Nutzer gemeldet in
[DshotDisplay-Issue #1](https://github.com/subtilitas/DshotDisplay/issues/1), offen, vom
Projekt nicht nachgestellt. Mit den ESCs RAZOR32 und EXUAV 70A unter
BLHeli_32 32.6 schließt der ESC seine Initialisierung nicht ab, und der Piepton
bei Gas null fehlt. Die Ursache ist unbekannt. Ein Behelf ist nicht bekannt.

---

## `HOLD TO ARM` ist nur ein Umriss und tut nichts

<img src="img/tester-card-held.png" width="240" alt="HOLD TO ARM als Umriss, der Balken zeigt EJECT THE USB DRIVE TO ARM">

Der Gasbalken zeigt `EJECT THE USB DRIVE TO ARM`: Ein Computer hält die SD-Karte
über USB (**MENU → SD LOG → USB DRIVE**; ziehe `MENU` um 36 px nach oben, um SD
LOG zu erreichen), und Scharfschalten ist gesperrt, bis er sie freigibt. Wirf
das Laufwerk am Computer aus oder drücke `EJECT` in SD LOG. Siehe
[SD-Aufzeichnung](SD-Aufzeichnung).

---

## Der Motor bleibt von selbst stehen

Drei Sperren halten den Motor an, und jede zeigt sich auf dem Bildschirm:

- **Leerlaufsperre.** 30 s ohne Berührung im scharfen Zustand entschärfen den
  Tester. `IDLE ns` zählt in den letzten 5 s oben rechts herunter. Beim
  Entschärfen dunkelt die Hintergrundbeleuchtung kurz ab.
- **`MENU` öffnen.** Das Öffnen entschärft immer.
- **Herzschlag-Sicherung.** Hält die Anzeige-Firmware an, setzt der zweite
  Kern das Gas nach 250 ms auf null. Der Bildschirm wäre dann ebenfalls
  eingefroren.

Bleibt der Motor ohne eines davon stehen, prüfe die Schutzfunktionen des ESC
(Unterspannungsabschaltung, Temperatur, Desync) und sieh dir `ESC STATUS` und
`ESC TEMP` an.

---

## Die Platine startet von selbst neu

Öffne **MENU → SETTINGS** und verschiebe bis DIAGNOSTICS. `LAST RESTART`
nennt, warum die Platine zuletzt gestartet ist. `BROWN-OUT` oder `GLITCH` in
Bernstein nach einem Neustart bei laufendem Motor weist auf die Versorgung
der Platine: Betreibe den ESC an seinem eigenen Akku und die Platine über
USB. Siehe [Die Bildschirme](Die-Bildschirme#settings).

---

## `ESC STATUS` zeigt `WARN`, `ERROR` oder `ALERT`

Die Kachel ist bernsteinfarben bei `WARN`, rot bei `ERROR` und magenta bei
`ALERT`. Der ESC sendet den Status über EDT; was jeder bedeutet, legt die
ESC-Firmware fest.

1. **Entschärfen.** Drücke `DISARM`.
2. **Den ESC abkühlen lassen.**
3. **`ESC TEMP` und `CURRENT` ablesen.** `ESC TEMP` wird ab 90 °C rot.
4. Schlage den Status in der Dokumentation der ESC-Firmware nach, bevor du
   wieder scharf schaltest. Siehe [Telemetrie](Telemetrie#die-kacheln).

---

## `LINK` zeigt eine hohe Fehlerrate

Die Fehlerrate ist oberhalb von 5 % bernsteinfarben. Die übliche Ursache ist
das Signalkabel: Länge, Verlegung oder eine fehlende Masse. Kürzen, von den
Motorphasen fernhalten, dann **DSHOT KBAUD** herunterstellen.

Fehlgeschlagene Pakete sind Frames, auf die der ESC nicht reagiert hat, auch
wenn die Drehzahlanzeige funktioniert.

---

## Die Karte wird nicht erkannt

<img src="img/log-nocard.png" width="240" alt="Keine Karte">

Sieh auf die `MOUNT`-Zeile des SD-LOG-Bildschirms.

- **`3 NOT READY`**: Die Karte ist nicht angelaufen. Mit
  `INSERT A CARD, THEN RETRY MOUNT` hat nichts geantwortet: keine Karte, oder
  sie sitzt nicht richtig. Mit `CARD NOT USABLE: RESEAT OR REPLACE` hat eine
  Karte geantwortet und ist gescheitert: neu einstecken oder eine andere
  probieren.
- **`13 NO FILESYSTEM`** mit einer Größe unter `CARD`: Die Karte antwortet,
  das Dateisystem ist das Problem. Als FAT32 formatieren.
- **Karte nach dem Start eingesteckt**: `RETRY MOUNT` drücken. Die Karte wird
  nur einmal beim Start eingebunden, und die Platine hat keinen
  Karten-Erkennungspin.

---

## Verlorene Frames in einer Aufzeichnung

`DROPPED FRAMES` über null heißt, die Aufzeichnung hat Lücken. Sieh dir
`BUF PEAK` und `WORST FLUSH` an. Ein Höchststand nahe der Puffergröße
(8192 Byte) oder ein Schreibvorgang, der länger dauert, als der Puffer
überbrückt (etwa 1,1 s), heißt, die Karte ist zu langsam.

Probiere zuerst eine andere Karte.

---

## AM32 verbindet nicht

Der Bootloader lauscht nur kurz nach dem Einschalten. **Zieh den ESC-Akku ab
und steck ihn wieder an, während der AM32-Bildschirm offen ist.** Ein Neustart
vor dem Öffnen des Bildschirms funktioniert nicht.

- **`REPLY NOT UNDERSTOOD`**: Die Verbindung funktioniert, die Rahmenbildung
  nicht. Mögliche Ursache: Der ESC läuft nicht mit AM32.
- **`NO VALID SETTINGS`**: Der ESC hat geantwortet, und der Einstellungsblock
  hat die Plausibilitätsprüfung nicht bestanden. Schreibe nichts.
- **`LAYOUT n NOT KNOWN`**: Der ESC speichert ein Einstellungslayout, das der
  Tester nicht kennt; er zeigt `THE TESTER KNOWS LAYOUTS 2 TO 4.` und
  schreibt nichts. Nimm für diesen ESC den AM32-Konfigurator.
- **`READ FAILED (...)`**: Der Verbindungsaufbau hat geklappt, das Lesen der
  Einstellungen nicht. Prüfe Signal- und Masseader.

---

## Meine Einstellungen setzen sich zurück

Sie wurden nicht gespeichert, oder eine neue Firmware hat das Format der
Einstellungen geändert. Polzahl und Gasobergrenze werden unter
**MENU → SETTINGS** geändert, und `HOLD TO SAVE` dort schreibt sie ins Flash.
Die Hinweiszeile zeigt `UNSAVED - HOLD SAVE TO KEEP`, solange Änderungen
ungespeichert sind, und `MENU` zeigt `UNSAVED` in der Zeile `SETTINGS`.

Eine Firmware, deren Einstellungsformat von dem im Flash abweicht, startet mit
ihren Voreinstellungen. Wer von einer früheren Firmware auf eine mit
Einstellungsformat Version 4 aktualisiert, findet alle gespeicherten
Einstellungen einmal auf der Voreinstellung, auch den ESC-Pin: Der ESC-Pin ist dann GP0,
KISS ist aus und SERVO SUPPLY steht auf `NONE`. Version 5 bringt CURRENT
SENSE, daher geschieht dasselbe beim Wechsel von Version 4 auf Version 5
noch einmal: Alle gespeicherten Einstellungen stehen wieder auf der
Voreinstellung, CURRENT SENSE auf `NONE`. Stelle sie neu ein und speichere
mit `HOLD TO SAVE`.

---

## KISS-Telemetrie ist aus

Das ist die Voreinstellung. Einschalten unter **MENU → SETTINGS → KISS TELEM**.

Die Platine hat zwei freie GPIOs (General Purpose Input/Output, frei
verwendbare Pins), GP0 und GP1, und das ESC-Signal liegt anfangs auf GP0.
KISS ist voreingestellt aus, damit der übrige Pin nicht für ein Kabel belegt
ist, das vielleicht nicht angeschlossen ist.

Beim Einschalten von KISS wird der KISS-Pin auf den anderen freien Pin
verschoben, falls er mit dem ESC-Pin zusammenfällt. Der Pin-Schrittschalter
überspringt den ESC-Pin. Der Empfänger ist eine PIO-Zustandsmaschine (PIO:
programmierbare Ein-/Ausgabe) und funktioniert auf beiden Pins.

Schaltet sich KISS mit `KISS OFF: NEEDS A PIN OF ITS OWN` selbst ab, war der
gespeicherte KISS-Pin nicht frei oder gleich dem ESC-Pin. Verschiebe zuerst
das ESC-Signal auf den anderen Pin und schalte dann KISS ein.

---

## Immer noch nicht weiter

Öffne ein Issue unter
[github.com/subtilitas/MiniRcBench/issues](https://github.com/subtilitas/MiniRcBench/issues).
Nützlich sind: die Firmware-Version (SETTINGS → VERSION), LAST RESTART, was
die `LINK`-Zeile zeigt, und welcher ESC mit welcher ESC-Firmware.
