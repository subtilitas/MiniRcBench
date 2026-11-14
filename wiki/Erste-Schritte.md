[English](First-Run) | **Deutsch**

# Erste Schritte

Diese Seite beschreibt, wie du einen ESC (Electronic Speed Controller,
Motorregler) anschließt, einschaltest und zum ersten Mal scharfschaltest. Du
brauchst zwei Kabel und einen Akku für den ESC. Die Teile, das Adapterkabel
und das Aufspielen der Firmware stehen vorher in
[Einrichtung](Einrichtung).

---

## Sicherheit zuerst

Die Platine bringt einen Motor zum Drehen. Erledige vorher diese drei Dinge:

- **Propeller abnehmen.**
- **Motor sichern.** Ein ungesicherter Motor kann sich beim Hochlaufen bewegen
  oder springen.
- **ESC am eigenen Akku betreiben.** Versorge keinen Motor über den 5-V-Pin der
  Platine.

Die Firmware hat diese Sperren. Sie ersetzen die Schritte oben nicht:

- Die Gasobergrenze steht ab Werk auf **20 %** des DShot-Bereichs.
- Scharfschalten verlangt **1 s Halten**, bei mindestens 250 ms Gas auf null.
- `DISARM` wirkt **beim Berühren**, ebenso `STOP` im Servotester. Alle
  anderen Bedienelemente wirken beim Loslassen.
- Im scharfen Zustand entschärft der Tester nach **30 s** ohne Berührung. Die
  letzten 5 s zählen auf dem Bildschirm herunter.
- Hält die Anzeige-Firmware an, setzt der zweite Kern das Gas nach **250 ms**
  ohne Herzschlag auf null.

---

## ESC anschließen

Zwei Kabel: Signal und Masse. [Verdrahtung](Verdrahtung) zeigt jede
Verbindung auf einem Blatt.

| | RP2350-Touch-LCD-2.8 |
|---|---|
| **Signal** | GP0, J4 Pin 9 oder J5 Pin 2 |
| **Masse** | GND, J4 Pin 1 oder 5 oder J5 Pin 4 |

Die Platine hat JST-SH-Stecker mit 1,0 mm Raster und keine 2,54-mm-Stiftleisten,
das ESC-Kabel braucht also ein Adapterkabel mit JST-SH-Stecker.

GP steht für GPIO (General Purpose Input/Output, frei verwendbarer Pin). Der
ESC-Pin lässt sich später unter **MENU → SETTINGS** ändern und wird im Flash
gespeichert.

> **Vorher das mittlere Kabel des ESC-Steckers abzwicken oder auspinnen.**
> Der mittlere Leiter eines dreipoligen Servosteckers ist der
> **+5-V-Ausgang des BEC** (Battery Eliminator Circuit) im ESC, und er kommt
> an keinen Pin. GP0 und GP1, die Signalpins, vertragen 5,5 V, solange die
> Platine eingeschaltet ist, und 3,63 V, solange sie aus ist. Der Tester
> braucht nur Signal und Masse.

> **Warnung: Ein Servo- oder ESC-Kabel nie 1:1 an J5 anschließen.**
> J5 Pin 2, 3 und 4 sind SIG, 3,3 V und GND: dieselbe Reihenfolge wie bei
> einem Servostecker, Signal, Plus und Minus. Pin 3 ist die 3,3-V-Versorgung
> der Platine. 1:1 verbunden, bekommt er die BEC-Ausgangsspannung
> (Battery Eliminator Circuit) des ESC, +5 V, oder die Versorgung des Servos,
> 4,8 V bis 6,0 V oder 8,4 V für ein Hochvolt-Servo, und das kann die Platine
> zerstören. J5 Pin 3 bleibt frei.

Halte das Signalkabel kurz. Die größte funktionierende Länge ist nicht
gemessen. Steigt die Fehlerrate unter `LINK`, stelle **DSHOT KBAUD** unter
**MENU → SETTINGS** auf 300.

---

## Einschalten

Die Platine läuft über USB-C (USB: Universal Serial Bus), der ESC über seinen
eigenen Akku. Die Reihenfolge ist egal. Die Firmware schickt die
Telemetrie-Freigabe an jeden ESC, der zu antworten beginnt. Du kannst den ESC
also jederzeit anstecken oder neu starten.

Der Startbildschirm steht 3 s lang, dann erscheint der Tester-Bildschirm. Er
nennt die Platine, für die die Firmware gebaut ist, RP2350-TOUCH-LCD-2.8, und
die Firmware-Version, zum Beispiel `VERSION 0.1.0`. Seine letzte Zeile nennt
in Bernstein den ESC-Pin: `UNPLUG ANY SERVO FROM GP0 NOW`. DShot startet
am Ende des Startbildschirms auf diesem Pin, und ein dort vergessenes Servo
liest die Frames als Impulse und fährt an ein Ende seines Wegs.

<img src="img/splash.png" width="240" alt="Startbildschirm">

Nach einem Firmware-Update, das den Aufbau der gespeicherten Einstellungen
von Version 2, 3 oder 4 geändert hat, werden die gespeicherten Einstellungen
übernommen: Der Startbildschirm zeigt `SETTINGS CARRIED OVER FROM AN UPDATE`
und `CHECK MENU > SETTINGS, THEN SAVE` in Bernstein über dem Namen. Prüfe
sie in SETTINGS und halte `HOLD TO SAVE`
([Fehlersuche](Fehlersuche#meine-einstellungen-setzen-sich-zurück)).

Nach einem Update von einem Format, das die Firmware nicht kennt, werden die
gespeicherten Einstellungen verworfen, und jede Einstellung beginnt mit ihrer
Voreinstellung. Der Startbildschirm zeigt dann über dem Namen zwei Zeilen in
Bernstein: `SETTINGS RESET BY UPDATE` und `CHECK MENU > SETTINGS`. Prüfe
mindestens Polzahl, `MAX THROTTLE`, `ESC PIN`, KISS, `SERVO SUPPLY` und
`CURRENT SENSE` und halte dann `HOLD TO SAVE`. Bis ein Speichern gelingt,
zeigt die Zeile SETTINGS in MENU `UPDATED`, nach einer Übernahme wie nach
einem Verwerfen. Eine Platine, die nie Einstellungen gespeichert hatte,
zeigt nichts davon.

<img src="img/splash-reset.png" width="240" alt="Startbildschirm, nachdem ein Update die gespeicherten Einstellungen verworfen hat">

Hat der Touch-Controller beim Einschalten nicht geantwortet, zeigt der
Startbildschirm über dem Namen in Bernstein `TOUCH CONTROLLER NOT FOUND`. Die
Zeile verschwindet, sobald der Controller antwortet. Steht sie noch, wenn der
Tester erscheint, funktioniert die Berührung nicht; siehe
[Fehlersuche](Fehlersuche#der-bildschirm-reagiert-nicht-auf-berührung).

<img src="img/splash-no-touch.png" width="240" alt="Der Startbildschirm mit TOUCH CONTROLLER NOT FOUND in Bernstein über dem Namen">

---

## Solange kein ESC angeschlossen ist

<img src="img/tester-noesc.png" width="240" alt="Tester-Bildschirm ohne ESC">

Antwortet kein ESC, zeigt der Bildschirm `NO TELEMETRY` unter der Drehzahl,
`--` in den fünf ESC-Kacheln und eine dunkelrote Null als Drehzahl. Die große
Spannung oben rechts, beschriftet mit `BOARD`, ist der platineneigene Akku,
nicht der des ESC; die Spannung des ESC steht in der Kachel `VOLTAGE`.

Ändert sich das trotz versorgtem und verkabeltem ESC nicht, geht es bei
[Fehlersuche](Fehlersuche#keine-telemetrie) weiter.

---

## Das erste Scharfschalten

<img src="img/tester-idle.png" width="240" alt="Der Tester entschärft: SAFE in Grün, ARM TO ENABLE im Gasbalken">

Entschärft zeigt das Feld `SAFE`, und der Gasbalken zeigt `ARM TO ENABLE`.

1. Prüfe, dass der Propeller ab ist.
2. `HOLD TO ARM` drücken und 1 s **halten**. Währenddessen füllt sich ein
   grüner Balken am unteren Rand des Felds.
3. Das Feld wird rot und zeigt `ARMED`. Die Hintergrundbeleuchtung sinkt für
   120 ms auf ein Viertel, damit du das Scharfschalten siehst, während du auf
   den Motor schaust.

<img src="img/tester-armed.png" width="240" alt="Scharf, mit eintreffender Telemetrie">

Zum Stoppen `DISARM` drücken. Es wirkt beim Berühren. Der Druck, der
entschärft, schaltet nicht wieder scharf, auch wenn der Finger liegen bleibt.
Zum Scharfschalten braucht es einen neuen Druck, der auf der Schaltfläche
beginnt.

**Den Gas-Finger vor `DISARM` heben.** Die Firmware liest nur den ersten
Berührungsplatz des Touch-Controllers. Welche Berührung der Controller dort
meldet, während zwei Finger auf dem Glas liegen, ist nicht gemessen; ein
Druck, während der Gas-Finger aufliegt, kommt also womöglich nicht an. Ein
nasser Bildschirm oder ein Kabel, das auf dem Glas liegt, kann als Finger
zählen und jedes Tippen blockieren.

Ein Finger, der schon auf dem Glas liegt, wenn der Tester erscheint,
schaltet nicht scharf und bewegt das Gas nicht. Wird er gehoben, wirkt das
trotzdem als Tippen auf eine Schaltfläche, die beim Loslassen auslöst, etwa
`HOLD` oder `MENU`, wenn er darauf liegt. Hast du beim Einschalten den
Bildschirm berührt, hebe den Finger vor `HOLD TO ARM`.

---

## Gas geben

Zwei Flächen stellen denselben Gaswert ein.

**Der Balken unten.** Mit `HOLD` aus ist die Fingerposition auf der Bahn das
Gas, und beim Loslassen geht das Gas auf null. Die volle Breite der Bahn ist
die *Obergrenze*: Bei den voreingestellten 20 % gibt das rechte Ende 20 %.

**Die Zahlenfläche** über dem Balken ist ein relatives Gas-Pad. Nach **oben**
wischen für mehr, nach **unten** für weniger. Ein Tippen bewirkt nichts, und
Wischer summieren sich. So lässt sich das Gas über mehrere kurze Wischer
erhöhen, ohne hinzusehen. Nach 6 px Bewegung wechselt der Hinweis unter der
Drehzahl auf `SWIPE ACTIVE`, und der Wischer bewegt das Gas. Steht an seiner
Stelle der 3D-Hinweis aus [Die Bildschirme](Die-Bildschirme), ersetzt
`SWIPE ACTIVE` ihn trotzdem, solange ein Wischer greift, und nach dem
Loslassen kehrt der 3D-Hinweis zurück.

`HOLD` macht den Balken relativ und haltend: Das Gas bleibt, wo du es
loslässt. Der Balken zeigt einen Griff. `HOLD` auszuschalten setzt das Gas auf
null.

---

## Die Obergrenze anheben

`MENU` auf dem Tester öffnet die Liste der Werkzeuge. Das Öffnen entschärft
immer. Tippe auf die Zeile `SETTINGS`.

<img src="img/settings.png" width="240" alt="SETTINGS nach einer Änderung: MOTOR POLES auf 16 und UNSAVED - HOLD SAVE TO KEEP">

- **MOTOR POLES**: rechnet die gemeldete eRPM (elektrische Drehzahl) mit
  `RPM = eRPM / (Pole / 2)` in die Drehzahl um. Bereich 2–28, nur gerade
  Werte. Voreinstellung 14. Ein falscher Wert verfälscht die Drehzahl um einen
  festen Faktor; andere Werte sind nicht betroffen.
- **MAX THROTTLE**: die Gas-Obergrenze, 5–100 % in Schritten von 5 %. `-` oder
  `+` gedrückt halten wiederholt den Schritt, mit der Zeit schneller.

Beide Zeilen stehen oben in der Liste. Beide Werte werden erst mit
`HOLD TO SAVE` unter der Liste gespeichert. Bis dahin zeigt die Hinweiszeile
`UNSAVED - HOLD SAVE TO KEEP`, und `MENU` zeigt `UNSAVED` in der Zeile
`SETTINGS`.

`ESC PIN`, weiter unten, legt DShot sofort auf den anderen Pin. Solange es
nicht der Pin ist, den der Startbildschirm nannte, wiederholt eine eigene
Zeile über `LINK` dessen Warnung für den neuen Pin in Bernstein,
`UNPLUG ANY SERVO FROM GP1 NOW`; die Hinweiszeile darunter sagt weiter, ob die
Einstellungen gespeichert sind.

<img src="img/settings-pin.png" width="240" alt="SETTINGS, nachdem ESC PIN auf GP1 gewechselt hat: UNPLUG ANY SERVO FROM GP1 NOW über LINK, UNSAVED in der Hinweiszeile">

---

## Weiter

- [Die Bildschirme](Die-Bildschirme): der Rest der Oberfläche
- [Telemetrie](Telemetrie): was die Kacheln zeigen
- [SD-Aufzeichnung](SD-Aufzeichnung): einen Lauf aufzeichnen und auswerten
