[English](USB-Bridge) | **Deutsch**

# USB-Brücke

`MENU → ESC CONFIG → USB BRIDGE` macht die Platine zu einem
USB-Seriell-Adapter am Signaldraht des ESC. Ein ESC-Programm auf deinem
Computer öffnet den seriellen Anschluss der Platine und spricht durch ihn mit
dem Bootloader des ESC, mit 19200 Baud, wie durch einen Adapter mit
verbundenem TX und RX: Jedes Byte des Programms geht an den ESC, und jedes
Byte auf dem Draht, das Echo der eigenen Bytes eingeschlossen, kommt an das
Programm zurück.

Nicht auf Hardware getestet: Noch kein Byte der Brücke ging von dieser
Platine an einen ESC, und noch kein Programm auf dem Computer lief damit.

<img src="img/bridge-waiting.png" width="240" alt="USB BRIDGE wartet: SIG GP0 19200 8N1, PORT CLOSED, MODE WAITING, beide Zähler bei 0 B, die sechs Zeilen"> <img src="img/bridge-passthrough.png" width="240" alt="USB BRIDGE nach einem Frame: PORT OPEN, PASSTHROUGH, 6 B und 7 B, FRAMES 1 LOST 0, LAST SET ADDRESS 7E00">

---

## Welche Programme

- **Geht, der Auslegung nach:** ein Programm, das die Frames des
  Bootloaders selbst spricht und das Echo erwartet: der AM32-Konfigurator
  mit angehaktem **Direct**.
- **Noch nicht:** ESC-Configurator und der AM32-Konfigurator ohne Direct.
  Sie sprechen MSP (MultiWii Serial Protocol) und die 4-Way-Schnittstelle
  von Flugcontrollern, auf die die Platine noch nicht antwortet. Sie
  bekommen ihre eigenen Bytes zurück und keine Antwort.
- **Nicht geprüft:** BLHeliSuite.

---

## Benutzen

1. Den ESC wie für DShot an SIG und GND anschließen, an seinem eigenen
   Akku. Den Propeller abnehmen.
2. Die Platine per USB mit dem Computer verbinden.
3. `MENU → ESC CONFIG` öffnen und `USB BRIDGE` antippen. DShot hält an;
   die Leiste zeigt kurz `RELEASING SIG GP0`, dann `SIG GP0  19200 8N1`.
4. Im Programm den seriellen Anschluss der Platine wählen, denselben, den
   die Konsole der Platine benutzt, und verbinden. Der Chip zeigt
   `PORT OPEN`, sobald das Programm den Anschluss öffnet; manche Programme
   melden das nicht, und die Brücke arbeitet trotzdem.
5. Im Programm lesen, schreiben oder flashen. Den ESC aus- und wieder
   einschalten, wenn das Programm keinen ESC findet, und nach dem Flashen.
6. Am Ende `BACK` antippen.

Solange die Brücke offen ist, ist die Konsole der Platine aus: Keine
Telemetriezeile erreicht den Anschluss. Sie kommt zurück, sobald DShot
wieder läuft.

Hält `USB DRIVE` auf SD LOG die Karte, startet die Brücke nicht und zeigt
`EJECT THE USB DRIVE FIRST.` (erst das USB-Laufwerk auswerfen). Nach dem
Auswerfen startet sie.

---

## Was der Bildschirm zeigt

| Zeile | Bedeutung |
|---|---|
| `PORT OPEN`, `PORT CLOSED` | Ob das Programm den Anschluss offen hat |
| `WAITING`, `PASSTHROUGH` | Warten auf das erste Byte des Programms, oder Bytes durchreichen. 2 s ohne Byte, oder das Programm schließt den Anschluss: zurück zu `WAITING` |
| `PC > ESC`, `ESC > PC` | Bytes in jede Richtung; `ESC > PC` enthält das Echo |
| `FRAMES`, `LOST` | Frames vom Programm; Bytes, die die Platine nicht halten konnte, über 0 in Bernstein |
| `LAST SET ADDRESS 7E00` | Der Befehl des letzten Frames, wie der Bootloader ihn nennt |

`THE PC CLOSED THE PORT.` erscheint, nachdem das Programm den Anschluss
geschlossen hat. `BYTES LOST: THE BRIDGE FELL BEHIND.` erscheint, sobald ein
Byte verloren ging; den ESC im Programm neu lesen.

---

## Schreiben und Verlassen

<img src="img/bridge-writing.png" width="240" alt="USB BRIDGE beim Schreiben: WRITING: DO NOT UNPLUG ESC OR USB. in Bernstein, BACK grau"> <img src="img/bridge-leave.png" width="240" alt="Die Verlassenseite: 1 POWER THE ESC OFF., 2 RESTART DSHOT., FLASH THE ESC AGAIN BEFORE USE., BRIDGE, RESTART DSHOT">

Während der ESC seinen Flash schreibt oder löscht, zeigt die erste Zeile
`WRITING: DO NOT UNPLUG ESC OR USB.` (schreibt: ESC und USB nicht
abziehen), und `BACK` ist grau, bis der ESC antwortet oder 3 s vergehen.

`BACK` sendet nichts an den ESC. Was danach geschieht, hängt von der
Sitzung ab:

- **Der Bootloader des ESC hat nie geantwortet**, oder das Programm hat am
  Ende die Firmware gestartet: `BACK` führt zu MENU, und DShot startet
  wieder.
- **Der Bootloader hat geantwortet, und das Programm hat die Firmware nicht
  gestartet**, auch nach bloßem Lesen: eine Verlassenseite, denn der ESC
  wartet in seinem Bootloader, und DShot-Frames können als seine Befehle
  durchgehen. Den ESC ausschalten, dann `RESTART DSHOT` antippen, aktiv 1 s
  nach dem Öffnen der Seite. `BRIDGE` führt stattdessen zur Brücke zurück.
- **Ein Schreiben ohne anschließenden Start der Firmware:** dieselbe Seite,
  mit `FLASH THE ESC AGAIN BEFORE USE.` (den ESC vor dem Einsatz neu
  flashen). Den ESC vor dem Einsatz neu flashen, wie die Notiz sagt.

MENU zeigt `POWER-CYCLE THE ESC TO RUN IT` (den ESC aus- und einschalten,
damit er läuft), solange ein Bootloader, den die Brücke erreicht hat, nicht
zum Start aufgefordert wurde.

Wird das USB-Kabel während eines Schreibens gezogen, während die Platine
an ihrer eigenen Zelle läuft, behält sie den Signalpin und sendet nichts.
Allein an USB-Strom geht die Platine mit dem Kabel aus, und das
Wiedereinstecken startet sie neu, mit DShot auf SIG. In beiden Fällen: den
ESC ausschalten, bevor das Kabel wieder eingesteckt wird, dann neu flashen.

---

## Noch nicht geprüft

- Dass irgendein Programm auf dem Computer durch die Platine arbeitet.
- Dass jedes Programm einen offenen Anschluss meldet (`PORT OPEN`).
- Dass die Platine die Antwort des ESC auf dem Signaldraht hört, und das
  Timing des Echos.
- Was ein ESC nach einem abgebrochenen Flashen mit DShot macht.
