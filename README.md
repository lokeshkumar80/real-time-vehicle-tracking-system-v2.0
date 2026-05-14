# 🚗 Real-Time Vehicle Tracking System — v2.0

<div align="center">

![Version](https://img.shields.io/badge/version-2.0.0-blue?style=flat-square)
![Platform](https://img.shields.io/badge/platform-ESP32-orange?style=flat-square)
![Protocol](https://img.shields.io/badge/protocol-MQTT%20%2B%20WebSocket-teal?style=flat-square)
![License](https://img.shields.io/badge/license-MIT-green?style=flat-square)
![Node](https://img.shields.io/badge/node-%3E%3D18.0.0-brightgreen?style=flat-square)
![Status](https://img.shields.io/badge/status-active-success?style=flat-square)

**An end-to-end IoT vehicle tracking system using ESP32 + GPS + 4G LTE, publishing real-time location data over MQTT to a Node.js backend with live WebSocket dashboard and geofencing.**

[Live Demo](#-live-demo) · [Quick Start](#-quick-start) · [Architecture](#-architecture) · [Hardware Setup](#-hardware-setup) · [Configuration](#-configuration) · [API Reference](#-api-reference) · [Roadmap](#-roadmap)

</div>

---

## 📋 Table of Contents

- [Project Overview](#-project-overview)
- [What's New in v2.0](#-whats-new-in-v20)
- [Architecture](#-architecture)
- [Hardware Requirements](#-hardware-requirements)
- [Wiring Diagram](#-wiring-diagram)
- [Software Requirements](#-software-requirements)
- [Quick Start](#-quick-start)
  - [Step 1 — ThingSpeak Setup](#step-1--thingspeak-setup)
  - [Step 2 — Mapbox Token](#step-2--mapbox-token)
  - [Step 3 — Configure Firmware](#step-3--configure-firmware)
  - [Step 4 — Upload to ESP32](#step-4--upload-to-esp32)
  - [Step 5 — Run Backend Locally](#step-5--run-backend-locally)
  - [Step 6 — Deploy to Render.com](#step-6--deploy-to-rendercom)
- [WiFi Test Mode (No GSM Required)](#-wifi-test-mode-no-gsm-required)
- [Configuration Reference](#-configuration-reference)
- [API Reference](#-api-reference)
- [Project Structure](#-project-structure)
- [How It Works](#-how-it-works)
  - [GPS Data Flow](#gps-data-flow)
  - [MQTT Protocol](#mqtt-protocol)
  - [Geofencing Algorithm](#geofencing-algorithm)
  - [WebSocket Real-Time Update](#websocket-real-time-update)
- [Dashboard Features](#-dashboard-features)
- [Performance](#-performance)
- [Troubleshooting](#-troubleshooting)
- [Roadmap](#-roadmap)
- [Version History](#-version-history)
- [Author](#-author)
- [Acknowledgements](#-acknowledgements)
- [License](#-license)

---

## 📖 Project Overview

This project is a complete, working **real-time GPS vehicle tracking system** built on an ESP32 microcontroller. The device reads GPS coordinates from a NEO-6M module via UART, computes geofence status using the Haversine formula, and transmits a JSON telemetry payload over a 4G cellular network using the MQTT protocol. A Node.js backend subscribes to the MQTT broker and relays all updates to connected browser dashboards over WebSocket — achieving sub-500ms end-to-end latency from GPS fix to browser display.

### 🎓 Academic Context

| Field | Details |
|---|---|
| **Original project** | B.Tech ECE Final Year Project — Pusa Institute of Technology, New Delhi (2022) |
| **Original supervisor** | Prof. Rajneesh Kumar Srivastava, Dept. of Electronics & Communication Engineering |
| **Original collaborator** | Rohit Tiwari (joint work, equal contribution) |
| **v2.0 author** | Lokesh Kumar — M.Tech CSE (RA), IIT Jammu (individual rebuild, 2025) |
| **Contact** | 2024pcr0046@iitjammu.ac.in · +91-7535919305 |

> **Note on v1.0 vs v2.0:** The original project used SIM900A (2G), HTTP polling to ThingSpeak, and a PHP dashboard on 000webhost (now shut down). Version 2.0 is an independent rebuild with a completely modernised stack. Every file in this repository is new.

---

## ✨ What's New in v2.0

| Component | v1.0 (2022) | v2.0 (2025) |
|---|---|---|
| **GSM Module** | SIM900A (2G GPRS) | SIM7600E-H (4G LTE) |
| **Data Protocol** | HTTP GET to ThingSpeak | MQTT pub/sub (HiveMQ) |
| **Update Latency** | ~15–17 seconds | **< 500 ms** |
| **Browser Updates** | `setInterval` XHR polling | **WebSocket push** |
| **Geofencing** | ❌ None | ✅ Haversine formula, configurable radius |
| **Backend** | PHP on 000webhost (shut down) | **Node.js on Render.com** |
| **Map Trail** | ❌ Single marker only | ✅ Animated trail (200 points) |
| **Deployment** | Manual file upload | **Git push → auto-deploy** |
| **Test Mode** | Full hardware required | ✅ WiFi-only test mode |
| **JSON Payload** | lat + lng URL params | Structured JSON with speed, satellites, geofence |
| **Data Backup** | ThingSpeak (primary) | ThingSpeak (secondary, every 30s) |

---

## 🏗 Architecture

```
┌─────────────────────────────────────────────────────────────────────┐
│                         HARDWARE LAYER                               │
│                                                                       │
│  ┌──────────────┐  UART   ┌──────────────────┐  UART   ┌──────────┐ │
│  │  NEO-6M GPS  │ ──────► │  ESP32 DevKit V1 │ ──────► │ SIM7600  │ │
│  │  1Hz NMEA    │ 9600bd  │  240MHz / 520KB  │ 115200  │ 4G LTE   │ │
│  └──────────────┘         └──────────────────┘         └────┬─────┘ │
└──────────────────────────────────────────────────────────────│───────┘
                                                               │ LTE/GPRS
                                                               ▼
┌──────────────────────────────────────────────────────────────────────┐
│                         CLOUD LAYER                                   │
│                                                                       │
│  ┌──────────────────┐  MQTT PUBLISH    ┌─────────────────────────┐   │
│  │   ESP32 Device   │ ───────────────► │   HiveMQ Public Broker  │   │
│  │  (publisher)     │  QoS 0, TCP 1883 │   broker.hivemq.com     │   │
│  └──────────────────┘                  └────────────┬────────────┘   │
│                                                      │ MQTT SUBSCRIBE │
│  ┌────────────────────────────────────────────────── ▼ ──────────┐   │
│  │              Node.js Backend (Render.com)                      │   │
│  │                                                                │   │
│  │  mqtt.subscribe()  ──►  history[]  ──►  ws.send()             │   │
│  │  REST API: /api/location/latest                                │   │
│  │           /api/location/history                                │   │
│  │           /api/status                                          │   │
│  └────────────────────────────────────────────────────────────────┘   │
│                          │ WebSocket (ws://)                           │
└──────────────────────────│─────────────────────────────────────────── ┘
                           │
┌──────────────────────────▼─────────────────────────────────────────────┐
│                       BROWSER DASHBOARD                                  │
│                                                                          │
│   WebSocket onmessage  ──►  Mapbox GL JS  ──►  marker.setLngLat()       │
│                         ──►  Geofence banner (green/red)                 │
│                         ──►  Trail LineString update                     │
│                         ──►  Speed / Satellites / History panel          │
└──────────────────────────────────────────────────────────────────────────┘

                    ┌─────────────────────────┐
                    │  ThingSpeak (backup log) │  ◄── HTTP GET every 30s
                    │  30-day GPS history      │      from ESP32 directly
                    └─────────────────────────┘
```

### Data Flow Summary

```
GPS Satellite → NEO-6M (NMEA) → ESP32 UART → TinyGPS++ parse
→ Haversine distance check (skip if < 5m moved)
→ Build JSON payload {lat, lng, speed_kmh, satellites, geofence_ok, distance_m}
→ SIM7600 AT+CIPSEND → raw MQTT PUBLISH bytes → HiveMQ broker
→ Node.js subscriber receives → WebSocket broadcast to all browser clients
→ Mapbox GL JS updates marker + trail in real-time (< 500ms total)
```

---

## 🔧 Hardware Requirements

| Component | Model | Approx. Cost (India) | Where to Buy |
|---|---|---|---|
| Microcontroller | ESP32 DevKit V1 | ₹350 | Robu.in / Amazon |
| GPS Module | NEO-6M + patch antenna | ₹280 | Robu.in / Amazon |
| GSM Module | SIM7600E-H 4G LTE | ₹1,500 | Robu.in |
| Voltage Regulator | LM7805 | ₹20 | Local electronics shop |
| Voltage Regulator | LM7812 × 2 | ₹40 | Local electronics shop |
| Decoupling Capacitors | 0.1µF ceramic × 3 | ₹10 | Local electronics shop |
| Bulk Capacitor | 470µF electrolytic × 1 | ₹10 | Local electronics shop |
| Power Supply | 12V 2A DC adapter | ₹200 | Amazon |
| SIM Card | Jio / Airtel with data plan | — | Any store |
| Perf board | General purpose | ₹30 | Local electronics shop |
| **Total** | | **~₹2,500** | |

> **Indoor testing:** Skip SIM7600. Use [`wifi_test_mode.ino`](hardware/wifi_test_mode.ino) — only ESP32 + NEO-6M required (~₹630 total).

---

## 🔌 Wiring Diagram

```
                    ┌─────────────────────────────────┐
                    │         ESP32 DevKit V1          │
                    │                                  │
NEO-6M GPS          │  3.3V ◄──── VCC                 │
┌──────────┐        │  GND  ◄──── GND                 │
│ VCC ─────┼────────┤                                  │
│ GND ─────┼────────┤  GPIO3 (RX2) ◄─── TX            │
│ TX  ─────┼────────┤  GPIO1 (TX2) ───► RX            │
│ RX  ─────┼────────┤                                  │        SIM7600
└──────────┘        │  GPIO22 ◄──────────────────── TX ┼───── ┌──────────┐
                    │  GPIO23 ───────────────────► RX  ┼───── │ TX       │
                    │                                  │      │ RX       │
                    │  GPIO2  ───────────────────► LED │      │ GND ─────┼── Common GND
                    │  VIN    ◄──── 5V (LM7805 out)   │      │ VCC ─────┼── 12V (LM7812 out)
                    │  GND    ◄──── Common GND         │      └──────────┘
                    └─────────────────────────────────-┘

Power Supply Circuit:
                  ┌─────────────────────────────────────────────┐
  12–19V DC (+) ──┤── LM7805 IN → OUT (5V) ──────────────────► ESP32 VIN
                  │
                  ├─── LM7812 IN → OUT (12V) ─┬─────────────► SIM7600 VCC
                  │                            │
                  │                          470µF cap (to GND) [spike buffer]
                  │
  12–19V DC (−) ──┴──────────────────────────────────────────► Common GND
                  
  0.1µF capacitor on each IC between VCC and GND (as close to IC as possible)
```

### Pin Reference Table

| ESP32 GPIO | Direction | Connected To | Purpose |
|---|---|---|---|
| GPIO 3 (RX2) | INPUT | NEO-6M TX | GPS NMEA data stream at 9600 baud |
| GPIO 1 (TX2) | OUTPUT | NEO-6M RX | GPS config commands (rarely used) |
| GPIO 22 | INPUT | SIM7600 TX | GSM AT command responses |
| GPIO 23 | OUTPUT | SIM7600 RX | GSM AT commands |
| GPIO 2 | OUTPUT | Onboard LED | Heartbeat: blinks on GPS fix / MQTT publish |
| GPIO 4 | OUTPUT | SIM7600 PWRKEY | Optional hardware power-on control |
| 3.3V | POWER | NEO-6M VCC | GPS module power |
| VIN (5V) | POWER | LM7805 output | Board power input |
| GND | GROUND | All modules | Common ground reference |

> ⚠️ **Power Warning:** SIM7600 draws up to 2A burst during LTE TX. Always use 2× LM7812 in parallel with a 470µF bulk capacitor. A single regulator will under-voltage and reset the module.

---

## 💻 Software Requirements

### Arduino IDE (Firmware)

- [Arduino IDE 2.x](https://www.arduino.cc/en/software)
- ESP32 board support by Espressif Systems

**Required Libraries** (Tools → Manage Libraries):

| Library | Author | Version |
|---|---|---|
| TinyGPS++ | Mikal Hart | Latest |
| ArduinoJson | Benoit Blanchon | 6.x |
| PubSubClient | Nick O'Leary | 2.8+ |

### Backend (Node.js)

- Node.js >= 18.0.0
- npm packages: `express`, `mqtt`, `ws` (auto-installed via `npm install`)

### External Services (all free)

| Service | Purpose | Free Tier |
|---|---|---|
| [HiveMQ](https://www.hivemq.com/mqtt/public-mqtt-broker/) | MQTT broker | Unlimited (public) |
| [ThingSpeak](https://thingspeak.com) | GPS history backup | 1 update / 15s, 30-day retention |
| [Mapbox](https://mapbox.com) | Map rendering | 50,000 loads / month |
| [Render.com](https://render.com) | Backend hosting | 750 hours / month free |

---

## 🚀 Quick Start

### Step 1 — ThingSpeak Setup

1. Create account at [thingspeak.com](https://thingspeak.com)
2. **Channels → New Channel** → set these fields:
   - Field 1: `latitude`
   - Field 2: `longitude`
3. **API Keys tab** → copy your **Write API Key**
4. Note your **Channel ID** from the URL: `/channels/XXXXXXXX/`

### Step 2 — Mapbox Token

1. Create account at [mapbox.com](https://mapbox.com)
2. **Account → Tokens → Create a token** (default public scopes are fine)
3. Copy the token — it starts with `pk.eyJ1...`
4. Paste it in `webapp/index.html`:
   ```javascript
   const MAPBOX_TOKEN = 'pk.eyJ1...your_token_here';
   ```

### Step 3 — Configure Firmware

Open `hardware/main.ino` and edit the configuration block at the top:

```cpp
// ─── EDIT THESE LINES ─────────────────────────────────────
const char* APN              = "jionet";          // Jio SIM
//                           = "airtelgprs.com"   // Airtel SIM
//                           = "bsnlnet"          // BSNL SIM

const char* MQTT_TOPIC       = "vehicletracker/YOUR_NAME/location"; // make unique!
const char* DEVICE_ID        = "TRACKER_001";

const char* THINGSPEAK_API_KEY = "PASTE_YOUR_WRITE_KEY_HERE";

// Geofence centre — paste your lat/lng here
// Find yours: maps.google.com → right-click → "What's here?"
const double GEOFENCE_LAT    = 32.7266;    // e.g. IIT Jammu
const double GEOFENCE_LNG    = 74.8570;
const double GEOFENCE_RADIUS = 500.0;      // metres
```

### Step 4 — Upload to ESP32

1. Connect ESP32 via USB
2. **Tools → Board → ESP32 Dev Module**
3. **Tools → Port → COM[X]** (select your port)
4. Click **Upload** (→)
5. Open **Serial Monitor** at `115200` baud
6. You should see:
   ```
   ========================================
     Vehicle Tracker v2.0 — Lokesh Kumar
     IIT Jammu  |  MQTT + Geofence Build
   ========================================

   [GSM] Initializing SIM7600...
   [GSM] Ready!
   [MQTT] Connecting to broker...
   [MQTT] Connected!
   [READY] Waiting for GPS fix...
   ```

### Step 5 — Run Backend Locally

```bash
# Clone the repo
git clone https://github.com/lokeshkumar80/real-time-vehicle-tracking-system.git
cd real-time-vehicle-tracking-system/webapp

# Install dependencies
npm install

# Start the server
node server.js
```

Expected output:
```
🚀 Server running on http://localhost:3000
   REST API : http://localhost:3000/api/status
   WebSocket: ws://localhost:3000
[MQTT] Connected to broker
[MQTT] Subscribed to: vehicletracker/lokesh/location
```

Open **http://localhost:3000** in your browser. The map loads and waits for GPS data.

### Step 6 — Deploy to Render.com

> Skip this step if you only need local testing.

1. Push your code to GitHub:
   ```bash
   git init
   git add .
   git commit -m "Vehicle tracker v2.0"
   git remote add origin https://github.com/YOUR_USERNAME/vehicle-tracker.git
   git push -u origin main
   ```

2. Go to [render.com](https://render.com) → **New → Web Service**
3. Connect your GitHub repository
4. Render auto-detects `render.yaml` — set:
   | Setting | Value |
   |---|---|
   | Build Command | `cd webapp && npm install` |
   | Start Command | `cd webapp && node server.js` |
   | Region | Singapore (lowest latency from India) |
5. Click **Create Web Service** — deploy takes ~2 minutes
6. Your live URL: `https://your-app-name.onrender.com`

> 💡 **Keep-alive:** Render free tier sleeps after 15 minutes of inactivity. Use [UptimeRobot](https://uptimerobot.com) (free) to ping `/api/status` every 14 minutes.

---

## 📶 WiFi Test Mode (No GSM Required)

If you don't have the SIM7600 yet, use `hardware/wifi_test_mode.ino` to test the full MQTT → WebSocket → Dashboard pipeline with just an ESP32 and NEO-6M.

**Edit the WiFi credentials:**

```cpp
const char* WIFI_SSID   = "YOUR_WIFI_NAME";
const char* WIFI_PASS   = "YOUR_WIFI_PASSWORD";
const char* MQTT_TOPIC  = "vehicletracker/lokesh/location"; // same as server.js
```

**GPIO changes for test mode:**

```cpp
#define GPS_RX_PIN  16   // different pins — no conflict with Serial0
#define GPS_TX_PIN  17
```

The test mode uses the `PubSubClient` library for cleaner high-level MQTT — no manual packet construction needed. Everything else (geofencing, JSON payload, dashboard) behaves identically to the main firmware.

---

## ⚙️ Configuration Reference

### Firmware (`main.ino` / `wifi_test_mode.ino`)

| Constant | Default | Description |
|---|---|---|
| `APN` | `"jionet"` | Carrier APN. Jio=`jionet`, Airtel=`airtelgprs.com`, BSNL=`bsnlnet` |
| `MQTT_BROKER` | `"broker.hivemq.com"` | MQTT broker hostname. Change for private broker. |
| `MQTT_PORT` | `1883` | MQTT port. Use `8883` for TLS. |
| `MQTT_TOPIC` | `"vehicletracker/lokesh/location"` | **Must be unique** — change `lokesh` to your name |
| `DEVICE_ID` | `"TRACKER_001"` | MQTT client ID — must be unique per device |
| `THINGSPEAK_API_KEY` | `"YOUR_WRITE_API_KEY"` | Your ThingSpeak channel Write API Key |
| `GEOFENCE_LAT` | `32.7266` | Geofence centre latitude (decimal degrees) |
| `GEOFENCE_LNG` | `74.8570` | Geofence centre longitude (decimal degrees) |
| `GEOFENCE_RADIUS` | `500.0` | Geofence radius in metres |
| `SEND_INTERVAL` | `3000` | Milliseconds between GPS transmissions |
| `GPS_RX_PIN` | `3` | ESP32 GPIO receiving GPS UART TX |
| `GPS_TX_PIN` | `1` | ESP32 GPIO sending to GPS UART RX |
| `GSM_RX_PIN` | `22` | ESP32 GPIO receiving SIM7600 UART TX |
| `GSM_TX_PIN` | `23` | ESP32 GPIO sending to SIM7600 UART RX |

### Backend (`server.js`)

| Constant | Default | Description |
|---|---|---|
| `PORT` | `3000` | HTTP + WebSocket server port (overridden by `process.env.PORT` on Render) |
| `MQTT_BROKER` | `"mqtt://broker.hivemq.com:1883"` | MQTT broker URL — must match firmware |
| `MQTT_TOPIC` | `"vehicletracker/lokesh/location"` | Must match firmware exactly |
| `MAX_HISTORY` | `500` | Maximum location points kept in memory |

### Dashboard (`webapp/index.html`)

| Constant | Default | Description |
|---|---|---|
| `MAPBOX_TOKEN` | `'YOUR_MAPBOX_TOKEN'` | Your Mapbox public token |
| `WS_URL` | `ws://${location.host}` | WebSocket URL — auto-detects, no change needed |
| `TRAIL_MAX` | `200` | Maximum trail dots rendered on the map |

---

## 📡 API Reference

### REST Endpoints

#### `GET /api/status`
Returns backend health and connection status.

```json
{
  "status": "online",
  "mqtt_connected": true,
  "ws_clients": 2,
  "history_count": 147,
  "latest_ts": "2025-05-14T10:23:45.123Z"
}
```

#### `GET /api/location/latest`
Returns the most recent GPS fix.

```json
{
  "device": "TRACKER_001",
  "lat": 32.726612,
  "lng": 74.857045,
  "speed_kmh": 42.3,
  "satellites": 9,
  "geofence_ok": true,
  "distance_m": 312,
  "ts": "2025-05-14T10:23:45.123Z"
}
```

**Error (no data yet):**
```json
{ "error": "No location data yet" }
```

#### `GET /api/location/history?limit=100`
Returns last N location points (default 100, max 500).

```json
[
  { "device": "TRACKER_001", "lat": 32.726612, "lng": 74.857045, ... },
  { "device": "TRACKER_001", "lat": 32.726890, "lng": 74.857210, ... }
]
```

### WebSocket Messages

Connect: `ws://localhost:3000` (or your Render.com URL with `wss://`)

#### Message: `init` (server → client, on connect)
```json
{
  "type": "init",
  "history": [ ...array of last 500 location objects... ],
  "latest": { ...most recent location object... }
}
```

#### Message: `location` (server → client, on each GPS update)
```json
{
  "type": "location",
  "data": {
    "device": "TRACKER_001",
    "lat": 32.726612,
    "lng": 74.857045,
    "speed_kmh": 42.3,
    "satellites": 9,
    "geofence_ok": false,
    "distance_m": 623,
    "ts": "2025-05-14T10:23:45.123Z"
  }
}
```

### MQTT Payload (ESP32 → Broker)

**Topic:** `vehicletracker/lokesh/location`  
**QoS:** 0 (fire and forget)

```json
{
  "device": "TRACKER_001",
  "lat": 32.726612,
  "lng": 74.857045,
  "speed_kmh": 42.3,
  "satellites": 9,
  "geofence_ok": true,
  "distance_m": 312,
  "timestamp": 1234567890
}
```

---

## 📁 Project Structure

```
real-time-vehicle-tracking-system/
│
├── hardware/
│   ├── main.ino                  # Main firmware: ESP32 + GPS + SIM7600 + MQTT
│   └── wifi_test_mode.ino        # WiFi-only firmware for testing without GSM
│
├── webapp/
│   ├── index.html                # Browser dashboard: Mapbox GL JS + WebSocket client
│   ├── server.js                 # Node.js backend: MQTT subscriber + WebSocket + REST API
│   └── package.json              # npm dependencies (express, mqtt, ws)
│
├── render.yaml                   # Render.com one-click deploy config
├── .gitignore                    # Excludes node_modules, .env, tracker.db
├── SETUP_GUIDE.md                # Detailed step-by-step setup instructions
└── README.md                     # This file
```

---

## 🔬 How It Works

### GPS Data Flow

The NEO-6M GPS module continuously outputs **NMEA 0183 sentences** over UART at 9600 baud, once per second. The primary sentence used is `$GPRMC`:

```
$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6A
         │     │  └── Latitude  └── Longitude    │       └── Date
         │     └── Status (A=active, V=void)      └── Speed (knots)
         └── UTC Time
```

The `TinyGPS++` library processes each byte through an internal state machine, validates the XOR checksum, and fires `isUpdated()` on a successful parse. The firmware only proceeds with transmission when:
1. `gps.location.isValid()` — a GPS fix exists
2. `gps.location.isUpdated()` — a new fix has arrived since last check
3. Haversine distance from last transmitted position > 5 metres

### MQTT Protocol

MQTT v3.1.1 packets are constructed **manually in firmware** (no library abstraction) because the SIM7600 exposes only a raw TCP byte interface via AT commands. This requires understanding the binary packet format from the specification:

```
MQTT PUBLISH packet (QoS 0):
┌──────────┬──────────────────┬───────────────────────┬─────────────────┐
│ 0x30     │ Remaining Length │ Topic (len + string)  │ Payload (JSON)  │
│ (1 byte) │ (1–4 bytes)      │ (2 + N bytes)         │ (M bytes)       │
└──────────┴──────────────────┴───────────────────────┴─────────────────┘

0x30 = 0b00110000
  bits 7–4: packet type = 3 (PUBLISH)
  bit 3:    DUP flag = 0
  bits 2–1: QoS = 0
  bit 0:    RETAIN = 0
```

The packet is transmitted via `AT+CIPSEND=<len>` followed by the raw bytes and `Ctrl+Z` (0x1A) to signal end of transmission.

### Geofencing Algorithm

The geofence uses the **Haversine formula** to compute the great-circle distance between the current GPS position and the configured centre point on the Earth's surface:

```
a = sin²(Δlat/2) + cos(lat₁) × cos(lat₂) × sin²(Δlng/2)
d = 2R × arctan2(√a, √(1−a))

where R = 6,371,000 metres (Earth mean radius)
      Δlat, Δlng in radians
```

**Why not Euclidean distance?** Longitude degrees shrink in physical distance as you move toward the poles. At IIT Jammu (latitude ~33°), 1° longitude ≈ 93 km but 1° latitude ≈ 111 km. Euclidean distance on raw degrees would produce ~19% geometric error — the geofence boundary would not be circular. Haversine error is < 0.5% for distances under 1000 km.

### WebSocket Real-Time Update

The architecture replaces the v1.0 polling approach with a push-based WebSocket relay:

```
v1.0 (polling):
Browser ──[XHR every 2s]──► ThingSpeak ──[HTTP response]──► Browser
Latency: up to 17s (2s poll + 15s ThingSpeak write limit)

v2.0 (WebSocket push):
ESP32 ──[MQTT publish]──► HiveMQ ──[MQTT subscribe]──► Node.js ──[ws.send()]──► Browser
Latency: < 500ms end-to-end
```

When the Node.js backend receives an MQTT message, it immediately calls `ws.send()` on all connected WebSocket clients — no delay, no polling. The browser's `onmessage` handler calls `map.flyTo()` and `marker.setLngLat()` to update the map in a single animation frame.

---

## 🗺 Dashboard Features

| Feature | Description |
|---|---|
| **Live marker** | Cyan glowing dot follows vehicle in real-time with smooth `flyTo()` animation |
| **Trail line** | Dashed line shows last 200 GPS positions |
| **Geofence circle** | Green dashed 500m boundary ring visible on map |
| **Geofence alert** | Sidebar banner turns red + toast notification on boundary breach |
| **Speed display** | Real-time speed in km/h from GPS Doppler measurement |
| **Satellite count** | Number of GPS satellites used for current fix |
| **Distance from base** | Live metres from geofence centre |
| **History panel** | Scrollable list of all locations — click any to fly map there |
| **Re-centre button** | One-click to fly map back to current vehicle position |
| **WebSocket status** | Live indicator (green = connected, red = disconnected with auto-reconnect) |
| **Last update time** | Timestamp of most recent GPS fix in local time |

---

## 📊 Performance

| Metric | v1.0 | v2.0 | Improvement |
|---|---|---|---|
| End-to-end latency | ~15–17 seconds | **< 500 ms** | ~34× faster |
| Protocol overhead per update | ~300 bytes | ~50 bytes | 6× smaller |
| TCP connections per hour | ~240 new connections | 1 persistent | Eliminated overhead |
| Update frequency | 1 per 15s (ThingSpeak limit) | 1 per 3s | 5× higher |
| Duplicate transmissions | Yes (stationary vehicle) | No (5m filter) | Eliminated |
| GPS cold start | 27s | 1s (hot start via BBR) | ~27× faster |

---

## 🐛 Troubleshooting

### ESP32 / Firmware

| Symptom | Likely Cause | Fix |
|---|---|---|
| `[GPS] No fix — check antenna` | Indoors or antenna not sky-facing | Go outside, wait 30s, antenna must face upward |
| `[GPS] Searching... Sats: 0` | Cold start, no cached data | Wait up to 60s for first cold-start fix |
| `[GSM] ERROR` responses | Wrong APN or poor signal | Check APN constant, ensure ≥ 2 bars signal |
| SIM7600 resets during TX | Insufficient current supply | Add second LM7812 in parallel + 470µF cap on VCC rail |
| Upload fails: `A fatal error occurred` | Wrong COM port or driver | Install CH340 driver, select correct port in Tools → Port |
| Serial Monitor garbage | Wrong baud rate | Set Serial Monitor to 115200 baud |

### Backend

| Symptom | Likely Cause | Fix |
|---|---|---|
| `[MQTT] Reconnecting...` | Broker unreachable | Check internet, try `ping broker.hivemq.com` |
| Dashboard loads but no data | MQTT topic mismatch | Verify `MQTT_TOPIC` in `server.js` exactly matches firmware |
| `npm install` fails | Node.js version < 18 | Run `node --version`, upgrade if < 18.0.0 |
| Port 3000 already in use | Another process | `lsof -i :3000` then `kill -9 <PID>`, or change PORT |

### Dashboard

| Symptom | Likely Cause | Fix |
|---|---|---|
| Map is blank / grey | Missing/invalid Mapbox token | Check `MAPBOX_TOKEN` in `index.html`, regenerate if expired |
| `OFFLINE` status indicator | WebSocket cannot connect | Check server is running, check browser console (F12) for errors |
| Marker does not appear | No GPS data received yet | Check `/api/status` — is `history_count` > 0? |
| Geofence circle not visible | Map zoom too low | Zoom in to level 12+ to see the 500m circle |

### Render.com Deployment

| Symptom | Fix |
|---|---|
| Build fails: `Cannot find module` | Ensure `cd webapp &&` prefix in build/start commands |
| App sleeps after 15 min | Set up UptimeRobot to ping `/api/status` every 14 min |
| WebSocket fails on live URL | Use `wss://` not `ws://` for HTTPS deployments (auto-handled by `location.host`) |

---

## 🗺 Roadmap

### v3.0 — Production Hardware (Planned)

- [ ] Migrate to **SIM7080G (LTE-M / NB-IoT)** — 10× lower power, PSM sleep mode
- [ ] Upgrade to **u-blox ZED-F9P RTK GPS** — centimetre-level accuracy
- [ ] **FreeRTOS** task architecture — GPS task, MQTT task, Watchdog task
- [ ] **Hardware Watchdog Timer** (`esp_task_wdt_init`) — auto-reset on firmware hang
- [ ] **TLS 1.3 + client certificates** — encrypted MQTT, device authentication
- [ ] **OTA firmware updates** — push new firmware over HTTPS without USB
- [ ] **NVS config storage** — save geofence params / MQTT credentials to flash
- [ ] **MPU-6050 IMU** — dead reckoning during GPS signal loss (tunnels)
- [ ] **SQLite persistence** in backend — GPS history survives server restart
- [ ] Custom PCB design replacing breadboard/perf board

### v4.0 — Machine Learning Analytics (Planned)

- [ ] **Trajectory anomaly detection** — Isolation Forest / LSTM autoencoder on GPS tracks
- [ ] **ETA prediction** — XGBoost regression on historical route data
- [ ] **Geofence boundary learning** — DBSCAN clustering on stop history
- [ ] **Traffic density mapping** — fleet aggregation with H3 hexagonal indexing
- [ ] **Speed alert** — configurable over-speed notification
- [ ] Research paper: *GPS Trajectory Anomaly Detection for Fleet Safety* (IIT Jammu)

### v5.0 — Multi-Modal Positioning (Research)

- [ ] GPS + Wi-Fi RSSI fingerprinting for seamless indoor-outdoor handoff
- [ ] Multi-constellation GNSS (GPS + GLONASS + Galileo + BeiDou)
- [ ] BLE advertising for offline/crowdsourced position reporting

---

## 📜 Version History

### v2.0.0 — May 2025
- Complete independent rebuild
- SIM900A (2G) → SIM7600E-H (4G LTE)
- HTTP polling → MQTT pub/sub (HiveMQ broker)
- PHP + 000webhost → Node.js + Render.com
- setInterval polling → WebSocket real-time push
- Added: Haversine geofencing with configurable radius
- Added: JSON telemetry payload (speed, satellites, geofence_ok, distance_m)
- Added: 5m movement threshold to filter stationary duplicates
- Added: WiFi test mode (no GSM hardware required)
- Added: Render.com CI/CD deploy via render.yaml
- Added: ThingSpeak as secondary backup (30s interval)
- Added: Animated map trail (200 points)
- Added: REST API (/api/status, /api/location/latest, /api/location/history)
- Added: WebSocket init payload (history on connect)

### v1.0.0 — 2022 (Original)
- Initial implementation: ESP32 + NEO-6M + SIM900A + ThingSpeak + Mapbox + PHP
- Joint project with Rohit Tiwari under Prof. Rajneesh Kumar Srivastava
- Dept. of Electronics & Communication Engineering, Pusa Institute of Technology

---

## 👤 Author

**Lokesh Kumar**

- 📧 Email: [2024pcr0046@iitjammu.ac.in](mailto:2024pcr0046@iitjammu.ac.in) · [lokeshkumar797876@gmail.com](mailto:lokeshkumar797876@gmail.com)
- 📱 Phone: +91-7535919305
- 🎓 M.Tech CSE (RA) — Indian Institute of Technology, Jammu (2024–2027) · CGPA: 8.10
- 🎓 B.Tech CSE — University of Allahabad (2020–2024) · CGPA: 8.32
- 🏆 GATE 96–97 percentile (2023, 2024)
- 💼 Co-Head, CDS IIT Jammu (Placement Cell)
- 🔗 LinkedIn: [linkedin.com/in/lokeshkumar](https://linkedin.com/in/lokeshkumar)

---

## 🙏 Acknowledgements

- **Prof. Rajneesh Kumar Srivastava** — Supervisor for the original v1.0 project, Dept. of ECE, JKIAPT, University Of Allahabad
- **Shivam Yadav** — Joint contributor to the original v1.0 project (2022)
- [u-blox](https://www.u-blox.com) — NEO-6M GNSS chip documentation
- [Mikal Hart](https://github.com/mikalhart/TinyGPSPlus) — TinyGPS++ library
- [SIMCOM](https://www.simcom.com) — SIM7600 AT command reference
- [HiveMQ](https://www.hivemq.com) — Free public MQTT broker
- [Mapbox](https://www.mapbox.com) — Maps and navigation APIs
- [OASIS](https://docs.oasis-open.org/mqtt/mqtt/v3.1.1/mqtt-v3.1.1.html) — MQTT v3.1.1 specification

---

## 📄 License

```
MIT License

Copyright (c) 2025 Lokesh Kumar

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

---

<div align="center">

**If this project helped you, please ⭐ the repository.**

Built with ❤️ at IIT Jammu

</div>
