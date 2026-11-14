[English](Setup) | **Deutsch**

# Einrichtung

Vom leeren Tisch bis zum Startbildschirm: was du brauchst, das Adapterkabel
und das Aufspielen der Firmware. [Erste Schritte](Erste-Schritte) macht dort
weiter.

---

## Was du brauchst

| Teil | Wofür | Hinweise |
|---|---|---|
| Waveshare RP2350-Touch-LCD-2.8 | alles | Die Firmware läuft nur auf dieser Platine. Sie prüft nicht, auf welcher Platine sie läuft: Auf eine andere Platine geflasht, treibt sie Pins, die dort mit anderen Bauteilen verbunden sind |
| USB-C-Kabel (USB: Universal Serial Bus) | Strom, Aufspielen der Firmware | Ein Kabel mit Datenadern |
| JST-SH-Kabel, 1,0 mm, 4 Pins | Steckverbinder J5: Signal und Masse | Offene Enden auf einer Seite. Ob der Platine eines beiliegt, ist nicht geprüft |
| Servo-Verlängerungskabel oder eine 3-polige Stiftleiste | der Stecker des ESC (Electronic Speed Controller, Motorregler) oder Servos | Siehe [Das Adapterkabel](#das-adapterkabel) |
| JST-SH-Kabel, 1,0 mm, 12 Pins | Steckverbinder J4, nur für die geschaltete Servoversorgung | Siehe [Verdrahtung](Verdrahtung), Teil C |
| microSD-Karte, FAT32 (File Allocation Table) | Aufzeichnungen, Sicherungen, Servodateien | Optional. Siehe [SD-Aufzeichnung](SD-Aufzeichnung) |

Für jedes Werkzeug brauchst du außerdem:

| Werkzeug | Du brauchst |
|---|---|
| ESC-Tester | Einen ESC, der **bidirektionales DShot** annimmt, ein digitales ESC-Protokoll, mit Motor und eigenem Akku. BLHeli_32, Bluejay und AM32 können das. Der ESC-Tester sendet nur DShot: Ein ESC, der nur PWM-Impulse (Pulsweitenmodulation) annimmt, lässt sich damit nicht testen. BLHeli_S ohne Bluejay ist nicht getestet |
| AM32-Konfiguration | Einen ESC mit AM32 |
| Servotester | Ein Servo und seine eigene Versorgung, 4,8 V bis 6,0 V, oder 8,4 V für ein Hochvolt-Servo |
| KST-Programmierung | Ein KST- oder Chaservo-Servo und eine schaltbare 5-V-Versorgung: ein Stecker oder ein Schalter in der Plusleitung |
| Hitec-Programmierung, D-Serie | Ein Hitec-Servo der D-Serie und eine 5-V-Versorgung |
| Hitec-Programmierung, klassisch | Ein klassisches Hitec-Servo (HS-5xxx, HS-7xxx) und die geschaltete Versorgung aus [Verdrahtung](Verdrahtung), Teil C: ein 5-V-DC-DC-Wandler (Gleichspannungswandler) mit Enable-Eingang (EN), ein Abwärtswandler oder einer mit Ausgangstrennung, für mehr als 1 A, seine Eingangsversorgung und ein Pull-down-Widerstand von 10 kΩ von EN nach Masse |

---

## Das Adapterkabel

Die Platine hat JST-SH-Steckverbinder mit 1,0 mm Raster und keine
2,54-mm-Stiftleisten. Ein Kabel verbindet J5 mit einem Servostecker:

| J5-Pin | Signal | Geht an |
|---|---|---|
| 2 | SIG (GP0) | die Signalader: orange oder weiß |
| 4 | GND (Masse) | die Minusader: braun oder schwarz |
| 1 | AUX (GP1) | optional: die Telemetrieader eines ESC mit KISS-Telemetrie, einem seriellen ESC-Telemetrieprotokoll, oder das Signal eines zweiten Servos |
| 3 | 3,3 V | nichts, niemals. Kurz abschneiden und isolieren |

> **Warnung: Ein Servo- oder ESC-Kabel nie gerade durch an J5 anschließen.**
> J5 Pin 2, 3 und 4 sind SIG, 3,3 V und GND: dieselbe Reihenfolge wie bei
> einem Servostecker, Signal, Plus und Minus. Pin 3 ist die 3,3-V-Versorgung
> der Platine. Gerade durch verbunden, bekommt er die BEC-Ausgangsspannung
> (Battery Eliminator Circuit) des ESC, +5 V, oder die Versorgung des Servos,
> 4,8 V bis 6,0 V oder 8,4 V für ein Hochvolt-Servo, und das kann die Platine
> zerstören. J5 Pin 3 bleibt frei.

GP0 und GP1 sind GPIO 0 und 1 der Platine (General-Purpose Input/Output,
frei nutzbare Ein-/Ausgangspins). Zum Verbinden der Adern brauchst du einen
Lötkolben und Schrumpfschlauch oder Crimpkontakte.

> **Die mittlere Ader eines Servosteckers geht an keinen Pin der Platine.**
> - Am ESC führt sie dessen +5-V-Ausgang. Durchtrenne sie oder ziehe ihren
>   Kontakt aus dem Stecker.
> - Am Servo ist sie die Versorgung des Servos. Verbinde sie und die
>   Minusader mit der eigenen Versorgung des Servos, wie in
>   [Verdrahtung](Verdrahtung), Teil B, oder Teil C für die geschaltete
>   Versorgung. Die Minusader geht außerdem an einen GND-Pin der Platine:
>   J5 Pin 4 oder J4 Pin 1 oder 5.

> **Kein Servo an SIG, während die Platine startet.** Außerhalb der
> Servobildschirme führt SIG DShot, vom Ende des 3 s langen Startbildschirms an.
> Ein Servo liest das als Impuls und fährt in Richtung eines Endes seines Wegs,
> mit einer Framerate, die ein analoges Servo überhitzt. Stecke das Servo erst
> an, wenn sein Bildschirm offen ist. Bei HITEC SERVO führt SIG DShot, bis eine
> Familie gewählt ist: Halte die Zeit in der Familienansicht kurz. Schalte die
> Platine vor der Servoversorgung ein und die Servoversorgung vor der Platine
> aus: [Servotester](Servotester#verkabelung).

Die Aderfarben eines JST-SH-Kabels folgen keiner Norm. Finde Pin 1 am
Steckverbinder, nicht an der Farbe: [Verdrahtung](Verdrahtung) nennt jeden
Pin. Wo Pin 1 am Steckverbinder der Platine sitzt, steht im Schaltplan des
Herstellers; in dieser Anleitung ist es nicht gezeichnet.

---

## Firmware aufspielen

1. Lade `MiniRcBench-<Version>-touch-lcd-2.8.uf2` von den
   [Releases](https://github.com/subtilitas/MiniRcBench/releases) herunter,
   zum Beispiel `MiniRcBench-v0.1.0-touch-lcd-2.8.uf2`. UF2 ist das USB
   Flashing Format.
2. Verbinde die Platine über das USB-C-Kabel mit einem Computer.
3. Halte **BOOT**, tippe **RESET** an, lasse **BOOT** los. Ein USB-Laufwerk
   erscheint. Sein Name und die Lage der beiden Tasten auf der Platine sind
   hier nicht geprüft.
4. Kopiere die `.uf2`-Datei auf dieses Laufwerk. Die Platine startet neu.
5. Der Startbildschirm erscheint 3 s lang und nennt die Platine,
   `RP2350-TOUCH-LCD-2.8`. Danach erscheint der Tester-Bildschirm.

<img src="img/splash.png" width="240" alt="Der Startbildschirm">

Für eine neuere Version wiederholst du die Schritte 1 bis 5. Das Laufwerk
erscheint, gleich welche Firmware die Platine enthält. Eine Version mit
anderem Einstellungslayout setzt jede gespeicherte Einstellung einmal auf
ihren Vorgabewert zurück.

---

## Strom

- **USB-C** versorgt die Platine, aus einem 5-V-USB-Netzteil oder einem
  Computer.
- **Eine Lithium-Polymer-Zelle** am Akkuanschluss der Platine ist optional.
  Die Platine lädt sie. Mit eingesetzter Zelle schaltest du mit der
  Einschalttaste ein. Steckertyp und Polung der Zelle stehen in der
  Dokumentation des Herstellers; sie sind hier nicht geprüft.
- Der ESC und jedes Servo laufen aus **ihrer eigenen Versorgung**, nie aus
  der Platine.

---

## Weiter

- [Erste Schritte](Erste-Schritte): einen ESC anschließen und zum ersten Mal
  scharfschalten
- [Verdrahtung](Verdrahtung): jede Verbindung auf einem Blatt
- [Fehlersuche](Fehlersuche): erst das Symptom
