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
Platine mit leerem Flash; beide lassen sich unter **MENU → SETTINGS** ändern.

| Teil | Was er zeigt | Details |
|---|---|---|
| Steckverbinder der Platine | J5, J4 und J3, jeder Pin mit seiner Aufgabe. J4 Pin 7 und 8 und J3 Pin 1 und 2 sind der gemeinsame I2C-Bus (Inter-Integrated Circuit) des Touch-Controllers: nicht benutzen | |
| A | Ein ESC (Electronic Speed Controller, Motorsteller): Signal an SIG, Masse an GND, das optionale KISS-Telemetriekabel an AUX. Die mittlere Ader des ESC-Kabels, die BEC-Ausgangsspannung (Battery Eliminator Circuit) von +5 V, ist durchtrennt | [Erste Schritte](Erste-Schritte#esc-anschließen), [Telemetrie](Telemetrie#das-optionale-dritte-kabel) |
| B | Ein Servo an seiner eigenen Versorgung, mit einem Schalter in der Plusleitung. Das zweite Servo des Servotesters kommt an AUX | [Servotester](Servotester#verkabelung), [KST-Programmierung](KST-Programmierung#verkabelung), [Hitec-Programmierung](Hitec-Programmierung#verkabelung) |
| C | Ein Servo an einem DC-DC-Wandler (Gleichspannungswandler), den der Prüfstand schaltet: sein Enable-Eingang (EN) an GP28 (J4 Pin 11) oder GP29 (J4 Pin 12), mit einem Pull-down von EN nach GND am Wandler. SIG, EN und GND kommen alle von J4. **MENU → SETTINGS → SERVO SUPPLY** auf den benutzten Pin stellen | [KST-Programmierung](KST-Programmierung#geschaltete-versorgung), [Hitec-Programmierung](Hitec-Programmierung#geschaltete-versorgung) |

> **Die mittlere Ader eines Servos oder ESC nie an einen Pin der
> Steckverbinder legen.** J4 Pin 2 ist die 5-V-USB-Spannung (Universal Serial
> Bus) der Platine und versorgt kein Servo und keinen ESC; die 3,3-V-Pins
> bleiben frei.

**Der Pull-down.** Er sitzt am Wandler, von EN nach GND, damit EN low bleibt,
wenn das EN-Kabel an einem Steckverbinder abgeht.

- Ein EN ohne internen Pull-up: 10 kΩ bis 100 kΩ.
- Ein EN mit internem Pull-up nach VIN, etwa die 475 kΩ des Pololu
  S13V30F5: Beide bilden einen Spannungsteiler. Bei der höchsten benutzten
  VIN muss der Pull-down EN unter der Ausschaltschwelle des Wandlers halten,
  bei höchstens 3,6 V und bei höchstens 0,5 V, solange die Platine aus ist.
  Für das Pololu-Modul sind das 10 kΩ: EN liegt bei 22 V auf 0,45 V.
  100 kΩ legen bei 12 V 2,09 V an EN, und der Wandler bleibt auch außerhalb
  der KST- und Hitec-Bildschirme an.

Die Rechnung steht im README-Abschnitt
[KST servo programming](https://github.com/subtilitas/MiniRcBench#kst-servo-programming)
und in der Entwurfsnotiz `docs/design/kst-programming.md`, "Example modules".
