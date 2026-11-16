[English](Angle-Meter) | **Deutsch**

# Winkelmesser

`MENU → SERVO → ANGLE METER` misst Winkel mit dem Beschleunigungssensor auf
der Platine selbst, aus der Richtung der Schwerkraft, solange die Platine
still liegt: den Einstellwinkel einer Fläche, die EWD zwischen Flügel und
Höhenleitwerk, die V-Form und den Ausschlag eines Ruders in Grad und als Weg
der Endleiste in mm. Er belegt keinen Signalpin und braucht keine
Verdrahtung.

Nicht auf Hardware getestet: Vom Winkelmesser ist noch nichts auf einer
Platine gelaufen. Die Genauigkeit seiner Messwerte ist nicht gemessen. Der
Bildschirm zeigt Zehntelgrad und Zehntelmillimeter; das ist die Auflösung
der Anzeige, nicht die Richtigkeit des Werts. Bis der Prüfstand den
Winkelmesser an bekannten Winkeln misst, nimm einen Wert als Vergleich
zweier Einstellungen, nicht als genauen Winkel.

<img src="img/servo-hub.png" width="240" alt="SERVO, die Auswahl: ANGLE METER ist ihr zweiter Eintrag, mit INCIDENCE, DECALAGE, DIHEDRAL, THROW darunter"> <img src="img/angle-main.png" width="240" alt="ANGLE METER nach einem Nullpunkt: -20.0 Grad vom Nullpunkt, -13.7 MM bei einer Länge von 40 mm, MAX -20.0 Grad">

---

## Die Hauptansicht

| Zeile | Was sie zeigt |
|---|---|
| Leiste | Die Kante der Platine, die entlang der Achse liegt, `LONG EDGE` oder `SHORT EDGE`, und die Neigung dieser Achse gegen die Waagerechte in ganzen Grad, ab 45° in Bernstein. Der Chip: `SETTLED` (grün), `MOVING` (ein Rahmen mit gedämpfter Beschriftung, ohne Füllung), `TOO STEEP`, `STARTING` oder `NO IMU` (Bernstein) |
| Winkel | Der Winkel in Zehntelgrad, vom Nullpunkt aus, vor einem Nullpunkt von der Waagerechten; die Zeile darunter zeigt `FROM ZERO` oder `NOT ZEROED: FROM LEVEL`. `---`, solange der Winkelmesser startet, wenn 300 ms kein Messwert kam und solange die Achse um 70° oder mehr geneigt ist |
| Weg | Mit gesetzter LENGTH in ANGLE SET, wie weit sich die Endleiste bewegt hat, und `L 40 MM, HINGE TO TRAILING EDGE`. Ohne sie `SET LENGTH IN ANGLE SET FOR MM` |
| MAX | Der größte ruhige Winkel je Seite seit ZERO oder RESET MAX, die negative Seite zuerst, `--` für eine Seite ohne; darunter ihr Weg |
| Temperatur | Die Temperatur des Sensors der Platine und nach einem Nullpunkt ihre Änderung seitdem, ab 2,0 °C in Bernstein |

`SETTLED` heißt, der Winkel ist eine Sekunde lang innerhalb von 0,5°
geblieben. Lies Werte nur ab, solange es angezeigt wird.

- **ZERO** wartet nach dem Loslassen des Fingers auf einen ruhigen Wert:
  Der Hinweis zeigt `ZERO WHEN SETTLED: HOLD STILL`, und der Nullpunkt
  kommt vom ersten ruhigen Wert mindestens 1 s nach dem Loslassen, sodass
  der Stoß des Tippens vorbei ist. Kommt in 10 s kein ruhiger Wert, zeigt er
  `NOT SETTLED IN 10 S: NO ZERO`, und der alte Nullpunkt bleibt. ZERO
  löscht MAX. Es ist als Umriss gezeichnet, solange der Sensor nicht
  antwortet.
- **RESET MAX** löscht MAX.
- **SET** öffnet ANGLE SET.
- **BACK** schaltet den Sensor ab und führt zu SERVO. Nullpunkt und MAX
  sind dann weg: Der nächste Besuch beginnt bei der Waagerechten, da sich
  das Modell inzwischen bewegt haben kann.

Der Winkelmesser merkt nicht, dass sich Modell oder Platine nach dem
Nullpunkt bewegt haben. Ein Nullpunkt gilt nur, solange beide bleiben, wo
sie waren.

<img src="img/angle-steep.png" width="240" alt="ANGLE METER mit 70 Grad geneigter Achse: --- und TOO STEEP, TURN THE AXIS NEARER LEVEL."> <img src="img/angle-zero-timeout.png" width="240" alt="ANGLE METER, nachdem ZERO keinen ruhigen Wert fand: NOT SETTLED IN 10 S: NO ZERO">

---

## ANGLE SET

<img src="img/angle-setup.png" width="240" alt="ANGLE SET: ALONG AXIS LONG, LENGTH 40, SIGN INVERT, CALIBRATION FLIP mit NOT CALIBRATED">

| Zeile | Werte | Was sie tut |
|---|---|---|
| ALONG AXIS | `LONG`, die Voreinstellung, oder `SHORT` | Welche Kante der Platine entlang der Achse liegt, um die sich der Winkel dreht. Eine Änderung verwirft Nullpunkt und MAX |
| LENGTH, MM | `OFF`, die Voreinstellung, oder 10 bis 500 | Die Tiefe des Ruders von der Scharnierlinie bis zur Endleiste, von dir gemessen. `-` und `+` wiederholen beim Halten |
| SIGN | `NORMAL`, die Voreinstellung, oder `INVERT` | Dreht das Vorzeichen des Werts, damit du oben oder unten positiv machen kannst. Nullpunkt und MAX bleiben; die Seiten von MAX tauschen |
| CALIBRATION | `FLIP` | Öffnet FLIP CAL. Darunter: `CALIBRATED` oder `NOT CALIBRATED` |

Eine Änderung gilt sofort. `BACK` führt zur Hauptansicht und speichert die
Einrichtung auf der Platine, wenn sie von der gespeicherten abweicht.
Schlägt das Speichern fehl, zeigt die Hauptansicht
`ANGLE SET NOT SAVED TO FLASH.`, auch bei späteren Besuchen, und die Werte
gelten bis zum Neustart der Platine. `BACK` auf ANGLE SET versucht das
Speichern erneut, mit oder ohne Änderung, und `HOLD TO SAVE` in SETUP
speichert sie ebenfalls. Einrichtung und Kalibrierung bleiben über einen
Neustart erhalten. `HOLD DEFAULTS` in SETUP behält sie, da SETUP sie
nicht zeigt.

### Welche Kante entlang der Achse

Die Platine ist 73 mm lang und 51 mm breit.

| Messung | Die Achse | Lage der Platine | ALONG AXIS |
|---|---|---|---|
| Ausschlag | die Scharnierlinie | lange Kante entlang des Scharniers | `LONG` |
| Ausschlag, Scharnierlinie kürzer als 73 mm | die Scharnierlinie | lange Kante quer zum Scharnier | `SHORT` |
| Einstellwinkel, EWD | die Spannweite | lange Kante entlang der Profiltiefe, auf einer geraden Leiste | `SHORT` |
| V-Form | die Längsachse des Rumpfs | lange Kante entlang der Spannweite | `SHORT` |

Der Winkel ist die Drehung um diese Achse. Er bleibt richtig, wenn die
Achse geneigt ist, aber der Wert wird mit der Neigung unruhiger: Die Leiste
wird ab 45° Bernstein, und ab 70° zeigt der Winkelmesser `---` mit
`TOO STEEP`. Das Scharnier eines Seitenruders steht senkrecht; lege das
Modell auf die Seite oder hebe das Heck, bis das Scharnier höchstens 45°
gegen die Waagerechte steht.

---

## Messen

Nimm beide Werte eines Paars mit demselben Ende der Platine zum selben Ende
oder zur selben Seite des Modells. Eine zwischen den beiden Werten
umgedrehte Platine dreht das Vorzeichen des zweiten Werts.

- **Ausschlag.** Die Platine auf dem Ruder in Neutralstellung, so nah am
  Scharnier, wie das Ruder es zulässt. Prüfe die Neutralstellung mit der
  Platine darauf mit dem Auge. ZERO, dann den Knüppel an jedes Ende: der
  Winkel je Seite, MAX je Seite und mit gesetzter LENGTH der Weg der
  Endleiste in mm.
- **Einstellwinkel und EWD.** Der Winkelmesser misst die Fläche, auf der er
  liegt, nicht die Profilsehne: Die Oberseite eines Profils ist nicht
  parallel zu seiner Sehne. Lege eine gerade Leiste über Nasen- und
  Endleiste oder nutze eine Lehre, die die Sehne hält, und setze die Platine
  auf die Leiste. Leiste auf dem Höhenleitwerk, ZERO; dieselbe Leiste auf
  dem Flügel nahe der Wurzel: Der Wert ist die EWD. Mit der Bezugslinie des
  Rumpfs als Nullpunkt ist jeder Wert der Einstellwinkel dieser Fläche.
- **V-Form.** Die Platine auf eine Flügelhälfte nahe der Wurzel, ZERO, dann
  auf die andere Hälfte: Der Wert ist der Winkel zwischen den Hälften.

## Die Platine halten

- Die Platine liegt auf ihren vier Abstandsbolzen, einer Fläche von etwa
  41 × 60 mm. Ob ein Stecker in einer Buchse auf der Rückseite, darunter der
  des LiPo, höher steht als die Bolzen, ist nicht geprüft: Sieh nach, dass
  die Platine auf allen vier Bolzen liegt und nicht auf einem Stecker.
- Betreibe die Platine beim Messen mit ihrem LiPo. Ein USB-Kabel ist steif
  und schwer gegen ein leichtes Ruder und zieht an der Platine. Lass die
  Zelle an ihrem Kabel neben der Platine hängen oder befestige sie außerhalb
  der Fläche der Bolzen; eine unter die Platine geklebte Zelle wird zu einer
  weichen Auflage.
- Eine Klammer an der Kante des Ruders oder ein Gummiband um Ruder und die
  Enden der Platine, am Glas vorbei, hält sie. Ob ein Band über dem Glas als
  Berührung zählt, ist nicht bekannt.
- Das Gewicht der Platine belastet das Ruder und dreht es durch das
  Getriebespiel des Servos. Das Gewicht der Platine ist nicht gemessen.
  Prüfe das Spiel: Drücke das Ruder leicht in eine Richtung, lass los, lies
  ab; dann in die andere, lass los, lies ab. Zwei ruhige Werte, die sich
  unterscheiden, zeigen das Spiel, das der Wert enthält.
- Die Platine darf nicht über das Scharnier auf die feste Fläche reichen.

---

## FLIP CAL, die Kalibrierung

<img src="img/angle-flip.png" width="240" alt="FLIP CAL mit Ergebnis: OFFSET X 30  Y -20  Z 40 MG, GAIN Z 1.020, HOLD TO KEEP und AGAIN">

Ein Nullpunkt entfernt jeden Fehler, der alle Werte gleich verschiebt. Die
Nullpunktfehler des Sensors machen aber auch einen großen Winkel falsch;
die Kalibrierung findet sie. Sie braucht eine ebene, feste Fläche, waagerecht
oder nicht.

1. `STEP 1 OF 3`: Lege die Platine mit dem Bildschirm nach oben auf die
   Fläche und halte sie 2 s still.
2. `STEP 2 OF 3`: Heb sie an, dreh sie Ende gegen Ende um und leg sie mit
   dem Bildschirm nach oben auf dieselbe Stelle zurück. Halte sie 2 s still.
3. `STEP 3 OF 3`: Dreh sie für 3 s mit dem Bildschirm nach unten, dann
   wieder nach oben. Solange sie mit dem Bildschirm nach unten liegt, wird
   jede Berührung übergangen, und `BACK` und `AGAIN` sind als Umriss
   gezeichnet, damit das Glas auf dem Tisch nichts antippt.

Die Seite zeigt dann den Nullpunktfehler jeder der drei Achsen des Sensors
in mg und die Verstärkung der Achse durch den Bildschirm. Halte
`HOLD TO KEEP` 1 s, um sie zu übernehmen und zu speichern: `KEPT AND SAVED.`.
Eine neue Kalibrierung verwirft Nullpunkt und MAX. `AGAIN` beginnt von
vorn; `BACK` führt zu ANGLE SET, ohne sie zu übernehmen. Ein Ergebnis mit einem
Nullpunktfehler über 0,2 g oder einer Verstärkung außerhalb 0,88 bis 1,12
wird nicht angeboten: `THE POSES GAVE NO CALIBRATION`. Wie gut die
Kalibrierung mit dem verbauten Sensor arbeitet und ob diese Grenzen zu ihm
passen, ist nicht gemessen.

---

## Wenn er nicht misst

| Bildschirm | Bedeutung |
|---|---|
| `NO IMU` und `NO ANSWER FROM THE IMU AT 0X6B.` | Der Sensor der Platine antwortet nicht oder nicht mehr. Der Winkelmesser fragt jede Sekunde neu |
| `NOT A QMI8658: ID 0X..` | Ein anderes Bauteil antwortet an der Adresse des Sensors. Nichts wird hineingeschrieben |
| `STARTING` und `---` | Der Sensor startet, oder 300 ms kam kein Messwert |
| `TOO STEEP` und `---` | Die Achse ist um 70° oder mehr geneigt. Bring sie näher an die Waagerechte |
| `MOVING` | Der Winkel ist nicht 1 s lang innerhalb von 0,5° geblieben. Halte Platine und Modell still |

<img src="img/angle-no-imu.png" width="240" alt="ANGLE METER, kein Sensor antwortet: NO IMU und NO ANSWER FROM THE IMU AT 0X6B.">
