[English](USB-Bridge) | **Deutsch**

# USB-Brücke

`MENU → ESC PROGRAM → USB BRIDGE` lässt ein ESC-Programm auf deinem
Computer über den seriellen Anschluss der Platine den Bootloader des ESC am
Signaldraht erreichen. Die Platine arbeitet auf eine von zwei Arten, die das
erste Byte des Programms wählt:

- **Durchreichen (Passthrough):** ein USB-Seriell-Adapter mit verbundenem
  TX und RX, mit 19200 Baud. Jedes Byte des Programms geht an den ESC, und
  jedes Byte auf dem Draht, das Echo der eigenen Bytes eingeschlossen,
  kommt an das Programm zurück.
- **4-Way-Schnittstelle:** Die Platine antwortet wie ein Flugcontroller mit
  einem ESC. Sie beantwortet die MSP-Fragen (MultiWii Serial Protocol) des
  Programms und führt dann BLHelis 4-Way-Schnittstelle aus: Jede Anfrage
  des Programms wird zu Befehlen des Bootloaders auf dem Signaldraht, und
  die Antwort geht zurück.

Nicht auf Hardware getestet: Noch kein Byte der Brücke ging von dieser
Platine an einen ESC, und noch kein Programm auf dem Computer lief damit.

<img src="img/bridge-waiting.png" width="240" alt="USB BRIDGE wartet: SIG GP0 19200 8N1, PORT CLOSED, MODE WAITING, beide Zähler bei 0 B, die sechs Zeilen"> <img src="img/bridge-passthrough.png" width="240" alt="USB BRIDGE nach einem Frame: PORT OPEN, PASSTHROUGH, 6 B und 7 B, FRAMES 1 LOST 0, LAST SET ADDRESS 7E00"> <img src="img/bridge-4way.png" width="240" alt="USB BRIDGE in der 4-Way-Schnittstelle: 4-WAY INTERFACE, LAST MSP SET MOTOR, MOTOR COMMAND FROM THE PC REFUSED. in Bernstein">

---

## Welche Programme

- **Durchreichen, der Auslegung nach:** ein Programm, das die Frames des
  Bootloaders selbst spricht und das Echo erwartet: der AM32-Konfigurator
  mit angehaktem **Direct**.
- **4-Way-Schnittstelle, der Auslegung nach:** ESC-Configurator, für
  Bluejay-, BLHeli_S- und AM32-ESCs, und der AM32-Konfigurator ohne
  Direct.
- **Nicht geprüft:** BLHeliSuite.

Die Platine erkennt die Art am ersten Byte des Programms; du wählst nichts.

---

## Benutzen

1. Den ESC wie für DShot an SIG und GND anschließen, an seinem eigenen
   Akku. Den Propeller abnehmen.
2. Die Platine per USB mit dem Computer verbinden.
3. `MENU → ESC PROGRAM` öffnen und `USB BRIDGE` antippen. Die Platine nimmt SIG;
   die Leiste zeigt kurz `RELEASING SIG GP0`, dann `SIG GP0  19200 8N1`.
4. Im Programm den seriellen Anschluss der Platine wählen, denselben, den
   die Konsole der Platine benutzt, und verbinden. Der Chip zeigt
   `PORT OPEN`, sobald das Programm den Anschluss öffnet; manche Programme
   melden das nicht, und die Brücke arbeitet trotzdem.
5. Im Programm lesen, schreiben oder flashen. Den ESC aus- und wieder
   einschalten, wenn das Programm keinen ESC findet, und nach dem Flashen.
6. Am Ende `BACK` antippen.

<img src="img/bridge-stopping.png" width="240" alt="USB BRIDGE, während die Platine SIG nimmt: RELEASING SIG GP0 auf der Leiste, kein Chip, die Hinweiszeilen"> <img src="img/bridge-failed.png" width="240" alt="USB BRIDGE nach einem fehlgeschlagenen Bind: SIG GP0 COULD NOT BE TAKEN (PIO) in Bernstein, RETRY in der Fußzeile">

Solange die Platine SIG nimmt, ist die Brücke noch nicht offen. Kann die Platine
SIG nicht übernehmen, steht eine Zeile in Bernstein:
`SIG GP0 COULD NOT BE TAKEN (PIO)` (SIG GP0 konnte nicht übernommen werden), mit
`REFUSED` oder `CLOCK` statt `PIO`, und `RETRY` erscheint:

- `PIO` (Programmable I/O, programmierbare Ein-/Ausgabe): Kein PIO-Block hat
  Platz für Sender und Empfänger der Leitung;
- `CLOCK`: Der Systemtakt (`clk_sys`) ist für die Rate der Leitung zu
  langsam;
- `REFUSED`: Der zweite Kern der Platine hat den Auftrag nicht übernommen.

`RETRY` versucht es erneut. `BACK` führt direkt zu ESC PROGRAM, ohne
Verlassenseite, solange die Platine SIG nimmt und nach einem
fehlgeschlagenen Bind: Die Brücke hat die Leitung nie übernommen.

Solange die Brücke offen ist, ist die Konsole der Platine aus: Keine
Telemetriezeile erreicht den Anschluss. Sie kommt zurück, sobald die Brücke
beendet ist.

Hält `USB DRIVE` auf LOGS die Karte, startet die Brücke nicht und zeigt
`EJECT THE USB DRIVE FIRST.` (erst das USB-Laufwerk auswerfen). Nach dem
Auswerfen startet sie.

<img src="img/bridge-eject.png" width="240" alt="USB BRIDGE, die Karte hält USB DRIVE: EJECT THE USB DRIVE FIRST. in Bernstein">

---

## Was der Bildschirm zeigt

| Zeile | Bedeutung |
|---|---|
| `PORT OPEN`, `PORT CLOSED` | Ob das Programm den Anschluss offen hat. `PORT OPEN` steht auf Hellgrün; `PORT CLOSED` ist ein Rahmen mit gedämpfter Beschriftung, ohne Füllung |
| `WAITING`, `PASSTHROUGH`, `4-WAY INTERFACE` | Warten auf das erste Byte des Programms, Bytes durchreichen, oder als Flugcontroller antworten. Beim Durchreichen, und solange das Programm noch seine ersten MSP-Fragen stellt, die schon `4-WAY INTERFACE` zeigen, führen 2 s ohne Byte, oder das Programm schließt den Anschluss, zurück zu `WAITING`; hat das Programm die 4-Way-Schnittstelle gestartet, bleibt sie, bis das Programm trennt oder den Anschluss schließt |
| `PC > ESC`, `ESC > PC` | Bytes in jede Richtung; beim Durchreichen enthält `ESC > PC` das Echo, in der 4-Way-Schnittstelle zählt es die Antworten der Platine |
| `FRAMES`, `LOST` | Frames an den ESC; Bytes, die die Platine nicht halten konnte, über 0 in Bernstein |
| `LAST SET ADDRESS 7E00` | Der letzte Befehl: wie der Bootloader ihn nennt, `LAST MSP UID` für eine MSP-Frage, `LAST READ 7C00` für eine 4-Way-Anfrage |

`THE PC CLOSED THE PORT.` erscheint, nachdem das Programm den Anschluss
geschlossen hat. `BYTES LOST: THE BRIDGE FELL BEHIND.` erscheint, sobald ein
Byte verloren ging; den ESC im Programm neu lesen.
`MOTOR COMMAND FROM THE PC REFUSED.` (Motorbefehl vom PC abgelehnt)
erscheint für 5 s, wenn der Motor-Reiter des Programms den Motor drehen
will: Die Platine tut das nie.

<img src="img/bridge-lost.png" width="240" alt="USB BRIDGE beim Durchreichen mit LOST 932 in Bernstein und BYTES LOST: THE BRIDGE FELL BEHIND. in Bernstein">

Eine Einstellungsseite eines AM32-ESC, die das Programm liest oder
schreibt, setzt den Hinweis `3D MODE ESC` des Testers wie ESC SETTINGS:
an nach dem Lesen oder Schreiben von eingeschaltetem 3D-Modus, aus nach dem
Lesen von ausgeschaltetem.

---

## Schreiben und Verlassen

<img src="img/bridge-writing.png" width="240" alt="USB BRIDGE beim Schreiben: WRITING: DO NOT UNPLUG ESC OR USB. in Bernstein, BACK grau"> <img src="img/bridge-leave.png" width="240" alt="Die Verlassenseite: 1 POWER THE ESC OFF., 2 BACK TO ESC PROGRAM., FLASH THE ESC AGAIN BEFORE USE., BRIDGE, ESC PROGRAM und BACK grau">

Während der ESC seinen Flash schreibt oder löscht, zeigt die erste Zeile
`WRITING: DO NOT UNPLUG ESC OR USB.` (schreibt: ESC und USB nicht
abziehen), und `BACK` ist grau, bis der ESC antwortet oder 3 s vergehen.
Das gilt für beide Arten.

`BACK` sendet nichts an den ESC. Was danach geschieht, hängt von der
Sitzung ab:

Bei laufender Brücke öffnet `BACK` jedes Mal eine Verlassenseite, gleich was
das Programm tat: Der ESC kann in seinem Bootloader sitzen, beim Einschalten
oder von selbst dorthin gelangt, und DShot-Frames beim nächsten ESC TESTER
oder ESC COMMANDS können als seine Befehle durchgehen. Den ESC ausschalten,
dann `ESC PROGRAM`, den Titel der Auswahl, antippen, aktiv 1 s nach dem
Öffnen der Seite: Es legt den Signalpin auf Low, startet kein DShot und
führt zu ESC PROGRAM zurück. `BRIDGE` führt stattdessen zur Brücke zurück.
Nach einem Schreiben ohne anschließenden Start der Firmware zeigt die Seite
zusätzlich `FLASH THE ESC AGAIN BEFORE USE.` (den ESC vor dem Einsatz neu
flashen).
In der 4-Way-Schnittstelle zählt nur das Zurücksetzen des ESC durch das
Programm als dieser Start, nicht sein Trennen. Der Hinweis bleibt nach
`BRIDGE`: Jedes spätere `BACK` zeigt ihn wieder, bis das Programm die
Firmware startet. `ESC PROGRAM` beendet die Sitzung, und das nächste
`USB BRIDGE` beginnt ohne den Hinweis.

<img src="img/bridge-leave-read.png" width="240" alt="Die Verlassenseite nach einer Sitzung, die nur las: 1 POWER THE ESC OFF., 2 BACK TO ESC PROGRAM., ohne FLASH-Hinweis, BRIDGE, ESC PROGRAM und BACK grau">

Ohne ein solches Schreiben, etwa nach einer Sitzung, die nur las, oder nach
einem Schreiben mit anschließendem Start, zeigt die Seite die zwei Schritte
ohne Hinweis.

MENU zeigt `POWER-CYCLE THE ESC TO RUN IT` (den ESC aus- und einschalten,
damit er läuft), sobald der Bildschirm den Signalpin genommen hat, auch nach
einem fehlgeschlagenen Bind, bis zur letzten Taste der Verlassenseite oder
bis der ESC auf ESC TESTER oder ESC COMMANDS wieder auf DShot antwortet.

Wird das USB-Kabel während eines Schreibens gezogen, während die Platine
an ihrer eigenen Zelle läuft, behält sie den Signalpin und sendet nichts.
Allein an USB-Strom geht die Platine mit dem Kabel aus, und das
Wiedereinstecken startet sie neu, mit SIG auf Low; DShot läuft erst, wenn
ESC TESTER oder ESC COMMANDS öffnet. In beiden Fällen: den
ESC ausschalten, bevor das Kabel wieder eingesteckt wird, dann neu flashen.

---

## Noch nicht geprüft

- Dass irgendein Programm auf dem Computer durch die Platine arbeitet.
- Dass jedes Programm einen offenen Anschluss meldet (`PORT OPEN`).
- Dass ESC-Configurator und der AM32-Konfigurator die Antworten der
  Platine als die eines Flugcontrollers annehmen.
- Dass die Platine die Antwort des ESC auf dem Signaldraht hört, und das
  Timing des Echos.
- Was ein ESC nach einem abgebrochenen Flashen mit DShot macht.
