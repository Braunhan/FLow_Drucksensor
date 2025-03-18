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
#include <math.h>             // Für isnan()
#include <ArduinoJson.h>      // JSON-Verarbeitung
#include <HTTPClient.h>      // HTTP-Client für Anfragen an die API
#include <Preferences.h>      // Einfache Speicherung von Einstellungen

/* ====================================================
 * 2. Globale Variablen und Konfigurationen
 * ==================================================== */

/* ----- WLAN Konfiguration ----- */
const char* ssid = "Druck-Durchflusssensor";  // SSID des Access Points
const char* password = "12345678";             // Passwort des Access Points
IPAddress local_IP(192, 168, 1, 1);            // Lokale IP-Adresse des ESP32
IPAddress gateway(192, 168, 1, 1);             // Gateway (gleich wie IP)
IPAddress subnet(255, 255, 255, 0);            // Subnetzmaske

/* ----- ADS1115 Konfiguration ----- */
Adafruit_ADS1115 ads;                          // Objekt für den ADS1115
#define I2C_SDA 21                            // I²C SDA-Pin (Datenleitung)
#define I2C_SCL 22                            // I²C SCL-Pin (Taktleitung)
#define ADS_VOLTAGE_PER_BIT 0.000125          // Umrechnungsfaktor: 0.000125 V pro Bit
// --- deutsches format der float-Zahlen
String toGermanFloatString(float f, unsigned int decimals = 2) {  // Ändere uint8_t zu unsigned int
  String s = String(f, decimals);
  s.replace('.', ',');
  return s;
}
// ---  In-Memory-Datenpuffer für Messwerte der letzten 10 Minuten ---
#define BUFFER_SIZE 600  // 600 Einträge = 10 Minuten bei 1 Hz

struct SensorData {
  time_t timestamp;      // Zeitpunkt der Messung
  float pressure[4];     // Druckwerte aller 4 Sensoren
  float flowRate1;       // Durchflusswert Sensor 1
  float flowRate2;       // Durchflusswert Sensor 2
};

SensorData dataBuffer[BUFFER_SIZE];  // Puffer für die letzten 10 Minuten
int bufferIndex = 0;                   // Aktueller Index im Puffer

// ---  In-Memory-Datenpuffer für Messwerte der geloggten Daten ---
#define LOGGING_BUFFER_SIZE 2000  // z.B. 1000 Einträge für den Logging-Puffer

struct LoggingData {
  time_t timestamp;
  float pressure[4];
  float flow1;
  float flow2;
};

LoggingData loggingBuffer[LOGGING_BUFFER_SIZE];
int loggingIndex = 0;

/* ----- Kalibrierungsvariablen für Drucksensoren -----
   Für jeden der 4 Sensoren werden nun vier Werte verwendet:
     - pressureSensor_V_min: Minimal gemessene Spannung, die 0 bar (0 PSI) entspricht (Standard: 0.5 V)
     - pressureSensor_V_max: Maximal gemessene Spannung, z. B. 4.5 V
     - pressureSensor_PSI_min: Der Druck in PSI, der der minimalen Spannung entspricht (normalerweise 0 PSI)
     - pressureSensor_PSI_max: Der Druck in PSI, der bei der maximalen Spannung erreicht wird (z. B. 30 PSI)
     
   Daraus wird in der Druckmessfunktion der Umrechnungsfaktor berechnet:
      convFactor = (PSI_max - PSI_min) / (V_max - V_min)
*/
float pressureSensor_V_min[4]    = {0.5, 0.5, 0.5, 0.5};
float pressureSensor_V_max[4]    = {4.5, 4.5, 4.5, 4.5};
float pressureSensor_PSI_min[4]  = {0.0, 0.0, 0.0, 0.0};
float pressureSensor_PSI_max[4]  = {30.0, 30.0, 30.0, 30.0};

// Globale Variable zum Speichern der kalibrierten v_min-Werte für jeden Sensor
float pressureSensor_V_min_cal[4] = {0.5, 0.5, 0.5, 0.5};

/* ----- Durchflusssensor Konfiguration ----- */
#define FLOW_SENSOR1_PIN 32                   // Pin für Durchflusssensor 1
#define FLOW_SENSOR2_PIN 33                   // Pin für Durchflusssensor 2
volatile uint32_t pulseCount1 = 0;             // Impulszähler für Sensor 1 (wird in ISR erhöht)
volatile uint32_t pulseCount2 = 0;             // Impulszähler für Sensor 2 (wird in ISR erhöht)
uint32_t lastPulseCount1 = 0;                  // Letzter Zählerstand Sensor 1
uint32_t lastPulseCount2 = 0;                  // Letzter Zählerstand Sensor 2
float flowRate1 = 0.0;                         // Momentaner Durchfluss Sensor 1 (L/min)
float flowRate2 = 0.0;                         // Momentaner Durchfluss Sensor 2 (L/min)
float cumulativeFlow1 = 0.0;                   // Kumulativer Durchfluss Sensor 1 (L)
float cumulativeFlow2 = 0.0;                   // Kumulativer Durchfluss Sensor 2 (L)

/* ----- Logging Konfiguration ----- */
unsigned long startRecordingMillis = 0; 
bool recording = false;                        // Datenlogging: Ein (true) / Aus (false)
String logFileName;                            // Dateiname für Logdaten im SPIFFS
String getFileTimestamp() {
  time_t now = time(nullptr);
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);
  char buf[30];
  // Format z.B. TT-MM-YYYY_hh-mm
  sprintf(buf, "%02d-%02d-%04d_%02d-%02d",
          timeinfo.tm_mday, timeinfo.tm_mon + 1, timeinfo.tm_year + 1900,
          timeinfo.tm_hour, timeinfo.tm_min);
  return String(buf);
}

/* ----- Zeitsteuerung ----- */
unsigned long previousMillis = 0;              // Hilfsvariable für Zeitmessung in der Loop
const unsigned long interval = 1000;           // Messintervall (1 Sekunde)

/* ----- Webserver Konfiguration ----- */
WebServer server(80);                          // Webserver, der auf Port 80 lauscht

/* ====================================================
 * 3. Funktionsprototypen (Vorwärtsdeklarationen)
 * ==================================================== */

// Sensor- und Logging-Funktionen
float readPressureSensor(uint8_t channel);     // Liest den Drucksensor an einem Kanal und berechnet den Druck in bar
void logData();                                // Schreibt Messdaten als CSV-Zeile in SPIFFS
String getTimeString();                        // Gibt den aktuellen Zeitstempel als String zurück

// Interrupt-Service-Routinen für Durchflusssensoren
void IRAM_ATTR flowSensor1ISR();               // ISR für Durchflusssensor 1 (FALLING-Edge)
void IRAM_ATTR flowSensor2ISR();               // ISR für Durchflusssensor 2 (FALLING-Edge)

// Funktionen zur Kalibrierung und EEPROM-Verwaltung
void loadCalibration();                        // Lädt Kalibrierungswerte aus dem EEPROM
void saveCalibration();                        // Speichert Kalibrierungswerte ins EEPROM

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
void handleUpdateCalibration();                // Aktualisiert die Kalibrierungswerte für einen Drucksensor
void handleCalibrateVmin();                    // Kalibriert den minimalen Spannungswert für einen Drucksensor
void handleCalibrateHtml();                    // Liefert die Kalibrierungsseite
void handleChartsHtml();                       // Liefert die Charts-Seite
void handleLast10Min();                        // Neu: Liefert Diagrammdaten der letzten 10 Minuten
void handleFileRead();                         // Liefert statische Dateien aus SPIFFS
void handleLoggingData();                      // Liefert die geloggten Daten als JSON
void handleResetCalibration();                 // Setzt die Kalibrierungswerte zurück
void handleGetCalibration();                   // Liefert die Kalibrierungswerte als JSON
void handleResetCalibration();                 // Setzt die Kalibrierungswerte zurück
/* ====================================================
 * 4. Setup – Initialisierung aller Module
 * ==================================================== */
void setup() {
  // Serielle Kommunikation initialisieren (für Debug-Ausgaben)
  Serial.begin(115200);
  delay(1000);

  // ----- EEPROM initialisieren -----
  // EEPROM-Größe: 4 Sensoren * 4 Float-Werte = 16 Floats
  EEPROM.begin(32 * sizeof(float));
  loadCalibration();

  // ----- I²C initialisieren -----
  Wire.begin(I2C_SDA, I2C_SCL);

  // ----- ADS1115 initialisieren -----
  Serial.println("Suche ADS1115...");
  while (!ads.begin(0x48)) { // 0x48 ist die Standardadresse
    Serial.println("ADS1115 nicht gefunden, versuche erneut in 1 Sekunde...");
    delay(1000);
  }
  Serial.println("ADS1115 erkannt!");
  ads.setGain(GAIN_ONE);

  // ----- SPIFFS initialisieren -----
  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS Initialisierung fehlgeschlagen!");
  }

  // ----- WLAN im Access Point-Modus konfigurieren -----
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

  // API-Endpunkte
  server.on("/api/sensorwerte", HTTP_GET, handleSensorwerte);               // JSON mit aktuellen Sensorwerten
  server.on("/getTime", HTTP_GET, handleGetTime);                           // Zeitstempel als Text
  server.on("/setTime", HTTP_GET, handleSetTime);                           // Zeit setzen (Parameter: t)
  server.on("/downloadlog", HTTP_GET, handleDownloadLog);                   // Logdatei herunterladen
  server.on("/toggleRecording", HTTP_GET, handleToggleRecording);           // Logging starten/stoppen
  server.on("/deleteLog", HTTP_GET, handleDeleteLog);                       // Logdatei löschen
  server.on("/clearCumulativeFlow", HTTP_GET, handleClearCumulativeFlow);   // Kumulativen Durchfluss zurücksetzen
  server.on("/updateCalibration", HTTP_POST, handleUpdateCalibration);       // Kalibrierungswerte aktualisieren
  server.on("/calibrateVmin", HTTP_GET, handleCalibrateVmin);               // Minimalen Spannungswert kalibrieren
  server.on("/calibrate.html", HTTP_GET, handleCalibrateHtml);              // Kalibrierungsseite
  server.on("/charts.html", HTTP_GET, handleChartsHtml);                    // Charts-Seite
  server.on("/api/last10min", HTTP_GET, handleLast10Min);                   // Neu: Endpunkt für Diagrammdaten der letzten 10 Minuten
  server.on("/api/loggingData", HTTP_GET, handleLoggingData);               // Neu: Endpunkt für geloggte Daten
  server.on("/resetCalibration", HTTP_GET, handleResetCalibration);         // Neu: Endpunkt zum Zurücksetzen der Kalibrierung
  server.on("/api/calibration", HTTP_GET, handleGetCalibration);            // Neu: Endpunkt für Kalibrierungswerte
  server.onNotFound(handleFileRead);


  server.begin();

  // ----- Durchflusssensor-Pins konfigurieren -----
  pinMode(FLOW_SENSOR1_PIN, INPUT_PULLUP);
  pinMode(FLOW_SENSOR2_PIN, INPUT_PULLUP);

  // ----- Interrupts binden -----
  attachInterrupt(digitalPinToInterrupt(FLOW_SENSOR1_PIN), flowSensor1ISR, FALLING);
  attachInterrupt(digitalPinToInterrupt(FLOW_SENSOR2_PIN), flowSensor2ISR, FALLING);

  // ----- Zeitsystem initialisieren -----
  configTime(0, 0, "pool.ntp.org");
}

/* ====================================================
 * 5. Loop – Hauptprogrammzyklus
 * ==================================================== */
void loop() {
  server.handleClient();

  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;

    // ----- a) Drucksensoren auslesen -----
    float pressures[4];
    for (uint8_t i = 0; i < 4; i++) {
      pressures[i] = readPressureSensor(i);
    }

    // ----- b) Durchfluss auswerten -----
    noInterrupts();
    uint32_t currentPulse1 = pulseCount1;
    uint32_t currentPulse2 = pulseCount2;
    interrupts();

    uint32_t delta1 = currentPulse1 - lastPulseCount1;
    uint32_t delta2 = currentPulse2 - lastPulseCount2;
    lastPulseCount1 = currentPulse1;
    lastPulseCount2 = currentPulse2;

    flowRate1 = delta1 / 11.0;
    flowRate2 = delta2 / 11.0;
    cumulativeFlow1 += flowRate1 / 60.0;    //  11.0 für YF-B2; 98.0 für yf-s401; 7.5 für yf-s201;
    cumulativeFlow2 += flowRate2 / 60.0;

    // Debug-Ausgabe (optional)
    Serial.print("Puls1: ");
    Serial.print(delta1);
    Serial.print(" / Flow1: ");
    Serial.print(flowRate1, 2);
    Serial.print(" L/min, Puls2: ");
    Serial.print(delta2);
    Serial.print(" / Flow2: ");
    Serial.print(flowRate2, 2);
    Serial.println(" L/min");

    // --- 1) 10-Minuten-Puffer (dataBuffer) immer befüllen ---
    time_t currentTime = time(nullptr);
    dataBuffer[bufferIndex].timestamp = currentTime;
    for (uint8_t i = 0; i < 4; i++) {
      dataBuffer[bufferIndex].pressure[i] = pressures[i];
    }
    dataBuffer[bufferIndex].flowRate1 = flowRate1;
    dataBuffer[bufferIndex].flowRate2 = flowRate2;
    bufferIndex = (bufferIndex + 1) % BUFFER_SIZE;

    // --- 2) Wenn recording => loggingBuffer + logData() ---
    if (recording) {
      // Logging-Puffer befüllen (loggingIndex hochzählen)
      loggingBuffer[loggingIndex].timestamp = currentTime;
      for (uint8_t i = 0; i < 4; i++) {
        loggingBuffer[loggingIndex].pressure[i] = pressures[i];
      }
      loggingBuffer[loggingIndex].flow1 = flowRate1;
      loggingBuffer[loggingIndex].flow2 = flowRate2;

      loggingIndex++;
      if (loggingIndex >= LOGGING_BUFFER_SIZE) {
        loggingIndex = 0; // optionaler Ringpuffer
      }

      // In die CSV-Datei schreiben
      logData();
    }
  } // Ende if (currentMillis - previousMillis >= interval)
} // Ende loop()


/* ====================================================
 * 6. Interrupt Service Routinen (ISRs)
 * ==================================================== */
void IRAM_ATTR flowSensor1ISR() {
  pulseCount1++;
}

void IRAM_ATTR flowSensor2ISR() {
  pulseCount2++;
}

/* ====================================================
 * 7. Funktionen zur Drucksensor-Abfrage und Datenlogging
 * ==================================================== */
float readPressureSensor(uint8_t channel) {
  int16_t rawValue = ads.readADC_SingleEnded(channel);
  float voltage = rawValue * ADS_VOLTAGE_PER_BIT;

  Serial.print("Kanal ");
  Serial.print(channel);
  Serial.print(" Rohwert: ");
  Serial.print(rawValue);
  Serial.print("  Spannung: ");
  Serial.print(voltage, 3);
  Serial.print(" V   ");

  // Berechnung des Umrechnungsfaktors:
  float denominator = pressureSensor_V_max[channel] - pressureSensor_V_min[channel];
  float convFactor = (denominator != 0) ? (pressureSensor_PSI_max[channel] - pressureSensor_PSI_min[channel]) / denominator : 0;
  
  // Lineare Umrechnung: (gemessene Spannung - V_min) * convFactor + PSI_min
  float pressurePSI = (voltage - pressureSensor_V_min[channel]) * convFactor + pressureSensor_PSI_min[channel];
  if (pressurePSI < 0) pressurePSI = 0;
  float pressureBar = pressurePSI / 14.5038;

  Serial.print("  Druck: ");
  Serial.print(pressureBar, 3);
  Serial.println(" bar");

  return pressureBar;
}

void logData() {
  File file = SPIFFS.open(logFileName, FILE_APPEND);
  if (!file) {
    Serial.println("Fehler beim Öffnen der Logdatei zum Anhängen");
    return;
  }

  // Laufzeit in Sekunden seit startRecordingMillis
  unsigned long sekunden = (millis() - startRecordingMillis) / 1000;

  // Zeitstempel
  String timeStr = getTimeString(); 
  // Drucksensoren abfragen
  float pressures[4];
  for (uint8_t i = 0; i < 4; i++) {
    pressures[i] = readPressureSensor(i);
  }

  // Zeile aufbauen – mit Semikolons
  // Zeit;laufzeit;dr1;dr2;dr3;dr4;flow1;flow2;cumFlow1;cumFlow2
  String line;
  line += timeStr + ";";              // z. B. "2023-09-19 15:02:12;"
  line += String(sekunden) + ";";     // "12;" (Beispiel: 12 s Laufzeit)

  // Drucksensoren in bar => Komma
  for (uint8_t i = 0; i < 4; i++) {
    line += toGermanFloatString(pressures[i], 3) + ";";
  }
  // Flowraten => Komma
  line += toGermanFloatString(flowRate1, 2) + ";";
  line += toGermanFloatString(flowRate2, 2) + ";";

  // Kumulierte Flows => Komma
  line += toGermanFloatString(cumulativeFlow1, 2) + ";";
  line += toGermanFloatString(cumulativeFlow2, 2) + "\n";

  file.print(line);
  file.close();
}




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
void handleRoot() {
  if (SPIFFS.exists("/index.html")) {
    File file = SPIFFS.open("/index.html", FILE_READ);
    server.streamFile(file, "text/html");
    file.close();
  } else {
    server.send(404, "text/plain", "Datei /index.html nicht gefunden");
  }
}

void handleCSS() {
  if (SPIFFS.exists("/style.css")) {
    File file = SPIFFS.open("/style.css", FILE_READ);
    server.streamFile(file, "text/css");
    file.close();
  } else {
    server.send(404, "text/plain", "Datei /style.css nicht gefunden");
  }
}

void handleJS() {
  if (SPIFFS.exists("/script.js")) {
    File file = SPIFFS.open("/script.js", FILE_READ);
    server.streamFile(file, "application/javascript");
    file.close();
  } else {
    server.send(404, "text/plain", "Datei /script.js nicht gefunden");
  }
}

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

void handleGetTime() {
  server.send(200, "text/plain", getTimeString());
}

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

void handleDownloadLog() {
  if (SPIFFS.exists(logFileName)) {
    File file = SPIFFS.open(logFileName, FILE_READ);
    server.streamFile(file, "text/csv");
    file.close();
  } else {
    server.send(404, "text/plain", "Logdatei nicht gefunden");
  }
}

void handleToggleRecording() {
  recording = !recording;
  if (recording) {
     // Setze den Logging-Puffer-Index zurück, damit alte Daten überschrieben werden.
     loggingIndex = 0;

    // Laufzeit-Startzeit merken
    startRecordingMillis = millis();
    
    // Neuen Dateinamen anlegen, Header schreiben
    String filePrefix = getFileTimestamp();
    logFileName = "/" + filePrefix + "_Rohdaten.csv";
    File file = SPIFFS.open(logFileName, FILE_WRITE);
    if (file) {
      // Angepasster Header mit Semikolon
      //   Zeitstempel;Laufzeit (s);Druck1 (bar);Druck2 (bar);Druck3 (bar);Druck4 (bar);FlowRate1 (L/min);FlowRate2 (L/min);kUmFlow1 (L);kUmFlow2 (L)
      String header = "Zeitstempel;Laufzeit (s);Pressure1 (bar);Pressure2 (bar);Pressure3 (bar);Pressure4 (bar);"
                      "FlowRate1 (L/min);FlowRate2 (L/min);CumulativeFlow1 (L);CumulativeFlow2 (L)\n";
      file.print(header);
      file.close();
    } else {
      Serial.println("Fehler beim Erstellen der Logdatei");
    }
  }
  server.send(200, "text/plain", recording ? "Recording gestartet" : "Recording gestoppt");
}



void handleDeleteLog() {
  if (SPIFFS.exists(logFileName)) {
    SPIFFS.remove(logFileName);
    server.send(200, "text/plain", "Logdatei gelöscht");
  } else {
    server.send(404, "text/plain", "Logdatei nicht gefunden");
  }  
}

void handleClearCumulativeFlow() {
  cumulativeFlow1 = 0;
  cumulativeFlow2 = 0;
  server.send(200, "text/plain", "Kumulativer Durchfluss zurückgesetzt");
}

// Neuer Endpoint zur Aktualisierung der Kalibrierungswerte für einen Drucksensor (manuelle Einstellung)
void handleUpdateCalibration() {
  if (server.hasArg("plain")) {
    DynamicJsonDocument doc(256);
    deserializeJson(doc, server.arg("plain"));

    int sensorIndex = doc["sensor"];
    if(sensorIndex >=0 && sensorIndex <4) {
      // Temporäre V-Werte
      pressureSensor_V_min[sensorIndex] = doc["v_min"];
      pressureSensor_V_max[sensorIndex] = doc["v_max"];
      
      // Permanente PSI-Werte
      pressureSensor_PSI_min[sensorIndex] = doc["psi_min"];
      pressureSensor_PSI_max[sensorIndex] = doc["psi_max"];
      saveCalibration();

      server.send(200, "application/json", 
        "{\"status\":\"success\",\"sensor\":"+String(sensorIndex)+"}");
    } else {
      server.send(400, "text/plain", "Ungültiger Sensorindex");
    }
  } else {
    server.send(400, "text/plain", "Keine Daten empfangen");
  }
}

// Fügt eine Seite hinzu, auf der die Kalibrierung in einem separaten Layout erfolgt.
void handleCalibrateHtml() {
  if (SPIFFS.exists("/calibrate.html")) {
    File file = SPIFFS.open("/calibrate.html", FILE_READ);
    server.streamFile(file, "text/html");
    file.close();
  } else {
    server.send(404, "text/plain", "Datei /calibrate.html nicht gefunden");
  }
}

// Liefert die Seite, auf der wissenschaftliche Diagramme angezeigt werden.
void handleChartsHtml() {
  if (SPIFFS.exists("/charts.html")) {
    File file = SPIFFS.open("/charts.html", FILE_READ);
    server.streamFile(file, "text/html");
    file.close();
  } else {
    server.send(404, "text/plain", "Datei /charts.html nicht gefunden");
  }
}
// Diese Funktion liest die Logdatei, filtert alle Zeilen, die innerhalb der letzten 10 Minuten liegen,
// und erstellt ein JSON-Objekt mit Arrays für Zeitstempel, Druck und Durchfluss.
// --- Neu/Geändert: API-Endpunkt, der alle Messwerte der letzten 10 Minuten aus dem in-memory Puffer liefert ---
void handleLast10Min() {
  time_t now = time(nullptr);
  time_t tenMinutesAgo = now - 600;  // 10 Minuten in Sekunden

  String timestampsArr = "";
  String pressure1Arr = "", pressure2Arr = "", pressure3Arr = "", pressure4Arr = "";
  String flow1Arr = "", flow2Arr = "";
  bool firstElement = true;

  // Iteriere über den Puffer in chronologischer Reihenfolge
  for (int i = 0; i < BUFFER_SIZE; i++) {
    // Berechne den effektiven Index im Ringpuffer
    int idx = (bufferIndex + i) % BUFFER_SIZE;
    
    // Überspringe nicht initialisierte Einträge
    if (dataBuffer[idx].timestamp == 0) continue;

    // Prüfe Zeitfenster
    if (dataBuffer[idx].timestamp >= tenMinutesAgo && dataBuffer[idx].timestamp <= now) {
      // Trennzeichen für CSV
      if (!firstElement) {
        timestampsArr += ",";
        pressure1Arr += ","; pressure2Arr += ","; pressure3Arr += ","; pressure4Arr += ",";
        flow1Arr += ","; flow2Arr += ",";
      } else {
        firstElement = false;
      }

      // Zeitstempel formatieren
      struct tm t;
      localtime_r(&(dataBuffer[idx].timestamp), &t);
      char buf[30];
      sprintf(buf, "%04d-%02d-%02d %02d:%02d:%02d",
              t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
              t.tm_hour, t.tm_min, t.tm_sec);
      
      // Werte anhängen
      timestampsArr += "\"" + String(buf) + "\"";
      pressure1Arr += String(dataBuffer[idx].pressure[0], 3);
      pressure2Arr += String(dataBuffer[idx].pressure[1], 3);
      pressure3Arr += String(dataBuffer[idx].pressure[2], 3);
      pressure4Arr += String(dataBuffer[idx].pressure[3], 3);
      flow1Arr += String(dataBuffer[idx].flowRate1, 2);
      flow2Arr += String(dataBuffer[idx].flowRate2, 2);
    }
  }

  // JSON zusammenbauen
  String json = "{";
  json += "\"timestamps\":[" + timestampsArr + "],";
  json += "\"pressure\":{";
  json += "\"sensor1\":[" + pressure1Arr + "],";
  json += "\"sensor2\":[" + pressure2Arr + "],";
  json += "\"sensor3\":[" + pressure3Arr + "],";
  json += "\"sensor4\":[" + pressure4Arr + "]";
  json += "},";
  json += "\"flow\":{";
  json += "\"sensor1\":[" + flow1Arr + "],";
  json += "\"sensor2\":[" + flow2Arr + "]";
  json += "}}";

  server.send(200, "application/json", json);
}
void handleLoggingData() {
  // Wir gehen davon aus, dass Einträge 0..(loggingIndex-1) gültig sind
  // (Falls du einen fortlaufenden Ring willst, passt du es an.)

  String timestampsArr;
  String pressure1Arr, pressure2Arr, pressure3Arr, pressure4Arr;
  String flow1Arr, flow2Arr;
  bool firstElement = true;

  for (int i = 0; i < loggingIndex; i++) {
    if (!firstElement) {
      timestampsArr += ",";
      pressure1Arr  += ",";
      pressure2Arr  += ",";
      pressure3Arr  += ",";
      pressure4Arr  += ",";
      flow1Arr      += ",";
      flow2Arr      += ",";
    } else {
      firstElement = false;
    }

    // Zeitstempel
    time_t t = loggingBuffer[i].timestamp;
    struct tm tmStruct;
    localtime_r(&t, &tmStruct);
    char buf[30];
    sprintf(buf, "%04d-%02d-%02d %02d:%02d:%02d",
            tmStruct.tm_year+1900, tmStruct.tm_mon+1, tmStruct.tm_mday,
            tmStruct.tm_hour, tmStruct.tm_min, tmStruct.tm_sec);
    timestampsArr += "\"" + String(buf) + "\"";

    // Druck
    pressure1Arr += String(loggingBuffer[i].pressure[0], 3);
    pressure2Arr += String(loggingBuffer[i].pressure[1], 3);
    pressure3Arr += String(loggingBuffer[i].pressure[2], 3);
    pressure4Arr += String(loggingBuffer[i].pressure[3], 3);

    // Flow
    flow1Arr += String(loggingBuffer[i].flow1, 2);
    flow2Arr += String(loggingBuffer[i].flow2, 2);
  }

  // JSON
  String json = "{";
  json += "\"timestamps\":[" + timestampsArr + "],";
  json += "\"pressure\":{";
  json += "\"sensor1\":[" + pressure1Arr + "],";
  json += "\"sensor2\":[" + pressure2Arr + "],";
  json += "\"sensor3\":[" + pressure3Arr + "],";
  json += "\"sensor4\":[" + pressure4Arr + "]";
  json += "},";
  json += "\"flow\":{";
  json += "\"sensor1\":[" + flow1Arr + "],";
  json += "\"sensor2\":[" + flow2Arr + "]";
  json += "}";
  json += "}";

  server.send(200, "application/json", json);
}

void handleFileRead() {
  String path = server.uri(); // z.B. "/chart.umd.min.js"
  if (SPIFFS.exists(path)) {
    File file = SPIFFS.open(path, FILE_READ);
    
    // MIME-Type bestimmen
    String contentType = "text/plain";
    if (path.endsWith(".html"))       contentType = "text/html";
    else if (path.endsWith(".css"))   contentType = "text/css";
    else if (path.endsWith(".js"))    contentType = "application/javascript";
    else if (path.endsWith(".json"))  contentType = "application/json";
    else if (path.endsWith(".png"))   contentType = "image/png";
    // usw.

    server.streamFile(file, contentType);
    file.close();
  } else {
    server.send(404, "text/plain", "Datei nicht gefunden");
  }
}

// Kalibrierungsdaten abfragen
void handleGetCalibration() {
  String json = "[";
  for (uint8_t i = 0; i < 4; i++) {
    json += "{";
    json += "\"v_min\":" + String(pressureSensor_V_min[i], 2) + ",";
    json += "\"v_max\":" + String(pressureSensor_V_max[i], 2) + ",";
    json += "\"psi_min\":" + String(pressureSensor_PSI_min[i], 1) + ",";
    json += "\"psi_max\":" + String(pressureSensor_PSI_max[i], 1);
    json += "}";
    if (i < 3) json += ",";
  }
  json += "]";
  server.send(200, "application/json", json);
}

// Kalibrierung zurücksetzen (aktualisierte Version)
void handleResetCalibration() {
  for (uint8_t i = 0; i < 4; i++) {
    pressureSensor_PSI_min[i] = 0.0;
    pressureSensor_PSI_max[i] = 10.0;
  }
  saveCalibration();
  server.send(200, "text/plain", "PSI-Werte zurückgesetzt");
}

/* ====================================================
 * 9. Funktionen zur Kalibrierung und EEPROM-Verwaltung
 * ==================================================== */
// Misst über 5 Sekunden den Mittelwert der Spannung des angegebenen Sensors,
// speichert diesen in pressureSensor_V_min_cal, aktualisiert pressureSensor_V_min
// und speichert die Kalibrierwerte im EEPROM.
float calibrateSensorVmin(uint8_t sensorIndex) {
  const unsigned long calibrationDuration = 5000; // 5 Sekunden Messung
  const int sampleRate = 100; // 100 ms zwischen Messungen
  const int maxSamples = calibrationDuration / sampleRate;
  float samples[maxSamples];

  unsigned long startTime = millis();
  int sampleCount = 0;

  // Messung durchführen
  while (millis() - startTime < calibrationDuration && sampleCount < maxSamples) {
      int16_t rawValue = ads.readADC_SingleEnded(sensorIndex);
      samples[sampleCount] = rawValue * ADS_VOLTAGE_PER_BIT;
      sampleCount++;
      delay(sampleRate);
  }

  // Mindestens 3 Samples benötigt
  if (sampleCount < 3) {
      Serial.println("Fehler: Zu wenige Messwerte");
      return NAN;
  }

  // Sortiere Samples für Median-Berechnung
  std::sort(samples, samples + sampleCount);
  
  // Berechne Median
  float median = samples[sampleCount / 2];
  
  // Aktualisiere nur den temporären V_min-Wert (kein EEPROM-Schreibvorgang!)
  pressureSensor_V_min[sensorIndex] = median;

  Serial.printf("Sensor %d: Neuer V_min = %.3f V (temporär, gilt bis zum Neustart)\n", 
               sensorIndex + 1, median);

  return median;
}

// Neuer Handler für den GET-Endpoint /calibrateVmin?sensor=X
void handleCalibrateVmin() {
  if (server.hasArg("sensor")) {
      int sensorIndex = server.arg("sensor").toInt();
      if (sensorIndex >= 0 && sensorIndex < 4) {
          float newVmin = calibrateSensorVmin(sensorIndex);
          if (!isnan(newVmin)) {
              String jsonResponse = "{\"status\":\"success\",\"v_min\":" + String(newVmin, 3) + "}";
              server.send(200, "application/json", jsonResponse);
          } else {
              server.send(500, "application/json", "{\"status\":\"error\"}");
          }
          return;
      }
  }
  server.send(400, "application/json", "{\"status\":\"invalid_sensor\"}");
}

// Speichert für jeden Sensor vier Float-Werte: V_min, V_max, PSI_min, PSI_max
void saveCalibration() {
  for (uint8_t i = 0; i < 4; i++) {
    int offset = i * 4 * sizeof(float);
    
    // Speichere nur PSI-Werte
    EEPROM.put(offset + 2 * sizeof(float), pressureSensor_PSI_min[i]);
    EEPROM.put(offset + 3 * sizeof(float), pressureSensor_PSI_max[i]);
  }
  EEPROM.commit();
}

// Lädt für jeden Sensor die vier Float-Werte und validiert sie ggf.
void loadCalibration() {
  for (uint8_t i = 0; i < 4; i++) {
    int offset = i * 4 * sizeof(float);
    
    // Lade nur PSI-Werte aus EEPROM
    EEPROM.get(offset + 2 * sizeof(float), pressureSensor_PSI_min[i]);
    EEPROM.get(offset + 3 * sizeof(float), pressureSensor_PSI_max[i]);
    
    // Setze V-Werte immer auf Standard
    pressureSensor_V_min[i] = 0.5;
    pressureSensor_V_max[i] = 4.5;

    // Validierung
    if (isnan(pressureSensor_PSI_min[i])) pressureSensor_PSI_min[i] = 0.0;
    if (isnan(pressureSensor_PSI_max[i])) pressureSensor_PSI_max[i] = 10.0;
  }
}
