[English](Servo-Tester) | **Deutsch**

# Servotester

Dieser Bildschirm steuert ein Servo oder zwei gleichzeitig mit üblichen
Servoimpulsen an: ein High-Pegel von 400 µs bis 2500 µs, 50- bis 560-mal pro
Sekunde wiederholt. Du bewegst das Servo, indem du das Servohorn auf dem
Bildschirm ziehst, mit drei Tasten oder mit einem Sweep. Das Servo wird nicht
vermessen: Es gibt keine Positionsrückmeldung. Das Horn auf dem Bildschirm
zeigt, wohin der Impuls das Servo schickt, nicht, wo das Servo steht.

<img src="img/servo-running.png" width="240" alt="Der Servobildschirm im Betrieb">

> **Zieh das Servo ab, bevor DShot läuft.** Außerhalb dieses Bildschirms liegt
> auf dem ESC-Pin DShot. Ein Servo an diesem Pin liest das als Impuls von etwa
> 970 µs mit 1000 Hz: Es fährt Richtung eines Endanschlags, mit einer
> Framerate, bei der ein analoges Servo überhitzt. Das passiert, wenn die
> Platine mit einem Servo am ESC-Pin **startet**, und wenn du diesen
> Bildschirm verlässt. Der Bildschirm fordert dich vorher zum Abziehen auf.

---

## Verkabelung

Das Servo braucht eine eigene Stromversorgung. Die Platine liefert nur Signal
und Masse.

```
  Servoversorgung, 4,8 V bis 6,0 V (8,4 V bei einem HV-Servo, Hochvolt)
     +  ----------------------------------------------  Servo +   (rot, Mitte)
     -  ----------------+-----------------------------  Servo -   (braun oder schwarz)
                        |
  Platine GND  ---------+
  Platine GP29 ----------------------------------------  Signal Servo A (orange oder weiß)
  Platine GP28 ----------------------------------------  Signal Servo B, nur bei zwei Servos

  GP29 = J4 Pin 12, GP28 = J4 Pin 11, GND = J4 Pin 1 oder 5
```

- Das mittlere Kabel des Servos geht an seine Versorgung, nie an einen Pin
  der Platine. Der 5-V-Pin der Platine ist ihre USB-Versorgung, und ein Servo
  kann mehr als 1 A ziehen.
- Schalte zuerst die Platine ein und dann die Servoversorgung, und beim
  Ausschalten zuerst die Servoversorgung.
- Verbinde die beiden Massen einmal, an der Platine.
- Servo A hängt am ESC-Pin, Servo B am KISS-Pin, so wie unter
  **MENU → SETTINGS** eingestellt. Voreingestellt sind GP29 und GP28.
- Zieh jeden ESC vom ESC-Pin ab. Der Bildschirm wartet, solange ein ESC
  darauf antwortet, erkennt aber nicht jeden ESC.

---

## Öffnen

**MENU → SERVO TESTER** öffnen.

Der Bildschirm übernimmt die beiden Signalpins vom ESC-Treiber. Antwortet ein
ESC auf dem ESC-Pin, wartet er und zeigt `AN ESC ANSWERS ON GP29.` Zieh den
ESC ab; 500 ms später übernimmt der Bildschirm die Pins von selbst.
Servoimpulse von 1000 µs bis 2000 µs sind für viele ESCs ein gültiges
Gassignal.

<img src="img/servo-esc.png" width="240" alt="Warten, solange ein ESC antwortet">

Der Bildschirm startet in **SAFE**: Die Pins liegen auf Low und führen keine
Impulse. Was ein Servo ohne Impulse tut, hängt vom Servo ab: Es hält die
Position, wird kraftlos oder fährt in seine eigene Failsafe-Stellung.

<img src="img/servo-safe.png" width="240" alt="Der Servobildschirm in SAFE">

Die Zeile unter dem Titel zeigt den Pin, die Framerate und den Impulsbereich,
dazu eine Statusanzeige: `SAFE` (grün), `RUNNING` (rot) oder `STALLED`
(magenta).

---

## Starten und Stoppen

**Halte `HOLD TO START` 1 s lang.** Während du hältst, füllt sich ein Balken.
Danach fährt jedes angeschlossene Servo sofort in die Mitte (CENTRE), egal,
was es vorher bekommen hat. Die Statusanzeige zeigt `RUNNING`, und die Taste
wird zu `STOP`.

**`STOP` wirkt beim Berühren.** Die Impulse enden mit dem nächsten Frame: Der
letzte Impuls endet innerhalb eines Frames, bei 50 Hz also innerhalb von
20 ms. Wer `STOP` berührt und den Finger liegen lässt, startet das Servo nicht
erneut; dafür braucht es einen neuen Druck.

Reagiert der Bildschirm 250 ms lang nicht, enden die Impulse von selbst, die
Statusanzeige zeigt `STALLED`, und der Bildschirm meldet
`STOPPED: SCREEN STALLED`. Nichts startet neu, bis du wieder
`HOLD TO START` hältst.

<img src="img/servo-stalled.png" width="240" alt="Gestoppt nach einem Hänger">

---

## Das Servo bewegen

| Bedienelement | Wirkung |
|---|---|
| Auf die Skala drücken und ziehen | Das Servo fährt auf den Winkel unter deinem Finger: links MIN, senkrecht CENTRE, rechts MAX. Über die Enden hinaus bleibt es bei MIN oder MAX |
| Loslassen | Das Servo bleibt, wo es ist |
| `MIN` `CENTRE` `MAX` | Beim Loslassen an dieses Ende oder in die Mitte |
| `SWEEP` | Ein Sweep um die Mitte; die Taste wird zu `STOP SWEEP` |
| `SPEED` | Wie schnell das Servo bewegt wird, 10 % bis 100 % |

- Skala und Tasten wirken nur im Zustand `RUNNING`; in `SAFE` sind sie als
  Umriss gezeichnet und tun nichts.
- Ein Ziehen muss auf der Skala beginnen. Ein Druck, der woanders beginnt und
  auf die Skala gleitet, bewegt nichts.
- Bei `SPEED` 100 % geht jede neue Position sofort hinaus, und das Servo fährt
  mit seiner eigenen Geschwindigkeit. Unter 100 % wandert der Impuls
  allmählich: Bei 10 % dauert der ganze Weg von MIN nach MAX 5 s.
- Ein Sweep beginnt in der Mitte und läuft auf dem zweiten Kern der Platine,
  daher bremst der Bildschirm ihn nicht. Während er läuft, zeigt die Anzeige
  `SWEEP 3` oder `SWEEP 3/10`: die erreichten Enden und die eingestellte
  Anzahl. `STOP SWEEP` lässt das Servo dort, wo die Kurve gerade war. Auch
  Ziehen, `MIN`, `CENTRE`, `MAX` und `STOP` beenden einen Sweep.

<img src="img/servo-sweep.png" width="240" alt="Ein laufender Sweep">

---

## Zwei Servos

Stell auf dem SET-Bildschirm `SERVOS` auf `TWO`. Servo B hängt am KISS-Pin.
Beide teilen sich ein Profil und eine Framerate.

<img src="img/servo-two.png" width="240" alt="Zwei Servos">

`A`, `B` und `AB` wählen, was Skala und Tasten bewegen: ein Servo oder beide
auf denselben Winkel. Das gewählte Horn ist breit gezeichnet. Mit
`REVERSE B` läuft B gegenläufig, wenn beide folgen, für ein Paar, das
spiegelbildlich eingebaut ist.

Zwei Servos brauchen einen freien KISS-Pin. Kommen dort KISS-Telegramme an,
oder sind ESC-Pin und KISS-Pin unter SETTINGS derselbe Pin, bietet der
Bildschirm nur ein Servo an, und der SET-Bildschirm nennt den Grund.
Solange der Bildschirm auf die Pins wartet, etwa weil ein ESC antwortet, weiß
er nicht, ob der KISS-Pin frei ist, und `TWO` lässt sich wählen. Ist der
KISS-Pin dann belegt, übernimmt der Bildschirm nur den ESC-Pin und schaltet
auf ein Servo zurück; `TWO` bleibt aus, bis du den Bildschirm erneut öffnest.

<img src="img/servo-samepin.png" width="240" alt="TWO nicht verfügbar: beide Pins sind derselbe GPIO">

---

## Der SET-Bildschirm

`SET` öffnet die Einstellungen. Das Servo läuft darunter weiter.

<img src="img/servo-set.png" width="240" alt="Der SET-Bildschirm">

Tippe auf eine Zeile, um sie auszuwählen. `-` und `+` ändern sie um einen
Schritt und wiederholen, solange du hältst. Wische die Zeile seitwärts für
große Schritte. Zieh nach oben oder unten, um zu blättern. Die Zeilen sind 40 px hoch, es
sind also drei auf einmal sichtbar.

| Zeile | Werte |
|---|---|
| SERVOS | ONE oder TWO |
| TYPE | STANDARD PWM, NARROW 760, WIDE, HELI CYCLIC, HELI TAIL 760 |
| RATE | die Frameraten, die der Typ anbietet, bis zu dem, was der Bereich zulässt |
| MIN, CENTRE, MAX | 400 µs bis 2500 µs, Schritte von 10 µs, in dieser Reihenfolge |
| REVERSE B | OFF oder ON |
| SHAPE | SQUARE, SINE oder TRIANGLE |
| RATE (Sweep) | 0,05 Hz bis 5,00 Hz |
| AMPLITUDE | 0 % bis 100 % des halben Wegs |
| DWELL | 0 ms bis 5000 ms an jedem Ende |
| MOVES | ENDLESS oder 1 bis 9999 Enden |

- Die PULSE-Zeilen lassen sich nur in `SAFE` ändern. In `RUNNING` zeigt der
  Bildschirm `STOP TO CHANGE THE PROFILE` und ihre Werte in Grau.

  <img src="img/servo-set-running.png" width="240" alt="SET im Betrieb: die PULSE-Zeilen gesperrt, STOP in der Fußzeile">

- In `RUNNING` wird `DEFAULTS` zu `STOP` und wirkt beim Berühren, wie auf dem
  Servobildschirm. Die SWEEP-Zeilen lassen sich weiter ändern.

- Ein Heli-Typ oder eine Framerate über 60 Hz kann ein Servo zerstören, das
  nicht dafür gebaut ist: Über 60 Hz überhitzt ein analoges Servo. Eine solche
  Änderung wartet, der Bildschirm nennt das Risiko, und `HOLD TO APPLY` (1 s)
  übernimmt sie. `BACK` verwirft sie.
- Die SWEEP-Zeilen lassen sich jederzeit ändern und gelten für den nächsten
  `SWEEP`. Ist `SPEED` langsamer als die Kurve, zeigt der Bildschirm
  `SPEED LIMITS THE SWEEP`.
- `DEFAULTS` gibt es nur in `SAFE`. Es stellt STANDARD PWM mit 50 Hz, ein
  Servo und einen Sinus mit 0,50 Hz ein.

<img src="img/servo-set-warning.png" width="240" alt="Ein Heli-Profil wartet auf HOLD TO APPLY">

Auf diesem Bildschirm wird nichts gespeichert. Jedes Mal, wenn du den
Servobildschirm öffnest, beginnt er mit STANDARD PWM, 50 Hz und einem Servo.

---

## Verlassen

`BACK` beendet die Impulse und fordert dich auf, das Servo abzuziehen. Der
letzte Impuls läuft vorher vollständig aus; bei 50 Hz dauert das bis zu
41 ms. Danach hält ein schwacher Pull-down den ESC-Pin auf Low, bis DShot
wieder läuft. Mit zwei Servos hält er auch den KISS-Pin auf Low, bis der
KISS-Empfänger ihn übernimmt; ist KISS aus, bleibt er auf Low.

<img src="img/servo-unplug.png" width="240" alt="UNPLUG THE SERVO">

Zieh das Servo ab und tippe dann auf `RESTART DSHOT`. Der ESC-Treiber
übernimmt die Pins wieder, und `MENU` erscheint. `BACK`
auf dieser Seite führt stattdessen zurück zum Servobildschirm, in SAFE, und
behält die Pins.

---

## Wenn es nicht funktioniert

- **`AN ESC ANSWERS ON GP29.`** Ein ESC hängt am ESC-Pin. Zieh ihn ab.
- **`GP29 COULD NOT BE TAKEN (PWM)`.** Die Platine konnte ihren
  Impulsausgang nicht einrichten. Tippe auf `RETRY`.

  <img src="img/servo-bind.png" width="240" alt="GP29 COULD NOT BE TAKEN (PWM) mit RETRY">

- **`TWO` lässt sich nicht wählen.** Das KISS-Kabel ist aktiv, oder beide
  Pins sind unter SETTINGS derselbe Pin. Der SET-Bildschirm nennt den Grund,
  wenn `SERVOS` ausgewählt ist.
- **Das Servo bewegt sich nicht.** Prüfe, ob die Statusanzeige `RUNNING`
  zeigt, ob das Servo eine eigene Versorgung hat und ob seine Masse mit der
  der Platine verbunden ist. Ob ein Servo die 3,3-V-Impulse der Platine
  annimmt, ist nicht für jedes Servo erfasst.

Der Servotester ist noch nicht auf Hardware getestet.
