[English](The-Screens) | **Deutsch**

# Die Bildschirme

Die Firmware hat sechzehn Bildschirme. `MENU` ist die Wurzel und der erste
Bildschirm nach dem Startbildschirm: sechs Zeilen, ohne `BACK`. Ihre erste
Zeile, `ESC TESTER`, öffnet den Tester, dessen Schaltfläche `MENU` zu
`MENU` zurückführt. Die nächsten drei Zeilen öffnen je eine Auswahl, eine
Liste der Werkzeuge für eine Geräteart: ESC PROGRAM die ESC-Werkzeuge, SERVO
die Servo-Werkzeuge, RECEIVER die Werkzeuge für Empfängersignale. Jedes
Werkzeug liegt zwei Tippen von `MENU` entfernt. Ein Werkzeug verlässt du mit
`BACK`, das zu der Auswahl zurückführt, die es geöffnet hat, und das `BACK`
der Auswahl führt zu `MENU` zurück. SETUP öffnet auch über
die Polzahl und die Obergrenze des Testers; `BACK` führt dann zum Tester
zurück. DShot läuft nur, solange `ESC TESTER` oder `ESC COMMANDS` offen ist,
oder SETUP über dem Tester. Überall sonst, `MENU` eingeschlossen und vom
Einschalten an, liegt der ESC-Signalpin auf Low, und nichts wird gesendet.

```
Startbildschirm ──3 s──> MENU (die Wurzel, kein BACK)
MENU ──┬── ESC TESTER ──> Tester ──MENU──> MENU
       │                    └── 14 POLES oder MAX 20% (entschärft) ──> SETUP ──BACK──> Tester
       ├── ESC PROGRAM > ──> ESC PROGRAM ──BACK──> MENU
       │    ├── ESC COMMANDS ──BACK──> ESC PROGRAM
       │    ├── ESC SETTINGS ──> ESC SETTINGS ──BACK──> ESC PROGRAM
       │    │   (eine Bluejay- oder BLHeli_S-Liste: BACK──> LEAVE gesendet──> ESC PROGRAM)
       │    │   (sonst nach einer Antwort von Bluejay oder BLHeli_S: BACK──> Verlassenseite ──LEAVE, ESC PROGRAM──> ESC PROGRAM)
       │    └── USB BRIDGE ──> USB BRIDGE ──BACK──> ESC PROGRAM
       │        (sobald die Leitung gebunden ist: BACK──> Verlassenseite ──ESC PROGRAM──> ESC PROGRAM)
       ├── SERVO > ──> SERVO ──BACK──> MENU
       │    ├── SERVO TESTER ──BACK──> Verlassenseite ──SERVO──> SERVO
       │    ├── ANGLE METER ──BACK──> SERVO   (ANGLE SET, FLIP CAL: BACK eine Ebene hoch)
       │    ├── KST / CHASERVO ──> KST SERVO   ──BACK──> Verlassenseite ──LEAVE, SERVO──> SERVO
       │    ├── HITEC D SERIES ──> HITEC SERVO ──BACK──> Verlassenseite ──LEAVE, SERVO──> SERVO
       │    └── HITEC CLASSIC  ──> HITEC SERVO, wie D SERIES
       │   (ohne Servo im Programmiermodus öffnet die Verlassenseite bei UNPLUG THE SERVO.)
       │   (ESC HEX, ESC WRITE, SET: BACK eine Ebene hoch)
       ├── RECEIVER > ──> RECEIVER ──BACK──> MENU
       │    ├── MONITOR ──> S.BUS MONITOR ──BACK──> Verlassenseite ──RECEIVER──> RECEIVER
       │    └── OUTPUT  ──> S.BUS OUTPUT  ──BACK──> Verlassenseite ──RECEIVER──> RECEIVER   (SET: BACK eine Ebene hoch)
       ├── LOGS       ──BACK──> MENU
       └── SETUP     ──BACK──> MENU
```

**BACK sitzt oben rechts** im Kopfbereich, auf jedem Bildschirm außer `MENU`
und dem Tester. Der Kopfbereich ist 36 px hoch, `BACK` misst 64 × 32 px.
`MENU` ist die Wurzel: Sein Kopfbereich zeigt `MENU` und kein `BACK`, denn
dahinter liegt nichts. Der Tester hat keinen Kopfbereich; seine Schaltfläche
`MENU` ist sein Weg zurück. Auf der Verlassenseite von SERVO TESTER,
KST SERVO, HITEC SERVO, S.BUS MONITOR und ESC SETTINGS ist `BACK` deaktiviert
gezeichnet, als Umriss mit grauer Beschriftung, und ein Tippen darauf bewirkt
nichts: Jeder Weg weiter ist eine beschriftete Taste, `STAY`, `NEXT SERVO`,
`BACK TO MONITOR` oder der Titel der Auswahl. Dasselbe gilt auf `ESC
WRITE`, `KST WRITE` und `HITEC WRITE`, solange ein Schreiben läuft oder auf die
SD-Karte wartet: Kein `BACK` lässt ein Gerät halb geschrieben zurück.

**Die Verlassenseite** ist eine Seite vom letzten Schritt an einem Servo,
einem Empfänger oder einem ESC, dessen Bluejay- oder BLHeli_S-Bootloader
geantwortet hat, zurück zur Auswahl, mit dem Signalpin auf Low. Sie
nummeriert ihre Schritte; der aktuelle ist bernsteinfarben, mit einem Balken
am linken Rand. Vor `LEAVE` heißen die Tasten `STAY`, das zum Werkzeug
zurückführt, und `LEAVE`. Nach `LEAVE` ist der nächste Schritt aktuell, und
die breite Taste trägt an der Stelle von `LEAVE` den Titel der Auswahl,
`SERVO`, `RECEIVER` oder `ESC PROGRAM`; der letzte Schritt lautet `BACK TO`
und dieser Titel. Sie und die linke Taste, `NEXT SERVO` oder
`BACK TO MONITOR` (keine auf ESC SETTINGS und USB BRIDGE), sind deaktiviert
gezeichnet, bis der Pin freigegeben ist; der Titel der Auswahl wartet danach
noch 1 s, damit ein Doppeltippen auf `LEAVE` den Claim nie beendet. Ein
Druck, der während der Wartezeit begann, bewirkt beim Loslassen nichts. Die
letzte Taste hält den Pin auf Low und startet kein DShot: Ein Servo, das an
SIG bleibt, trifft beim nächsten `ESC TESTER` oder `ESC COMMANDS` auf DShot,
und der Schritt zum Abziehen nennt das.

<img src="img/leave-kst.png" width="240" alt="Die KST-Verlassenseite vor LEAVE: Schritt 1 POWER THE SERVO OFF, THEN LEAVE., STAY und LEAVE"> <img src="img/leave-kst-released.png" width="240" alt="Die KST-Verlassenseite nach LEAVE: Schritt 2 aktuell, NEXT SERVO aktiv, SERVO noch grau">

**BACK nennt, was es verwerfen würde.** Auf der Liste von ESC SETTINGS mit
ungeschriebenen Änderungen und auf SERVO SET mit einer Änderung, die auf
`HOLD TO APPLY` wartet, bleibt das erste `BACK` stehen und setzt zwei
bernsteinfarbene Zeilen auf den Streifen, zum Beispiel
`2 CHANGES NOT WRITTEN.` und `BACK AGAIN DISCARDS THEM.`; ein zweites `BACK`
verwirft die Änderungen, jede andere Berührung behält sie.

**Die Farben folgen einer Tabelle von Rollen.** Grün heißt sicher oder
erledigt: `SAFE`, `EDT ON`, `WRITE VERIFIED`, `SAVED`. Hellgrün ist ein
laufender Messwert: die Drehzahl, die Gasfüllung, der Servoimpuls, der
Fortschrittsbalken beim Halten. Rot heißt Ausgang an oder ein Fehler: `ARMED`,
`RUNNING`, `STOP`, die Halte-Schaltfläche eines Werkzeugs, solange sie gehalten
wird, `REC`, ein Schreiben mit unklarem Ausgang. Bernstein ist eine Warnung oder
der Schritt, der jetzt dran ist. Magenta ist ein Alarm, etwa `STALLED`. Blau
ist eine Einstellung, die an bleibt: `LATCH`, ein eingeschalteter Schalter,
`PROG MODE`, Servo B. Cyan ist die Akzentfarbe, wie bei rcbench, und hat vier
Verwendungen. Es ist der Weg zurück: `BACK` in einem Kopfbereich und die
Schaltfläche `MENU` des Testers sind cyan mit dunkler Beschriftung, und ein
`BACK`, das nichts auslöst, ist deaktiviert gezeichnet, nicht cyan. Es ist
das Zeichen `>` an einer Zeile von `MENU`, die eine Auswahl öffnet. Es ist
eine Einstellung der Werkbank, als cyanfarbener Text: die Werte und die
Abschnittsnamen von SETUP, die Werte von SERVO SET und OUTPUT SET, LENGTH
auf ANGLE SET. Und es ist ein geänderter Wert, der noch nicht
geschrieben oder übernommen ist: dunkler Text auf einer cyanfarbenen Fläche,
mit einem cyanfarbenen Balken am linken Rand seiner Zeile. Ein Wert, der aus
einem ESC oder einem Servo gelesen wurde, steht in der Textfarbe. Beschriftungen, Überschriften und die Schaltflächen, die etwas öffnen
oder auslösen, stehen in der Textfarbe oder in Grau. Eine Schaltfläche hat
abgerundete Ecken. Ein Feld auf der Leiste unter einem Titel ist eckig, damit
es nicht wie eine Schaltfläche aussieht; das Scharf-Feld des Testers und sein
Feld `SDERR` sind ebenfalls eckig. Nicht jede Verwendung folgt der Tabelle:
Hellgrün markiert auch Zustände wie `READY` auf LOGS und die über USB
bereitgestellte Karte, und viele Fehler stehen in Bernstein oder in der
Textfarbe, zum Beispiel `CARD ERROR` auf LOGS und `WRITE FAILED` bei AM32.

**Das normale Farbschema übernimmt seine Farben von rcbench,** der größeren
Werkbank desselben Besitzers: ein fast schwarzer Bildschirm mit einem
Blaustich und blaugraue Neutraltöne. Eine Beschriftung auf einer farbigen
Füllung, etwa `ARMED`, `STOP` oder `PROG MODE`, ist fast schwarz. Ein
Kopfbereich endet in einer dünnen Linie. Ein Hinweis wie `SWIPE = COARSE`
steht in der gedämpften Farbe der Beschriftungen; eine deaktivierte
Beschriftung und ein nur lesbarer Wert stehen in der blasseren Farbe der
Rahmen. Ein Feld auf der Leiste, das keinen Zustand meldet, `NO ESC`,
`PORT CLOSED` oder `MOVING`, ist ein Rahmen mit gedämpfter Beschriftung, ohne
Füllung. `-` und `+` unter einer Liste und `REPAIR` bei Hitec sehen aus wie
jede andere Schaltfläche, die etwas auslöst. Wo diese Seiten „grau“ sagen,
ist einer der beiden blaugrauen Neutraltöne gemeint. Die Farben sind
berechnet und nicht auf dem Display gesehen: Wie das Display sie zeigt, ist
nicht gemessen. Hoher Kontrast behält seine eigenen Farben. Rahmen und
Beschriftungen sind dort fast schwarz; deshalb hat eine Schaltfläche, die
nichts auslöst, dort einen gestrichelten Rahmen mit ganzen, abgerundeten
Ecken und ein Schieberegler, der nichts auslöst, einen hohlen Griff. Cyan
ist dort ein dunkles Cyan: `BACK` hat eine weiße Beschriftung, und ein
geänderter Wert ist weißer Text auf einer dunkelcyanfarbenen Fläche.

---

## Der Tester

<img src="img/tester-armed.png" width="240" alt="Tester-Bildschirm">

Die erste Zeile von `MENU`, `ESC TESTER`, öffnet den Tester. Er öffnet
**entschärft**, mit Gas 0 und `LATCH` aus, jedes Mal. Er hat keinen
Kopfbereich: Seine Schaltfläche `MENU` unten rechts führt zu `MENU` zurück
und entschärft vorher, setzt das Gas auf 0 und schaltet `LATCH` aus.
Solange die Signalpins belegt sind, ist die Zeile `ESC TESTER` als Umriss
gezeichnet, und ein Tippen darauf öffnet nichts.

DShot startet, wenn der Tester öffnet: Der ESC hört die EDT-Freigabe, dann
Gas 0, und `HOLD TO ARM` wartet 1,5 s nach dem Start von DShot, damit der ESC
DShot gesehen hat, bevor er scharf werden kann. Solange ist `HOLD TO ARM` als
Umriss gezeichnet, und der Balken zeigt `DSHOT STARTING: WAIT 1.5 S` in
Bernstein; ein Druck in dieser Zeit bewirkt nichts, also loslassen und erneut
drücken, sobald die Schaltfläche bedienbar ist. Frühestens 2,5 s nach dem
Öffnen ist der Tester scharf. Fehlt der Platine die freie Hardware, um DShot
zu senden (zwei freie State Machines eines PIO-Blocks, der programmierbaren
Ein-/Ausgabe des Chips; `DSERR` in der Statusleiste), zeigt der Balken
`DSHOT COULD NOT START` in Bernstein, und `HOLD TO ARM` bleibt ein Umriss.
`MENU` beendet DShot: Der ESC hört mindestens 250 ms nach dem letzten Gas
noch Gas 0, dann nichts, und der Pin liegt auf Low.

**Obere Zeile.** Von links nach rechts:

- das Scharf-Feld: `SAFE` (grün) oder `ARMED` (rot);
- `DSHOT` und die DShot-Bitrate in kbit/s, zum Beispiel `DSHOT600`, wie unter
  SETUP → DSHOT eingestellt, gedämpft, und in der Textfarbe, sobald eine
  EDT-Freigabe (Extended DShot Telemetry) gesendet wurde. `DSERR`
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
  plausibel, steht er in der normalen Textfarbe, ab 2,55 V wieder rot.
  Solange er rot ist, schreiben KST SERVO, HITEC SERVO und ESC SETTINGS nichts
  und melden `BOARD VOLTAGE IS RED: NOT WRITTEN.`; Bernstein verweigert
  nichts, und kein Scharfschalten wird deswegen verweigert. In den letzten
  5 s vor dem Entschärfen durch die Leerlaufsperre ersetzt `IDLE ns` in
  Bernstein die Spannung und `BOARD`.

**Drehzahl.** Mechanische Drehzahl (RPM, Umdrehungen pro Minute), berechnet
aus der vom ESC (Electronic Speed Controller, Motorregler) gemeldeten eRPM
(elektrische Drehzahl) und der Polzahl. Der `ERPM`-Wert und die Polzahl, zum
Beispiel `14 POLES`, stehen darunter. Grüne Ziffern: eRPM ist in den letzten
500 ms eingetroffen.
Dunkelrote Null: nicht, und die Zeile zeigt `NO TELEMETRY`. Die Zeile
darunter, rechts, ist der Wisch-Hinweis: `SWIPE = THROTTLE`, oder
`SWIPE ACTIVE`, solange ein Wischer das Gas bewegt.

**RPM/V und die kV-Schätzung.** Scharf zeigt die `ERPM`-Zeile zusätzlich
`RPM/V`, die Drehzahl geteilt durch die Versorgungsspannung des ESC, den Wert
der Kachel `VOLTAGE`: `ERPM 156800  RPM/V 1400`. Die Zeile darunter zeigt an
Stelle der Polzahl `KV EST`: eine Schätzung des kV des Motors, seiner
Drehzahlkonstante in Umdrehungen pro Minute und Volt ohne Last, berechnet als
RPM × 2000 / (Spannung × Gas), mit dem gesendeten Gas, 0 bis 2000. Sie ist
eine Schätzung unter zwei Annahmen: Das Tastverhältnis (Duty) des ESC
entspricht dem Gas, und der Motor läuft ohne Propeller.

- `KV EST 1758`: die Schätzung, auf eine ganze Zahl gerundet.
- `KV EST 75%+`: Das Gas liegt unter 75 %, dort erscheint keine Schätzung.
  Bei der voreingestellten Obergrenze `MAX 20%` erscheint sie nie; für die
  Messung `MAX` in SETUP anheben, Propeller ab.
- `KV EST --`: kein Wert. Eine Eingabe fehlt, das Gas hat sich in der
  letzten Sekunde um mehr als 1 % bewegt, oder der Motor wird noch
  schneller.

Beide Werte brauchen eRPM aus den letzten 500 ms, eine Drehzahl über 0 und eine
Spannung über 0,5 V von KISS (aus den letzten 500 ms) oder EDT (aus den letzten
1000 ms); sonst zeigt die Zeile `RPM/V --` und die Zeile darunter `KV EST --`.
Die Schätzung braucht außerdem mindestens 75 % Gas, 1 s lang innerhalb von 1 %
(20 von 2000) um einen Wert, damit ein auf dem Balken ruhender Finger sie nicht
aufhält, einen Motor, der seine Drehzahl erreicht hat (die geglättete eRPM
innerhalb von 0,5 % der letzten Antwort; einmal erfüllt, gilt das, bis das Gas
das Band verlässt, eine Bedingung fehlt oder die geglättete eRPM mehr als 3 %
von der letzten Antwort abweicht oder die Antwort 0 ist, sodass verrauschte
Antworten die Zahl nicht flackern lassen und ein Abwürgen sofort `KV EST --`
zeigt), und keinen 3D-Hinweis; der 3D-Hinweis nimmt die
Zeile ein. Beide Werte enden bei 99999. Keiner bleibt nach
dem Entschärfen stehen: Entschärft zeigt die Zeile nur `ERPM` und die Zeile
darunter `14 POLES`.

Was die Schätzung nicht wissen kann:

- **Einen Propeller.** Mit Propeller liest sie um den Anteil der Last zu
  niedrig.
- **Das Tastverhältnis des ESC.** Bei 75 % unter AM32 2.22 ergibt die
  Entwurfsnotiz × 0,92 bis × 1,08 des Nenn-kV für die Einstellungen von SINE
  STARTUP und MIN DUTY, die sie durchrechnet, mit RUNNING LEVEL 10 und auf
  einem Target ohne Startup-Boost, aus dem Quelltext von AM32; nicht
  gemessen. Wie Bluejay, BLHeli_S und BLHeli_32 das Gas auf das
  Tastverhältnis abbilden, ist nicht geprüft.
- **Einen 3D-Modus, von dem der Tester nichts weiß.** Ein ESC, den ein
  anderes Werkzeug oder ein früheres Einschalten des Testers auf 3D gestellt
  hat, zeigt keinen 3D-Hinweis und gibt ohne Warnung einen falschen Wert:
  Unter AM32 läuft der Motor bei 75 % mit einem Tastverhältnis von etwa 50 %,
  und die Schätzung liest etwa × 0,67 des Nenn-kV.
- **Verluste.** Spannungsabfall im ESC und in den Leitungen, Reibung und
  Eisenverluste senken die Drehzahl; um wie viel, ist nicht bekannt.
- **Die Polzahl.** Ein falsches `MOTOR POLES` skaliert beide Werte: 12
  eingestellt bei einem Motor mit 14 Polen liest 16,7 % zu hoch.

Keiner der beiden Werte ist an einem Motor mit diesem Board gemessen.

<img src="img/tester-kv.png" width="240" alt="Der Tester scharf bei 79 %: ERPM 156800  RPM/V 1400 und KV EST 1758 unter der Drehzahl">

Hat ESC SETTINGS seit dem Einschalten einen ESC mit 3D MODE an gelesen, oder
einen Bluejay- oder BLHeli_S-ESC mit DIRECTION auf 3D oder 3D REVERSED, oder
3D MODE an geschrieben (vom ESC angenommen, bestätigt oder nicht) oder
DIRECTION auf 3D (bestätigt oder halb geschrieben), oder kam
`3D ON` auf [ESC COMMANDS](#esc-commands), zeigt die Zeile unter der
`ERPM`-Zeile in Bernstein `3D MODE ESC: 0-50% REVERSED AT 2X` (3D-ESC: 0 bis
50 % rückwärts mit doppeltem Gas), an Stelle der Polzahl und des
Wisch-Hinweises, scharf oder nicht; `NO TELEMETRY` bleibt auf seiner eigenen
Zeile darüber. Solange ein Wischer auf der Gasfläche greift, steht
`SWIPE ACTIVE` in Grün in der Zeile, und nach dem Loslassen kehrt der Hinweis
zurück; auch `TOUCH NOT ANSWERING` (Berührung, unten) nimmt die Zeile ein, bis
der Controller antwortet. Im 3D-Modus dreht 0 bis 50 % Gas des Testers den
Motor entgegen seiner eingestellten Richtung mit doppeltem Gas, 50 % ist also
volle Leistung rückwärts; über 50 % dreht er in der eingestellten Richtung,
von unten beginnend. Den Propeller abnehmen. Scharfschalten wird deswegen nicht
verweigert. Der Hinweis weiß nicht, ob der ESC am Pin gegen einen anderen
getauscht wurde; nur ein bestätigtes Schreiben oder ein Lesen, das die Liste
öffnet, mit 3D MODE aus oder DIRECTION auf NORMAL oder REVERSED, oder ein
Neustart nimmt ihn weg; `3D OFF` auf ESC COMMANDS, die Nur-Lese-Ansicht eines
unbekannten Layouts und eine Bluejay- oder BLHeli_S-Seite mit falscher
Einstellungsmarke lassen ihn.
Siehe [AM32-Konfiguration](AM32-Konfiguration#felder-mit-hinweis).

<img src="img/tester-3d.png" width="240" alt="Der Tester scharf, mit 3D MODE ESC: 0-50% REVERSED AT 2X in Bernstein unter der Drehzahl">

**Sechs Kacheln.** Spannung, Strom, ESC-Temperatur, Stress, ESC-Status und
Verbindungsqualität. Siehe [Telemetrie](Telemetrie).

**Gas.** Die Anzeige, die Obergrenze und der befohlene Prozentwert. Der
Prozentwert bezieht sich auf den vollen DShot-Bereich (0–2000 = 0–100 %): Bei
20 % Obergrenze zeigt ein Zug über die volle Breite `20%`. Die Obergrenze
steht als `MAX 20%` in der Textfarbe neben `THROTTLE`; mit `LATCH` an steht
dort `LATCHED`. Im entschärften Zustand zeigt der Balken `ARM TO ENABLE`.

**Polzahl und Obergrenze öffnen SETUP.** Solange der Tester entschärft ist,
sind `14 POLES` und `MAX 20%` unterstrichen, und ein Tippen auf eines davon
öffnet SETUP. Das Tippen entschärft wie die Schaltfläche `MENU`, und `BACK`
in SETUP führt zum Tester zurück. Scharf verschwindet die Unterstreichung,
`KV EST` nimmt den Platz der Polzahl ein, und ein Tippen darauf tut nichts: Die
Polzahl liegt in der Gasfläche und `MAX` reicht in den Greifbereich des Balkens,
also steuert der Druck dort das Gas. Solange ein anderes Werkzeug die
Signalpins hält, verschwindet die Unterstreichung ebenfalls und ein Tippen tut
nichts, so wie `MENU` SETUP dann gesperrt zeichnet; bei offenem Tester kann
kein Werkzeug sie nehmen. Solange der 3D-Hinweis die Zeile
einnimmt, öffnet nur `MAX` SETUP. Entschärft steht der Hinweis
`SWIPE = THROTTLE` in der gedämpften Farbe der Beschriftungen; scharf steht
er in der Textfarbe.

<img src="img/tester-disarmed.png" width="240" alt="Der Tester entschärft mit Telemetrie: SAFE in Grün, 14 POLES und MAX 20% unterstrichen, SWIPE = THROTTLE gedämpft, ARM TO ENABLE im Balken">

**Schaltflächen.** `HOLD TO ARM` / `DISARM`, `LATCH` (das Gas bleibt, wo der
Finger es loslässt; blau, solange an), `MENU`.

Solange ein Computer die SD-Karte über USB hält, ist Scharfschalten gesperrt:
`HOLD TO ARM` ist nur als Umriss gezeichnet, und der Balken zeigt
`EJECT THE USB DRIVE TO ARM` in Bernstein. Wirf das Laufwerk am Computer aus
oder drücke `EJECT` in LOGS, dann ist die Schaltfläche wieder bedienbar
(siehe [SD-Aufzeichnung](SD-Aufzeichnung)). Ebenso gesperrt ist
Scharfschalten 1,5 s lang nach dem Start von DShot, und solange der
Touch-Controller nicht antwortet.

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
null und `LATCH` aus, wie beim Entschärfen durch die Leerlaufsperre. Der
Hinweis verschwindet, wenn der Controller wieder antwortet; der Tester
bleibt entschärft. SERVO TESTER und S.BUS OUTPUT halten nach derselben
Regel an (unten).

Nicht gemessen: ob der Touch-Controller ohne Finger auf dem Glas über
Minuten weiter antwortet. Tut er es nicht, erschiene dieser Hinweis, und
ein mit `LATCH` gehaltener Motor bliebe kurz nach dem Abheben des Fingers
stehen. Die Prüfung am Prüfstand steht in STATUS.md im Repository, offener
Punkt 23.

<img src="img/tester-touch-lost.png" width="240" alt="Der Tester mit TOUCH NOT ANSWERING in Bernstein unter der Drehzahl">

---

## MENU

<img src="img/menu.png" width="240" alt="Der MENU-Bildschirm">

Der erste Bildschirm nach dem Startbildschirm und die Wurzel: Der
Kopfbereich zeigt `MENU` und kein `BACK`. Die Schaltfläche `MENU` des
Testers führt hierher zurück und **entschärft** vorher, setzt das Gas auf 0
und schaltet `LATCH` aus. Auf `MENU` ist DShot aus: Der ESC-Signalpin liegt
auf Low, und `MENU` zeigt kein EDT-Feld. Das Feld steht auf
[ESC COMMANDS](#esc-commands), das DShot laufen lässt.

Unter dem Titel nennt eine Leiste die beiden Signalpins, SIG für den ESC und
AUX für das KISS-Kabel oder ein zweites Servo, und die DShot-Rate, die der
Tester verwendet, zum Beispiel `SIG GP0  AUX GP1  DSHOT600`.

Nach einer Sitzung in ESC SETTINGS, USB BRIDGE oder S.BUS OUTPUT, nach der
der Bootloader eines ESC noch warten kann, steht unter der Zeile eine
zweite: `POWER-CYCLE THE ESC TO RUN IT` in Bernstein, bis `START DSHOT` im
Startschritt unten getippt wird, die letzte Taste der Verlassenseite des
Werkzeugs getippt wird, ein eRPM-Frame eintrifft oder die Platine neu
startet ([AM32-Konfiguration](AM32-Konfiguration#verlassen)). Ein Besuch in
USB BRIDGE, der ohne Verlassenseite endet, bevor die Leitung benutzt wurde,
nimmt sie weg, außer ein Byte kam vom ESC oder die Zeile stand schon, als
der Besuch begann.

Solange diese Zeile steht, öffnen ESC TESTER und ESC COMMANDS mit einem
Startschritt und ohne DShot, denn ein wartender Bootloader kann DShot als
Befehl lesen: `1 POWER-CYCLE THE ESC.` über `ITS BOOTLOADER MAY STILL WAIT`
und `AND READ DSHOT AS A COMMAND.` Den ESC aus- und einschalten, dann
`START DSHOT` tippen, das in der ersten Sekunde grau ist. Der Tipp nimmt die
Zeile auf MENU weg und startet DShot; auf dem Tester wartet `HOLD TO ARM`
danach 1,5 s wie nach jedem Öffnen. `MENU` auf dem Tester und `BACK` auf ESC
COMMANDS verlassen den Schritt sofort, DShot bleibt aus und die Zeile
bleibt, sodass das nächste Öffnen den Schritt wieder zeigt. Sonst wirkt
nichts auf dem Bildschirm im Startschritt. Nur auf dem Host getestet.

<img src="img/tester-start-step.png" width="240" alt="ESC TESTER im Startschritt: POWER-CYCLE THE ESC., START DSHOT grau, MENU"> <img src="img/esccmd-start-step.png" width="240" alt="ESC COMMANDS im Startschritt: der Schritt, START DSHOT grau, BACK">

Unter der Leiste steht eine Liste mit sechs Zeilen von 40 px Höhe und 42 px
Abstand: zuerst `ESC TESTER`, dann eine Zeile je Geräteart am Signalpin, der
ESC, ein Servo, ein Empfänger, dann die Aufzeichnung und die Einstellungen.
Jede Zeile zeigt links ihren Namen. `LOGS` und `SETUP` zeigen rechts
ihren Zustand. `ESC PROGRAM`, `SERVO` und `RECEIVER` zeigen dort
stattdessen `>`: Jede öffnet eine Auswahl, eine Liste von Werkzeugen, eine
Ebene tiefer.

**Kein Verschieben.** Die sechs Zeilen passen auf den Bildschirm: Jede Zeile
ist ganz sichtbar und nimmt ein Tippen sofort an. MENU hat keinen
Rollbalken, und ein Ziehen verschiebt nichts.

| Zeile | Rechts | Ein Tippen |
|---|---|---|
| **ESC TESTER** | nichts | Öffnet den Tester, entschärft, mit Gas 0 und `LATCH` aus. Siehe [Der Tester](#der-tester) |
| **ESC PROGRAM** | `>` | Öffnet ESC PROGRAM, die Auswahl der ESC-Werkzeuge: ESC COMMANDS, ESC SETTINGS und USB BRIDGE. Siehe [ESC PROGRAM](#esc-program) |
| **SERVO** | `>` | Öffnet SERVO, die Auswahl der Servo-Werkzeuge: SERVO TESTER, ANGLE METER, KST / CHASERVO, HITEC D SERIES und HITEC CLASSIC. Siehe [SERVO](#servo) |
| **RECEIVER** | `>` | Öffnet RECEIVER, die Auswahl der Werkzeuge für Empfängersignale: MONITOR und OUTPUT. Siehe [RECEIVER](#receiver) |
| **LOGS** | `NO CARD`, `READY`, `RECORDING`, `CARD ERROR`, oder `USB DRIVE`, solange ein Computer die Karte hält | Öffnet den Aufzeichnungs-Bildschirm |
| **SETUP** | `UNSAVED`, solange ein Wert vom Flash abweicht; sonst `UPDATED`, solange ein Zurücksetzen oder eine Übernahme durch ein Update nicht überspeichert ist; sonst nichts. Alles in Bernstein | Öffnet SETUP |

Jedes Werkzeug liegt zwei Tippen von MENU entfernt: die Zeile der Auswahl,
dann der Eintrag des Werkzeugs. `BACK` auf einem Werkzeug führt zu seiner
Auswahl zurück, `BACK` auf der Auswahl zu `MENU`.

Solange die Signalpins noch belegt sind, zeigt die Leiste in Bernstein
`SETTING SIG GP0 LOW` für den Moment, in dem der Pin nach einer
Verlassenseite auf Low gesetzt wird, oder `RELEASING SIG GP0`, solange ein
Werkzeug den Pin freigibt, wie die USB-Brücke bis zu 250 ms lang. ESC TESTER
und SETUP sind dann als Umriss unter ihrem üblichen Zustand gezeichnet,
und ein Tippen auf eine solche Zeile öffnet nichts. Sie wirken wieder,
sobald der Pin frei ist. LOGS belegt keinen Pin und bleibt bedienbar. ESC
PROGRAM, SERVO und RECEIVER öffnen wie immer; die Auswahl zeichnet ihre
Einträge so lange als Umriss, mit derselben Bernstein-Zeile als zweiter
Zeile ihrer Leiste.

<img src="img/menu-parking.png" width="240" alt="MENU, während der Pin auf Low gesetzt wird: SETTING SIG GP0 LOW in der Leiste, ESC TESTER und SETUP als Umriss, ESC PROGRAM, SERVO und RECEIVER gefüllt, jede mit ihrem Zeichen">

`UNSAVED` in der Zeile SETUP heißt, dass ein Wert vom Flash abweicht. Die
Schaltfläche zum Speichern liegt in SETUP.

<img src="img/menu-unsaved.png" width="240" alt="UNSAVED in der Zeile SETUP, Änderungen noch nicht im Flash">

`UPDATED` in der Zeile SETUP heißt, dass ein Firmware-Update die
gespeicherten Einstellungen verändert hat. Entweder hat es Einstellungen
eines älteren Formats übernommen, Version 2, 3, 4 oder 5: Die Werte, die das
alte Format kennt, bleiben erhalten, geprüft wie beim Speichern, und die
neueren, darunter `CURRENT SENSE` sowie Einrichtung und Kalibrierung des
Winkelmessers, stehen auf ihrer Voreinstellung. Oder es
fand ein Format, das es nicht kennt, und hat es verworfen: Jeder Wert,
darunter Polzahl, `MAX THROTTLE`, `SIG PIN`, KISS, `SERVO SUPPLY` und
`CURRENT SENSE`, steht auf seiner Voreinstellung. Prüfe sie in SETUP und
halte `HOLD TO SAVE`; ein gelungenes Speichern beendet die Markierung, auch
ohne Änderung. Siehe
[Fehlersuche](Fehlersuche#meine-einstellungen-setzen-sich-zurück). Der
Startbildschirm sagt dasselbe beim Einschalten (siehe
[Erste Schritte](Erste-Schritte#einschalten)).

<img src="img/menu-updated.png" width="240" alt="UPDATED in der Zeile SETUP, nachdem ein Update die gespeicherten Einstellungen verworfen hat">

### ESC PROGRAM

<img src="img/esc-chooser.png" width="240" alt="ESC PROGRAM: ESC COMMANDS, ESC SETTINGS und USB BRIDGE, jedes mit seinen zwei Zeilen">

`MENU → ESC PROGRAM` öffnet eine Auswahl aus drei Einträgen, je eine
Schaltfläche mit zwei Zeilen darunter: **ESC COMMANDS**, das DShot-Befehle
sendet, darunter `BEEP 1 TO 5, DIRECTION, 3D MODE.` und
`SENT AS DSHOT COMMANDS.`; **ESC SETTINGS**, das den Bildschirm für
ESC-Einstellungen öffnet, AM32, Bluejay und BLHeli_S gelesen und
geschrieben; und **USB BRIDGE**, über das ein ESC-Programm auf dem Computer
den ESC erreicht, darunter `A DESKTOP ESC TOOL OVER USB.` und
`MSP, 4-WAY AND PASSTHROUGH.`. Die Leiste nennt den Signalpin, `SIG GP0`.
ESC PROGRAM belegt keinen Pin; das Tippen auf ESC SETTINGS oder USB BRIDGE
belegt SIG, ESC COMMANDS belegt keinen. `BACK` führt zu `MENU` zurück;
`BACK` auf jedem der drei Werkzeuge führt zu ESC PROGRAM zurück. Solange der
Signalpin noch belegt ist, sind die drei Einträge Umrisse, und die zweite
Zeile der Leiste zeigt `RELEASING SIG GP0` oder `SETTING SIG GP0 LOW` in
Bernstein. Siehe
[ESC COMMANDS](#esc-commands) weiter unten,
[AM32-Konfiguration](AM32-Konfiguration)
und [USB-Brücke](USB-Bruecke).

### ESC COMMANDS

<img src="img/esccmd.png" width="240" alt="ESC COMMANDS in Ruhe: BEEP 1 bis 5, NORMAL und REVERSED, 3D OFF und 3D ON und die sechs gedimmten Zeilen"> <img src="img/esccmd-3d-on.png" width="240" alt="ESC COMMANDS nach 3D ON: SENT 10 (3D ON), THEN 12 (SAVE). und drei bernsteinfarbene Zeilen">

`MENU → ESC PROGRAM → ESC COMMANDS` sendet DShot-Befehle an den ESC an SIG.
Die Seite belegt keinen Pin. DShot startet an SIG, wenn sie öffnet, und
endet, wenn `BACK` zu ESC PROGRAM zurückführt; der Pin liegt dann auf Low. Die
Leiste zeigt `SIG GP0  DSHOT600` und das EDT-Feld: `EDT ON` in Grün, solange
Extended-DShot-Telemetry-Frames eintreffen, `EDT OFF` in Rot, solange
eRPM-Frames eintreffen und EDT-Frames nicht, und `NO ESC`, ein Rahmen mit
gedämpfter Beschriftung, solange seit 500 ms kein eRPM-Frame kam. Die
Firmware schickt die EDT-Freigabe selbst; das Feld zeigt empfangene Frames.

- **BEEP 1 bis 5**, je ein Tippen: Der ESC spielt seinen Beacon-Ton 1 bis 5.
  Damit hörst du, welcher ESC angeschlossen ist und dass er Befehle annimmt.
- **NORMAL** und **REVERSED** unter DIRECTION, je 1 s halten: die
  Drehrichtung, Befehl 7 oder 8, danach 12, der sie im ESC speichert.
- **3D OFF** und **3D ON** unter 3D MODE, je 1 s halten: Befehl 9 oder 10,
  danach 12.

Eine Halte-Taste füllt sich beim Halten rot, und ein Balken darunter füllt
sich über die Sekunde; nach 1 s wirkt sie. Eine Richtungs- oder 3D-Änderung
dauert auf der Leitung 620 ms. Die Tasten wirken, solange der ESC bereit
ist; sonst sind sie als Umriss gezeichnet, und eine bernsteinfarbene Zeile
sagt warum: `NO ESC ANSWERS ON SIG GP0.`,
`MOTOR TURNING: WAIT UNTIL IT STOPS.` oder `ESC STARTING: WAIT 1.5 S.` (AM32
nimmt einen Befehl erst nach einer Sekunde Nullgas an). In den ersten 1,5 s
nach dem Start von DShot steht ohne Antwort stattdessen
`DSHOT STARTING ON SIG GP0.`: So lange braucht der ESC, um DShot zu sehen.
Ein Druck,
während das Board noch etwas anderes sendet, etwa die Pause von 260 ms bis
1020 ms nach einem Beacon, wartet: Er geht hinaus, sobald die Leitung frei
ist, oder verfällt nach 2 s, und die Zeilen zeigen dann
`NOT SENT: THE ESC WAS NOT READY.`. Solange ein Befehl der Seite wartet oder
gesendet wird, sind die Tasten und `BACK` grau, und ein Druck, der dann auf
ihnen beginnt, bewirkt beim Loslassen nichts.

Die Zeilen unter den Tasten sagen, was gesendet wurde, zum Beispiel
`SENT 8 (REVERSED), THEN 12 (SAVE).`, oder warum nicht:
`NOT SENT: THE ESC STOPPED ANSWERING.`, bevor der ESC gehandelt haben kann,
`SAVE NOT SENT: POWER-CYCLE THE ESC.`, wenn die Änderung ungespeichert im
ESC stehen kann, und `NOT SENT: THE ESC WAS NOT READY.`, wenn nichts
gestartet ist. Nach 3D ON sagen die Zeilen, dass der Tester seinen
3D-Hinweis zeigt und dass ein Neustart des Boards ihn löscht, oder ESC
SETTINGS, wenn es 3D MODE aus liest.

Der ESC antwortet auf keinen Befehl, der Prüfstand sieht die Drehrichtung
also nicht. Prüfe sie auf dem Tester mit 3D aus: Propeller ab, ein
Klebestreifen auf der Glocke, scharf schalten, eine Sekunde 5 %, entschärfen.
Bei AM32 spielt das Speichern den Ton von Beacon 1 für normal und von
Beacon 2 für umgekehrt. Hält die Richtung nach einem Aus- und Einschalten des
ESC, war sie gespeichert. Nach `3D ON` zeigt der Tester seinen 3D-Hinweis
([Der Tester](#der-tester)); `3D OFF` lässt den Hinweis stehen, bis das Board
neu startet oder ESC SETTINGS 3D MODE aus liest. Keiner dieser Befehle wurde
bisher von diesem Board an einen ESC gesendet.

<img src="img/esccmd-sending.png" width="240" alt="ESC COMMANDS, während Beacon 3 gesendet wird: alle Tasten und BACK grau"> <img src="img/esccmd-no-esc.png" width="240" alt="ESC COMMANDS ohne antwortenden ESC: NO ESC ANSWERS ON SIG GP0. in Bernstein, alle Tasten grau">

### SERVO

<img src="img/servo-hub.png" width="240" alt="SERVO am Anfang: SERVO TESTER, ANGLE METER und KST / CHASERVO ganz sichtbar, jedes mit seiner Zeile, HITEC D SERIES am Bildschirmrand abgeschnitten, rechts der Rollbalken"> <img src="img/servo-hub-scrolled.png" width="240" alt="SERVO bis ans Ende gezogen, mit SERVO SUPPLY auf OWN: KST / CHASERVO und HITEC D SERIES mit POWER THE SERVO UP AFTER THE TAP., HITEC CLASSIC als Umriss mit NEEDS THE CONVERTER OF WIRING PART C und AND SETUP > SERVO SUPPLY GP28/29. in Bernstein"> <img src="img/servo-hub-switched.png" width="240" alt="SERVO am Ende mit SERVO SUPPLY auf GP28: die drei Programmierer mit THE BENCH SWITCHES IT ON GP28., HITEC CLASSIC bedienbar">

`MENU → SERVO` öffnet SERVO, die Auswahl der fünf Servo-Werkzeuge, in der
Reihenfolge der Nutzung: **SERVO TESTER** treibt ein Servo, **ANGLE METER**
misst den Ausschlag eines Ruders, und **KST / CHASERVO**, **HITEC D SERIES** und
**HITEC CLASSIC** programmieren ein Servo. Die Leiste zeigt
`SIG GP0  AUX GP1  SERVO SUPPLY OWN` oder den Pin der geschalteten Versorgung.
SERVO belegt keinen Pin, und DShot ist aus, solange es zu sehen ist: SIG liegt
auf Low. Eine zweite Zeile der Leiste zeigt
`NO ESC CHECK: UNPLUG ANY ESC FIRST.` in Bernstein: Bei abgeschaltetem DShot
sieht der Prüfstand keinen ESC an SIG, und ein Servopuls ist für einen ESC ein
gültiges Gas. Zieh jeden ESC ab, bevor du einen Eintrag antippst. Das Tippen auf
einen Eintrag öffnet das Werkzeug, und ein Werkzeug, das SIG braucht, belegt es
mit diesem Tippen.

**Verschieben.** Drei Einträge passen auf den Bildschirm. SERVO TESTER,
ANGLE METER und KST / CHASERVO sind ganz sichtbar, wenn SERVO öffnet. Von
HITEC D SERIES sind nur die oberen 18 px sichtbar, und dort nimmt es kein
Tippen an: Es zeigt, dass die Liste weitergeht. Ziehe SERVO nach oben, um
die beiden Hitec-Einträge zu erreichen; die Liste bewegt sich um höchstens
146 px. Ein 3 px breiter Rollbalken am rechten Rand zeigt die Lage. Ein
Werkzeug, das zu SERVO zurückführt, findet die Liste dort, wo sie stand.

Unter jeder Schaltfläche steht, was das Werkzeug tut oder welche Servos es
programmiert. Unter den drei Programmierern steht, wann das Servo Strom
bekommt: `POWER THE SERVO UP AFTER THE TAP.` mit SERVO SUPPLY auf `OWN`,
oder `THE BENCH SWITCHES IT ON GP28.` mit geschalteter Versorgung. Ein
Servo, das schon vor dem Tippen Strom hat, sähe die ersten Frames des
Programmierers schon mit dem Tippen. HITEC CLASSIC braucht die geschaltete
Versorgung: unter `OWN` ist es als Umriss gezeichnet, tut nichts und sagt in
Bernstein warum, `NEEDS THE CONVERTER OF WIRING PART C` und
`AND SETUP > SERVO SUPPLY GP28/29.`

| Eintrag | Zeile darunter | Öffnet |
|---|---|---|
| **SERVO TESTER** | `ONE SERVO ON SIG, OR TWO WITH AUX.` | [Servotester](Servotester) |
| **ANGLE METER** | `INCIDENCE, DECALAGE, DIHEDRAL, THROW` | [Winkelmesser](Winkelmesser) |
| **KST / CHASERVO** | die Stromzeile | [KST-Programmierung](KST-Programmierung) |
| **HITEC D SERIES** | `D485HW, D645MW AND THE OTHERS`, die Stromzeile | [Hitec-Programmierung](Hitec-Programmierung) |
| **HITEC CLASSIC** | `HS-5XXX, HS-7XXX`, die Stromzeile oder die zwei Bernstein-Zeilen | [Hitec-Programmierung](Hitec-Programmierung#klassische-servos) |

`BACK` führt zu `MENU` zurück. `SERVO`, die letzte Taste der Verlassenseite
am Ende einer Sitzung des Servotesters oder eines Programmierers, und `BACK`
in einem Werkzeug, bevor es SIG belegt, führen zu SERVO zurück; ebenso
`BACK` auf dem Winkelmesser. Solange der Signalpin noch belegt ist, öffnet
SERVO mit seinen Einträgen als Umriss und `SETTING SIG GP0 LOW` oder
`RELEASING SIG GP0` in Bernstein an der Stelle der Zeile NO ESC CHECK; ein
Tippen auf einen Eintrag öffnet nichts, bis der Pin frei ist.

### RECEIVER

<img src="img/rx-chooser.png" width="240" alt="RECEIVER: MONITOR mit S.BUS FROM A RECEIVER, ON GP1., OUTPUT mit S.BUS TO A FLIGHT CONTROLLER. und ON GP0, NEVER TO AN ESC.">

`MENU → RECEIVER` öffnet eine Auswahl aus zwei Einträgen:
**MONITOR**, das S.BUS MONITOR öffnet, und **OUTPUT**, das S.BUS OUTPUT
öffnet, einen S.BUS-Ausgang zu einem Flugcontroller, mit seiner Warnung
darunter: `S.BUS TO A FLIGHT CONTROLLER.` `ON GP0, NEVER TO AN ESC.` Die
Leiste nennt beide Signalpins, `SIG GP0  AUX GP1`, über der Bernstein-Zeile
`NO ESC CHECK: UNPLUG ANY ESC FIRST.`, wie auf SERVO. RECEIVER belegt
keinen Pin, und DShot ist aus, solange es zu sehen ist. Mit SIG auf GP1 trägt
MONITOR eine zweite Zeile, `GP1 IS SIG: PLUG IN AFTER THE TAP.`: GP1 liegt bis
zum Tippen auf Low. `BACK` führt zu `MENU` zurück; `RECEIVER`, die letzte
Taste der Verlassenseite beider Werkzeuge, führt zu RECEIVER zurück.
Solange der Signalpin noch belegt ist, sind beide Einträge Umrisse, wie auf
SERVO. Siehe [Empfänger](Empfaenger).

---

## SETUP

<img src="img/settings.png" width="240" alt="Der SETUP-Bildschirm">

Motor- und Verkabelungseinstellungen, Anzeigeeinstellungen, die Platine,
Diagnose und die Schaltfläche, die alle Einstellungen ins Flash schreibt. Eine
Liste in Abschnitten, die sich mit dem Finger verschieben lässt. Sie öffnet
über `MENU` oder, entschärft, über ein Tippen auf die Polzahl oder die
Obergrenze des Testers; `BACK` führt dorthin zurück, wo sie geöffnet wurde.
Die Abschnittsnamen und die Werte neben `-` und `+` sind cyan, die Farbe
einer Einstellung; nur lesbare Werte sind grau.

| Abschnitt | Zeile | Bedeutung |
|---|---|---|
| ESC TESTER | **MOTOR POLES** | 2–28, nur gerade Werte. Die Drehzahlanzeige teilt die eRPM durch die Hälfte davon |
| ESC TESTER | **MAX THROTTLE** | Die Gas-Obergrenze, `5%` bis `100%` in Schritten von 5 % |
| WIRING | **SIG PIN** | SIG, der GPIO (General Purpose Input/Output, frei verwendbarer Pin) des Signalkabels zum ESC, mit seinem Steckerpin darunter: `GP0` über `J5 PIN 2`, `GP1` über `J5 PIN 1`. Angeboten werden nur die zwei freien GPIOs der Platine, GP0 und GP1, daher tauscht ein Schritt SIG und AUX. Über dem Tester wechselt DShot sofort auf den neuen Pin; von MENU aus nimmt ihn der nächste ESC TESTER; siehe die Hinweiszeile unten |
| WIRING | **DSHOT** | Die DShot-Rate in kbit/s: 150 / 300 / 600 / 1200. Herunterstellen, wenn die `LINK`-Fehlerrate steigt |
| WIRING | **KISS TELEM** | Ob auf dem optionalen dritten Telemetriekabel empfangen wird, an AUX |
| WIRING | **AUX PIN** | AUX, der GPIO des KISS-Kabels und des zweiten Servos im Servotester, mit seinem J5-Pin darunter. Wirkt, ob KISS TELEM an oder aus ist. Ein Schritt tauscht SIG und AUX, sodass beide nie auf einem Pin liegen |
| SERVO SUPPLY | **SERVO SUPPLY** | `OWN`, `GP28` oder `GP29`: `OWN` heißt, das Servo hat eine eigene Versorgung und der Prüfstand schaltet nichts; `GP28` oder `GP29` heißt, ein DC-DC-Wandler (Gleichspannungswandler) für die Versorgung des Servos hat seinen Enable-Eingang an diesem Pin, unter dem Wert genannt: `J4 PIN 11` für GP28, `J4 PIN 12` für GP29. Standard ist `OWN`. - und + schalten durch die drei Werte. Mit `GP28` oder `GP29` schalten SERVO TESTER, KST SERVO und HITEC SERVO die Versorgung des Servos |
| SERVO SUPPLY | **CURRENT SENSE** | `NONE` oder `INA219`: ob ein INA219-Strommessmodul an J3 hängt. Standard ist `NONE`. Ein Tippen wechselt zwischen beiden. Mit `INA219` steht unter dem Namen `FOUND`, `NOT FOUND` oder, für einen anderen Chip an der Adresse des Sensors, `OTHER PART`. Ein INA219, den eine andere Firmware anders konfiguriert hat, zeigt ebenfalls `OTHER PART`: das Modul kurz stromlos machen. Servotester, KST SERVO und HITEC SERVO zeigen dann den Strom der Versorgung |
| DISPLAY | **CONTRAST** | `NORMAL` oder `HIGH`. Hoher Kontrast ist für Tageslicht |
| DISPLAY | **BACKLIGHT** | 16–255, in Schritten von 32 |
| BOARD | **BUILT FOR** | Nur Anzeige. Die Platine, für die die Firmware gebaut ist, `RP2350-TOUCH-LCD-2.8` |
| BOARD | **VERSION** | Nur Anzeige. Die Firmware-Version, zum Beispiel `0.1.0` |
| BOARD | **CLOCK** | Nur Anzeige. Die Uhr der Platine auf die Minute, `2026-11-14 15:42`, oder grau `NOT SET`. Die Zeile darunter, `SET AT EACH POWER-UP` (bei jedem Einschalten stellen): Die Platine hat keine Zelle für ihre Uhr, also stellt ein Computer sie nach jedem Einschalten mit `tools/set_clock.py` neu ([SD-Aufzeichnung](SD-Aufzeichnung#die-uhr)). Wird einmal pro Minute neu gezeichnet, solange die Zeile sichtbar ist |
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

- **Die Warnung zum ESC-Pin**, bei y 230, nur solange DShot unter SETUP
  läuft, über dem Tester geöffnet: in Bernstein der Pin, auf dem DShot läuft,
  zum Beispiel `UNPLUG ANY SERVO FROM GP0 NOW`, und nach einem Schritt von
  SIG PIN ab dem Schritt der neue Pin. Ein dort verbliebenes Servo, etwa nach
  dem Servotester mit zwei Servos, bewegt sich. Von MENU aus geöffnet ist
  DShot aus, und die Zeile erscheint nicht, auch nach einem Schritt von SIG
  PIN: Der nächste ESC TESTER sendet DShot auf dem neuen Pin. Die Liste endet
  dann 12 px höher, bei y 227, und lässt sich 12 px weiter verschieben; ihre
  oberen Zeilen bleiben, wo sie sind. Mit KISS TELEM an tauscht ein Schritt
  SIG und AUX, DShot läuft also auf dem Kabel, an dem das KISS-Pad des ESC
  hing: Solange SIG nicht auf dem Pin liegt, der beim Einschalten galt, zeigt
  die Zeile stattdessen `SWAP THE ESC'S SIGNAL AND KISS WIRES`, über dem
  Tester wie von MENU aus geöffnet.
- **LINK**: live, Pakete pro Sekunde und Fehlerrate. Grün heißt, es kommen
  Pakete an, der ESC-Pin stimmt also. Bernstein heißt, mehr als 5 % Fehler.
  Von MENU aus geöffnet, wo DShot aus ist, zeigt es `OFF` in Grau.
- Die Hinweiszeile: ein Text, der erste, der zutrifft, in dieser Reihenfolge:
  1. `SAVE FAILED - FLASH REFUSED THE WRITE` (rot) oder
     `NOT WHILE ARMED - DISARM TO SAVE` (rot), bis zum nächsten Speichern
     oder Verlassen von SETUP;
  2. `ADJUSTED TO THIS BOARD'S LIMITS` (Bernstein), bis zur nächsten
     Änderung;
  3. `DEFAULTS LOADED - RESTART TO UNDO` (Voreinstellungen geladen, ein
     Neustart macht es rückgängig; Bernstein) nach `HOLD DEFAULTS`, solange ein
     Neustart die gespeicherten Werte zurückbrächte, bis zur nächsten
     Änderung oder zu einem `HOLD TO SAVE`, gescheitert oder nicht;
  4. `UNSAVED - HOLD SAVE TO KEEP` (Bernstein) nach einer Änderung;
  5. `SAVED` (grün) nach einem Speichern, bis zur nächsten Änderung;
  6. `RESET BY UPDATE - CHECK AND SAVE` (Bernstein), nachdem ein Update die
     gespeicherten Einstellungen verworfen hat, bis ein Speichern gelingt;
  7. `CARRIED OVER BY UPDATE - CHECK, SAVE` (Bernstein), nachdem ein Update
     Einstellungen eines älteren Formats übernommen hat, bis ein Speichern
     gelingt;
  8. `SAVED SETTINGS IN USE`, oder `NOTHING SAVED: DEFAULTS IN USE`, wenn
     nichts gespeichert ist.

<img src="img/settings-carried.png" width="240" alt="SETUP, nachdem ein Update die gespeicherten Einstellungen übernommen hat: CARRIED OVER BY UPDATE - CHECK, SAVE in der Hinweiszeile">

<img src="img/settings-pin.png" width="240" alt="SETUP über dem Tester, nachdem SIG PIN auf GP1 gewechselt hat: UNPLUG ANY SERVO FROM GP1 NOW über LINK, UNSAVED in der Hinweiszeile">

- `HOLD TO SAVE` (150 × 46 px) und `HOLD DEFAULTS` (72 × 46 px), die
  Beschriftung in zwei Zeilen, `HOLD` über `DEFAULTS`, in Bernstein.

**Änderungen wirken sofort. Nur das Speichern braucht das Halten.** Eine neue
Bitrate baut den DShot-Treiber vor dem nächsten Frame neu auf. `HOLD TO SAVE`
1 s lang schreibt ins Flash. `HOLD DEFAULTS` 1 s lang ersetzt die laufenden
Einstellungen durch die kompilierten Voreinstellungen, Polzahl und
Gas-Obergrenze eingeschlossen; Einrichtung und Kalibrierung des
Winkelmessers, die SETUP nicht zeigt, behalten ihre Werte, und das Flash
bleibt unverändert. Ein Tippen
bewirkt nichts, ein Wegschieben vom Knopf bricht das Halten ab. Ein Balken
über dem Knopf füllt sich, solange er gehalten wird, und der Knopf wird rot,
wie bei jedem Halten. Wer den
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

<img src="img/settings-supply.png" width="240" alt="SETUP bis SERVO SUPPLY verschoben, auf GP29"> <img src="img/settings-scrolled.png" width="240" alt="SETUP bis zum Ende verschoben: BOARD und DIAGNOSTICS">

<img src="img/settings-sense.png" width="240" alt="SETUP: CURRENT SENSE auf INA219, FOUND">

<img src="img/settings-contrast.png" width="240" alt="SETUP mit hohem Kontrast"> <img src="img/tester-contrast.png" width="240" alt="Der Tester-Bildschirm mit hohem Kontrast">

*Hoher Kontrast gilt für jeden Bildschirm und setzt die
Hintergrundbeleuchtung auf 255, solange er an ist.*

---

## LOGS

<img src="img/log-ready.png" width="240" alt="LOGS-Bildschirm, Karte bereit">

Aufzeichnungszustand und Zähler, nach dem Beenden der Lauf der letzten
Aufzeichnung. `BACK` führt zu `MENU` zurück. Siehe
[SD-Aufzeichnung](SD-Aufzeichnung).

---

## ESC SETTINGS

<img src="img/am32-list.png" width="240" alt="AM32-Einstellungsliste">

Liest, ändert und schreibt die Einstellungen eines AM32-, Bluejay- oder
BLHeli_S-ESC über das Signalkabel. `WRITE...` öffnet mit einem Tippen
`ESC WRITE`: Die Seite listet jede Änderung, alt und neu, und ihr
`HOLD TO WRITE`, 1 s, schreibt, zuvor mit einer Sicherung auf der SD-Karte.
`HEX` zeigt die Bytes unter dem Titel `ESC HEX`. Einstellungen in einem
Layout, das der Tester nicht kennt, also nicht 2 bis 4, öffnen nur zum
Lesen: die Bytes hexadezimal, `NOT VERIFIED.`, eine Sicherung auf der
SD-Karte und kein Schreiben. `BACK` führt zu ESC PROGRAM zurück, bei
ungeschriebenen Änderungen nach einer Warnung. Siehe
[AM32-Konfiguration](AM32-Konfiguration).

---

## SERVO TESTER

<img src="img/servo-safe.png" width="240" alt="Der Servobildschirm in SAFE">

Steuert ein oder zwei Servos mit Servoimpulsen auf SIG und AUX an, dem ESC-
und dem KISS-Pin. Die Leiste nennt sie, `SIG GP0  50 HZ  1000-2000 US`, oder
`SIG+AUX  50 HZ  1000-2000 US` mit zwei Servos. Er startet in SAFE;
`HOLD TO START` 1 s lang gehalten startet die Impulse in der Mitte, `STOP`
wirkt beim Berühren. Sind vier Lesungen des Touch-Controllers in Folge
fehlgeschlagen, über 100 ms oder länger, hält ein laufendes Servo an wie bei
`STOP`, die Skala zeigt `TOUCH NOT ANSWERING`, bis er wieder antwortet, und
`HOLD TO START` ist als Umriss gezeichnet. In SAFE mit einem Servo zeigt die
Anzeigezeile `TWO SERVOS:` und `SET > SERVOS`: Dort wird das zweite Servo
eingeschaltet. Mit SERVO SUPPLY auf `GP28` oder `GP29` schaltet der
Bildschirm den Wandler: ein bei `HOLD TO START`, aus bei `STOP`, bei einem
Hänger und bei `BACK`. Die zweite Zeile der Leiste zeigt in Bernstein, wo er
steht: `GP28 SUPPLY ON AT START`, `GP28 SUPPLY COMING UP`, `GP28 SUPPLY ON`,
oder `GP28 SUPPLY NOT OFF`, wenn der Stromsensor 2 s nach dem Ausschalten
noch mehr als 2,5 V misst. `SET` öffnet die Einstellungen für Profil und
Sweep; sein `BACK` warnt bei einer Änderung, die auf `HOLD TO APPLY`
wartet, zuerst: `1 CHANGE NOT APPLIED.` `BACK`
öffnet die Verlassenseite bei `1 UNPLUG THE SERVO.`; `NEXT SERVO` führt zum
Tester zurück, und `SERVO` legt die Pins auf Low und führt zu SERVO, der
Auswahl. Mit CURRENT SENSE auf `INA219`
zeigt die Zeile unter der Skala statt des Hinweises Strom und Spannung der
Servoversorgung, und die Messwerte des Sensors halten das Servo an: Die
Anzeige zeigt dann `OVERCURRENT`, `STALL`, `LOW SUPPLY` oder `NO SENSOR` in
Magenta. Siehe [Servotester](Servotester).

---

## KST SERVO

<img src="img/kst-list.png" width="240" alt="Die KST-Feldliste">

Öffnet über `MENU → SERVO → KST / CHASERVO`; dieses Tippen
belegt SIG. Liest und schreibt die Einstellungen eines KST- oder
Chaservo-Servos über das Signalkabel an SIG, dem ESC-Pin. Der Bildschirm
sucht das Servo, während du es einschaltest, liest seine 32 Register und
listet 27 Felder zum Verstellen.
Ein Tippen auf `WRITE...` öffnet die Schreibseite, und ihr `HOLD TO WRITE`,
1 s, schreibt, zuvor mit einer Sicherung auf der SD-Karte. `BACK` öffnet
die Verlassenseite, deren erster Schritt dich auffordert, das Servo
auszuschalten; `LEAVE` macht dann `UNPLUG THE SERVO.` zum aktuellen Schritt,
`NEXT SERVO` startet eine neue Suche, und `SERVO` legt den Pin auf Low und
führt zu SERVO, der Auswahl. Mit
CURRENT SENSE auf `INA219` zeigt der Streifen den Strom hinter dem Zustand der
Versorgung, `SUPPLY ON 0.08 A`. Siehe [KST-Programmierung](KST-Programmierung).

---

## HITEC SERVO

<img src="img/hitec-list.png" width="240" alt="Die Hitec-Feldliste">

<img src="img/hitec-read-wait.png" width="240" alt="HITEC SERVO für ein Servo der D-Serie mit eigener Versorgung: SIG GP0 IS THE SERVO'S: DSHOT IS OFF. in Grün, POWER THE SERVO UP, THEN TAP READ. in Bernstein, und READ">

Öffnet über `MENU → SERVO → HITEC D SERIES` oder `HITEC CLASSIC`. Liest und
schreibt die Einstellungen eines Hitec-Servos der D-Serie oder der klassischen
Familie über das Signalkabel an SIG, dem ESC-Pin. Das Tippen auf den Eintrag
übernimmt den Pin, auf dem in der Auswahl kein DShot läuft; die Leiste zeigt
`RELEASING SIG GP0 FROM DSHOT`, bis die Platine ihn freigegeben hat. Ein
Servo der D-Serie mit eigener Versorgung (SERVO SUPPLY auf `OWN`) muss
laufen, bevor der Bildschirm es liest, deshalb wartet der Bildschirm dann:
Die Leiste zeigt `SIG GP0 IS THE SERVO'S: DSHOT IS OFF.` in Grün und
`POWER THE SERVO UP, THEN TAP READ.` in Bernstein. Schalte das Servo ein und
tippe `READ`. Mit geschalteter Versorgung und für ein klassisches Servo versorgt
der Prüfstand das Servo selbst, und der Bildschirm macht sofort weiter. Solange
ein ESC an SIG antwortet, belegt der Bildschirm nichts und zeigt
`AN ESC ANSWERS ON SIG GP0: UNPLUG IT.` in Bernstein; `BACK` führt dann zu
SERVO, der Auswahl, zurück. Der Bildschirm tritt in das Servo ein und liest es.
Ein Servo der D-Serie liefert 21 Register und 20 Parameter, davon 11
verstellbar. An der Leitung hängt ein Servo, ohne Pull-up-Widerstand. Ein Tippen
auf `WRITE...` öffnet die Schreibseite, und ihr `HOLD TO WRITE`, 1 s, schreibt,
zuvor mit einer Sicherung auf der SD-Karte. `BACK` öffnet die Verlassenseite;
`LEAVE` macht dann `UNPLUG THE SERVO.` zum aktuellen Schritt, `NEXT SERVO`
startet eine neue Sitzung derselben Familie, und `SERVO` legt den Pin auf Low
und führt zu SERVO, der Auswahl. Mit CURRENT SENSE auf `INA219` zeigt der
Streifen den Strom hinter `SUPPLY ON`. Nicht auf Hardware getestet. Siehe
[Hitec-Programmierung](Hitec-Programmierung).

---

## S.BUS MONITOR

<img src="img/sbus-live.png" width="240" alt="S.BUS MONITOR mit Frames: der Chip OK, 16 lindgrüne Balken und ihre Werte">

Zeigt den S.BUS-Ausgang eines Empfängers an GP1: den Verbindungs-Chip in
der Leiste, 16 Kanalbalken über dem Rohwert mit dem Wert in µs, die
digitalen Kanäle CH17 und CH18, die Framerate und die Fehlerzähler. Er
nimmt GP1: als AUX in der Standardverdrahtung, als SIG, wenn
SIG PIN auf GP1 steht; dort wartet er, solange ein ESC antwortet. `BACK`
öffnet die Verlassenseite bei `1 UNPLUG THE RECEIVER.`; `BACK TO MONITOR`
beginnt von vorn, jeder Zähler auf 0, und `RECEIVER` legt die Pins auf
Low und führt zu RECEIVER. Siehe [Empfänger](Empfaenger).

---

## S.BUS OUTPUT

<img src="img/sbus-out-running.png" width="240" alt="S.BUS OUTPUT läuft: der Chip RUNNING, die Obergrenze des Gaskanals und die Flags, acht Schieberegler mit lindgrünen Werten, STOP">

Sendet S.BUS-Frames an GP0 zu einem Flugcontroller, nie zu einem ESC: 16
Kanäle an Schiebereglern auf zwei Seiten, und unter SET den Gaskanal, CH17,
CH18 und die Flags für Failsafe und verlorenen Frame. Er nimmt GP0
und prüft den Pin auf den Bootloader eines ESC, bevor `HOLD TO START`
angeboten wird. Der Gaskanal hält an der Obergrenze von MAX THROTTLE.
`STOP`, ein hängender Bildschirm, 30 s ohne Berührung abseits des ersten
Frames und ein Touch-Controller, der nicht antwortet, wie beim Tester,
halten die Frames an; der letzte Fall zeigt `STOPPED: TOUCH DID NOT ANSWER`.
`BACK` öffnet die Verlassenseite bei
`1 UNPLUG THE FLIGHT CONTROLLER.`; `BACK TO OUTPUT` prüft GP0 erneut, und
`RECEIVER` legt die Pins auf Low und führt zu RECEIVER. Siehe
[Empfänger](Empfaenger).

---

## ANGLE METER

<img src="img/angle-main.png" width="240" alt="ANGLE METER nach einem Nullpunkt: -20.0 Grad, -13.7 MM, MAX -20.0 Grad, die Temperatur der IMU">

Misst Winkel an der Schwerkraft mit dem Beschleunigungssensor der Platine:
den Winkel um die Achse in Größe 5, die Neigung der Achse und den Chip
`SETTLED` in der Leiste, mit gesetzter LENGTH den Weg der Endleiste in mm,
MAX je Seite und die Temperatur der Platine. In der Fußleiste `ZERO`,
`RESET MAX` und `SET`; ANGLE SET enthält ALONG AXIS, LENGTH, SIGN und die
Kalibrierung in drei Lagen, FLIP CAL. `BACK` auf der Hauptansicht schaltet
den Beschleunigungssensor ab und führt zu SERVO, der Auswahl; auf ANGLE SET und
FLIP CAL geht es eine Ebene hoch. Nicht auf Hardware getestet; die Genauigkeit
ist nicht gemessen. Siehe [Winkelmesser](Winkelmesser).

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
  oder ein Servo schreiben. Ein Gerät wird nur von seiner Schreibseite aus
  geschrieben: Ein Tippen auf `WRITE...` oder `RESTORE...` öffnet sie, sie
  listet die Änderungen, und nur ihr Halten schreibt. Ein Servo starten und
  ein riskantes Servoprofil übernehmen brauchen dasselbe Halten von 1 s.
