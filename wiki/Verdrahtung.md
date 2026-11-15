[English](Wiring) | **Deutsch**

# Verdrahtung

Ein Blatt mit jeder Verbindung, die der Prüfstand nutzt: die Steckverbinder
der Platine Pin für Pin und drei Arten, ein Gerät daran anzuschließen. Auf
SIG hängt immer nur ein Gerät. Die Massen werden einmal verbunden, an der
Platine.

![Verdrahtung: die Steckverbinder der Platine, ein ESC, ein Servo an einer von Hand geschalteten Versorgung und ein Servo an der geschalteten Versorgung](img/wiring.svg)

Die Platine hat JST-SH-Steckverbinder mit 1,0 mm Raster und keine
2,54-mm-Stiftleisten, daher braucht jedes Kabel einen Adapter mit
JST-SH-Stecker. SIG ist GP0 (J5 Pin 2) und AUX ist GP1 (J5 Pin 1) auf einer
Platine mit leerem Flash; beide lassen sich unter **MENU → SETTINGS → SIG
PIN** und **AUX PIN** ändern, die den J5-Pin unter dem Wert zeigen. Ein Schritt
einer der beiden tauscht SIG und AUX, sodass beide nie einen Pin teilen.

| Teil | Was er zeigt | Details |
|---|---|---|
| Steckverbinder der Platine | J5, J4 und J3, jeder Pin mit seiner Aufgabe. J4 Pin 7 und 8 und J3 Pin 1 und 2 sind der I2C-Bus (Inter-Integrated Circuit), den Touch-Controller (0x1A), IMU (Inertial Measurement Unit, Inertialsensor, 0x6B) und RTC (Real-Time Clock, Echtzeituhr, 0x51) teilen. J3 ist der Steckverbinder des optionalen INA219-Stromsensors, auf 0x40: SCL, SDA, seine 3,3 V und seine Masse, sonst nichts. J4 Pin 7 und 8 bleiben frei | [Servotester](Servotester#strom) |
| A | Ein ESC (Electronic Speed Controller, Motorregler): Signal an SIG, Masse an GND, das optionale KISS-Telemetriekabel an AUX. Die mittlere Ader des ESC-Kabels, die BEC-Ausgangsspannung (Battery Eliminator Circuit) von +5 V, ist durchtrennt | [Erste Schritte](Erste-Schritte#esc-anschließen), [Telemetrie](Telemetrie#das-optionale-dritte-kabel) |
| B | Ein Servo an seiner eigenen Versorgung, mit einem Schalter in der Plusleitung. Das zweite Servo des Servotesters kommt an AUX | [Servotester](Servotester#verkabelung), [KST-Programmierung](KST-Programmierung#verkabelung), [Hitec-Programmierung](Hitec-Programmierung#verkabelung) |
| C | Ein Servo an einem Aufwärtswandler (Boost), den der Prüfstand schaltet: 5 V Ausgang, mindestens 3 A, mit Ausgangstrennung, aus einer eigenen LiPo-Zelle (Lithium-Polymer) mit einer Sicherung an der Zelle. Sein Enable-Eingang (EN) an GP28 (J4 Pin 11) oder GP29 (J4 Pin 12), mit einem Pull-down von EN nach GND am Wandler. SIG, EN und GND kommen alle von J4. **MENU → SETTINGS → SERVO SUPPLY** auf den benutzten Pin stellen. Der optionale INA219 sitzt in der Ausgangsleitung des Wandlers, VIN+ von VOUT, VIN- zum Plus des Servos, seine Stiftleiste über ein Adapterkabel an J3 | [Servotester](Servotester#die-geschaltete-versorgung), [KST-Programmierung](KST-Programmierung#geschaltete-versorgung), [Hitec-Programmierung](Hitec-Programmierung#geschaltete-versorgung), [Servotester](Servotester#strom) |

> **Die mittlere Ader eines Servos oder ESC nie an einen Pin der
> Steckverbinder legen.** J4 Pin 2 ist die 5-V-USB-Spannung (Universal Serial
> Bus) der Platine und versorgt kein Servo und keinen ESC. Die 3,3 V an J3
> versorgen den optionalen INA219-Stromsensor
> ([Servotester](Servotester#strom)); die anderen 3,3-V-Pins
> bleiben frei.

> **Warnung: Ein Servo- oder ESC-Kabel nie 1:1 an J5 anschließen.**
> J5 Pin 2, 3 und 4 sind SIG, 3,3 V und GND: dieselbe Reihenfolge wie bei
> einem Servostecker, Signal, Plus und Minus. Pin 3 ist die 3,3-V-Versorgung
> der Platine. 1:1 verbunden, bekommt er die BEC-Ausgangsspannung
> (Battery Eliminator Circuit) des ESC, +5 V, oder die Versorgung des Servos,
> 4,8 V bis 6,0 V oder 8,4 V für ein Hochvolt-Servo, und das kann die Platine
> zerstören. J5 Pin 3 bleibt frei.

**Der Wandler.** Ein Aufwärtswandler gibt seinen Eingang an den Ausgang
weiter, solange er aus ist, außer er trennt die Last ab: Nur ein
synchroner Aufwärtswandler mit Ausgangstrennung kommt in Frage, ein Modul
ohne EN nicht. Aus einer Zelle, 3,0 V bis 4,2 V, bleibt ein
Aufwärtswandler über die ganze Entladung unter seinen 5 V am Ausgang. Aus
einem Akku mit zwei Zellen oder einem Labornetzteil tut es ein Abwärts-
oder ein Auf-/Abwärtswandler, etwa der Pololu S13V30F5. Das Modul von Teil
C ist nicht benannt: Die 5-V-Boost-Platine in den mechanischen Modellen des
Projekts nennt keinen Chip, daher sind ihre Ausgangstrennung, ihre
EN-Schwelle und ihre Strombegrenzung unbekannt, bis sie am Prüfstand
gemessen sind.

**Der Pull-down.** Er sitzt am Wandler, von EN nach GND, damit EN low bleibt,
wenn das EN-Kabel an einem Steckverbinder abgeht.

- Ein EN ohne internen Pull-up: 10 kΩ bis 100 kΩ.
- Ein EN mit internem Pull-up nach VIN, der Eingangsspannung des Wandlers:
  Beide bilden einen Spannungsteiler. Bei der höchsten benutzten VIN muss
  der Pull-down EN unter der Ausschaltschwelle des Wandlers und auf
  höchstens 3,6 V halten, und auf höchstens 0,5 V, solange die Platine aus
  ist. An einer Zelle, höchstens 4,2 V, sind das höchstens 0,135 mal der
  Pull-up: 13,5 kΩ bei einem Pull-up von 100 kΩ.
- Das EN des Pololu S13V30F5 hat einen Pull-up von 475 kΩ. Für ihn ist der
  Pull-down 10 kΩ: EN liegt bei 22 V auf 0,45 V. 100 kΩ legen bei 12 V
  2,09 V an EN, und der Wandler bleibt auch außerhalb der Servotester-, KST-
  und Hitec-Bildschirme an.

Die Regel steht im README-Abschnitt
[KST servo programming](https://github.com/subtilitas/MiniRcBench#kst-servo-programming),
die Rechnung in den Entwurfsnotizen `docs/design/kst-programming.md`,
"Example modules", und `docs/design/current-sense.md`, "Enable and its
pull-down".
