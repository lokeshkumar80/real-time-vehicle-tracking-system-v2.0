// ============================================================
//  REAL-TIME VEHICLE TRACKING SYSTEM  —  Updated 2025
//  Hardware : ESP32 DevKit V1 + NEO-6M GPS + SIM7600 4G
//  Protocol : MQTT over GPRS  (replace WiFi credentials to
//             switch to WiFi-only mode for indoor testing)
//  Author   : Lokesh Kumar  |  IIT Jammu
// ============================================================

#include <TinyGPS++.h>
#include <SoftwareSerial.h>
#include <ArduinoJson.h>       // Install: ArduinoJson by Benoit Blanchon
#include <PubSubClient.h>      // Install: PubSubClient by Nick O'Leary

// ─────────────────────────────────────────────
//  PIN DEFINITIONS
// ─────────────────────────────────────────────
#define GPS_RX_PIN   3    // ESP32 GPIO3  ← GPS TX
#define GPS_TX_PIN   1    // ESP32 GPIO1  → GPS RX
#define GSM_RX_PIN   22   // ESP32 GPIO22 ← SIM7600 TX
#define GSM_TX_PIN   23   // ESP32 GPIO23 → SIM7600 RX
#define GSM_PWR_PIN  4    // ESP32 GPIO4  → SIM7600 PWRKEY (optional)
#define LED_PIN      2    // Onboard LED for status

// ─────────────────────────────────────────────
//  USER CONFIGURATION  ← EDIT THESE
// ─────────────────────────────────────────────
const char* APN          = "jionet";          // Jio | "airtelgprs.com" for Airtel
const char* MQTT_BROKER  = "broker.hivemq.com"; // Free public broker
const int   MQTT_PORT    = 1883;
const char* MQTT_TOPIC   = "vehicletracker/lokesh/location"; // Unique topic
const char* DEVICE_ID    = "TRACKER_001";

// ThingSpeak (backup logging)
const char* THINGSPEAK_API_KEY = "YOUR_WRITE_API_KEY"; // Replace this

// Geofence centre (your home/college coordinates)
const double GEOFENCE_LAT    = 32.7266;   // IIT Jammu lat
const double GEOFENCE_LNG    = 74.8570;   // IIT Jammu lng
const double GEOFENCE_RADIUS = 500.0;     // metres — alert if outside this

// ─────────────────────────────────────────────
//  OBJECTS
// ─────────────────────────────────────────────
TinyGPSPlus   gps;
SoftwareSerial gpsSerial(GPS_RX_PIN, GPS_TX_PIN);
SoftwareSerial gsmSerial(GSM_RX_PIN, GSM_TX_PIN);

// ─────────────────────────────────────────────
//  STATE
// ─────────────────────────────────────────────
bool    gsmReady      = false;
bool    mqttConnected = false;
double  lastLat       = 0, lastLng = 0;
unsigned long lastSend = 0;
const unsigned long SEND_INTERVAL = 3000; // ms between publishes

// ─────────────────────────────────────────────
//  HELPERS
// ─────────────────────────────────────────────
void blinkLED(int times, int ms = 200) {
  for (int i = 0; i < times; i++) {
    digitalWrite(LED_PIN, HIGH); delay(ms);
    digitalWrite(LED_PIN, LOW);  delay(ms);
  }
}

void sendAT(const String& cmd, unsigned long timeout = 2000) {
  gsmSerial.println(cmd);
  unsigned long start = millis();
  while (millis() - start < timeout) {
    while (gsmSerial.available()) Serial.write(gsmSerial.read());
  }
}

bool waitForResponse(const String& expected, unsigned long timeout = 5000) {
  String response = "";
  unsigned long start = millis();
  while (millis() - start < timeout) {
    while (gsmSerial.available()) {
      char c = gsmSerial.read();
      response += c;
      Serial.write(c);
      if (response.indexOf(expected) != -1) return true;
    }
  }
  return false;
}

// Haversine distance in metres
double haversine(double lat1, double lng1, double lat2, double lng2) {
  const double R = 6371000.0;
  double dLat = radians(lat2 - lat1);
  double dLng = radians(lng2 - lng1);
  double a = sin(dLat/2)*sin(dLat/2) +
             cos(radians(lat1))*cos(radians(lat2))*sin(dLng/2)*sin(dLng/2);
  return R * 2 * atan2(sqrt(a), sqrt(1-a));
}

// ─────────────────────────────────────────────
//  GSM INIT
// ─────────────────────────────────────────────
void initGSM() {
  Serial.println("\n[GSM] Initializing SIM7600...");
  gsmSerial.begin(115200);
  delay(3000);

  sendAT("AT");            delay(1000);
  sendAT("AT+CPIN?");      delay(1000);   // Check SIM
  sendAT("AT+CREG?");      delay(2000);   // Network reg
  sendAT("AT+CGATT=1");    delay(2000);   // Attach GPRS

  // Set APN
  sendAT("AT+CGDCONT=1,\"IP\",\"" + String(APN) + "\"");
  delay(2000);
  sendAT("AT+CGACT=1,1");  delay(5000);   // Activate PDP context

  // Check IP
  sendAT("AT+CGPADDR=1");  delay(2000);

  gsmReady = true;
  Serial.println("[GSM] Ready!");
  blinkLED(3);
}

// ─────────────────────────────────────────────
//  MQTT OVER AT COMMANDS  (TCP raw)
// ─────────────────────────────────────────────
//  Note: For production use ESP32 WiFi + PubSubClient.
//  This AT-command MQTT works over SIM7600 TCPIP stack.
// ─────────────────────────────────────────────
bool openMQTTConnection() {
  Serial.println("[MQTT] Connecting to broker...");

  // Open TCP connection
  sendAT("AT+CIPSTART=\"TCP\",\"" + String(MQTT_BROKER) + "\",\"" + String(MQTT_PORT) + "\"", 10000);
  if (!waitForResponse("CONNECT OK", 10000)) {
    Serial.println("[MQTT] TCP connect failed");
    return false;
  }

  // Build MQTT CONNECT packet manually
  // Fixed header: 0x10 (CONNECT)
  String clientId = String(DEVICE_ID);
  int payloadLen  = 10 + 2 + clientId.length(); // protocol + clientId
  
  sendAT("AT+CIPSEND=" + String(payloadLen + 2));
  delay(1000);

  // Raw MQTT CONNECT bytes
  gsmSerial.write(0x10);                        // CONNECT type
  gsmSerial.write((uint8_t)payloadLen);         // Remaining length
  gsmSerial.write((uint8_t)0x00); gsmSerial.write((uint8_t)0x04); // Protocol name length
  gsmSerial.print("MQTT");                       // Protocol name
  gsmSerial.write((uint8_t)0x04);               // Protocol level (3.1.1)
  gsmSerial.write((uint8_t)0x02);               // Connect flags (clean session)
  gsmSerial.write((uint8_t)0x00); gsmSerial.write((uint8_t)0x3C); // Keep-alive 60s
  gsmSerial.write((uint8_t)0x00); gsmSerial.write((uint8_t)clientId.length());
  gsmSerial.print(clientId);
  gsmSerial.write((uint8_t)26);                 // Ctrl+Z to send

  delay(3000);
  mqttConnected = true;
  Serial.println("[MQTT] Connected!");
  return true;
}

void publishMQTT(double lat, double lng, double speed, int satellites) {
  // Build JSON payload
  StaticJsonDocument<256> doc;
  doc["device"]     = DEVICE_ID;
  doc["lat"]        = lat;
  doc["lng"]        = lng;
  doc["speed_kmh"]  = speed * 1.852;  // knots → km/h
  doc["satellites"] = satellites;
  doc["timestamp"]  = millis();

  // Check geofence
  double dist = haversine(lat, lng, GEOFENCE_LAT, GEOFENCE_LNG);
  doc["geofence_ok"] = (dist <= GEOFENCE_RADIUS);
  doc["distance_m"]  = (int)dist;

  String payload;
  serializeJson(doc, payload);

  int totalLen = 2 + strlen(MQTT_TOPIC) + 2 + payload.length();

  sendAT("AT+CIPSEND=" + String(totalLen + 2));
  delay(500);

  gsmSerial.write(0x30);                              // PUBLISH, QoS 0
  gsmSerial.write((uint8_t)totalLen);
  gsmSerial.write((uint8_t)0x00);
  gsmSerial.write((uint8_t)strlen(MQTT_TOPIC));
  gsmSerial.print(MQTT_TOPIC);
  gsmSerial.print(payload);
  gsmSerial.write((uint8_t)26);

  delay(1000);
  Serial.println("[MQTT] Published: " + payload);
}

// ─────────────────────────────────────────────
//  THINGSPEAK BACKUP LOG
// ─────────────────────────────────────────────
void logToThingSpeak(double lat, double lng) {
  sendAT("AT+CIPSTART=\"TCP\",\"api.thingspeak.com\",\"80\"", 8000);
  waitForResponse("CONNECT OK", 8000);

  sendAT("AT+CIPSEND");
  delay(2000);

  String req = "GET /update?api_key=" + String(THINGSPEAK_API_KEY)
             + "&field1=" + String(lat, 6)
             + "&field2=" + String(lng, 6)
             + " HTTP/1.0\r\nHost: api.thingspeak.com\r\n\r\n";

  gsmSerial.print(req);
  gsmSerial.write((uint8_t)26);
  delay(5000);

  sendAT("AT+CIPSHUT");
  Serial.println("[ThingSpeak] Logged backup.");
}

// ─────────────────────────────────────────────
//  SETUP
// ─────────────────────────────────────────────
void setup() {
  pinMode(LED_PIN, OUTPUT);
  Serial.begin(115200);
  delay(1000);

  Serial.println("========================================");
  Serial.println("  Vehicle Tracker v2.0 — Lokesh Kumar  ");
  Serial.println("  IIT Jammu  |  MQTT + Geofence Build  ");
  Serial.println("========================================\n");

  // Start GPS
  gpsSerial.begin(9600);
  Serial.println("[GPS] Serial started at 9600 baud");

  // Init GSM
  initGSM();

  // Connect MQTT
  if (gsmReady) openMQTTConnection();

  blinkLED(5, 100);
  Serial.println("[READY] Waiting for GPS fix...\n");
}

// ─────────────────────────────────────────────
//  LOOP
// ─────────────────────────────────────────────
void loop() {
  // Feed GPS data
  while (gpsSerial.available() > 0) {
    gps.encode(gpsSerial.read());
  }

  // Every SEND_INTERVAL ms, if GPS fix available
  if (millis() - lastSend >= SEND_INTERVAL) {
    lastSend = millis();

    if (gps.location.isValid() && gps.location.isUpdated()) {
      double lat  = gps.location.lat();
      double lng  = gps.location.lng();
      double spd  = gps.speed.isValid() ? gps.speed.knots() : 0.0;
      int    sats = gps.satellites.isValid() ? gps.satellites.value() : 0;

      Serial.printf("[GPS] Lat: %.6f  Lng: %.6f  Speed: %.1f km/h  Sats: %d\n",
                    lat, lng, spd * 1.852, sats);

      // LED heartbeat
      digitalWrite(LED_PIN, HIGH);

      // Check if location changed meaningfully (>5m)
      double moved = haversine(lat, lng, lastLat, lastLng);
      if (moved > 5.0 || lastLat == 0) {
        lastLat = lat;
        lastLng = lng;

        // 1. Publish via MQTT
        if (mqttConnected) publishMQTT(lat, lng, spd, sats);

        // 2. Backup log to ThingSpeak every 30s
        static unsigned long lastTS = 0;
        if (millis() - lastTS > 30000) {
          lastTS = millis();
          logToThingSpeak(lat, lng);
        }

        // 3. Geofence check — print alert
        double dist = haversine(lat, lng, GEOFENCE_LAT, GEOFENCE_LNG);
        if (dist > GEOFENCE_RADIUS) {
          Serial.printf("[GEOFENCE ALERT] %.0f m outside boundary!\n", dist);
          // TODO: sendSMSAlert() — add if needed
        }
      }

      digitalWrite(LED_PIN, LOW);

    } else {
      // No fix yet — show satellite count
      if (gps.satellites.isValid())
        Serial.printf("[GPS] Searching... Sats visible: %d\n", gps.satellites.value());
      else
        Serial.println("[GPS] No fix — check antenna connection");
    }
  }
}
