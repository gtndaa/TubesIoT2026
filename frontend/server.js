require('dotenv').config();
const express = require('express');
const http = require('http');
const socketIo = require('socket.io');
const mqtt = require('mqtt');

const app = express();
const server = http.createServer(app);
const io = socketIo(server);

app.use(express.static('public'));

// ---------- Daftar Ruangan (mockup 6 ruangan dengan device_id berbeda) ----------
const rooms = new Map();
const deviceToRoom = new Map();

const initialRooms = [
  { deviceId: 'ESP32-FIRESYS-01', name: 'Ruang Server Lt.2', location: 'Lantai 2' },
  { deviceId: 'ESP32-FIRESYS-02', name: 'Ruang Kantor Lt.1', location: 'Lantai 1' },
  { deviceId: 'ESP32-FIRESYS-03', name: 'Ruang Arsip Lt.2', location: 'Lantai 2' },
  { deviceId: 'ESP32-FIRESYS-04', name: 'Lab Komputer Lt.3', location: 'Lantai 3' },
  { deviceId: 'ESP32-FIRESYS-05', name: 'Ruang Panel Lt.1', location: 'Lantai 1' },
  { deviceId: 'ESP32-FIRESYS-06', name: 'Storage Lt.2', location: 'Lantai 2' }
];

initialRooms.forEach(room => {
  rooms.set(room.deviceId, {
    deviceId: room.deviceId,
    name: room.name,
    location: room.location,
    flame: { detected: false, intensity: 'LOW', analog: 0 },
    gas: { detected: false, ppm: 0, gasType: 'NONE' },
    temperature: { value: 25.0, overThreshold: false, threshold: 60.0 },
    alertLevel: 0,
    lastUpdate: new Date().toISOString()
  });
  deviceToRoom.set(room.deviceId, room.name);
});

// ---------- Koneksi MQTT ke HiveMQ (TLS) ----------
const mqttOptions = {
  host: process.env.MQTT_HOST,
  port: parseInt(process.env.MQTT_PORT),
  protocol: 'mqtts',
  username: process.env.MQTT_USERNAME,
  password: process.env.MQTT_PASSWORD,
  clientId: `backend-${Date.now()}`,
  clean: false,
  reconnectPeriod: 3000,
  rejectUnauthorized: true  // untuk valid certificate
};

const mqttClient = mqtt.connect(mqttOptions);

mqttClient.on('connect', () => {
  console.log('✅ Terhubung ke MQTT broker (HiveMQ)');
  mqttClient.subscribe('firedetect/#', { qos: 1 }, (err) => {
    if (!err) console.log('📡 Subscribe ke firedetect/# berhasil');
    else console.error('❌ Gagal subscribe:', err);
  });
});

// ---------- Proses setiap pesan MQTT dan update status ruangan ----------
mqttClient.on('message', (topic, payload) => {
  try {
    const data = JSON.parse(payload.toString());
    const deviceId = data.device_id;
    
    if (!rooms.has(deviceId)) {
      // Abaikan device_id yang tidak dikenal (bisa ditambahkan nanti)
      console.warn(`⚠️ Device ID tidak dikenal: ${deviceId}`);
      return;
    }
    
    const room = rooms.get(deviceId);
    room.lastUpdate = data.timestamp || new Date().toISOString();
    
    // Update sesuai topic
    if (topic === 'firedetect/sensor/flame') {
      room.flame = {
        detected: data.flame_detected,
        intensity: data.intensity,
        analog: data.analog_value
      };
    } 
    else if (topic === 'firedetect/sensor/gas') {
      room.gas = {
        detected: data.gas_detected,
        ppm: data.ppm,
        gasType: data.gas_type
      };
    } 
    else if (topic === 'firedetect/sensor/temperature') {
      room.temperature = {
        value: data.temperature_c,
        overThreshold: data.over_threshold,
        threshold: data.threshold_c
      };
    }
    else if (topic === 'firedetect/alert/event') {
      room.alertLevel = data.alert_level;
      // Kirim notifikasi real-time ke semua client
      io.emit('push-notification', {
        title: `🚨 ALERT LEVEL ${data.alert_level} - ${room.name}`,
        body: data.alert_label,
        level: data.alert_level,
        room: room.name,
        route: data.evacuation_route || 'Ikuti jalur evakuasi terdekat',
        timestamp: data.timestamp
      });
    }
    
    // Update alert level dari ESP32 jika belum di-update oleh alert event
    if (data.alert_level !== undefined && topic !== 'firedetect/alert/event') {
      room.alertLevel = Math.max(room.alertLevel, data.alert_level);
    }
    
    // Kirim status ruangan terbaru ke semua client
    broadcastRoomStatus();
  } catch (err) {
    console.error('❌ Gagal parsing payload MQTT:', err.message);
  }
});

// Fungsi mengirim semua status ruangan ke client
function broadcastRoomStatus() {
  const allRooms = Array.from(rooms.values());
  io.emit('rooms-update', allRooms);
}

// ---------- Socket.IO untuk komunikasi real-time dengan frontend ----------
io.on('connection', (socket) => {
  console.log('🟢 Client terhubung:', socket.id);
  
  // Kirim status awal
  socket.emit('rooms-update', Array.from(rooms.values()));
  
  socket.on('disconnect', () => {
    console.log('🔴 Client terputus:', socket.id);
  });
});

// ---------- Jalankan HTTP server ----------
const PORT = process.env.PORT || 3000;
server.listen(PORT, () => {
  console.log(`🌐 Website monitoring berjalan di http://localhost:${PORT}`);
});

// Handle error MQTT
mqttClient.on('error', (err) => {
  console.error('❌ MQTT Error:', err.message);
});
mqttClient.on('reconnect', () => {
  console.log('🔄 MQTT reconnect...');
});