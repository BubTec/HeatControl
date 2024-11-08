# HeatControl - Zweikanal Temperatursteuerung

## Beschreibung
HeatControl ist eine ESP8266-basierte Steuerung für zwei unabhängige Heizkreise mit folgenden Features:
- Zwei unabhängige Temperaturregelkreise
- PID-Regelung für präzise Temperaturkontrolle
- Webinterface zur Konfiguration
- Zwei Betriebsmodi: Normal und Durchheizen (100%)
- Automatisches Captive Portal für einfache Konfiguration
- Logging-System für Fehlersuche

## Hardware-Komponenten

### Benötigte Bauteile
- 1x Wemos D1 Mini Pro (ESP8266)
- 2x DS18B20 Temperatursensoren (wasserdicht, mit Kabel)
- 2x MOSFET IRLZ44N (oder ähnliche Logic-Level MOSFETs)
- 1x 4.7kΩ Pullup-Widerstand (für OneWire Bus)
- 1x 10kΩ Pullup-Widerstand (für Mode-Pin)
- 1x 1000µF Elektrolytkondensator (min. 6.3V)
- Stromversorgung 5V (mindestens 2A)
- Lochrasterplatine oder Platine
- Anschlussklemmen für:
  - 2x Heizungen
  - 2x Temperatursensoren
  - 1x Stromversorgung
- Gehäuse (wasserdicht, wenn im Feuchtraum)
- Kabel in verschiedenen Farben

### Detaillierter Schaltplan

#### Spannungsversorgung
```
   5V Netzteil
      ┌─────────────┐
      │   5V    GND │
      └─┬─────────┬─┘
        │         │
    ┌───┴─────────┴───┐
    │   1000µF/6.3V   │      ┌─────────────────┐
    └───┬─────────┬───┘      │    Wemos D1     │
        │         │          │                  │
        ├────────►│ GND      │ GND             │
        │         │          │                  │
        └────────►│ 5V       │ 5V              │
                  │          │                  │
                  └──────────┘
```

#### Temperatursensoren (OneWire Bus)
```
                     Wemos D1
                  ┌──────────┐
                  │          │
        ┌────────►│ D2      │
        │         │          │
        │   4.7kΩ │          │
        ├────┳────┘          │
        │    │               │
    ┌───┴────┴───┐          │
    │ DS18B20 #1 │          │
    │ VCC DATA GND│          │
    └──┬────┬──┬─┘          │
       │    │  │            │
    ┌──┴────┴──┴─┐          │
    │ DS18B20 #2 │          │
    │ VCC DATA GND│          │
    └──┬────────┬─┘         │
       │        │           │
       └────────┴───────────┘
```

#### MOSFET Ansteuerung
```
                     Wemos D1
                  ┌──────────┐
                  │          │           IRLZ44N
        ┌────────►│ D4      │              ┌──────┐
        │         │          │      Gate ──►│G     │
        │         │          │              │      │ Drain
        │         │          │              │  D   ├───► Heizung 1 (-)
        │         │          │              │      │
        │         │          │              │  S   │
        │         │          │      Source  └──┬───┘
        │         │          │                 │
        │         │          │                 │
        │         │          │                 │
        └─────────┴──────────┴─────────────────┘
                    GND

        (Gleicher Aufbau für Heizung 2 mit D5)
```

#### Moduswechsel-Schaltung
```
                     Wemos D1
                  ┌──────────┐
     VCC (5V)     │          │
        ┌─────────┤ 5V       │
        │         │          │
        │   10kΩ  │          │
        ├──┳──────┤ D5       │
        │  │      │          │
   1000µF│  │     │          │
        │  │      │          │
        │  │      │          │
        └──┴──────┤ GND      │
                  └──────────┘
```

### Pin-Belegung Wemos D1 Mini Pro
- D2 (GPIO4): OneWire Bus für Temperatursensoren
- D4 (GPIO2): MOSFET 1 Gate
- D5 (GPIO1): MOSFET 2 Gate
- D5 (GPIO14): Mode-Pin für Betriebsartenumschaltung
- 5V: Spannungsversorgung
- GND: Masse

### Kompletter Aufbau

#### 1. Spannungsversorgung
- 5V und GND vom Netzteil an Wemos D1
- 1000µF Kondensator parallel zur Versorgung
- Ausreichend dimensionierte Leitungen (min. 0.75mm² für Heizungen)
- Netzteil muss Gesamtstrom der Heizungen plus 500mA Reserve liefern können

#### 2. Temperatursensoren
- Gemeinsamer OneWire Bus an D2
- 4.7kΩ Pullup zwischen VCC und DATA
- Sensoren parallel geschaltet
- Abgeschirmte Kabel bei längeren Leitungen verwenden
- Wasserdichte Ausführung bei Bedarf
- Maximale Kabellänge: 20m

#### 3. MOSFET Treiber
- Gate-Anschlüsse an D4 (Heizung 1) und D5 (Heizung 2)
- Source an GND
- Drain an jeweilige Heizung (-)
- Kühlkörper bei Strömen > 3A
- Freilaufdioden nicht nötig (im MOSFET integriert)
- Gute thermische Anbindung wichtig

#### 4. Moduswechsel-Schaltung
- 10kΩ Pullup zwischen VCC und D5
- 1000µF zwischen VCC und GND
- Möglichst nahe am ESP platzieren
- Dient zur Umschaltung zwischen Normal- und Durchheizmodus

### Montage-Hinweise

#### 1. Allgemeine Montage
- Komponenten auf Lochrasterplatine oder Platine montieren
- Ausreichend Abstand zwischen Leistungsbauteilen
- Kabelquerschnitte nach Strom dimensionieren
- Zugentlastung für alle Kabel vorsehen
- Gehäuse mit ausreichender Belüftung wählen

#### 2. Temperatursensoren
- Guten thermischen Kontakt sicherstellen
- Wasserdicht vergießen wenn nötig
- Kabel können verlängert werden (max. 20m)
- Bei Störungen: abgeschirmtes Kabel verwenden

#### 3. MOSFETs
- Kühlkörper bei Strömen > 3A
- Thermische Leitpaste verwenden
- Ausreichend Abstand zwischen den MOSFETs
- Gute Belüftung sicherstellen

#### 4. Gehäuse
- IP-Schutzart nach Einsatzort wählen
- Ausreichende Belüftung für MOSFETs
- Kabeldurchführungen abdichten
- Zugentlastung für alle Kabel

### Betriebsmodi

#### 1. Normal-Modus
- PID-geregelte Temperatursteuerung
- Separate Sollwerte für beide Heizkreise
- Einstellbare PID-Parameter
- Temperaturbereich: 10-40°C

#### 2. Durchheiz-Modus
- Beide Heizkreise auf 100% Leistung
- Keine Temperaturregelung
- Nützlich zum schnellen Aufheizen
- Automatische Rückkehr in Normal-Modus bei längerem Stromausfall

#### Moduswechsel
- Kurzes Aus- und Einschalten (< 2s): Wechsel zwischen Modi
- Längeres Ausschalten (> 10s): Start im Normal-Modus
- Status wird im Web-Interface angezeigt

### Sicherheitshinweise

#### 1. Elektrische Sicherheit
- Kurzschlussfestes Netzteil verwenden
- Überlastschutz vorsehen
- Saubere Kabelführung
- Gehäuse erden wenn metallisch

#### 2. Temperatursicherheit
- Regelmäßige Funktionsprüfung der Sensoren
- Redundanz durch zwei Sensoren
- Fail-Safe bei Sensorausfall
- Temperaturüberwachung implementiert

#### 3. Brandschutz
- Brandschutzvorschriften beachten
- Ausreichende Belüftung sicherstellen
- Keine brennbaren Materialien in der Nähe
- Regelmäßige Wartung durchführen

### Erste Inbetriebnahme

1. Hardware aufbauen und prüfen
2. ESP mit dem Code flashen
3. Stromversorgung anschließen
4. Mit dem WLAN "HeatControl" verbinden (Passwort: HeatControl)
5. Im Browser 192.168.4.1 aufrufen
6. Temperaturen und PID-Parameter einstellen
7. Funktion testen

### Wartung und Pflege

1. Regelmäßige Kontrollen
   - Temperatursensoren auf Funktion prüfen
   - Kabelverbindungen kontrollieren
   - Kühlkörper reinigen
   - Gehäusebelüftung prüfen

2. Software-Updates
   - Aktuelle Version prüfen
   - Backup der Einstellungen
   - Update durchführen
   - Einstellungen wiederherstellen

3. Fehlersuche
   - LED-Status am ESP beachten
   - Serial Monitor (115200 Baud) für Debug-Ausgaben
   - Temperaturwerte auf Plausibilität prüfen
   - PWM-Ausgänge mit Multimeter/Oszilloskop prüfen
   - Log-Dateien analysieren

### Technische Daten

- Betriebsspannung: 5V DC
- Stromaufnahme: max. 500mA (ohne Heizungen)
- Temperaturbereich: 10-40°C
- Regelgenauigkeit: ±0.5°C
- WLAN: 2.4GHz
- Schutzart: abhängig vom Gehäuse
- Maße: abhängig vom Aufbau