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

## Der Bildschirm reagiert nicht auf Berührung

<img src="img/splash-no-touch.png" width="240" alt="Der Startbildschirm mit TOUCH CONTROLLER NOT FOUND in Bernstein über dem Namen">

`TOUCH CONTROLLER NOT FOUND` in Bernstein auf dem Startbildschirm bedeutet,
dass der Touch-Controller der Firmware beim Einschalten nicht geantwortet
hat. Die Zeile verschwindet, sobald er antwortet. Steht sie noch, wenn
`MENU` erscheint, hat die Berührung während des Startbildschirms nicht
geantwortet; die Firmware liest sie weiter, und ein Controller, der später
antwortet, funktioniert. Bewirkt Tippen weiter nichts, helfen die Schritte
unten. Nach einem Update, das die
Einstellungen zurückgesetzt hat, steht sie unter den beiden Zeilen zu den
Einstellungen.

<img src="img/splash-reset-no-touch.png" width="240" alt="Der Startbildschirm mit SETTINGS RESET BY UPDATE, CHECK MENU > SETUP und TOUCH CONTROLLER NOT FOUND über dem Namen">

Ohne Berührung lässt sich die Platine nicht bedienen: Scharfschalten,
`DISARM`, `LATCH` und `MENU` brauchen sie alle. Die Leerlaufsperre
entschärft 30 s nach der letzten Berührung, die der Controller meldet. Ein
nasser Bildschirm oder etwas, das auf dem Glas liegt, zählt als Berührung
und hält einen scharfen Tester scharf. Um einen Motor anzuhalten, trenne
den Akku des ESC.

Versuche der Reihe nach:

1. Den Bildschirm trocknen und alles vom Glas nehmen, ein Kabel oder ein
   Werkzeug: Es kann als Finger zählen und jedes Tippen blockieren.
2. Die Platine aus- und einschalten: USB abziehen. Mit eingesetzter Zelle
   läuft die Platine von der Zelle weiter, also auch die Zelle abstecken.

Nicht bekannt: warum ein Controller nicht antwortet. Ein Controller, der
nicht startet, sein Flachbandkabel in der Anzeige und ein hängender I2C-Bus
(Inter-Integrated Circuit) werden nicht unterschieden. Das Flachbandkabel
gehört zur Platine und ist nichts zum Öffnen; kein Reparaturschritt ist hier
geprüft. Bleibt die Zeile nach einem Neustart, melde es wie unter
[Immer noch nicht weiter](#immer-noch-nicht-weiter) beschrieben.

---

## Keine Telemetrie

<img src="img/tester-noesc.png" width="240" alt="Keine Telemetrie">

Auf dem Signalkabel antwortet nichts: Der Tester zeigt keine Telemetrie,
und das Feld unter dem Titel von ESC COMMANDS zeigt `NO ESC`. DShot läuft nur,
solange der Tester oder ESC COMMANDS offen ist, prüfe also dort. Prüfe in
dieser Reihenfolge:

1. **Der ESC ist seit ESC SETTINGS nicht neu gestartet.** Beim Verlassen
   dieses Bildschirms schickt die Firmware dem Bootloader keinen Befehl, die
   ESC-Firmware zu starten, und was der ESC dann tut, ist nicht
   aufgezeichnet. Starte den ESC neu, bevor du ihn testest:
   [AM32-Konfiguration](AM32-Konfiguration#verlassen).
2. **Falscher Pin.** Vergleiche **SETUP → SIG PIN** damit, wo das Kabel
   steckt; der J5-Pin steht unter dem Wert. Öffne SETUP vom Tester aus,
   mit einem Tippen auf `MAX` im entschärften Zustand: Dort läuft DShot
   weiter, und die `LINK`-Zeile ist live. Eine Paketrate über null heißt, der
   Pin stimmt.
3. **Keine gemeinsame Masse.** Die Masse des ESC und die der Platine müssen
   verbunden sein.
4. **Der ESC hat keinen Strom.** Er läuft an seinem eigenen Akku, nicht an der
   Platine.
5. **Der ESC beherrscht kein bidirektionales DShot**, oder es ist in seinen
   Einstellungen abgeschaltet. Unidirektionales DShot sendet nichts zurück.
6. **Das Kabel ist zu lang oder fängt Störungen ein.** Stelle **DSHOT**
   auf 300 und beobachte `LINK`.

---

## Drehzahl läuft, jede Kachel zeigt `--`

Der ESC sendet eRPM (elektrische Drehzahl) und kein EDT (Extended DShot
Telemetry). Entweder beherrscht er kein EDT, oder EDT ist noch nicht
freigegeben.

Sieh auf das Feld unter dem Titel von ESC COMMANDS. Steht dort
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
über USB (**MENU → LOGS → USB DRIVE**), und Scharfschalten ist gesperrt,
bis er sie freigibt. Wirf
das Laufwerk am Computer aus oder drücke `EJECT` in LOGS. Siehe
[SD-Aufzeichnung](SD-Aufzeichnung).

Ein Computer, der das Laufwerk abmeldet, gibt die Karte ebenfalls zurück. Ein
gezogenes Kabel bei einer Platine am Akku tut das nicht: Der USB-Stack dieser
Firmware sieht ein gezogenes Kabel wie einen Computer, der schlafen ging, und
ein schlafender Computer behält die Karte. Drücke dann `EJECT` in LOGS.
Beides ist auf der Hardware nicht geprüft.

---

## Der Motor bleibt von selbst stehen

Vier Sperren halten den Motor an, und jede zeigt sich auf dem Bildschirm:

- **Leerlaufsperre.** 30 s ohne vom Touch-Controller gemeldete Berührung
  im scharfen Zustand entschärfen den Tester. Ein nasser Bildschirm oder
  etwas auf dem Glas zählt als Berührung. `IDLE ns` zählt in den letzten
  5 s oben rechts herunter. Beim
  Entschärfen dunkelt die Hintergrundbeleuchtung kurz ab.
- **`BACK` des Testers.** Der Weg vom Tester zu `MENU`
  entschärft immer.
- **Touch antwortet nicht.** Vier fehlgeschlagene Lesungen des
  Touch-Controllers in Folge, über 100 ms oder länger, entschärfen den
  scharfen Tester. `TOUCH NOT ANSWERING` steht in Bernstein unter der
  Drehzahl, bis er wieder antwortet. Ein lockeres Flachbandkabel der
  Anzeige oder ein hängender I2C-Bus
  (Inter-Integrated Circuit) kann die Ursache sein; welche, wird nicht
  unterschieden.
- **Herzschlag-Sicherung.** Hält die Anzeige-Firmware bei scharfem Tester
  länger als 250 ms an, schaltet der zweite Kern scharf aus, setzt das Gas auf
  null und hält den Stopp fest. Das Gas bleibt auf null, wenn der Bildschirm
  wieder läuft: Der Tester ist entschärft, und erneutes Scharfschalten
  verlangt das 1-s-Halten. Der Bildschirm wäre dann ebenfalls eingefroren.

Bleibt der Motor ohne eines davon stehen, prüfe die Schutzfunktionen des ESC
(Unterspannungsabschaltung, Temperatur, Desync) und sieh dir `ESC STATUS` und
`ESC TEMP` an.

---

## Die Platine startet von selbst neu

Öffne **MENU → SETUP** und verschiebe bis DIAGNOSTICS. `LAST RESTART`
nennt, warum die Platine zuletzt gestartet ist. `BROWN-OUT` oder `GLITCH` in
Bernstein nach einem Neustart bei laufendem Motor weist auf die Versorgung
der Platine: Betreibe den ESC an seinem eigenen Akku und die Platine über
USB. Siehe [Die Bildschirme](Die-Bildschirme#setup).

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
Motorphasen fernhalten, dann **DSHOT** herunterstellen.

Fehlgeschlagene Pakete sind Frames, auf die der ESC nicht reagiert hat, auch
wenn die Drehzahlanzeige funktioniert.

---

## Die Karte wird nicht erkannt

<img src="img/log-nocard.png" width="240" alt="Keine Karte">

Sieh auf die `MOUNT`-Zeile des LOGS-Bildschirms.

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
- **`EFM8 E8B2 ANSWERED: READING.`**: Ein Bluejay- oder BLHeli_S-ESC hat
  geantwortet, und der Bildschirm liest seine Einstellungen; Bluejay 0.21
  und BLHeli_S 16.3 bis 16.7 lassen sich schreiben. `BACK` aus seiner Liste
  sendet LEAVE, das den ESC startet; `BACK` vor der Liste öffnet eine
  Verlassenseite: ESC-Akku abziehen, `LEAVE`, `ESC PROGRAM`, dann wieder
  anstecken ([AM32-Konfiguration](AM32-Konfiguration#efm8-escs)).
- **`HALF WRITTEN: KEEP THE ESC POWERED.`** oder
  **`NO ANSWER IN THE WRITE: KEEP POWER.`**: Ein Schreiben auf Bluejay oder
  BLHeli_S wurde abgebrochen, nachdem die Seite des ESC gelöscht war. Den
  ESC-Akku nicht abziehen: Signalkabel prüfen, `READ AGAIN`, dann schreiben,
  oder `REVERT` und die Seite wie gelesen schreiben. Ein Aus- und
  Einschalten vorher verliert die Tags des ESC
  ([AM32-Konfiguration](AM32-Konfiguration#efm8-escs)). Steht darunter
  `SESSION ENDED: RESTORE ON A COMPUTER.`, ist die Sitzung beendet
  (Leitungstreiber, Herzschlag), und keine spätere Sitzung schreibt die
  Seite: ESC eingeschaltet lassen und am Computer wiederherstellen. Die
  alte Seite steht in der Sicherung auf der Karte und, bis du ESC SETTINGS
  verlässt, unter `OLD PAGE` auf der Verbindungsseite.
- **`ESC LOST ITS TAGS: READ-ONLY.`**: Die Seite des ESC wurde gelöscht und
  der ESC neu gestartet; er läuft mit seinen Standardwerten. Nur ein
  Firmware-Flash für diesen ESC am Computer bringt die Tags zurück; die
  Sicherung auf der Karte nennt sie.
- **`THE ESC ANSWERED, THEN STOPPED.`**: Der ESC hat geantwortet und dann
  nicht mehr, bevor seine Seite gelesen war; die Suche läuft erneut. Prüfe
  das Signalkabel und lass den ESC eingeschaltet.
- **`NO VALID SETTINGS`**: Der ESC hat geantwortet, und der Einstellungsblock
  hat die Plausibilitätsprüfung nicht bestanden. Schreibe nichts.
- **`LAYOUT n NOT KNOWN: READ-ONLY.`**: Der Bildschirm hat verbunden, und
  der ESC speichert ein Einstellungslayout, das der Tester nicht kennt. Der
  Bildschirm zeigt die Bytes, sichert sie und schreibt nichts. Nimm zum
  Ändern dieses ESC den AM32-Konfigurator.
- **`READ FAILED (...)`**: Der Verbindungsaufbau hat geklappt, das Lesen der
  Einstellungen nicht. Prüfe Signal- und Masseader.

---

## Meine Einstellungen setzen sich zurück

Sie wurden nicht gespeichert, oder eine neue Firmware konnte das Format der
Einstellungen im Flash nicht lesen. Polzahl und Gasobergrenze werden unter
**MENU → SETUP** geändert, und `HOLD TO SAVE` dort schreibt sie ins Flash.
Die Hinweiszeile zeigt `UNSAVED - HOLD SAVE TO KEEP`, solange Änderungen
ungespeichert sind, und `MENU` zeigt `UNSAVED` in der Zeile `SETUP`, der
letzten.

Die Firmware speichert ihre Einstellungen im Format Version 6. Einstellungen,
die eine frühere Firmware im Format Version 2, 3, 4 oder 5 gespeichert hat,
darunter die von DshotDisplay 2.x, werden übernommen: Jede Einstellung, die
das alte Format kennt, behält ihren Wert, geprüft nach denselben Regeln wie
gespeicherte Einstellungen, und jede neuere Einstellung beginnt mit ihrer
Voreinstellung. Also:

- Polzahl, `MAX THROTTLE`, `DSHOT`, KISS, Helligkeit und Kontrast
  behalten ihre Werte; ein Wert außerhalb des Bereichs wird angepasst, wie
  beim Speichern;
- ein ESC-Pin, den diese Platine nicht frei hat, etwa GP28 oder GP29 aus
  einer Firmware vor Version 4, kehrt zur Voreinstellung (GP0) zurück, und
  KISS auf einem solchen Pin wird ausgeschaltet, sein Pin wieder die
  Voreinstellung (GP1);
- ein KISS-Pin auf dem ESC-Pin wechselt auf den anderen freien Pin, ob KISS
  an oder aus ist, und KISS behält seine Einstellung: SIG und AUX teilen nie
  einen Pin;
- CURRENT SENSE steht nach Version 2, 3 oder 4 auf `NONE`; nach Version 5
  behält es seinen Wert. SERVO SUPPLY steht nach Version 2 oder 3 auf
  `OWN`, denn der Schalter auf GP0 aus Version 3 liegt auf dem Pin
  des ESC-Kabels; nach Version 4 oder 5 behält es `GP28` oder `GP29`;
- ALONG AXIS, LENGTH, SIGN und die Kalibrierung des Winkelmessers stehen
  auf ihrer Voreinstellung: `LONG`, `OFF`, `NORMAL` und keine.

Eine Übernahme wird angezeigt: Der Startbildschirm zeigt
`SETTINGS CARRIED OVER FROM AN UPDATE` und `CHECK MENU > SETUP, THEN SAVE`,
die Zeile `SETUP` in MENU zeigt `UPDATED`, und die Hinweiszeile in
SETUP zeigt `CARRIED OVER BY UPDATE - CHECK, SAVE`. Prüfe die Werte und
speichere mit `HOLD TO SAVE`; das Speichern schreibt auch ohne Änderung. Bis
dahin behält das Flash den alten Block, und jeder Start übernimmt ihn erneut.

<img src="img/splash-carried.png" width="240" alt="Der Startbildschirm mit SETTINGS CARRIED OVER FROM AN UPDATE und CHECK MENU > SETUP, THEN SAVE über dem Namen">

Einstellungen eines Formats, das die Firmware nicht kennt, Version 1 oder das
einer späteren Firmware, werden verworfen: Alle Einstellungen stehen wieder
auf der Voreinstellung, auch der ESC-Pin. Stelle sie neu ein und speichere
mit `HOLD TO SAVE`. Ein solches Zurücksetzen wird angezeigt: Der
Startbildschirm zeigt `SETTINGS RESET BY UPDATE` und `CHECK MENU > SETUP`,
die Zeile `SETUP` in MENU zeigt `UPDATED`, und die Hinweiszeile in
SETUP zeigt `RESET BY UPDATE - CHECK AND SAVE`, bis ein Speichern gelingt.
Ein beschädigter Block, statt eines von einer anderen Version geschriebenen,
wird ohne diese Hinweise verworfen; die Hinweiszeile zeigt dann
`NOTHING SAVED: DEFAULTS IN USE`.

---

## `SDERR` auf dem Tester, oder eine Aufzeichnung, die anhielt

Die Aufzeichnung endete ohne `STOP`. **MENU → LOGS** sagt über `START`,
wie: eine volle Karte, ein fehlgeschlagener Schreibvorgang, eine Datei, die
sich nicht öffnen ließ, oder das Werkzeug (KST SERVO, HITEC SERVO, AM32
CONFIG), das die Karte nahm. Siehe
[SD-Aufzeichnung](SD-Aufzeichnung#wie-die-letzte-aufzeichnung-endete).

---

## Ein Schreiben meldet `BOARD VOLTAGE IS RED: NOT WRITTEN.`

KST SERVO, HITEC SERVO und ESC SETTINGS schreiben nichts und stellen nichts
wieder her, solange die BOARD-Spannung auf dem Tester rot ist: unter 3,30 V,
bis sie wieder 3,35 V erreicht. Lade die Zelle der Platine, dann halte
erneut. Die Änderungen bleiben. Mit USB-Versorgung ohne Zelle ist der Wert
nicht gemessen; zeigt er dort Rot, setze eine geladene Zelle ein.

---

## KISS-Telemetrie ist aus

Das ist die Voreinstellung. Einschalten unter **MENU → SETUP → KISS TELEM**.

Die Platine hat zwei freie GPIOs (General Purpose Input/Output, frei
verwendbare Pins), GP0 und GP1, und das ESC-Signal liegt anfangs auf GP0.
KISS ist voreingestellt aus, damit der übrige Pin nicht für ein Kabel belegt
ist, das vielleicht nicht angeschlossen ist.

KISS empfängt auf AUX, das nie der Pin des ESC, SIG, ist: Ein Schritt von
**SIG PIN** oder **AUX PIN** tauscht die beiden, und das Einschalten von KISS
verschiebt keinen Pin. Der Empfänger ist eine PIO-Zustandsmaschine (PIO:
programmierbare Ein-/Ausgabe) und funktioniert auf beiden Pins. Prüfe, ob
das KISS-Kabel an dem Pin steckt, den **AUX PIN** nennt, mit seinem J5-Pin
unter dem Wert.

---

## Der Servotester hält bei MIN oder MAX mit `STALL` an

Mit dem Stromsensor hält der Servotester an, wenn er 400 ms lang mehr als
1,0 A misst. Viele Servos, die gegen ihren eigenen Endanschlag fahren,
ziehen dort Blockierstrom, also löst `MIN` oder `MAX` jenseits des
Servowegs ihn aus. Verenge MIN und MAX auf dem SET-Bildschirm, bis das
Servo vor seinem Endanschlag stehen bleibt. `OVERCURRENT` ist ein Messwert
von 3,0 A oder mehr: Prüfe Servo und Verkabelung auf einen Kurzschluss vor
dem nächsten Start. Siehe [Servotester](Servotester#die-abschaltungen).
Nicht auf Hardware getestet.

---

## Immer noch nicht weiter

Öffne ein Issue unter
[github.com/subtilitas/MiniRcBench/issues](https://github.com/subtilitas/MiniRcBench/issues).
Nützlich sind: die Firmware-Version (SETUP → VERSION), LAST RESTART, was
die `LINK`-Zeile zeigt, und welcher ESC mit welcher ESC-Firmware.
