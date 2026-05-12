// ============================================================
//  VEHICLE TRACKER BACKEND — Node.js + MQTT + WebSocket
//  Subscribes to MQTT topic, relays to browser over WebSocket
//  Stores location history in memory (upgradeable to SQLite)
//  Deploy: Render.com (free tier) or Railway.app
// ============================================================

const express    = require('express');
const http       = require('http');
const WebSocket  = require('ws');
const mqtt       = require('mqtt');
const path       = require('path');

const app    = express();
const server = http.createServer(app);
const wss    = new WebSocket.Server({ server });

// ─── CONFIG ─────────────────────────────────
const PORT         = process.env.PORT || 3000;
const MQTT_BROKER  = 'mqtt://broker.hivemq.com:1883';
const MQTT_TOPIC   = 'vehicletracker/lokesh/location';
const MAX_HISTORY  = 500;   // keep last 500 points in memory

// ─── STATE ──────────────────────────────────
let locationHistory = [];   // [{lat, lng, speed_kmh, satellites, geofence_ok, distance_m, ts}]
let latestLocation  = null;
let connectedClients = 0;

// ─── MQTT CLIENT ────────────────────────────
console.log(`[MQTT] Connecting to ${MQTT_BROKER}...`);
const mqttClient = mqtt.connect(MQTT_BROKER, {
  clientId: 'backend_server_' + Math.random().toString(16).substr(2, 8),
  clean: true,
  reconnectPeriod: 5000,
});

mqttClient.on('connect', () => {
  console.log('[MQTT] Connected to broker');
  mqttClient.subscribe(MQTT_TOPIC, { qos: 0 }, (err) => {
    if (err) console.error('[MQTT] Subscribe error:', err);
    else     console.log(`[MQTT] Subscribed to: ${MQTT_TOPIC}`);
  });
});

mqttClient.on('message', (topic, message) => {
  try {
    const data = JSON.parse(message.toString());
    data.ts    = new Date().toISOString();

    // Store in history
    locationHistory.push(data);
    if (locationHistory.length > MAX_HISTORY)
      locationHistory.shift();

    latestLocation = data;

    console.log(`[LOC] ${data.lat.toFixed(6)}, ${data.lng.toFixed(6)} | ${data.speed_kmh?.toFixed(1)} km/h | ${data.geofence_ok ? '✓' : '⚠ OUTSIDE FENCE'}`);

    // Broadcast to all WebSocket clients
    const payload = JSON.stringify({ type: 'location', data });
    wss.clients.forEach(client => {
      if (client.readyState === WebSocket.OPEN)
        client.send(payload);
    });

  } catch (e) {
    console.error('[MQTT] Parse error:', e.message);
  }
});

mqttClient.on('error',       err  => console.error('[MQTT] Error:', err.message));
mqttClient.on('reconnect',   ()   => console.log('[MQTT] Reconnecting...'));
mqttClient.on('disconnect',  ()   => console.log('[MQTT] Disconnected'));

// ─── WEBSOCKET ──────────────────────────────
wss.on('connection', (ws, req) => {
  connectedClients++;
  console.log(`[WS] Client connected (total: ${connectedClients})`);

  // Send current history + latest on connect
  ws.send(JSON.stringify({
    type:    'init',
    history: locationHistory,
    latest:  latestLocation,
  }));

  ws.on('close', () => {
    connectedClients--;
    console.log(`[WS] Client disconnected (total: ${connectedClients})`);
  });

  ws.on('error', err => console.error('[WS] Error:', err.message));
});

// ─── REST API ───────────────────────────────
app.use(express.json());
app.use(express.static(path.join(__dirname, '../webapp')));

// GET latest location
app.get('/api/location/latest', (req, res) => {
  if (!latestLocation)
    return res.status(404).json({ error: 'No location data yet' });
  res.json(latestLocation);
});

// GET location history (last N points)
app.get('/api/location/history', (req, res) => {
  const limit = Math.min(parseInt(req.query.limit) || 100, MAX_HISTORY);
  res.json(locationHistory.slice(-limit));
});

// GET status
app.get('/api/status', (req, res) => {
  res.json({
    status:           'online',
    mqtt_connected:   mqttClient.connected,
    ws_clients:       connectedClients,
    history_count:    locationHistory.length,
    latest_ts:        latestLocation?.ts || null,
  });
});

// Fallback → serve index.html
app.get('*', (req, res) => {
  res.sendFile(path.join(__dirname, '../webapp/index.html'));
});

// ─── START ──────────────────────────────────
server.listen(PORT, () => {
  console.log(`\n🚀 Server running on http://localhost:${PORT}`);
  console.log(`   REST API : http://localhost:${PORT}/api/status`);
  console.log(`   WebSocket: ws://localhost:${PORT}`);
});
