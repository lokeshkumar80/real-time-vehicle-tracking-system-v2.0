// ============================================================
//  VEHICLE TRACKER — WiFi TEST MODE
//  Use this to test without GSM hardware.
//  ESP32 connects to WiFi, publishes MQTT directly.
//  Switch to main.ino for final GSM deployment.
// ============================================================

#include <WiFi.h>
#include <PubSubClient.h>
#include <TinyGPS++.h>
#include <ArduinoJson.h>

// ─── CONFIG ────────────────────────────────
const char* WIFI_SSID    = "YOUR_WIFI_NAME";
const char* WIFI_PASS    = "YOUR_WIFI_PASSWORD";
const char* MQTT_BROKER  = "broker.hivemq.com";
const int   MQTT_PORT    = 1883;
const char* MQTT_TOPIC   = "vehicletracker/lokesh/location";
const char* DEVICE_ID    = "TRACKER_001";

#define GPS_RX_PIN  16
#define GPS_TX_PIN  17
#define LED_PIN      2

// Geofence
const double GEO_LAT    = 32.7266;
const double GEO_LNG    = 74.8570;
const double GEO_RADIUS = 500.0;

// ─── OBJECTS ───────────────────────────────
TinyGPSPlus  gps;
HardwareSerial gpsSerial(2);   // UART2 on ESP32
WiFiClient   wifiClient;
PubSubClient mqtt(wifiClient);

unsigned long lastPublish = 0;

// ─── HELPERS ───────────────────────────────
double haversine(double lat1, double lng1, double lat2, double lng2) {
  const double R = 6371000.0;
  double dLat = radians(lat2-lat1), dLng = radians(lng2-lng1);
  double a = sin(dLat/2)*sin(dLat/2) + cos(radians(lat1))*cos(radians(lat2))*sin(dLng/2)*sin(dLng/2);
  return R * 2 * atan2(sqrt(a), sqrt(1-a));
}

void reconnectMQTT() {
  while (!mqtt.connected()) {
    Serial.print("[MQTT] Connecting...");
    if (mqtt.connect(DEVICE_ID)) {
      Serial.println(" Connected!");
    } else {
      Serial.printf(" Failed (rc=%d), retrying in 3s\n", mqtt.state());
      delay(3000);
    }
  }
}

// ─── SETUP ─────────────────────────────────
void setup() {
  pinMode(LED_PIN, OUTPUT);
  Serial.begin(115200);
  gpsSerial.begin(9600, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);

  // WiFi
  Serial.printf("\n[WiFi] Connecting to %s...", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.println("\n[WiFi] Connected! IP: " + WiFi.localIP().toString());

  // MQTT
  mqtt.setServer(MQTT_BROKER, MQTT_PORT);
  reconnectMQTT();

  Serial.println("[READY] Tracking started.\n");
}

// ─── LOOP ──────────────────────────────────
void loop() {
  if (!mqtt.connected()) reconnectMQTT();
  mqtt.loop();

  while (gpsSerial.available()) gps.encode(gpsSerial.read());

  if (millis() - lastPublish >= 3000) {
    lastPublish = millis();

    if (gps.location.isValid()) {
      double lat  = gps.location.lat();
      double lng  = gps.location.lng();
      double spd  = gps.speed.isValid() ? gps.speed.knots() * 1.852 : 0;
      int    sats = gps.satellites.isValid() ? gps.satellites.value() : 0;
      double dist = haversine(lat, lng, GEO_LAT, GEO_LNG);

      StaticJsonDocument<256> doc;
      doc["device"]     = DEVICE_ID;
      doc["lat"]        = lat;
      doc["lng"]        = lng;
      doc["speed_kmh"]  = spd;
      doc["satellites"] = sats;
      doc["geofence_ok"]  = (dist <= GEO_RADIUS);
      doc["distance_m"]   = (int)dist;

      char buf[256];
      serializeJson(doc, buf);
      mqtt.publish(MQTT_TOPIC, buf);

      Serial.printf("[PUB] %.6f, %.6f | %.1f km/h | %s\n",
        lat, lng, spd, dist > GEO_RADIUS ? "OUTSIDE GEOFENCE!" : "Inside geofence");

      digitalWrite(LED_PIN, !digitalRead(LED_PIN)); // Toggle LED

    } else {
      Serial.println("[GPS] Waiting for fix...");
    }
  }
}
