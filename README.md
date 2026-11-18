# AFG Control

Windows-Anwendung zur Steuerung des Rohde & Schwarz Function Generator AFG 377.2100.02 über NI-488.2. Die Bedienung und Befehle folgen dem „AFG Betriebshandbuch.pdf“ im Projektordner, insbesondere den Abschnitten 2.3 und 2.4 sowie Bild 2-37 auf PDF-Seite 122.

## Start und Verbindung

build\AFG-Control.exe starten. Unter Einstellungen → Verbindung sind GPIB-Bus 0 und Primäradresse 12 voreingestellt. Die am AFG eingestellte Adresse muss dazu passen. Der Hardwarebetrieb benötigt den installierten 64-Bit-NI-488.2-Treiber und einen zugehörigen GPIB-Controller.

Verbinden aktiviert die Fernsteuerung und richtet TRANSFERMODE(2) sowie SRQ für Fertigmeldung, Fehler und Überspannung ein. Signalparameter werden mit „Einstellungen übertragen“ gesendet. „Ausgang ein“ und „Ausgang aus“ schalten den Ausgang gezielt. Trennen und Schließen geben das Gerät an die lokale Bedienung zurück.

## Bedienung

- Signal bietet Sinus, Dreieck, Rechteck, Trapez und Puls. Pegel (Spitze–Spitze) und Offset beziehen sich auf eine 50-Ω-Last und können in V oder mV eingegeben werden. Für Signal-, Sweep-, Marker- und FSK-Frequenzen stehen Hz und MHz zur Wahl. Beim Wechsel der Einheit wird der angezeigte Wert umgerechnet. Zeitwerte werden in Sekunden eingegeben. Exponentialschreibweise ist möglich, beispielsweise 25e-9 für 25 ns oder 1e6 für 1 MHz. Das Dezimaltrennzeichen folgt der Sprache.
- Betrieb & Trigger bietet kontinuierlichen Betrieb, Burst, Gate und Halbzyklus. Für externe oder manuelle Triggerung zuerst die Einstellungen übertragen. Anschließend lässt sich „Manuell triggern“ verwenden.
- Sweep bietet linearen oder logarithmischen Verlauf, aufwärts, abwärts und abwechselnd auf/ab, automatische Wiederholung, einmaligen Ablauf und externen Trigger sowie vier Frequenzmarken. „Sweep starten“ überträgt die vorbereiteten Signal- und Sweep-Einstellungen. Anhalten, Zur Startfrequenz und Sweep aus senden die entsprechenden AFG-Befehle. Die Richtung bestimmt der AFG unabhängig von der Reihenfolge von Frequenz 1 und 2.
- Modulation bietet AM, FM, VCO, FSK und Pulsmodulation PM. Zulässige Kombinationen und Betriebsarten werden gemäß Handbuch geprüft. AM, FM und VCO benötigen das externe Modulationssignal am jeweiligen Eingang.
- Gerät enthält die Speicherplätze 0 bis 15, das Ausgangsverhalten bei abgeschaltetem Signal, die Länge der Gerätefehlermeldungen, Statusabfrage, PRESET, Selbsttests und Kalibrierung. Die Warmlauf- und Anschlussbedingungen zur Kalibrierung stehen in Handbuch 2.3.10.2. PRESET schaltet gemäß Gerätehandbuch den Ausgang mit 1 kHz Sinus und 10 mVss ein.

Bei Ausgang aus und Ri OFF = 50 Ω liegt der eingestellte DC-Offset weiterhin an. Ri OFF = hochohmig trennt den Ausgang per Relais. Siehe Handbuch 2.3.10.3.

Die Oberfläche zeigt vorbereitete PC-Einstellungen und den zuletzt bestätigten Ausgangsbefehl. Nach Verbinden, Gerätefehler oder Speicheraufruf gilt der Ausgangsstatus als unbekannt. Der AFG-Befehlssatz bietet keine Abfrage seiner gesamten Einstellung. Nach RECALL gelten die Gerätespeicherwerte, während die PC-Felder ihre vorbereiteten Werte behalten.

„Einstellungen übertragen“ beendet einen laufenden Sweep und überträgt Signalform, Signalparameter, Betriebsart, Trigger und Modulation. Abhängige Parameter werden gemeinsam in einer Befehlsgruppe übertragen. Bei einem Gerätefehler wird die Befehlsfolge abgebrochen. Bereits ausgeführte Befehle bleiben am Gerät wirksam.

## Menüs und Einstellungen

Die Menüzeile enthält Datei mit Beenden, Einstellungen mit GPIB-Verbindung, Hell/Dunkel und Einstellungen speichern, rechts das Sprachsymbol für Deutsch/English/Français sowie About und About Qt. About zeigt Anwendung, Version und das gemeinsame animierte Logo.

Strg+S speichert Eingaben, gewählte Einheiten, GPIB-Ziel und Darstellung im eigenen Benutzerbereich Tralow/AFG-Control. Die Sprachwahl wird sofort gespeichert. Ein Neustart stellt die Eingabefelder wieder her. Das Gerät wird erst nach Verbinden und den jeweiligen Bedienaktionen angesteuert.

Unter **Über → Nach Updates suchen** lässt sich das neueste stabile GitHub-Release manuell abfragen. Bei einem privaten Repository benötigt die Abfrage die [GitHub CLI](https://cli.github.com/) und ein bereits per `gh auth login` angemeldetes Konto mit Zugriff auf das Repository. Bei einer neueren Version öffnet die Schaltfläche die Release-Seite im Browser.

## Build

Qt 6 mit Widgets und Linguist Tools, CMake und das passende 64-Bit-MinGW-Kit werden benötigt.

Das Build-Skript benötigt CMake ab 3.24 und baut über eine temporäre Windows-Verzeichnisverknüpfung ohne Umlaut, damit die Qt/MinGW-Werkzeuge den Projektpfad verarbeiten können. Quellen und Ergebnis liegen im AFG-Verzeichnis.

~~~powershell
.\build.ps1
~~~

Qt-Kit und Compilerpfad lassen sich über die Parameter QtKit und CompilerDirectory des Skripts anpassen.

Die GPIB-Anbindung und Gestaltung übernehmen Bausteine der vorhandenen Geräteprogramme. Der AFG-Quellcode und das animierte OsciViewer-Logo von Ralf Tralow stehen unter der [MIT-Lizenz](LICENSE). Hinweise zu den mitgelieferten Bibliotheken stehen in [THIRD-PARTY-NOTICES.txt](THIRD-PARTY-NOTICES.txt). Die Software wurde mit einer simulierten GPIB-Verbindung und anhand der Handbuchbefehle geprüft. Die Verbindung zum physischen AFG ist noch zu prüfen.
