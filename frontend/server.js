require('dotenv').config();
const express = require('express');
const http = require('http');
const socketIo = require('socket.io');
const mqtt = require('mqtt');

const app = express();
const server = http.createServer(app);
const io = socketIo(server);

app.use(express.static('public'));

// ==================== KONFIGURASI RUANGAN (6 RUANGAN) ====================
const rooms = new Map();
const roomList = [
  { id: 'R101', name: 'R101', location: 'Lantai 1', lantai: 1, ruang: '101' },
  { id: 'R102', name: 'R102', location: 'Lantai 1', lantai: 1, ruang: '102' },
  { id: 'R103', name: 'R103', location: 'Lantai 1', lantai: 1, ruang: '103' },
  { id: 'R201', name: 'R201', location: 'Lantai 2', lantai: 2, ruang: '201' },
  { id: 'R202', name: 'R202', location: 'Lantai 2', lantai: 2, ruang: '202' },
  { id: 'R203', name: 'R203', location: 'Lantai 2', lantai: 2, ruang: '203' }
];

roomList.forEach(room => {
  rooms.set(room.id, {
    deviceId: room.id,
    name: room.name,
    location: room.location,
    flame: { detected: false, intensity: 'LOW', analog: 0 },
    gas: { detected: false, ppm: 0, gasType: 'NONE' },
    temperature: { value: 25.0, overThreshold: false, threshold: 60.0 },
    alertLevel: 0,
    lastUpdate: new Date().toISOString(),
    actuatorTopic: `gedung/lantai${room.lantai}/ruang${room.ruang}/aktuator`
  });
});

console.log(`✅ ${rooms.size} ruangan siap: ${Array.from(rooms.keys()).join(', ')}`);

// ==================== STATE GLOBAL ALERT ====================
let globalAlert = {
  active: false,
  level: 0,
  sourceRoom: null,
  timeoutId: null,
  intervalId: null
};

// Fungsi mengirim perintah ke semua ESP (tanpa log sukses)
function sendBuzzerCommandToAll(state) {
  const payload = JSON.stringify({ buzzer: state ? 1 : 0 });
  const allRooms = Array.from(rooms.values());
  allRooms.forEach(room => {
    if (mqttClient.connected && room.actuatorTopic) {
      mqttClient.publish(room.actuatorTopic, payload, { qos: 1 }, (err) => {
        if (err) console.error(`❌ Gagal kirim ke ${room.name}:`, err);
        // tidak mencetak sukses
      });
    }
  });
}

// ==================== POLA BUZZER DURASI 10 DETIK ====================
function startBuzzerPattern(level, durationMs = 10000) {
  if (globalAlert.intervalId) clearInterval(globalAlert.intervalId);
  if (globalAlert.timeoutId) clearTimeout(globalAlert.timeoutId);

  let onMs = 0, offMs = 0;
  switch (level) {
    case 1:
      onMs = 500;   // 0.5 detik nyala
      offMs = 1000; // 1 detik mati
      break;
    case 2:
      onMs = 250;   // 0.25 detik nyala
      offMs = 250;  // 0.25 detik mati
      break;
    case 3:
      onMs = durationMs;
      offMs = 0;
      break;
    default:
      return;
  }

  if (level === 3) {
    sendBuzzerCommandToAll(true);
    globalAlert.timeoutId = setTimeout(() => {
      sendBuzzerCommandToAll(false);
      stopGlobalAlert();
    }, durationMs);
  } else {
    let state = true;
    const tick = () => {
      sendBuzzerCommandToAll(state);
      state = !state;
    };
    tick();
    globalAlert.intervalId = setInterval(tick, onMs + offMs);
    globalAlert.timeoutId = setTimeout(() => {
      clearInterval(globalAlert.intervalId);
      sendBuzzerCommandToAll(false);
      stopGlobalAlert();
    }, durationMs);
  }
}

function stopGlobalAlert() {
  if (globalAlert.active) {
    globalAlert.active = false;
    globalAlert.level = 0;
    globalAlert.sourceRoom = null;
    if (globalAlert.intervalId) clearInterval(globalAlert.intervalId);
    if (globalAlert.timeoutId) clearTimeout(globalAlert.timeoutId);
    globalAlert.intervalId = null;
    globalAlert.timeoutId = null;
    io.emit('global-alert-update', { level: 0, sourceRoom: null });
    console.log('✅ Global alert berakhir');
  }
}

function triggerGlobalAlert(level, sourceRoomId, notificationTitle, notificationBody, route) {
  if (level < 1 || level > 3) return;
  if (globalAlert.active && level > globalAlert.level) {
    stopGlobalAlert();
  } else if (globalAlert.active) {
    return;
  }

  globalAlert.active = true;
  globalAlert.level = level;
  globalAlert.sourceRoom = sourceRoomId;

  io.emit('push-notification', {
    title: notificationTitle,
    body: notificationBody,
    level: level,
    room: sourceRoomId,
    route: route,
    timestamp: new Date().toISOString()
  });

  io.emit('global-alert-update', { level: level, sourceRoom: sourceRoomId });
  startBuzzerPattern(level, 10000);
}

// ==================== KONEKSI MQTT ====================
const mqttOptions = {
  host: process.env.MQTT_HOST,
  port: parseInt(process.env.MQTT_PORT),
  protocol: 'mqtts',
  username: process.env.MQTT_USERNAME,
  password: process.env.MQTT_PASSWORD,
  clientId: `backend-${Date.now()}`,
  clean: false,
  reconnectPeriod: 3000,
  rejectUnauthorized: true
};

const mqttClient = mqtt.connect(mqttOptions);

mqttClient.on('connect', () => {
  console.log('✅ Terhubung ke MQTT broker');
  mqttClient.subscribe('gedung/+/+/sensor', { qos: 1 }, (err) => {
    if (!err) console.log('📡 Subscribe ke gedung/+/+/sensor berhasil');
  });
});

mqttClient.on('message', (topic, payload) => {
  try {
    const data = JSON.parse(payload.toString());
    const { lantai, ruang, level, gas, api, suhu } = data;
    if (!lantai || !ruang) return;
    const roomId = ruang.toUpperCase();
    if (!rooms.has(roomId)) {
      console.warn(`⚠️ Ruangan ${roomId} tidak dikenal`);
      return;
    }

    const room = rooms.get(roomId);
    room.lastUpdate = new Date().toISOString();
    room.flame = { detected: api === true || api === 1, intensity: api ? 'HIGH' : 'LOW', analog: api ? 800 : 0 };
    const gasValue = typeof gas === 'number' ? gas : 0;
    room.gas = { detected: gasValue > 2000, ppm: gasValue, gasType: gasValue > 2000 ? 'COMBUSTIBLE' : 'NONE' };
    const tempValue = typeof suhu === 'number' ? suhu : 25.0;
    room.temperature = { value: tempValue, overThreshold: tempValue > 50.0, threshold: 50.0 };
    
    const oldLevel = room.alertLevel;
    const newLevel = typeof level === 'number' ? level : 0;
    room.alertLevel = newLevel;

    broadcastRoomStatus();

    if (newLevel > oldLevel && newLevel > 0) {
      let title, body, route;
      switch (newLevel) {
        case 1: title = `⚠️ ALERT LEVEL 1 - Waspada di ${room.name}`; body = 'Terdeteksi asap/suhu/api ringan.'; route = 'Pantau terus'; break;
        case 2: title = `🔥 ALERT LEVEL 2 - Siaga di ${room.name}`; body = 'Dua sensor aktif! Tim evakuasi menuju lokasi.'; route = 'Bersiap evakuasi sebagian'; break;
        case 3: title = `🚨 ALERT LEVEL 3 - EVAKUASI TOTAL di ${room.name}`; body = 'Kebakaran besar! Semua karyawan segera meninggalkan gedung.'; route = 'Gunakan tangga darurat, jangan lift!'; break;
      }
      triggerGlobalAlert(newLevel, room.name, title, body, route);
    }
  } catch (err) {
    console.error('❌ Gagal parsing MQTT:', err.message);
  }
});

function broadcastRoomStatus() {
  io.emit('rooms-update', Array.from(rooms.values()));
}

// ==================== SOCKET.IO ====================
io.on('connection', (socket) => {
  console.log('🟢 Client terhubung:', socket.id);
  socket.emit('rooms-update', Array.from(rooms.values()));
  socket.emit('global-alert-update', { level: globalAlert.level, sourceRoom: globalAlert.sourceRoom });

  socket.on('test-notification', (data) => {
    const { level, roomName } = data;
    let title, body, route;
    switch (level) {
      case 1: title = `⚠️ ALERT LEVEL 1 - Waspada di ${roomName}`; body = 'Simulasi: Terdeteksi asap/suhu/api ringan.'; route = 'Pantau terus'; break;
      case 2: title = `🔥 ALERT LEVEL 2 - Siaga di ${roomName}`; body = 'Simulasi: Dua sensor aktif! Tim evakuasi menuju lokasi.'; route = 'Bersiap evakuasi sebagian'; break;
      default: title = `🚨 ALERT LEVEL 3 - EVAKUASI TOTAL di ${roomName}`; body = 'Simulasi: Kebakaran besar! Semua karyawan segera meninggalkan gedung.'; route = 'Gunakan tangga darurat, jangan lift!';
    }
    triggerGlobalAlert(level, roomName, title, body, route);
  });

  socket.on('disconnect', () => console.log('🔴 Client terputus:', socket.id));
});

const PORT = process.env.PORT || 3000;
server.listen(PORT, () => {
  console.log(`🌐 Server berjalan di http://localhost:${PORT}`);
});

mqttClient.on('error', (err) => console.error('❌ MQTT Error:', err.message));
mqttClient.on('reconnect', () => console.log('🔄 MQTT reconnect...'));