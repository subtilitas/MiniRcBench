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
| Steckverbinder der Platine | J5, J4 und J3, jeder Pin mit seiner Aufgabe. J3 und J4 Pin 7 und 8 sind der gemeinsame I2C-Bus (Inter-Integrated Circuit) des Touch-Controllers: nicht benutzen | |
| A | Ein ESC (Electronic Speed Controller, Motorsteller): Signal an SIG, Masse an GND, das optionale KISS-Telemetriekabel an AUX. Die mittlere Ader des ESC-Kabels, die BEC-Ausgangsspannung (Battery Eliminator Circuit) von +5 V, ist durchtrennt | [Erste Schritte](Erste-Schritte#esc-anschließen), [Telemetrie](Telemetrie#das-optionale-dritte-kabel) |
| B | Ein Servo an seiner eigenen Versorgung, mit einem Schalter in der Plusleitung. Das zweite Servo des Servotesters kommt an AUX | [Servotester](Servotester#verkabelung), [KST-Programmierung](KST-Programmierung#verkabelung), [Hitec-Programmierung](Hitec-Programmierung#verkabelung) |
| C | Ein Servo an einem DC-DC-Wandler, den der Prüfstand schaltet: sein Freigabeeingang (EN) an GP28 (J4 Pin 11) oder GP29 (J4 Pin 12), mit 10 kΩ Pull-down zum Minus der Versorgung. **MENU → SETTINGS → SERVO SUPPLY** auf den benutzten Pin stellen | [KST-Programmierung](KST-Programmierung#verkabelung), [Hitec-Programmierung](Hitec-Programmierung#verkabelung) |

> **Die mittlere Ader eines Servos oder ESC nie an einen Pin der
> Steckverbinder legen.** J4 Pin 2 ist die 5-V-USB-Spannung (Universal Serial
> Bus) der Platine und versorgt kein Servo und keinen ESC; die 3,3-V-Pins
> bleiben frei.

Der Pull-down von 10 kΩ passt zum Wandler Pololu S13V30F5. Ein Wandler mit
einem anderen Freigabeeingang braucht den Wert, den seine Seite nennt: siehe
[KST-Programmierung](KST-Programmierung#verkabelung).
