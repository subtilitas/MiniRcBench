[English](Setup) | **Deutsch**

# Einrichtung

Vom leeren Tisch bis zum Startbildschirm: was du brauchst, das Adapterkabel
und das Aufspielen der Firmware. [Erste Schritte](Erste-Schritte) macht dort
weiter.

---

## Was du brauchst

| Teil | Wofür | Hinweise |
|---|---|---|
| Waveshare RP2350-Touch-LCD-2.8 | alles | Die Firmware läuft nur auf dieser Platine |
| USB-C-Kabel (USB: Universal Serial Bus) | Strom, Aufspielen der Firmware | Ein Kabel mit Datenadern; an einem reinen Ladekabel erscheint kein Laufwerk |
| JST-SH-Kabel, 1,0 mm, 4 Pins | Steckverbinder J5: Signal und Masse | Offene Enden auf einer Seite. Ob der Platine eines beiliegt, ist nicht geprüft |
| Servo-Verlängerungskabel oder eine 3-polige Stiftleiste | der Stecker des ESC oder Servos | Siehe [Das Adapterkabel](#das-adapterkabel) |
| JST-SH-Kabel, 1,0 mm, 12 Pins | Steckverbinder J4, nur für die geschaltete Servoversorgung | Siehe [Verdrahtung](Verdrahtung), Teil C |
| microSD-Karte, FAT32 (File Allocation Table) | Aufzeichnungen, Sicherungen, Servodateien | Optional. Siehe [SD-Aufzeichnung](SD-Aufzeichnung) |

Für jedes Werkzeug brauchst du außerdem:

| Werkzeug | Du brauchst |
|---|---|
| ESC-Tester | Einen ESC (Electronic Speed Controller, Motorsteller), der **bidirektionales DShot** annimmt, mit Motor und eigenem Akku. Die Firmwares BLHeli_32, Bluejay und AM32 tun das. Ein ESC, der nur PWM-Impulse (Pulsweitenmodulation) annimmt, wird nicht angesteuert. BLHeli_S ohne Bluejay ist nicht getestet |
| AM32-Konfiguration | Einen ESC mit AM32 |
| Servotester | Ein Servo und seine eigene Versorgung, 4,8 V bis 6,0 V, oder 8,4 V für ein Hochvolt-Servo |
| KST-Programmierung | Ein KST- oder Chaservo-Servo und eine schaltbare 5-V-Versorgung: ein Stecker oder ein Schalter in der Plusleitung |
| Hitec-Programmierung, D-Serie | Ein Hitec-Servo der D-Serie und eine 5-V-Versorgung |
| Hitec-Programmierung, klassisch | Die geschaltete Versorgung aus [Verdrahtung](Verdrahtung), Teil C: ein 5-V-DC-DC-Wandler (Gleichspannungswandler) mit Enable-Eingang |

---

## Das Adapterkabel

Die Platine hat JST-SH-Steckverbinder mit 1,0 mm Raster und keine
2,54-mm-Stiftleisten. Ein Kabel verbindet J5 mit einem Servostecker:

| J5 Pin | Signal | Geht an |
|---|---|---|
| 2 | SIG (GP0) | die Signalader: orange, gelb oder weiß |
| 4 | GND (Masse) | die Minusader: braun oder schwarz |
| 1 | AUX (GP1) | optional: KISS-Telemetrie oder ein zweites Servo |
| 3 | 3,3 V | nichts. Kurz abschneiden und isolieren |

> **Die mittlere Ader eines Servosteckers geht an keinen Pin.** Am ESC führt
> sie dessen +5-V-Ausgang, am Servo ist sie die Versorgung des Servos.
> Durchtrenne sie oder ziehe ihren Kontakt auf der Platinenseite aus dem
> Stecker.

Die Aderfarben eines JST-SH-Kabels folgen keiner Norm. Finde Pin 1 am
Steckverbinder, nicht an der Farbe: [Verdrahtung](Verdrahtung) nennt jeden
Pin. Wo Pin 1 am Steckverbinder der Platine sitzt, steht im Schaltplan des
Herstellers; in dieser Anleitung ist es nicht gezeichnet.

---

## Firmware aufspielen

1. Lade `MiniRcBench-<Version>-touch-lcd-2.8.uf2` von den
   [Releases](https://github.com/subtilitas/MiniRcBench/releases). UF2 ist
   das USB Flashing Format.
2. Verbinde die Platine über das USB-C-Kabel mit einem Computer.
3. Halte **BOOT**, tippe **RESET**, lasse **BOOT** los. Ein USB-Laufwerk
   namens `RP2350` erscheint.
4. Kopiere die `.uf2`-Datei auf dieses Laufwerk. Das Laufwerk verschwindet,
   und die Platine startet neu.
5. Der Startbildschirm steht 3 s und nennt die Platine,
   `RP2350-TOUCH-LCD-2.8`. Danach erscheint der Tester.

<img src="img/splash.png" width="240" alt="Der Startbildschirm">

Für eine neuere Version wiederholst du die Schritte 1 bis 5. Die Platine
geht in diesen Modus, gleich welche Firmware sie enthält. Eine Version mit
anderem Einstellungslayout setzt jede gespeicherte Einstellung einmal auf
ihren Vorgabewert zurück.

---

## Strom

- **USB-C** versorgt die Platine. Jedes 5-V-USB-Netzteil genügt; nach dem
  Aufspielen der Firmware braucht es keinen Computer.
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
