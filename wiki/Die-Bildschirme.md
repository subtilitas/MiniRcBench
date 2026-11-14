[English](The-Screens) | **Deutsch**

# Die Bildschirme

Die Firmware hat acht Bildschirme. Der Tester ist die Wurzel. `MENU` öffnet
die Liste der Werkzeuge, und jedes Werkzeug verlässt du mit `BACK`, das zu
`MENU` zurückführt.

```
Tester ──MENU──> MENU ──┬── AM32 CONFIG  ──BACK──> MENU
   ▲                    ├── SERVO TESTER ──BACK──> Abziehseite ──> MENU
   │                    ├── KST SERVO    ──BACK──> Verlassenseite ──> Abziehseite ──> MENU
   │                    ├── HITEC SERVO  ──BACK──> Verlassenseite ──> Abziehseite ──> MENU
   │                    │   (in der Familienauswahl führt BACK zu MENU; während des Eintritts zur Abziehseite)
   │                    ├── SETTINGS     ──BACK──> MENU
   │                    └── SD LOG       ──BACK──> MENU
   └──── BACK auf MENU ─┘
```

`BEEP THE ESC` ist eine Zeile von `MENU`, die etwas auslöst und keinen
Bildschirm öffnet.

**BACK sitzt oben rechts** im Kopfbereich, auf jedem Bildschirm außer dem
Tester. Der Kopfbereich ist 36 px hoch, `BACK` misst 64 × 32 px. Der Tester ist
die Wurzel und hat keinen Kopfbereich. Auf der Abziehseite von SERVO TESTER,
KST SERVO und HITEC SERVO wartet `BACK`, bis die Pins freigegeben sind: Bis
dahin ist es deaktiviert gezeichnet, als Umriss mit grauer Beschriftung, und
ein Tippen darauf bewirkt nichts.

---

## Der Tester

<img src="img/tester-armed.png" width="240" alt="Tester-Bildschirm">

**Obere Zeile.** Von links nach rechts:

- das Scharf-Feld: `SAFE` (grün) oder `ARMED` (rot);
- `DS` und die DShot-Bitrate in kbit/s, wie unter SETTINGS eingestellt, cyan,
  sobald eine EDT-Freigabe (Extended DShot Telemetry) gesendet wurde. `DSERR`
  in Bernstein ersetzt sie, wenn die Anzeige ihren DShot-Ausgang nicht
  einrichten konnte; dann wird nichts an den ESC gesendet;
- der Aufzeichnungszustand: `REC`, `SD`, `SDERR` oder `NOSD`
  (siehe [SD-Aufzeichnung](SD-Aufzeichnung)). `SDERR`, eine Aufzeichnung,
  die von selbst endete, steht auf einem bernsteinfarbenen Feld und lässt
  Scharf-Feld, Drehzahl und Gas frei;

  <img src="img/tester-sderr.png" width="240" alt="Der Tester scharf, SDERR auf einem bernsteinfarbenen Feld in der oberen Zeile">

- `BOARD` und die Spannung des platineneigenen Akkus, gemessen über den
  Teiler aus 200 kΩ und 100 kΩ; höchstens 9,90 V. Es ist nicht der Akku des
  ESC: Dessen Spannung steht in der Kachel `VOLTAGE`. Was sie bei reiner
  USB-Versorgung (Universal Serial Bus) anzeigt, ist nicht gemessen, und die
  Firmware kann USB-Versorgung nicht vom Akku unterscheiden: Kein Pin der
  Platine erfasst die USB-Versorgung. Die Spannung ist unter 3,50 V
  bernsteinfarben und unter 3,30 V rot, der übliche untere Bereich einer
  Lithium-Polymer-Zelle, auf dieser Platine nicht gemessen. Die Farbe folgt
  dem angezeigten Wert: `3.50V` ist nie bernsteinfarben, `3.30V` nie rot.
  Sie erhält die Farbe darüber zurück, sobald sie 50 mV über der Schwelle
  liegt. Bei reiner USB-Versorgung kann der Wert niedrig sein und sagt dann
  nichts über eine Zelle: Unter 2,50 V, für eine arbeitende Zelle nicht
  plausibel, steht er in der normalen Textfarbe, ab 2,55 V wieder rot. Kein
  Schreiben und kein Scharfschalten wird deswegen verweigert. In den letzten
  5 s vor dem Entschärfen durch die Leerlaufsperre ersetzt `IDLE ns` in
  Bernstein die Spannung und `BOARD`.

**Drehzahl.** Mechanische Drehzahl (RPM, Umdrehungen pro Minute), berechnet
aus der vom ESC (Electronic Speed Controller, Motorregler) gemeldeten eRPM
(elektrische Drehzahl) und der Polzahl. Der `ERPM`-Wert und die Polzahl stehen
darunter. Grüne Ziffern: eRPM ist in den letzten 500 ms eingetroffen.
Dunkelrote Null: nicht, und die Zeile zeigt `NO TELEMETRY`. Die Zeile
darunter, rechts, ist der Wisch-Hinweis: `SWIPE = THROTTLE`, oder
`SWIPE ACTIVE`, solange ein Wischer das Gas bewegt.

**Sechs Kacheln.** Spannung, Strom, ESC-Temperatur, Stress, ESC-Status und
Verbindungsqualität. Siehe [Telemetrie](Telemetrie).

**Gas.** Die Anzeige, die Obergrenze und der befohlene Prozentwert. Der
Prozentwert bezieht sich auf den vollen DShot-Bereich (0–2000 = 0–100 %): Bei
20 % Obergrenze zeigt ein Zug über die volle Breite `20%`. Im entschärften
Zustand zeigt der Balken `ARM TO ENABLE`.

**Schaltflächen.** `HOLD TO ARM` / `DISARM`, `HOLD` (haltendes Gas), `MENU`.

Solange ein Computer die SD-Karte über USB hält, ist Scharfschalten gesperrt:
`HOLD TO ARM` ist nur als Umriss gezeichnet, und der Balken zeigt
`EJECT THE USB DRIVE TO ARM` in Bernstein. Wirf das Laufwerk am Computer aus
oder drücke `EJECT` in SD LOG, dann ist die Schaltfläche wieder bedienbar
(siehe [SD-Aufzeichnung](SD-Aufzeichnung)).

<img src="img/tester-card-held.png" width="240" alt="Der Tester, während ein Computer die Karte hält: HOLD TO ARM als Umriss, der Balken zeigt EJECT THE USB DRIVE TO ARM">

**Berührung.** Die Firmware liest nur den ersten Berührungsplatz des
Touch-Controllers. Welche Berührung der Controller dort meldet, während
zwei Finger auf dem Glas liegen, ist nicht gemessen; ein Druck, während der
Gas-Finger aufliegt, kommt also womöglich nicht an. Erst den Gas-Finger
heben, dann `DISARM` drücken. Ein nasser Bildschirm oder ein Kabel, das auf
dem Glas liegt, kann als Finger zählen und jedes Tippen blockieren.

Hat der Touch-Controller vier Lesungen in Folge verpasst und seit 100 ms
oder länger nicht geantwortet, ersetzt `TOUCH NOT ANSWERING` in Bernstein
den Wisch-Hinweis; `NO TELEMETRY` behält seine Zeile. Solange der Hinweis
steht, lässt sich der Tester nicht scharf schalten, `HOLD TO ARM` ist als
Umriss gezeichnet, und ein scharfer Tester entschärft: Das Gas geht auf
null und `HOLD` aus, wie beim Entschärfen durch die Leerlaufsperre. Der
Hinweis verschwindet, wenn der Controller wieder antwortet; der Tester
bleibt entschärft.

Nicht gemessen: ob der Touch-Controller ohne Finger auf dem Glas über
Minuten weiter antwortet. Tut er es nicht, erschiene dieser Hinweis, und
ein mit `HOLD` gehaltener Motor bliebe kurz nach dem Abheben des Fingers
stehen. Die Prüfung am Prüfstand steht in STATUS.md im Repository, offener
Punkt 22.

<img src="img/tester-touch-lost.png" width="240" alt="Der Tester mit TOUCH NOT ANSWERING in Bernstein unter der Drehzahl">

---

## MENU

<img src="img/menu.png" width="240" alt="Der MENU-Bildschirm">

Erreichbar über die Schaltfläche `MENU` des Testers. Das Öffnen **entschärft**,
setzt das Gas auf 0 und schaltet `HOLD` aus.

Unter dem Titel zeigt eine Leiste den ESC-Pin und die DShot-Rate, zum Beispiel
`GP0  600 KBIT/S`, sowie das EDT-Feld. Das Feld ist nur eine Anzeige:

| Feld | Wann |
|---|---|
| `EDT ON`, grün | Extended-DShot-Telemetry-Frames treffen ein |
| `EDT OFF`, rot | eRPM-Frames treffen ein, EDT-Frames nicht |
| `NO ESC`, gedämpftes Grau | seit 500 ms kein eRPM-Frame: kein ESC angeschlossen, oder keiner antwortet |

Nach einer Sitzung in AM32 CONFIG, in der der Bootloader des ESC geantwortet
hat, steht unter der Zeile eine zweite: `POWER-CYCLE THE ESC TO RUN IT` in
Bernstein, bis ein eRPM-Frame eintrifft oder die Platine neu startet
([AM32-Konfiguration](AM32-Konfiguration#verlassen)).

<img src="img/menu-no-esc.png" width="240" alt="MENU ohne angeschlossenen ESC: das Feld zeigt NO ESC"> <img src="img/menu-edt-off.png" width="240" alt="MENU mit einem ESC, der eRPM und kein EDT sendet: das Feld zeigt EDT OFF in Rot">

Es gibt keine Einschalt-Schaltfläche. Die Firmware schickt die Freigabe an
jeden ESC, der zu antworten beginnt, und wiederholt sie jede Sekunde, solange
eRPM eintrifft und EDT nicht. Der ESC bestätigt die Freigabe nicht, daher zeigt
das Feld *empfangene Frames*, nicht gesendete Befehle.

Unter der Leiste steht eine Liste mit sieben Zeilen von 40 px Höhe und 42 px
Abstand. Jede Zeile zeigt links ihren Namen und rechts ihren Zustand. `BACK`
führt zum Tester zurück.

**Verschieben.** Sechs Zeilen passen auf den Bildschirm. SETTINGS ist die
sechste, ganz sichtbar und ohne Ziehen antippbar, so bleibt ihr Zustand
`UNSAVED` im Blick. Von der siebten, SD LOG, sind zuerst nur 6 px sichtbar.
Ziehe MENU um 36 px nach oben, um SD LOG ganz zu sehen; erst dann nimmt die
Zeile ein Tippen an. Ein 3 px breiter Rollbalken am rechten Rand zeigt die
Lage.

| Zeile | Zustand rechts | Ein Tippen |
|---|---|---|
| **BEEP THE ESC** | `DISARMED`; 350 ms nach einem Tippen `SENT` oder `REFUSED` | Sendet den DShot-Beacon-Befehl, der Motor piept. Damit erkennst du, welcher ESC angeschlossen ist |
| **AM32 CONFIG** | der ESC-Pin, `GP0`; `PINS BUSY` | Öffnet den AM32-Editor |
| **SERVO TESTER** | die beiden Signalpins, `GP0+1`; `PINS BUSY` | Öffnet den Servotester |
| **KST SERVO** | der ESC-Pin, `GP0`; `PINS BUSY` | Öffnet den KST-Programmierer. Siehe [KST-Programmierung](KST-Programmierung) |
| **HITEC SERVO** | der ESC-Pin, `GP0`; `PINS BUSY` | Öffnet den Hitec-Programmierer. Siehe [Hitec-Programmierung](Hitec-Programmierung) |
| **SETTINGS** | `UNSAVED`, solange ein Wert vom Flash abweicht; sonst `UPDATED`, solange ein Zurücksetzen durch ein Update nicht überspeichert ist; sonst nichts. Alles in Bernstein. `PINS BUSY`, solange die Pins noch zurückgegeben werden, und öffnet sich nicht | Öffnet SETTINGS |
| **SD LOG** | `NO CARD`, `READY`, `RECORDING`, `CARD ERROR`, oder `USB DRIVE`, solange ein Computer die Karte hält | Öffnet den Aufzeichnungs-Bildschirm |

`PINS BUSY` heißt, dass die Signalpins noch belegt sind, zum Beispiel während
DShot nach dem Servotester, dem KST-Programmierer oder dem Hitec-Programmierer
neu startet. AM32 CONFIG, SERVO TESTER, KST SERVO, HITEC SERVO und SETTINGS
zeigen es, und ein Tippen auf eine solche Zeile öffnet nichts. Die Zeile zeigt ihre Pins wieder, sobald DShot läuft.
SD LOG belegt keinen Pin und bleibt bedienbar.

<img src="img/menu-pins-busy.png" width="240" alt="MENU nach oben gezogen, während die Pins zurückgegeben werden: PINS BUSY bei AM32 CONFIG, SERVO TESTER, KST SERVO, HITEC SERVO und SETTINGS">

BEEP funktioniert nur im entschärften Zustand, und das Öffnen von `MENU`
entschärft, daher ist es von hier aus immer erlaubt. Der Befehl dauert 6 ms
(6 Frames bei 1 kHz). Der Bildschirm wird alle 25 ms neu gezeichnet, daher ist
die Zeile nach jeder Betätigung 350 ms lang gefüllt: weiß mit `SENT`, oder
bernsteinfarben mit `REFUSED`. `REFUSED` ist die Antwort, wenn der ESC scharf
ist. Das Öffnen von `MENU` entschärft, daher ist es nicht zu erwarten.

<img src="img/menu-beep-flash.png" width="240" alt="BEEP angenommen"> <img src="img/menu-beep-refused.png" width="240" alt="BEEP abgelehnt, weil der ESC scharf ist">

`UNSAVED` in der Zeile SETTINGS heißt, dass ein Wert vom Flash abweicht. Die
Schaltfläche zum Speichern liegt in SETTINGS.

<img src="img/menu-unsaved.png" width="240" alt="UNSAVED in der Zeile SETTINGS, Änderungen noch nicht im Flash">

`UPDATED` in der Zeile SETTINGS heißt, dass ein Firmware-Update die
gespeicherten Einstellungen verworfen hat: Jeder Wert, darunter Polzahl,
`MAX THROTTLE`, `ESC PIN`, KISS, `SERVO SUPPLY` und `CURRENT SENSE`, steht auf
seiner Voreinstellung. Prüfe sie in SETTINGS und halte `HOLD TO SAVE`; ein
gelungenes Speichern beendet die Markierung. Der Startbildschirm sagt dasselbe
beim Einschalten (siehe [Erste Schritte](Erste-Schritte#einschalten)).

<img src="img/menu-updated.png" width="240" alt="UPDATED in der Zeile SETTINGS, nachdem ein Update die gespeicherten Einstellungen verworfen hat">

---

## SETTINGS

<img src="img/settings.png" width="240" alt="Der SETTINGS-Bildschirm">

Motor- und Verkabelungseinstellungen, Anzeigeeinstellungen, die Platine,
Diagnose und die Schaltfläche, die alle Einstellungen ins Flash schreibt. Eine
Liste in Abschnitten, die sich mit dem Finger verschieben lässt.

| Abschnitt | Zeile | Bedeutung |
|---|---|---|
| ESC TESTER | **MOTOR POLES** | 2–28, nur gerade Werte. Die Drehzahlanzeige teilt die eRPM durch die Hälfte davon |
| ESC TESTER | **MAX THROTTLE** | Die Gas-Obergrenze, `5%` bis `100%` in Schritten von 5 % |
| WIRING | **ESC PIN** | GPIO (General Purpose Input/Output, frei verwendbarer Pin) für das Signalkabel. Angeboten werden nur die zwei freien GPIOs der Platine, GP0 und GP1. Ein Schritt legt DShot sofort auf den neuen Pin; siehe die Hinweiszeile unten |
| WIRING | **DSHOT KBAUD** | 150 / 300 / 600 / 1200. Herunterstellen, wenn die `LINK`-Fehlerrate steigt |
| WIRING | **KISS TELEM** | Ob auf dem optionalen dritten Telemetriekabel empfangen wird |
| WIRING | **KISS PIN** | GPIO für dieses Kabel. Überspringt den ESC-Pin, sodass beide nie zusammenfallen. Zeigt `--` und reagiert nicht, solange KISS TELEM aus ist |
| SERVO SUPPLY | **SERVO SUPPLY** | `NONE`, `GP28` oder `GP29`: ob ein DC-DC-Wandler (Gleichspannungswandler) für die Versorgung eines zu programmierenden Servos einen Enable-Eingang hat und an welchem Pin: GP28, J4 Pin 11, oder GP29, J4 Pin 12. Standard ist `NONE`. - und + schalten durch die drei Werte. Mit `GP28` oder `GP29` schalten KST SERVO und HITEC SERVO die Versorgung des Servos |
| SERVO SUPPLY | **CURRENT SENSE** | `NONE` oder `INA219`: ob ein INA219-Strommessmodul an J3 hängt. Standard ist `NONE`. Ein Tippen wechselt zwischen beiden. Mit `INA219` steht unter dem Namen `FOUND`, `NOT FOUND` oder, für einen anderen Chip an der Adresse des Sensors, `OTHER PART`. Ein INA219, den eine andere Firmware anders konfiguriert hat, zeigt ebenfalls `OTHER PART`: das Modul kurz stromlos machen. Servotester, KST SERVO und HITEC SERVO zeigen dann den Strom der Versorgung |
| DISPLAY | **CONTRAST** | `NORMAL` oder `HIGH`. Hoher Kontrast ist für Tageslicht |
| DISPLAY | **BACKLIGHT** | 16–255, in Schritten von 32 |
| BOARD | **BUILT FOR** | Nur Anzeige. Die Platine, für die die Firmware gebaut ist, `RP2350-TOUCH-LCD-2.8` |
| BOARD | **VERSION** | Nur Anzeige. Die Firmware-Version, zum Beispiel `0.1.0` |
| DIAGNOSTICS | **LAST RESTART** | Nur Anzeige. Warum die Platine zuletzt gestartet ist: `POWER-ON` (eingeschaltet), `BROWN-OUT` (die Versorgung ist zu weit abgesunken), `RUN PIN` (der Reset-Eingang), `WATCHDOG`, `UPDATE` (der Neustart, nachdem eine neue Firmware auf die Platine kopiert wurde), `REBOOT` (ein Neustart über den Watchdog, den ein anderes Programm angefordert hat, zum Beispiel picotool), `DEBUGGER`, `GLITCH` (ein kurzer Einbruch der Versorgung), `POWER-DOWN` oder `UNKNOWN`. Bernstein bei `BROWN-OUT`, `GLITCH` und `WATCHDOG` |
| DIAGNOSTICS | **UPTIME** | Nur Anzeige. Zeit seit dem Start der Platine, Stunden, Minuten und Sekunden, zum Beispiel `1:02:03`. Wird einmal pro Sekunde neu gezeichnet, solange die Zeile sichtbar ist |

`MOTOR POLES` und `MAX THROTTLE` stehen ganz oben, du musst dafür nicht
verschieben. Die Schaltflächen `-` und `+` messen 44 × 44 px. Gedrückt
halten wiederholt den Schritt.

**Verschieben.** Ziehe die Liste nach oben oder unten, an einer Stelle, die
keine Schaltfläche ist: ein Abschnittsname, der Name oder Wert einer Zeile oder
die Lücke zwischen Zeilen. Ein Zug, der auf `-`, `+` oder einem Umschalter
beginnt, verschiebt nie; er ändert den Wert. Eine Schaltfläche, die der
Listenrand abschneidet, nimmt eine Berührung nur an, solange mindestens 40 px
von ihr sichtbar sind.

Unter der Liste bleiben diese Elemente an ihrem Platz:

- **Die Warnung zum ESC-Pin**, bei y 230, nur solange der ESC-Pin nicht der
  ist, den der Startbildschirm beim Einschalten nannte: dessen Warnung für den
  neuen Pin in Bernstein, zum Beispiel `UNPLUG ANY SERVO FROM GP1 NOW`. DShot
  läuft ab dem Schritt auf diesem Pin, und ein dort verbliebenes Servo, etwa
  nach dem Servotester mit zwei Servos, bewegt sich. Die Liste endet dann
  12 px höher, bei y 227, und lässt sich 12 px weiter verschieben; ihre
  oberen Zeilen bleiben, wo sie sind.
- **LINK**: live, Pakete pro Sekunde und Fehlerrate. Grün heißt, es kommen
  Pakete an, der ESC-Pin stimmt also. Bernstein heißt, mehr als 5 % Fehler.
- Die Hinweiszeile: ein Text, der erste, der zutrifft, in dieser Reihenfolge:
  1. `SAVE FAILED - FLASH REFUSED THE WRITE` (rot) oder
     `NOT WHILE ARMED - DISARM TO SAVE` (rot), bis zum nächsten Speichern
     oder Verlassen von SETTINGS;
  2. `ADJUSTED TO THIS BOARD'S LIMITS` oder `KISS OFF: NEEDS A PIN OF ITS OWN`
     (Bernstein), bis zur nächsten Änderung;
  3. `DEFAULTS LOADED - RESTART TO UNDO` (Voreinstellungen geladen, ein
     Neustart macht es rückgängig; Bernstein) nach `RESET`, solange ein
     Neustart die gespeicherten Werte zurückbrächte, bis zur nächsten
     Änderung oder zu einem `HOLD TO SAVE`, gescheitert oder nicht;
  4. `UNSAVED - HOLD SAVE TO KEEP` (Bernstein) nach einer Änderung;
  5. `SAVED` (grün) nach einem Speichern, bis zur nächsten Änderung;
  6. `RESET BY UPDATE - CHECK AND SAVE` (Bernstein), nachdem ein Update die
     gespeicherten Einstellungen verworfen hat, bis ein Speichern gelingt;
  7. `SAVED SETTINGS IN USE` oder `COMPILED DEFAULTS IN USE`.

<img src="img/settings-pin.png" width="240" alt="SETTINGS, nachdem ESC PIN auf GP1 gewechselt hat: UNPLUG ANY SERVO FROM GP1 NOW über LINK, UNSAVED in der Hinweiszeile">

- `HOLD TO SAVE` (150 × 48 px) und `RESET` (70 × 48 px).

**Änderungen wirken sofort. Nur das Speichern braucht das Halten.** Eine neue
Bitrate baut den DShot-Treiber vor dem nächsten Frame neu auf. `HOLD TO SAVE`
1 s lang schreibt ins Flash. Ein Tippen auf `RESET` ersetzt die laufenden
Einstellungen durch die kompilierten Voreinstellungen, Polzahl und
Gas-Obergrenze eingeschlossen; das Flash bleibt unverändert. Wer den
Bildschirm verlässt, behält die Voreinstellungen in Gebrauch. Die
gespeicherten Werte lädt die Platine nur beim Start, ein Neustart ohne
`HOLD TO SAVE` kehrt also zu ihnen zurück.

**LAST RESTART.** Die Firmware liest den Grund einmal beim Start. Startet die
Platine bei drehendem Motor von selbst neu und zeigt danach `BROWN-OUT`, liegt
die Ursache bei der Versorgung. Die Firmware schaltet keinen Watchdog ein,
daher erscheint `WATCHDOG` mit dieser Firmware nicht. Laut dem Quelltext des
Boot-ROMs, nicht auf Hardware geprüft, zeigt der Neustart nach dem Kopieren
einer Firmware-Datei auf die Platine `UPDATE`. Nicht auf Hardware getestet:
Welchen Grund ein bestimmter Neustart auf der Platine zeigt, ist nicht
festgehalten.

<img src="img/settings-supply.png" width="240" alt="SETTINGS bis SERVO SUPPLY verschoben, auf GP29"> <img src="img/settings-scrolled.png" width="240" alt="SETTINGS bis zum Ende verschoben: BOARD und DIAGNOSTICS">

<img src="img/settings-sense.png" width="240" alt="SETTINGS: CURRENT SENSE auf INA219, FOUND">

<img src="img/settings-contrast.png" width="240" alt="SETTINGS mit hohem Kontrast"> <img src="img/tester-contrast.png" width="240" alt="Der Tester-Bildschirm mit hohem Kontrast">

*Hoher Kontrast gilt für jeden Bildschirm und setzt die
Hintergrundbeleuchtung auf 255, solange er an ist.*

---

## SD LOG

<img src="img/log-ready.png" width="240" alt="SD-LOG-Bildschirm, Karte bereit">

Aufzeichnungszustand und Zähler. `BACK` führt zu `MENU` zurück. Siehe
[SD-Aufzeichnung](SD-Aufzeichnung).

---

## AM32

<img src="img/am32-list.png" width="240" alt="AM32-Einstellungsliste">

Liest, ändert und schreibt die Einstellungen eines AM32-ESC über das
Signalkabel. `BACK` führt zu `MENU` zurück. Siehe
[AM32-Konfiguration](AM32-Konfiguration).

---

## SERVO

<img src="img/servo-safe.png" width="240" alt="Der Servobildschirm in SAFE">

Steuert ein oder zwei Servos mit Servoimpulsen auf dem ESC- und dem KISS-Pin
an. Er startet in SAFE; `HOLD TO START` 1 s lang gehalten startet die Impulse
in der Mitte, `STOP` wirkt beim Berühren. Sind vier Lesungen des
Touch-Controllers in Folge fehlgeschlagen, über 100 ms oder länger, hält
ein laufendes Servo an wie bei `STOP`, und die
Skala zeigt `TOUCH NOT ANSWERING`, bis er wieder antwortet. In SAFE mit
einem Servo zeigt die
Anzeigezeile `TWO SERVOS:` und `SET > SERVOS`: Dort wird das zweite Servo
eingeschaltet. Mit SERVO SUPPLY auf `GP28` oder `GP29` zeigt die zweite Zeile
der Leiste `GP28 SUPPLY STAYS OFF HERE.` in Bernstein: Der Servotester
schaltet den Wandler nie ein, das Servo braucht hier also eine eigene
Versorgung. `SET` öffnet die Einstellungen für Profil und Sweep. `BACK`
zeigt `UNPLUG THE SERVO.`, bevor der ESC-Treiber die Pins zurückbekommt;
`RESTART DSHOT` führt danach zu `MENU`. Mit CURRENT SENSE auf `INA219`
zeigt die Zeile unter der Skala statt des Hinweises Strom und Spannung der
Servoversorgung. Siehe [Servotester](Servotester).

---

## KST SERVO

<img src="img/kst-list.png" width="240" alt="Die KST-Feldliste">

Liest und schreibt die Einstellungen eines KST- oder Chaservo-Servos über das
Signalkabel am ESC-Pin. Der Bildschirm sucht das Servo, während du es
einschaltest, liest seine 32 Register und listet 27 Felder zum Verstellen.
`HOLD TO WRITE` 1 s lang gehalten öffnet die Schreibseite, ein zweites Halten
von 1 s schreibt, zuvor mit einer Sicherung auf der SD-Karte. `BACK` öffnet
die Verlassenseite, die dich auffordert, das Servo auszuschalten; `LEAVE`
zeigt dann `UNPLUG THE SERVO.`, und `RESTART DSHOT` führt zu `MENU`. Mit
CURRENT SENSE auf `INA219` zeigt der Streifen den Strom hinter dem Zustand der
Versorgung, `SUPPLY ON 0.08 A`. Siehe [KST-Programmierung](KST-Programmierung).

---

## HITEC SERVO

<img src="img/hitec-list.png" width="240" alt="Die Hitec-Feldliste">

Liest und schreibt die Einstellungen eines Hitec-Servos der D-Serie oder der
klassischen Familie über das Signalkabel am ESC-Pin. Der Bildschirm öffnet mit
der Familienauswahl: `D SERIES` wirkt, `CLASSIC` wirkt mit SERVO SUPPLY auf
`GP28` oder `GP29`; unter `NONE` ist es abgeblendet, mit
`NEEDS THE CONVERTER OF WIRING PART C` und
`AND SETTINGS > SERVO SUPPLY GP28/29.`: Zuerst kommt der Wandler aus Teil C
der [Verdrahtung](Verdrahtung), dann die Einstellung. Unter `NONE` steht bei
D SERIES `POWER THE SERVO UP BEFORE THE TAP.`. Der Bildschirm tritt in das
Servo ein und liest es. Ein Servo der D-Serie
liefert 21 Register und 20 Parameter, davon 11 verstellbar. An der Leitung
hängt ein Servo, ohne Pull-up-Widerstand. `HOLD TO WRITE` 1 s lang gehalten
öffnet die Schreibseite, ein zweites Halten von 1 s schreibt, zuvor mit einer
Sicherung auf der SD-Karte. `BACK` öffnet
die Verlassenseite; `LEAVE` zeigt dann `UNPLUG THE SERVO.`, und
`RESTART DSHOT` führt zu `MENU`. Mit CURRENT SENSE auf `INA219` zeigt der
Streifen den Strom hinter `SUPPLY ON`. Nicht auf Hardware getestet. Siehe
[Hitec-Programmierung](Hitec-Programmierung).

---

## Was jeder Bildschirm gleich macht

- **Ein Tippen wirkt beim Loslassen, nicht beim Berühren.** Rutschst du vor
  dem Loslassen von der Schaltfläche, wird nichts ausgelöst. Die Ausnahmen
  wirken beim Berühren: `DISARM` sowie `STOP` auf dem Servobildschirm und auf
  seinem SET-Bildschirm.
- **Eine gedrückte Schaltfläche zeigt einen Druckrahmen.** Zeigt ein
  Bedienelement keinen Rahmen, hat es die Berührung nicht erkannt.
- **`-` / `+` wiederholen beim Halten und werden schneller.** Ein Schritt beim
  Drücken; nach 400 ms ein Schritt alle 120 ms; nach 1,2 s alle 60 ms; nach
  2,5 s alle 25 ms.
- **Schreiben ins Flash braucht 1 s Halten** mit Fortschrittsbalken:
  Einstellungen auf der Platine speichern und Einstellungen in einen ESC
  schreiben. Ein Servo starten und ein riskantes Servoprofil übernehmen
  brauchen dasselbe Halten von 1 s.
