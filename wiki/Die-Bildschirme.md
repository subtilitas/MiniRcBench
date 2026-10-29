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
   │                    ├── SD LOG       ──BACK──> MENU
   │                    └── SETTINGS     ──BACK──> MENU
   └──── BACK auf MENU ─┘
```

`BEEP THE ESC` ist eine Zeile von `MENU`, die etwas auslöst und keinen
Bildschirm öffnet.

**BACK sitzt oben rechts** im Kopfbereich, auf jedem Bildschirm außer dem
Tester. Der Kopfbereich ist 36 px hoch, `BACK` misst 64 × 32 px. Der Tester ist
die Wurzel und hat keinen Kopfbereich.

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
  (siehe [SD-Aufzeichnung](SD-Aufzeichnung));
- die Spannung des platineneigenen Akkus, gemessen über den
  200k/100k-Teiler. Was sie bei reiner USB-Versorgung (Universal Serial Bus)
  anzeigt, ist nicht gemessen. In den letzten 5 s vor dem Entschärfen durch
  die Leerlaufsperre ersetzt `IDLE ns` in Bernstein die Spannung.

**Drehzahl.** Mechanische Drehzahl (RPM, Umdrehungen pro Minute), berechnet
aus der vom ESC (Electronic Speed Controller, Motorregler) gemeldeten eRPM
(elektrische Drehzahl) und der Polzahl. Der `ERPM`-Wert und die Polzahl stehen
darunter. Grüne Ziffern: eRPM ist in den letzten 500 ms eingetroffen.
Dunkelrote Null: nicht, und die Zeile zeigt `NO TELEMETRY`.

**Sechs Kacheln.** Spannung, Strom, ESC-Temperatur, Stress, ESC-Status und
Verbindungsqualität. Siehe [Telemetrie](Telemetrie).

**Gas.** Die Anzeige, die Obergrenze und der befohlene Prozentwert. Der
Prozentwert bezieht sich auf den vollen DShot-Bereich (0–2000 = 0–100 %): Bei
20 % Obergrenze zeigt ein Zug über die volle Breite `20%`. Im entschärften
Zustand zeigt der Balken `ARM TO ENABLE`.

**Schaltflächen.** `HOLD TO ARM` / `DISARM`, `HOLD` (haltendes Gas), `MENU`.

---

## MENU

<img src="img/menu.png" width="240" alt="Der MENU-Bildschirm">

Erreichbar über die Schaltfläche `MENU` des Testers. Das Öffnen **entschärft**,
setzt das Gas auf 0 und schaltet `HOLD` aus.

Unter dem Titel zeigt eine Leiste den ESC-Pin und die DShot-Rate, zum Beispiel
`GP0  600 KBIT/S`, sowie das Feld **EDT ON / EDT OFF**. Das Feld ist nur eine
Anzeige. Es ist grün, solange Extended-DShot-Telemetry-Frames eintreffen, sonst
rot. Es gibt keine Einschalt-Schaltfläche. Die Firmware schickt die Freigabe an
jeden ESC, der zu antworten beginnt, und wiederholt sie jede Sekunde, solange
eRPM eintrifft und EDT nicht. Der ESC bestätigt die Freigabe nicht, daher zeigt
das Feld *empfangene Frames*, nicht gesendete Befehle.

Unter der Leiste steht eine Liste mit sieben Zeilen von 40 px Höhe und 42 px
Abstand. Jede Zeile zeigt links ihren Namen und rechts ihren Zustand. `BACK`
führt zum Tester zurück.

**Verschieben.** Sechs Zeilen passen auf den Bildschirm. Von der siebten,
SETTINGS, sind zuerst nur 6 px sichtbar. Ziehe MENU um 36 px nach oben, um
SETTINGS ganz zu sehen; erst dann nimmt die Zeile ein Tippen an. Ein 3 px
breiter Rollbalken am rechten Rand zeigt die Lage.

| Zeile | Zustand rechts | Ein Tippen |
|---|---|---|
| **BEEP THE ESC** | `DISARMED`; 350 ms nach einem Tippen `SENT` oder `REFUSED` | Sendet den DShot-Beacon-Befehl, der Motor piept. Damit erkennst du, welcher ESC angeschlossen ist |
| **AM32 CONFIG** | der ESC-Pin, `GP0`; `PINS BUSY` | Öffnet den AM32-Editor |
| **SERVO TESTER** | die beiden Signalpins, `GP0+1`; `PINS BUSY` | Öffnet den Servotester |
| **KST SERVO** | der ESC-Pin, `GP0`; `PINS BUSY` | Öffnet den KST-Programmierer. Siehe [KST-Programmierung](KST-Programmierung) |
| **HITEC SERVO** | der ESC-Pin, `GP0`; `PINS BUSY` | Öffnet den Hitec-Programmierer. Siehe [Hitec-Programmierung](Hitec-Programmierung) |
| **SD LOG** | `NO CARD`, `READY`, `RECORDING`, `CARD ERROR`, oder `USB DRIVE`, solange ein Computer die Karte hält | Öffnet den Aufzeichnungs-Bildschirm |
| **SETTINGS** | `UNSAVED`, solange ein Wert vom Flash abweicht, sonst nichts; `PINS BUSY`, solange die Pins noch zurückgegeben werden, und öffnet sich nicht | Öffnet SETTINGS |

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

---

## SETTINGS

<img src="img/settings.png" width="240" alt="Der SETTINGS-Bildschirm">

Motor- und Verkabelungseinstellungen, Anzeigeeinstellungen und die
Schaltfläche, die alle Einstellungen ins Flash schreibt. Eine Liste in
Abschnitten, die sich mit dem Finger verschieben lässt.

| Abschnitt | Zeile | Bedeutung |
|---|---|---|
| ESC TESTER | **MOTOR POLES** | 2–28, nur gerade Werte. Die Drehzahlanzeige teilt die eRPM durch die Hälfte davon |
| ESC TESTER | **MAX THROTTLE** | Die Gas-Obergrenze, `5%` bis `100%` in Schritten von 5 % |
| WIRING | **ESC PIN** | GPIO (General Purpose Input/Output, frei verwendbarer Pin) für das Signalkabel. Angeboten werden nur die zwei freien GPIOs der Platine, GP0 und GP1 |
| WIRING | **DSHOT KBAUD** | 150 / 300 / 600 / 1200. Herunterstellen, wenn die `LINK`-Fehlerrate steigt |
| WIRING | **KISS TELEM** | Ob auf dem optionalen dritten Telemetriekabel empfangen wird |
| WIRING | **KISS PIN** | GPIO für dieses Kabel. Überspringt den ESC-Pin, sodass beide nie zusammenfallen. Zeigt `--` und reagiert nicht, solange KISS TELEM aus ist |
| SERVO SUPPLY | **SERVO SUPPLY** | `NONE`, `GP28` oder `GP29`: ob ein DC-DC-Wandler (Gleichspannungswandler) für die Versorgung eines zu programmierenden Servos einen Enable-Eingang hat und an welchem Pin: GP28, J4 Pin 11, oder GP29, J4 Pin 12. Standard ist `NONE`. - und + schalten durch die drei Werte. Mit `GP28` oder `GP29` schalten KST SERVO und HITEC SERVO die Versorgung des Servos |
| DISPLAY | **CONTRAST** | `NORMAL` oder `HIGH`. Hoher Kontrast ist für Tageslicht |
| DISPLAY | **BACKLIGHT** | 16–255, in Schritten von 32 |
| BOARD | **BUILT FOR** | Nur Anzeige. Die Platine, für die die Firmware gebaut ist, `RP2350-TOUCH-LCD-2.8` |

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

- **LINK**: live, Pakete pro Sekunde und Fehlerrate. Grün heißt, es kommen
  Pakete an, der ESC-Pin stimmt also. Bernstein heißt, mehr als 5 % Fehler.
- Die Hinweiszeile, die die verwendeten Werte nennt: `SAVED SETTINGS IN USE`
  oder `COMPILED DEFAULTS IN USE`. Nach einer Änderung zeigt sie
  `UNSAVED - HOLD SAVE TO KEEP`.
- `HOLD TO SAVE` (150 × 48 px) und `RESET` (70 × 48 px).

**Änderungen wirken sofort. Nur das Speichern braucht das Halten.** Eine neue
Bitrate baut den DShot-Treiber vor dem nächsten Frame neu auf. `HOLD TO SAVE`
1 s lang schreibt ins Flash. `RESET` lädt die kompilierten Voreinstellungen,
Polzahl und Gas-Obergrenze eingeschlossen, nur in die laufenden Einstellungen.
Wer ohne Speichern weggeht, lässt das Flash unverändert.

<img src="img/settings-supply.png" width="240" alt="SETTINGS bis SERVO SUPPLY verschoben, auf GP29"> <img src="img/settings-scrolled.png" width="240" alt="SETTINGS bis zum Ende verschoben: DISPLAY und BOARD">

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
in der Mitte, `STOP` wirkt beim Berühren. `SET` öffnet die Einstellungen für
Profil und Sweep. `BACK` zeigt `UNPLUG THE SERVO.`, bevor der ESC-Treiber die
Pins zurückbekommt; `RESTART DSHOT` führt danach zu `MENU`. Siehe
[Servotester](Servotester).

---

## KST SERVO

<img src="img/kst-list.png" width="240" alt="Die KST-Feldliste">

Liest und schreibt die Einstellungen eines KST- oder Chaservo-Servos über das
Signalkabel am ESC-Pin. Der Bildschirm sucht das Servo, während du es
einschaltest, liest seine 32 Register und listet 27 Felder zum Verstellen.
`HOLD TO WRITE` 1 s lang gehalten öffnet die Schreibseite, ein zweites Halten
von 1 s schreibt, zuvor mit einer Sicherung auf der SD-Karte. `BACK` öffnet
die Verlassenseite, die dich auffordert, das Servo auszuschalten; `LEAVE`
zeigt dann `UNPLUG THE SERVO.`, und `RESTART DSHOT` führt zu `MENU`. Siehe
[KST-Programmierung](KST-Programmierung).

---

## HITEC SERVO

<img src="img/hitec-list.png" width="240" alt="Die Hitec-Feldliste">

Liest und schreibt die Einstellungen eines Hitec-Servos der D-Serie über das
Signalkabel am ESC-Pin. Der Bildschirm öffnet mit der Familienauswahl:
`D SERIES` wirkt, `CLASSIC` ist mit `LINE LEVEL NOT MEASURED` abgeblendet. Er
tritt in das Servo ein, liest seine 21 Register und listet 20 Parameter, davon 11
verstellbar. An der Leitung hängt ein Servo, ohne Pull-up-Widerstand.
`HOLD TO WRITE` 1 s lang gehalten öffnet die Schreibseite, ein zweites Halten
von 1 s schreibt, zuvor mit einer Sicherung auf der SD-Karte. `BACK` öffnet
die Verlassenseite; `LEAVE` zeigt dann `UNPLUG THE SERVO.`, und
`RESTART DSHOT` führt zu `MENU`. Nicht auf Hardware getestet. Siehe
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
