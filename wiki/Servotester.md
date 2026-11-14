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
> Platine mit einem Servo am ESC-Pin **startet**, wovor der Startbildschirm
> mit `UNPLUG ANY SERVO FROM GP0 NOW` warnt, und wenn du diesen Bildschirm
> verlässt. Der Bildschirm fordert dich vorher zum Abziehen auf.

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
  Platine SIG  ----------------------------------------  Signal Servo A (orange oder weiß)
  Platine AUX  ----------------------------------------  Signal Servo B, nur bei zwei Servos

  SIG GP0 = J4 Pin 9 oder J5 Pin 2, AUX GP1 = J4 Pin 10 oder J5 Pin 1,
  GND = J4 Pin 1 oder 5 oder J5 Pin 4
```

> **Ein Servokabel nie 1:1 an J5 anschließen.** J5 Pin 3 ist die
> 3,3-V-Versorgung der Platine, an der Stelle des Plus eines Servosteckers:
> siehe [Einrichtung](Einrichtung#das-adapterkabel).

- Das mittlere Kabel des Servos geht an seine Versorgung, nie an einen Pin
  der Platine. Der 5-V-Pin der Platine ist ihre USB-Versorgung, und ein Servo
  kann mehr als 1 A ziehen.
- Schalte zuerst die Platine ein und dann die Servoversorgung, und beim
  Ausschalten zuerst die Servoversorgung.
- Verbinde die beiden Massen einmal, an der Platine.
- Servo A hängt an SIG, dem ESC-Pin, Servo B an AUX, dem KISS-Pin, so wie
  unter **MENU → SETTINGS → SIG PIN** und **AUX PIN** eingestellt, die den
  J5-Pin unter dem Wert zeigen. Voreingestellt sind GP0 und GP1. AUX PIN
  wirkt, ob KISS an oder aus ist, und ein Schritt einer der beiden Zeilen
  tauscht die beiden.
- Zieh jeden ESC von SIG ab. Der Bildschirm wartet, solange ein ESC
  darauf antwortet, erkennt aber nicht jeden ESC.

---

## Öffnen

**MENU → SERVO TESTER** öffnen.

Der Bildschirm übernimmt die beiden Signalpins vom ESC-Treiber. Antwortet ein
ESC auf SIG, wartet er und zeigt `AN ESC ANSWERS ON SIG GP0.` Zieh den
ESC ab; 500 ms später übernimmt der Bildschirm die Pins von selbst.
Servoimpulse von 1000 µs bis 2000 µs sind für viele ESCs ein gültiges
Gassignal.

<img src="img/servo-esc.png" width="240" alt="Warten, solange ein ESC antwortet">

Der Bildschirm startet in **SAFE**: Die Pins liegen auf Low und führen keine
Impulse. Was ein Servo ohne Impulse tut, hängt vom Servo ab: Es hält die
Position, wird kraftlos oder fährt in seine eigene Failsafe-Stellung.

<img src="img/servo-safe.png" width="240" alt="Der Servobildschirm in SAFE">

Die Zeile unter dem Titel zeigt den Pin mit Rolle und Nummer, die Framerate
und den Impulsbereich, `SIG GP0  50 HZ  1000-2000 US`, oder
`SIG+AUX  50 HZ  1000-2000 US` mit zwei Servos, dazu eine Statusanzeige:
`SAFE` (grün), `RUNNING` (rot) oder `STALLED` (magenta). Bei 333 Hz mit
einem MIN ab 1000 µs hat die Zeile keinen Platz für die Nummer und zeigt
`SIG  333 HZ  1000-2000 US`, oder `SIG+AUX  333 HZ` mit zwei Servos.

In SAFE mit einem Servo steht rechts in der Anzeigezeile `TWO SERVOS:` und
`SET > SERVOS`: Die Zeile SERVOS des SET-Bildschirms schaltet Servo B ein
(siehe [Zwei Servos](#zwei-servos)). Solange AUX verweigert ist, fehlt der
Hinweis.

Steht **SERVO SUPPLY** unter SETTINGS auf `GP28` oder `GP29`, zeigt eine
zweite Zeile unter dem Titel `GP28 SUPPLY STAYS OFF HERE.` in Bernstein. Der
Servotester schaltet den Wandler des KST- und des Hitec-Programmierers nie
ein; ein Servo, das nur diesen Wandler als Versorgung hat, bekommt auf diesem
Bildschirm keinen Strom. Gib ihm eine eigene Versorgung wie unter
[Verkabelung](#verkabelung).

<img src="img/servo-supply.png" width="240" alt="Der Servobildschirm mit SERVO SUPPLY auf GP28: GP28 SUPPLY STAYS OFF HERE. unter dem Titel">

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

Stell auf dem SET-Bildschirm `SERVOS` auf `TWO`. Servo B hängt an AUX.
Beide teilen sich ein Profil und eine Framerate.

<img src="img/servo-two.png" width="240" alt="Zwei Servos">

`A`, `B` und `AB` wählen, was Skala und Tasten bewegen: ein Servo oder beide
auf denselben Winkel. Das gewählte Horn ist breit gezeichnet. Mit
`REVERSE B` läuft B gegenläufig, wenn beide folgen, für ein Paar, das
spiegelbildlich eingebaut ist.

Zwei Servos brauchen ein freies AUX. Kommen dort KISS-Telegramme an, bietet
der Bildschirm nur ein Servo an, und der SET-Bildschirm nennt den Grund:
`KISS WIRE ON AUX GP1 IS LIVE.` SETTINGS legt SIG und AUX nie auf einen Pin;
auf anderem Weg so gesetzte Pins zeigen `SIG AND AUX ARE THE SAME GPIO.`
Solange der Bildschirm auf die Pins wartet, etwa weil ein ESC antwortet, weiß
er nicht, ob AUX frei ist, und `TWO` lässt sich wählen. Ist AUX dann belegt,
übernimmt der Bildschirm nur SIG und schaltet
auf ein Servo zurück; `TWO` bleibt aus, bis du den Bildschirm erneut öffnest.
Das gilt auch für ein `TWO`, das noch auf `HOLD TO APPLY` wartet: Es geht auf
ein Servo zurück, und ein Warten, das nur für `TWO` bestand, endet.

<img src="img/servo-samepin.png" width="240" alt="TWO nicht verfügbar: beide Pins sind derselbe GPIO">

---

## Strom

Mit einem INA219-Strommessmodul an J3 und SETTINGS → CURRENT SENSE auf
`INA219` zeigt die Zeile unter der Skala den Strom der Servoversorgung,
`0.42 A`, und darunter die Spannung auf der Servoseite des Sensors,
`5.02 V`. Der Sensor ist optional und nicht auf Hardware getestet. Seine
Verkabelung steht in der Entwurfsnotiz `docs/design/current-sense.md`,
"Place in the circuit": VCC des Moduls an J3 Pin 3, 3,3 V, nie an 5 V, und
der Strom des Servos nur über die Schraubklemme des Moduls.

<img src="img/servo-sense.png" width="240" alt="Der Servobildschirm mit Strom und Spannung rechts vom Impuls">

- Mit zwei Servos an einer Versorgung zeigt die Zeile ihre Summe,
  `A+B 0.84 A`.
- `>3.2 A` in Bernstein: Der Strom liegt am Ende des Messbereichs, 3,2 A,
  oder darüber; der wahre Wert ist unbekannt.
- `-- A`: seit 250 ms kein Messwert. Der Sensor fehlt oder der Bus
  versagt; SETTINGS zeigt `NOT FOUND` unter CURRENT SENSE.
- Während ein Sweep läuft, steht sein Zähler oben auf der Skala.

Der Strom wird 40-mal pro Sekunde gelesen, jeder Wert als Mittel über 17 ms.

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
- Ein Puls über den Stellweg eines Servos hinaus fährt es gegen seinen
  Endanschlag, wo es blockiert und heiß wird. Mit TYPE WIDE, MIN unter 900 µs
  oder MAX über 2100 µs zeigt der Bildschirm in Bernstein
  `PAST ITS TRAVEL A SERVO STALLS AND HEATS. TRY MIN AND MAX HORN-FREE.` (über
  den Stellweg hinaus blockiert ein Servo und wird heiß; MIN und MAX zuerst ohne
  Ruderhorn probieren). HELI CYCLIC zeigt ihn schon mit seinen eigenen 820 µs
  bis 2220 µs. NARROW 760 und HELI TAIL 760 werden an ihrem eigenen Bereich
  gemessen: Der Hinweis steht, wenn MIN mehr als 100 µs unter dem MIN des Typs
  liegt oder MAX mehr als 100 µs über seinem MAX. Eine Änderung von TYPE, MIN
  oder MAX, nach der das Profil über den Stellweg hinaus reicht, wartet, wenn
  sie TYPE wechselt oder MIN oder MAX weiter hinaus legt als im übernommenen
  Profil: Der Hinweis ist ihre Warnung, auf jeder Zeile, und `HOLD TO APPLY`
  (1 s) übernimmt sie. Weitere Schritte hinaus lassen sie warten; ein Schritt
  zurück innerhalb der Grenze oder auf den übernommenen Wert gilt sofort, wenn
  sonst nichts wartet. Ein Schritt zur Mitte hin gilt sofort. `BACK` verwirft
  eine wartende Änderung. Eine Heli- oder 60-Hz-Warnung steht zuerst, und bei
  HELI CYCLIC übernimmt ein Halten beides. Mit zwei Servos gilt das Halten für
  beide. Der Wechsel von SERVOS ONE auf TWO wartet bei einem Profil über den
  Stellweg hinaus ebenfalls auf das Halten, denn er treibt dort ein zweites
  Servo an; bei einem Profil innerhalb des Stellwegs und von TWO auf ONE gilt er
  sofort. Nach dem Übernehmen steht der Hinweis, solange TYPE, MIN oder MAX
  gewählt ist, und auf anderen Zeilen, wenn dort kein anderer Hinweis steht. Den
  Stellweg nennt das Datenblatt des Servos; der Prüfstand misst ihn nicht.
- Die SWEEP-Zeilen lassen sich jederzeit ändern und gelten für den nächsten
  `SWEEP`. Ist `SPEED` langsamer als die Kurve, zeigt der Bildschirm
  `SPEED LIMITS THE SWEEP`.
- `DEFAULTS` gibt es nur in `SAFE`. Es stellt STANDARD PWM mit 50 Hz, ein
  Servo und einen Sinus mit 0,50 Hz ein.

<img src="img/servo-set-warning.png" width="240" alt="Ein Heli-Profil wartet auf HOLD TO APPLY"> <img src="img/servo-set-travel.png" width="240" alt="TYPE WIDE wartet hinter dem Endanschlag-Hinweis auf HOLD TO APPLY">

Auf diesem Bildschirm wird nichts gespeichert. Jedes Mal, wenn du den
Servobildschirm öffnest, beginnt er mit STANDARD PWM, 50 Hz und einem Servo.

---

## Verlassen

`BACK` beendet die Impulse und fordert dich auf, das Servo abzuziehen. Der
letzte Impuls läuft vorher vollständig aus; bei 50 Hz dauert das bis zu
41 ms. Danach hält ein schwacher Pull-down SIG auf Low, bis DShot wieder
läuft. Mit zwei Servos hält er auch AUX auf Low, bis der KISS-Empfänger ihn
übernimmt; ist KISS aus, bleibt er auf Low.

<img src="img/servo-unplug.png" width="240" alt="UNPLUG THE SERVO">

Zieh das Servo ab und tippe dann auf `RESTART DSHOT`. Der ESC-Treiber
übernimmt die Pins wieder, und `MENU` erscheint. `BACK`
auf dieser Seite führt stattdessen zurück zum Servobildschirm, in SAFE, und
behält die Pins. Bis der letzte Impuls ausgelaufen ist, zeigt die Seite
`RELEASING THE PINS`, und `BACK` ist deaktiviert gezeichnet, als Umriss mit
grauer Beschriftung: Es wirkt ab dem Moment, in dem `UNPLUG THE SERVO.`
erscheint.

---

## Wenn es nicht funktioniert

- **`AN ESC ANSWERS ON SIG GP0.`** Ein ESC hängt an SIG, dem ESC-Pin. Zieh
  ihn ab.
- **`SIG GP0 COULD NOT BE TAKEN (PWM)`.** Die Platine konnte ihren
  Impulsausgang nicht einrichten. Tippe auf `RETRY`.

  <img src="img/servo-bind.png" width="240" alt="SIG GP0 COULD NOT BE TAKEN (PWM) mit RETRY">

- **`TWO` lässt sich nicht wählen.** Das KISS-Kabel an AUX ist aktiv, oder
  beide Pins wurden auf anderem Weg als über SETTINGS auf einen Pin gesetzt. Der SET-Bildschirm nennt den Grund,
  wenn `SERVOS` ausgewählt ist.
- **Das Servo bewegt sich nicht.** Prüfe, ob die Statusanzeige `RUNNING`
  zeigt, ob das Servo eine eigene Versorgung hat und ob seine Masse mit der
  der Platine verbunden ist. Ob ein Servo die 3,3-V-Impulse der Platine
  annimmt, ist nicht für jedes Servo erfasst. Steht
  `GP28 SUPPLY STAYS OFF HERE.` unter dem Titel, ist der geschaltete Wandler
  auf diesem Bildschirm aus.

Der Servotester ist noch nicht auf Hardware getestet.
