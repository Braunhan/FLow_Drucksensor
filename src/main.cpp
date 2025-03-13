/*****************************************************
 * main.cpp – Firmware für die Messstation auf dem ESP32
 *
 * Funktionen:
 *   - Messung des Drucks über 4 analoge Kanäle (ADS1115)
 *   - Messung des Durchflusses über 2 digitale Sensoren (Interrupts)
 *   - Datenlogging in SPIFFS (CSV-Format)
 *   - Speicherung und Verwaltung von Kalibrierungswerten im EEPROM
 *   - Webserver im Access Point-Modus (AP) mit API-Endpunkten
 *   - Auslieferung statischer Dateien (HTML, CSS, JavaScript) aus SPIFFS
 *
 * Hinweis: Die Webseitendateien (index.html, style.css, script.js)
 *          liegen im Ordner "data" und werden über das "ESP32 Sketch Data Upload"
 *          Tool in das SPIFFS hochgeladen.
 *****************************************************/

/* ====================================================
 * 1. Einbinden der benötigten Bibliotheken
 * ==================================================== */
#include <SPI.h>              // SPI-Kommunikation (wird z. B. für SPIFFS genutzt)
#include <Arduino.h>          // Grundlegende Arduino-Funktionen
#include <WiFi.h>             // WLAN-Funktionalität
#include <WebServer.h>        // Webserver-Bibliothek
#include <Wire.h>             // I²C-Kommunikation
#include <Adafruit_ADS1X15.h> // ADS1115 Bibliothek (Analog-Digital-Wandler)
#include <EEPROM.h>           // EEPROM-Verwaltung (Kalibrierungswerte speichern)
#include <SPIFFS.h>           // SPIFFS (Dateisystem auf dem ESP32)
#include <time.h>             // Zeitfunktionen (für NTP und Zeitstempel)

/* ====================================================
 * 2. Globale Konfigurationen und Definitionen
 * ==================================================== */

// ----- WLAN-Konfiguration (Access Point) -----
const char* ssid = "Druck-Durchflusssensor";  // SSID des AP
const char* password = "12345678";             // Passwort des AP
IPAddress local_IP(192, 168, 1, 1);            // Lokale IP-Adresse des ESP32
IPAddress gateway(192, 168, 1, 1);             // Gateway-Adresse
IPAddress subnet(255, 255, 255, 0);            // Subnetzmaske

// ----- ADS1115-Konfiguration für Drucksensoren ----- 
Adafruit_ADS1115 ads;                          // Erstellen eines ADS1115-Objekts
#define I2C_SDA 21                            // I²C SDA-Pin
#define I2C_SCL 22                            // I²C SCL-Pin
#define ADS_VOLTAGE_PER_BIT 0.000125          // Umrechnung: 0.000125 Volt pro Bit (bei GAIN_ONE)

// ----- Konfiguration der Durchflusssensoren (digitale Sensoren) -----
#define FLOW_SENSOR1_PIN 32                   // Pin für Durchflusssensor 1
#define FLOW_SENSOR2_PIN 33                   // Pin für Durchflusssensor 2
volatile uint32_t pulseCount1 = 0;             // Impulszähler für Sensor 1 (wird in ISR erhöht)
volatile uint32_t pulseCount2 = 0;             // Impulszähler für Sensor 2 (wird in ISR erhöht)
uint32_t lastPulseCount1 = 0;                  // Letzter Zählerstand für Sensor 1 (zur Delta-Berechnung)
uint32_t lastPulseCount2 = 0;                  // Letzter Zählerstand für Sensor 2 (zur Delta-Berechnung)
float flowRate1 = 0.0;                         // Momentaner Durchfluss (L/min) für Sensor 1
float flowRate2 = 0.0;                         // Momentaner Durchfluss (L/min) für Sensor 2
float cumulativeFlow1 = 0.0;                   // Kumulativer Durchfluss (L) für Sensor 1
float cumulativeFlow2 = 0.0;                   // Kumulativer Durchfluss (L) für Sensor 2

// ----- Drucksensor-Kalibrierung -----
// Früher wurde immer 0,5 V als Referenz genommen (0 PSI).
// Zur späteren Kalibrierung wird hier sensorZero geführt.
// Zur Fehlerbehebung wurde hier die Umrechnung auf den festen Wert 0,5 V umgestellt.
float sensorZero[4] = {0.5, 0.5, 0.5, 0.5};

// ----- Logging-Konfiguration -----
bool recording = false;                        // Aufnahmemodus (Recording): Ein/Aus
const char* logFileName = "/log.csv";          // Dateiname für Logdaten im SPIFFS

// ----- Zeitsteuerung (Messintervall) ----- 
unsigned long previousMillis = 0;              // Hilfsvariable für Zeitmessung
const unsigned long interval = 1000;           // Intervall in Millisekunden (1 Sekunde)

// ----- Webserver-Objekt -----
WebServer server(80);                          // Webserver, der auf Port 80 lauscht

/* ====================================================
 * 3. Funktionsprototypen (Vorwärtsdeklarationen)
 * ==================================================== */

// Sensor- und Logging-Funktionen
float readPressureSensor(uint8_t channel);     // Liest den Drucksensor am angegebenen Kanal aus
void logData();                                // Schreibt Messdaten als CSV-Zeile in SPIFFS
String getTimeString();                        // Gibt die aktuelle Systemzeit als String zurück

// Interrupt-Service-Routinen für Durchflusssensoren
void IRAM_ATTR flowSensor1ISR();               // ISR für Durchflusssensor 1 (wird bei FALLING-Edge aufgerufen)
void IRAM_ATTR flowSensor2ISR();               // ISR für Durchflusssensor 2 (wird bei FALLING-Edge aufgerufen)

// Funktionen zur Kalibrierung und EEPROM-Verwaltung
void loadCalibration();                        // Lädt Kalibrierungswerte aus dem EEPROM
void saveCalibration();                        // Speichert aktuelle Kalibrierungswerte ins EEPROM

// Webserver-Handler (HTTP-Endpunkte)
// Statische Dateien (Webseitendateien)
void handleRoot();                             // Liefert index.html aus SPIFFS
void handleCSS();                              // Liefert style.css aus SPIFFS
void handleJS();                               // Liefert script.js aus SPIFFS

// API-Endpunkte
void handleSensorwerte();                      // Liefert aktuelle Sensorwerte (JSON)
void handleGetTime();                          // Liefert die aktuelle Zeit als Text
void handleSetTime();                          // Setzt die Systemzeit (Parameter: t)
void handleDownloadLog();                      // Ermöglicht das Herunterladen der Logdatei (CSV)
void handleToggleRecording();                  // Schaltet das Recording (Datenlogging) um
void handleDeleteLog();                        // Löscht die Logdatei
void handleClearCumulativeFlow();              // Setzt den kumulativen Durchfluss zurück
void handleSetCalibration();                   // Setzt den Kalibrierungswert für einen Sensor

/* ====================================================
 * 4. Setup – Initialisierung aller Module
 * ==================================================== */
void setup() {
  // Serielle Kommunikation initialisieren (für Debug-Ausgaben)
  Serial.begin(115200);
  delay(1000);

  // ----- EEPROM initialisieren -----
  // 64 Byte reichen zur Speicherung von 4 Float-Werten (Kalibrierungswerte)
  EEPROM.begin(64);
  loadCalibration();

  // ----- I²C initialisieren -----
  // Initialisiert den I²C-Bus mit den festgelegten SDA- und SCL-Pins
  Wire.begin(I2C_SDA, I2C_SCL);

  // ----- ADS1115 initialisieren -----
  // Suche solange nach dem ADS1115, bis es gefunden wurde
  Serial.println("Suche ADS1115...");
  while (!ads.begin(0x48)) { // 0x48 ist die Standardadresse
    Serial.println("ADS1115 nicht gefunden, versuche erneut in 1 Sekunde...");
    delay(1000);
  }
  Serial.println("ADS1115 erkannt!");
  // Setze den Gain auf GAIN_ONE (optimal für den Messbereich 0,5V bis 4,5V)
  ads.setGain(GAIN_ONE);

  // ----- SPIFFS initialisieren -----
  // SPIFFS wird zur Speicherung von Webseitendateien und Logs verwendet.
  if (!SPIFFS.begin(true)) { // Falls nötig, erfolgt eine automatische Formatierung
    Serial.println("SPIFFS konnte nicht eingebunden werden!");
  }

  // ----- WLAN im Access Point-Modus konfigurieren -----
  // Der ESP32 erstellt einen eigenen WLAN-Hotspot
  WiFi.softAPConfig(local_IP, gateway, subnet);
  WiFi.softAP(ssid, password);
  Serial.print("Access Point IP: ");
  Serial.println(WiFi.softAPIP());

  // ----- Webserver-Routen definieren -----
  // Statische Dateien: index.html, style.css, script.js
  server.on("/", HTTP_GET, handleRoot);
  server.on("/index.html", HTTP_GET, handleRoot);
  server.on("/style.css", HTTP_GET, handleCSS);
  server.on("/script.js", HTTP_GET, handleJS);

  // API-Endpunkte für Sensorwerte, Zeit, Logging und Kalibrierung
  server.on("/api/sensorwerte", HTTP_GET, handleSensorwerte);
  server.on("/getTime", HTTP_GET, handleGetTime);
  server.on("/setTime", HTTP_GET, handleSetTime);
  server.on("/downloadlog", HTTP_GET, handleDownloadLog);
  server.on("/toggleRecording", HTTP_GET, handleToggleRecording);
  server.on("/deleteLog", HTTP_GET, handleDeleteLog);
  server.on("/clearCumulativeFlow", HTTP_GET, handleClearCumulativeFlow);
  server.on("/setCalibration", HTTP_GET, handleSetCalibration);

  // Starte den Webserver
  server.begin();

  // ----- Konfiguration der Durchflusssensor-Pins -----
  // Verwende interne Pullup-Widerstände, um Fehltrigger zu minimieren
  pinMode(FLOW_SENSOR1_PIN, INPUT_PULLUP);
  pinMode(FLOW_SENSOR2_PIN, INPUT_PULLUP);

  // ----- Interrupts für die Durchflusssensoren binden -----
  // Bei fallender Flanke (FALLING) wird die jeweilige ISR aufgerufen.
  attachInterrupt(digitalPinToInterrupt(FLOW_SENSOR1_PIN), flowSensor1ISR, FALLING);
  attachInterrupt(digitalPinToInterrupt(FLOW_SENSOR2_PIN), flowSensor2ISR, FALLING);

  // ----- Zeitsystem initialisieren -----
  // Synchronisation mit einem NTP-Server ("pool.ntp.org") zur korrekten Zeitstempelung
  configTime(0, 0, "pool.ntp.org");
}

/* ====================================================
 * 5. Loop – Hauptprogrammzyklus
 * ==================================================== */
void loop() {
  // Bearbeite eingehende HTTP-Anfragen
  server.handleClient();

  // Zeitgesteuerte Aufgaben: Alle 1 Sekunde
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;

    // ----- a) Drucksensoren auslesen -----
    // Lese alle 4 Kanäle des ADS1115 und berechne den Druck in bar.
    float pressures[4];
    for (uint8_t i = 0; i < 4; i++) {
      pressures[i] = readPressureSensor(i);
    }

    // ----- b) Durchfluss auswerten -----
    // Deaktiviere kurzzeitig Interrupts, um konsistente Zählerstände zu erhalten.
    noInterrupts();
    uint32_t currentPulse1 = pulseCount1;
    uint32_t currentPulse2 = pulseCount2;
    interrupts();

    // Berechne die Impulsdifferenz seit der letzten Auswertung.
    uint32_t delta1 = currentPulse1 - lastPulseCount1;
    uint32_t delta2 = currentPulse2 - lastPulseCount2;
    lastPulseCount1 = currentPulse1;
    lastPulseCount2 = currentPulse2;

    // Berechne den momentanen Durchfluss in L/min (Impulsfrequenz in Hz / 98)
    flowRate1 = delta1 / 98.0;
    flowRate2 = delta2 / 98.0;
    // Aktualisiere den kumulativen Durchfluss (Umrechnung: L/min in Liter pro Sekunde)
    cumulativeFlow1 += flowRate1 / 60.0;
    cumulativeFlow2 += flowRate2 / 60.0;

    // Zusätzliche Debug-Ausgabe für die Flow-Werte
    Serial.print("Puls1: ");
    Serial.print(delta1);
    Serial.print(" / Flow1: ");
    Serial.print(flowRate1, 2);
    Serial.print(" L/min, Puls2: ");
    Serial.print(delta2);
    Serial.print(" / Flow2: ");
    Serial.print(flowRate2, 2);
    Serial.println(" L/min");

    // ----- c) Datenlogging (Recording) -----
    if (recording) {
      logData();
    }

    // ----- d) Debug-Ausgabe der Druckwerte -----
    Serial.print("Druck (bar): ");
    for (uint8_t i = 0; i < 4; i++) {
      Serial.print(pressures[i], 3);
      Serial.print("  ");
    }
    Serial.println();
  }
}

/* ====================================================
 * 6. Interrupt Service Routinen (ISRs) für Durchflusssensoren
 * ==================================================== */
// Erhöht den Impulszähler für Sensor 1, wenn ein Impuls erkannt wird.
void IRAM_ATTR flowSensor1ISR() {
  pulseCount1++;
}

// Erhöht den Impulszähler für Sensor 2, wenn ein Impuls erkannt wird.
void IRAM_ATTR flowSensor2ISR() {
  pulseCount2++;
}

/* ====================================================
 * 7. Funktionen zur Drucksensor-Abfrage und Datenlogging
 * ==================================================== */

// Liest den angegebenen Drucksensor (Kanal) aus und wandelt den gemessenen
// Spannungswert in einen Druck in bar um. Hier wird wieder der feste Referenzwert
// 0,5 V genutzt, um die Umrechnung so durchzuführen wie im alten Code.
// Formel: PSI = (Spannung - 0,5) * 7.5, Bar = PSI / 14.5038
float readPressureSensor(uint8_t channel) {
  // Lese den Rohwert des ADC am entsprechenden Kanal
  int16_t rawValue = ads.readADC_SingleEnded(channel);
  // Berechne die Spannung anhand des Umrechnungsfaktors
  float voltage = rawValue * ADS_VOLTAGE_PER_BIT;

  // Debug-Ausgabe: Zeige Kanal, Rohwert und Spannung an
  Serial.print("Kanal ");
  Serial.print(channel);
  Serial.print(" Rohwert: ");
  Serial.print(rawValue);
  Serial.print("  Spannung: ");
  Serial.print(voltage, 3);
  Serial.print(" V   ");

  // Umrechnung der Spannung in PSI unter Verwendung des festen Offsets 0,5 V
  float pressurePSI = (voltage - 0.5) * 7.5;
  if (pressurePSI < 0) pressurePSI = 0;  // Verhindere negative Werte
  // Umrechnung von PSI in bar
  float pressureBar = pressurePSI / 14.5038;

  // Debug-Ausgabe: Zeige den berechneten Druck an
  Serial.print("  Druck: ");
  Serial.print(pressureBar, 3);
  Serial.println(" bar");

  return pressureBar;
}

// Schreibt eine Zeile mit Messdaten in eine CSV-Datei im SPIFFS.
// Fügt bei einer neuen Datei zuerst eine Kopfzeile ein.
void logData() {
  File file = SPIFFS.open(logFileName, FILE_APPEND);
  if (!file) {
    Serial.println("Fehler beim Öffnen der Logdatei zum Anhängen");
    return;
  }

  // Falls die Datei leer ist, wird eine Kopfzeile geschrieben.
  if (file.size() == 0) {
    String header = "Timestamp,Pressure1 (bar),Pressure2 (bar),Pressure3 (bar),Pressure4 (bar),"
                    "FlowRate1 (L/min),FlowRate2 (L/min),CumulativeFlow1 (L),CumulativeFlow2 (L)\n";
    file.print(header);
  }

  // Erstelle den Zeitstempel
  String timeStr = getTimeString();
  // Lese alle Drucksensoren aus
  float pressures[4];
  for (uint8_t i = 0; i < 4; i++) {
    pressures[i] = readPressureSensor(i);
  }

  // Erstelle die CSV-Zeile
  String line = "";
  line += timeStr + ",";
  for (uint8_t i = 0; i < 4; i++) {
    line += String(pressures[i], 3) + ",";
  }
  line += String(flowRate1, 2) + ",";
  line += String(flowRate2, 2) + ",";
  line += String(cumulativeFlow1, 2) + ",";
  line += String(cumulativeFlow2, 2) + "\n";

  file.print(line);
  file.close();
}

// Gibt die aktuelle Systemzeit als formatierten String zurück (YYYY-MM-DD HH:MM:SS)
String getTimeString() {
  time_t now = time(nullptr);
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);
  char buf[30];
  sprintf(buf, "%04d-%02d-%02d %02d:%02d:%02d",
          timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
          timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
  return String(buf);
}

/* ====================================================
 * 8. Webserver-Handler: Ausliefern statischer Dateien und API-Endpunkte
 * ==================================================== */

// ----- Auslieferung statischer Dateien ----- 

// Liefert die Datei "index.html" aus dem SPIFFS.
// Falls die Datei nicht existiert, wird ein 404-Fehler zurückgegeben.
void handleRoot() {
  if (SPIFFS.exists("/index.html")) {
    File file = SPIFFS.open("/index.html", FILE_READ);
    server.streamFile(file, "text/html");
    file.close();
  } else {
    server.send(404, "text/plain", "Datei /index.html nicht gefunden");
  }
}

// Liefert die Datei "style.css" aus dem SPIFFS.
void handleCSS() {
  if (SPIFFS.exists("/style.css")) {
    File file = SPIFFS.open("/style.css", FILE_READ);
    server.streamFile(file, "text/css");
    file.close();
  } else {
    server.send(404, "text/plain", "Datei /style.css nicht gefunden");
  }
}

// Liefert die Datei "script.js" aus dem SPIFFS.
void handleJS() {
  if (SPIFFS.exists("/script.js")) {
    File file = SPIFFS.open("/script.js", FILE_READ);
    server.streamFile(file, "application/javascript");
    file.close();
  } else {
    server.send(404, "text/plain", "Datei /script.js nicht gefunden");
  }
}

// ----- API-Endpunkte -----

// Liefert aktuelle Sensorwerte als JSON-String.
// Enthält Zeit, Druckwerte aller 4 Sensoren, momentanen und kumulativen Durchfluss sowie den Recording-Status.
void handleSensorwerte() {
  float pressures[4];
  for (uint8_t i = 0; i < 4; i++) {
    pressures[i] = readPressureSensor(i);
  }
  
  String json = "{";
  json += "\"time\":\"" + getTimeString() + "\",";
  json += "\"pressure\":[";
  for (uint8_t i = 0; i < 4; i++) {
    json += String(pressures[i], 3);
    if (i < 3) json += ",";
  }
  json += "],";
  json += "\"flowRate\":[";
  json += String(flowRate1, 2) + "," + String(flowRate2, 2);
  json += "],";
  json += "\"cumulativeFlow\":[";
  json += String(cumulativeFlow1, 2) + "," + String(cumulativeFlow2, 2);
  json += "],";
  json += "\"recording\":" + String(recording ? "true" : "false");
  json += "}";
  
  server.send(200, "application/json", json);
}

// Liefert die aktuelle Systemzeit als einfachen Text.
void handleGetTime() {
  server.send(200, "text/plain", getTimeString());
}

// Setzt die Systemzeit. Erwartet einen GET-Parameter "t", der den Zeitstempel enthält.
void handleSetTime() {
  if (server.hasArg("t")) {
    time_t t = server.arg("t").toInt();
    struct timeval tv;
    tv.tv_sec = t;
    tv.tv_usec = 0;
    settimeofday(&tv, NULL);
    server.send(200, "text/plain", "Zeit aktualisiert");
  } else {
    server.send(400, "text/plain", "Fehlender Parameter 't'");
  }
}

// Ermöglicht das Herunterladen der Logdatei (CSV-Format).
void handleDownloadLog() {
  if (SPIFFS.exists(logFileName)) {
    File file = SPIFFS.open(logFileName, FILE_READ);
    server.streamFile(file, "text/csv");
    file.close();
  } else {
    server.send(404, "text/plain", "Logdatei nicht gefunden");
  }
}

// Schaltet das Recording (Datenlogging) um und liefert eine Bestätigung.
void handleToggleRecording() {
  recording = !recording;
  server.send(200, "text/plain", recording ? "Recording gestartet" : "Recording gestoppt");
}

// Löscht die Logdatei aus dem SPIFFS.
void handleDeleteLog() {
  if (SPIFFS.exists(logFileName)) {
    SPIFFS.remove(logFileName);
    server.send(200, "text/plain", "Logdatei gelöscht");
  } else {
    server.send(404, "text/plain", "Logdatei nicht gefunden");
  }
}

// Setzt den kumulativen Durchfluss zurück (für beide Sensoren).
void handleClearCumulativeFlow() {
  cumulativeFlow1 = 0;
  cumulativeFlow2 = 0;
  server.send(200, "text/plain", "Kumulativer Durchfluss zurückgesetzt");
}

// Setzt den Kalibrierungswert für einen Drucksensor.
// Erwartet GET-Parameter "sensor" (Index 0-3) und "value" (neuer Spannungswert bei 0 PSI).
void handleSetCalibration() {
  if (server.hasArg("sensor") && server.hasArg("value")) {
    int sensorIndex = server.arg("sensor").toInt();
    float value = server.arg("value").toFloat();
    if (sensorIndex >= 0 && sensorIndex < 4) {
      sensorZero[sensorIndex] = value;
      saveCalibration();
      server.send(200, "text/plain", "Kalibrierung für Sensor " + String(sensorIndex + 1) + " aktualisiert");
      return;
    }
  }
  server.send(400, "text/plain", "Ungültige Parameter");
}

/* ====================================================
 * 9. Funktionen zur Kalibrierung und EEPROM-Verwaltung
 * ==================================================== */
// Lädt die Kalibrierungswerte aus dem EEPROM.
// Falls ein Wert außerhalb des erwarteten Bereichs (0-5V) liegt, wird der Standardwert 0,5V verwendet.
void loadCalibration() {
  for (uint8_t i = 0; i < 4; i++) {
    EEPROM.get(i * sizeof(float), sensorZero[i]);
    if (sensorZero[i] < 0.0 || sensorZero[i] > 5.0) {
      sensorZero[i] = 0.5;
    }
  }
}

// Speichert die aktuellen Kalibrierungswerte im EEPROM.
void saveCalibration() {
  for (uint8_t i = 0; i < 4; i++) {
    EEPROM.put(i * sizeof(float), sensorZero[i]);
  }
  EEPROM.commit();
}
