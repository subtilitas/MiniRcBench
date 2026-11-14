[English](SD-Logging) | **Deutsch**

# SD-Aufzeichnung

Der Tester zeichnet die Telemetrie im Betaflight-Blackbox-Format auf microSD
auf, mit 500 Frames pro Sekunde. Ein Frame hat im Mittel 14,5 Byte (gemessen),
also 7,3 kB/s oder 26 MB pro Stunde.

<img src="img/log-screen.png" width="240" alt="SD-LOG-Bildschirm während der Aufzeichnung">

---

## Die Karte

Verwende eine microSD-Karte mit FAT32 (File Allocation Table). Die Karte wird
einmal beim Start eingebunden, und die Platine hat keinen
Karten-Erkennungspin. Steck die Karte vor dem Einschalten ein, oder drück nach
dem Einstecken `RETRY MOUNT` auf dem SD-LOG-Bildschirm.

Die Dateien landen im Verzeichnis `LOGS` als `LOG00001.BFL`, `LOG00002.BFL`
und so weiter. Jede neue Datei erhält die niedrigste freie Nummer von 00001
bis 09999. Vorhandene Dateien werden nicht überschrieben. Logs, die eine
frühere Firmware ins Wurzelverzeichnis schrieb, bleiben dort und zählen nicht
mit.

**Die höchste Nummer ist nicht immer die neueste Aufzeichnung.** Eine am
Computer frei gewordene Nummer wird wieder vergeben: Liegen `LOG00001.BFL` bis
`LOG00005.BFL` auf der Karte und ist `LOG00002.BFL` gelöscht, heißt die
nächste Aufzeichnung `LOG00002.BFL`. Die Platine stellt keine Uhr, also geben
auch die Datumsangaben der Dateien die Reihenfolge nicht an; welches Datum die
Dateien tragen, ist nicht geprüft. Der SD-LOG-Bildschirm nennt die letzte
Aufzeichnung der Sitzung (siehe `FILE` unten). Ob neue Aufzeichnungen nach der
höchsten Datei nummeriert werden sollen, ist beim Eigentümer offen.

Die Platine liest die Karte über 4-Bit-SDIO (Secure Digital Input/Output) mit
10 MHz.

---

## Aufzeichnen

**Automatisch:** Scharfschalten startet eine Aufzeichnung, Entschärfen beendet
sie.

**Manuell:** `START` auf dem SD-LOG-Bildschirm startet jederzeit eine
Aufzeichnung. Eine von Hand gestartete Aufzeichnung wird durch Entschärfen
nicht beendet. `STOP` beendet sie.

Der Tester-Bildschirm zeigt den Zustand in der oberen Zeile: `REC` (rot)
während der Aufzeichnung, `SD` im Leerlauf, `SDERR` nach einem Kartenfehler,
`NOSD` ohne Karte. `SDERR` steht auf einem bernsteinfarbenen Feld zwischen der
Scharf-Anzeige und `BOARD`, damit eine Aufzeichnung, die von selbst endete,
auch im scharfen Zustand auffällt. SD LOG sagt, was geschah.

Eine Aufzeichnung endet auch, wenn KST SERVO, HITEC SERVO oder AM32 CONFIG
öffnet: Das Werkzeug braucht die Karte für seine Sicherung. SD LOG zeigt dann
`RECORDING STOPPED BY KST SERVO` (oder `HITEC SERVO`, `AM32 CONFIG`). Nach dem
Werkzeug startet `START` eine neue Aufzeichnung.

---

## Den Bildschirm lesen

Öffnen über **MENU → SD LOG**. SD LOG ist die siebte Zeile: Ziehe `MENU` um
36 px nach oben, damit sie sichtbar wird. Die Zeile zeigt denselben Zustand:
`NO CARD`, `READY`, `RECORDING`, `CARD ERROR`, oder `USB DRIVE`, solange ein
Computer die Karte hält.

<img src="img/log-ready.png" width="240" alt="Karte eingebunden, bereit">

| Zeile | Bedeutung |
|---|---|
| **STATUS** | `NO CARD`, `READY`, `RECORDING` oder `CARD ERROR` |
| **FILE** | Die geöffnete Datei. Nach dem Ende einer Aufzeichnung ihre Datei mit `(LAST)`, zum Beispiel `LOG00012.BFL (LAST)`, neben ihren Zählern, bis zum nächsten `START` oder Einbinden. `--` vor der ersten Aufzeichnung nach dem Einschalten und nach einem Einbinden (`RETRY MOUNT`, das Ende von `USB DRIVE`, das neue Einbinden durch ein Werkzeug): Es kann eine andere Karte sein, daher werden Name und Zähler zurückgesetzt |
| **FRAMES** | Bisher geschriebene Frames |
| **WRITTEN** | Geschriebene Kilobyte |
| **DROPPED FRAMES** | Frames, die der Puffer nicht halten konnte. **Sollte null sein.** Sonst rot |
| **BUF PEAK** | Höchster Füllstand des 8192-Byte-Schreibpuffers. Bernstein über 1/2, rot über 3/4 |
| **WORST FLUSH** | Der längste einzelne Schreibvorgang auf die Karte, in ms. Bernstein über 50 ms |
| **CARD** | Kartentyp und Größe, wie die Karte sie meldet |
| **MOUNT** | Der Ergebniscode des Dateisystems. `0 OK` heißt eingebunden |

Nähert sich `BUF PEAK` der Puffergröße, oder dauert ein `WORST FLUSH` länger,
als der Puffer überbrückt (8192 Byte sind etwa 1,1 s bei 7,3 kB/s), ist die
Karte für den Puffer zu langsam. `DROPPED FRAMES` über null heißt, die
Aufzeichnung hat bereits Lücken.

Meist ist eine langsame Karte die Ursache. Probiere zuerst eine andere Karte.

<img src="img/log-last.png" width="240" alt="FILE zeigt LOG00001.BFL (LAST) nach STOP">

### Wie die letzte Aufzeichnung endete

<img src="img/log-card-full.png" width="240" alt="SD LOG, nachdem ein Schreibvorgang die Karte voll fand">

Endet eine Aufzeichnung von selbst, oder nimmt ein Werkzeug die Karte, sagen
Zeilen in Bernstein über `START`, wie, und was zu tun ist. Ein Entschärfen,
das eine durch Scharfschalten begonnene Aufzeichnung beendet, und `STOP`
zeigen keine. Ein Kartenfehler bleibt bis zum nächsten `START` oder Einbinden
(`RETRY MOUNT`, das Ende von `USB DRIVE`, oder das neue Einbinden durch KST
SERVO und HITEC SERVO nach einem fehlgeschlagenen Kartenschreiben). Das Ende
durch ein Werkzeug hat eine Zeile und bleibt bis zum nächsten `START`, auch
über ein Einbinden hinweg, denn es stimmt weiter für die letzte Aufzeichnung;
ohne Karte steht der Einbinde-Hinweis in der zweiten Zeile.

| Zeilen | Bedeutung | Was tun |
|---|---|---|
| `RECORDING STOPPED: CARD FULL` / `FREE SPACE ON A COMPUTER, RETRY MOUNT` | Ein Schreibvorgang fand keinen freien Platz | Dateien am Computer löschen (`USB DRIVE` ist schreibgeschützt), Karte zurückstecken, `RETRY MOUNT` drücken |
| `RECORDING STOPPED: CARD WRITE FAILED` / `REMOVED? REINSERT, THEN RETRY MOUNT` | Ein Schreibvorgang oder das abschließende Sichern oder Schließen schlug fehl: eine herausgezogene Karte oder ein Kartenfehler. Beides wird nicht unterschieden | Karte neu einstecken, `RETRY MOUNT` drücken |
| `NO LOG STARTED: THE FILE DID NOT OPEN` / `CHECK THE CARD, THEN RETRY MOUNT` | `START` konnte `LOGS` nicht anlegen oder die nächste Datei nicht öffnen | Karte am Computer prüfen, `RETRY MOUNT` drücken |
| `RECORDING STOPPED BY KST SERVO` (`HITEC SERVO`, `AM32 CONFIG`) | Das Werkzeug nahm die Karte für seine Sicherung | `START` für eine neue Aufzeichnung |

Eine volle Karte unterscheidet sich von einem fehlgeschlagenen Schreibvorgang
durch das, was das Dateisystem meldet: Ein Schreibvorgang, der keinen freien
Cluster findet, endet mit weniger geschriebenen Bytes und ohne Fehler. Die
ersten drei lassen `STATUS` auf `CARD ERROR` und den Tester auf `SDERR`.
Solange eine Ablehnung von `USB DRIVE` angezeigt wird, belegt sie die untere
Zeile und der Hinweis wartet.

### Keine Karte

<img src="img/log-nocard.png" width="240" alt="Keine Karte">

`NO CARD` mit `MOUNT 3 NOT READY` heißt, die Karte ist nicht angelaufen. Hat
auf dem Bus nichts geantwortet, keine Karte oder eine, die nicht richtig
sitzt, zeigt die Hinweiszeile über `START`
`INSERT A CARD, THEN RETRY MOUNT` (Karte einstecken, dann `RETRY MOUNT`). Hat
eine Karte geantwortet und ist trotzdem nicht angelaufen, zeigt sie in
Bernstein `CARD NOT USABLE: RESEAT OR REPLACE`: Karte heraus- und wieder
hineinstecken oder eine andere probieren. `MOUNT 13 NO FILESYSTEM` mit einer
Größe unter `CARD` heißt, die Karte antwortet und das Dateisystem ist das
Problem. Formatiere sie als FAT32; die Hinweiszeile bleibt leer, denn
Einstecken hilft hier nicht.

### Eine nach dem Start eingesteckte Karte

<img src="img/log-mounted.png" width="240" alt="Karte über RETRY MOUNT gefunden">

Drück `RETRY MOUNT`. Ohne das wird eine nach dem Einschalten eingesteckte
Karte nicht eingebunden.

---

## Aufzeichnungen ohne Kartenleser holen

<img src="img/log-usb-serving.png" width="240" alt="Die Karte als USB-Laufwerk">

`USB DRIVE` auf dem SD-LOG-Bildschirm stellt die Karte dem Computer am
USB-Kabel (Universal Serial Bus) als Wechsellaufwerk bereit. `.BFL`-Dateien
herüberkopieren, das Laufwerk am Computer auswerfen, und der Tester nimmt die
Karte zurück. Der Bildschirm zeigt die bisher gelesene Datenmenge.

Die Platine ist ohnehin ein USB-Gerät, für Strom und den seriellen Anschluss.
Das Laufwerk ist eine zweite USB-Schnittstelle am selben Kabel.

**Das Laufwerk ist schreibgeschützt.** Der Computer kann Dateien kopieren und
nicht schreiben. Ein Dateisystemtreiber des Computers kann daher weder die
Karte noch die Nummerierung verändern. Eine Meldung „schreibgeschützt" vom
Computer ist zu erwarten.

**Die Karte wird übergeben, nicht geteilt.** FAT kann zwischen zwei Schreibern
nicht vermitteln. Bevor das Laufwerk erscheint, leert, schließt und hängt der
Logger die Karte aus, und er rührt sie bis zum Auswerfen nicht an. Währenddessen
zeigt der SD-LOG-Bildschirm die Übergabe statt seiner Zähler.

Ohne Karte ist `USB DRIVE` als nicht verfügbar gezeichnet und tut nichts.

**Mit Karte lehnt die Schaltfläche in zwei Fällen ab und nennt den Grund:**

<img src="img/log-usb-refused.png" width="240" alt="USB DRIVE während der Aufzeichnung abgelehnt">

| Meldung | Was zu tun ist |
|---|---|
| `DISARM FIRST` | Entschärfen. Die Übergabe hält den Logger an |
| `STOP RECORDING FIRST` | Die Aufzeichnung beenden |

**Scharfschalten ist gesperrt, solange der Computer die Karte hat.** `BACK`
verlässt den Bildschirm Richtung `MENU`, ohne die Übergabe abzubrechen, und
`BACK` auf `MENU` erreicht den Tester. Der Tester schaltet erst nach dem
Auswerfen scharf: `HOLD TO ARM` ist nur als Umriss gezeichnet, und der
Gasbalken zeigt `EJECT THE USB DRIVE TO ARM`. Ein Druck,
der beginnt, solange der Computer die Karte hat, schaltet auch nach dem
Auswerfen nicht scharf. Neu drücken.

Am Computer auswerfen oder `EJECT` auf dem Bildschirm drücken. Beides beendet
die Übergabe.
Ein Computer, der das Laufwerk abmeldet, beendet sie ebenfalls. Ein gezogenes
Kabel bei einer Platine am Akku tut das nicht: Der USB-Stack dieser Firmware
sieht es wie einen Computer, der schlafen ging, und der behält die Karte.
Drücke dann `EJECT`. Auf der Hardware nicht geprüft.

---

## Die Aufzeichnungen öffnen

<a href="https://subtilitas.github.io/logwiju/"><img src="https://img.shields.io/badge/open%20your%20logs%20in-logwiju-07b0c8?style=for-the-badge" alt="Aufzeichnungen in logwiju öffnen"></a>

### [logwiju](https://subtilitas.github.io/logwiju/) — der vorgesehene Viewer

logwiju läuft im Browser ohne Installation. Die Datei wird lokal gelesen und
nicht hochgeladen. Zieh eine `.BFL`-Datei auf die Seite, um die Felder über der
Zeit darzustellen.

### Betaflight Blackbox Explorer

Die Dateien sind Betaflight-Blackbox-Logs, daher öffnen
[Blackbox Explorer](https://blackbox.betaflight.com/) und das
Kommandozeilenwerkzeug `blackbox_decode` sie ebenfalls. Einige Feldnamen
stammen aus Flugsteuerungen, etwa `motor[0]` für das Gas.

---

## Was in einer Aufzeichnung steht

Felder pro Frame:

| Feld | Inhalt |
|---|---|
| `loopIteration`, `time` | Frame-Zähler und Zeit |
| `motor[0]` | Befohlenes Gas |
| `eRPM[0]` | eRPM (elektrische Drehzahl) aus bidirektionalem DShot |
| `eRPMkiss[0]` | eRPM aus KISS |
| `vbatLatest`, `amperageLatest` | Spannung und Strom aus KISS; 0, solange KISS nicht aktiv ist |
| `vbatEdt`, `amperageEdt` | Spannung und Strom aus EDT (Extended DShot Telemetry) |
| `escTemperature[0]` | Temperatur des ESC (Electronic Speed Controller, Motorregler) |
| `escConsumption` | Verbrauchte mAh (nur KISS) |
| `escStress` | EDT-Stresswert |

Das ESC-Statusbyte, der Scharf-Zustand und die Verbindungszähler werden nicht
aufgezeichnet.

Mit KISS-Kabel werden beide Quellen getrennt aufgezeichnet, sodass sich eine
Quelle an der anderen prüfen lässt.

---

## Wenn die Karte nicht das Problem ist

Die Aufzeichnung ist standardmäßig einkompiliert. Eine mit `SD_LOG_ENABLE=0`
gebaute Firmware zeigt den SD-LOG-Bildschirm weiterhin, und er meldet immer
`NO CARD`.
