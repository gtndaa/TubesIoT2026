const socket = io();

let currentGlobalAlert = { level: 0, sourceRoom: null };
let lastRoomsData = [];

function renderRooms(rooms) {
  const grid = document.getElementById('roomsGrid');
  if (!rooms.length) {
    grid.innerHTML = '<div class="loading">Belum ada data ruangan</div>';
    return;
  }

  grid.innerHTML = rooms.map(room => {
    const effectiveLevel = Math.max(room.alertLevel, currentGlobalAlert.level);
    let levelClass = `level-${effectiveLevel}`;
    let alertText = 'AMAN';
    if (effectiveLevel === 1) alertText = '⚠️ LEVEL 1 (Waspada)';
    else if (effectiveLevel === 2) alertText = '🔥 LEVEL 2 (Siaga)';
    else if (effectiveLevel === 3) alertText = '🚨 LEVEL 3 (Evakuasi!)';

    const flameStatus = room.flame.detected ? 
      `<span class="flame-true">🔥 TERDETEKSI (${room.flame.intensity})</span>` : 
      `<span>✅ Tidak Terdeteksi</span>`;
    const gasStatus = room.gas.detected ?
      `<span class="gas-true">💨 TERDETEKSI (${room.gas.ppm.toFixed(0)} ppm - ${room.gas.gasType})</span>` :
      `<span>✅ Normal (${room.gas.ppm.toFixed(0)} ppm)</span>`;
    const tempStatus = room.temperature.overThreshold ?
      `<span class="temp-high">🌡️ ${room.temperature.value.toFixed(1)}°C (OVER THRESHOLD!)</span>` :
      `<span>🌡️ ${room.temperature.value.toFixed(1)}°C / ${room.temperature.threshold}°C</span>`;

    let globalNote = '';
    if (currentGlobalAlert.level > 0 && currentGlobalAlert.sourceRoom !== room.name) {
      globalNote = `<div style="font-size:0.7rem; color:#ffa502; margin-top:8px;">⚠️ Peringatan dari ${currentGlobalAlert.sourceRoom}</div>`;
    }

    return `
      <div class="room-card ${levelClass}">
        <div class="room-title">
          <h2>🏢 ${room.name}</h2>
          <div class="alert-badge">${alertText}</div>
        </div>
        <div class="sensor-row">
          <span class="sensor-label">🔥 API:</span>
          <span class="sensor-value">${flameStatus}</span>
        </div>
        <div class="sensor-row">
          <span class="sensor-label">💨 GAS:</span>
          <span class="sensor-value">${gasStatus}</span>
        </div>
        <div class="sensor-row">
          <span class="sensor-label">🌡️ SUHU:</span>
          <span class="sensor-value">${tempStatus}</span>
        </div>
        ${globalNote}
        <div class="location">📍 ${room.location} | Terakhir: ${new Date(room.lastUpdate).toLocaleTimeString()}</div>
      </div>
    `;
  }).join('');
}

function showToast(notification) {
  const container = document.getElementById('toastContainer');
  const toast = document.createElement('div');
  toast.className = `toast level-${notification.level}`;
  toast.innerHTML = `
    <div class="toast-title">${notification.title}</div>
    <div class="toast-body">${notification.body}</div>
    <div style="font-size:0.75rem; margin-top:6px;">📍 ${notification.room} | ${new Date(notification.timestamp).toLocaleTimeString()}</div>
    ${notification.route ? `<div style="font-size:0.7rem; color:#ffa502;">🚪 Jalur: ${notification.route}</div>` : ''}
  `;
  container.appendChild(toast);
  setTimeout(() => toast.remove(), 6000);
  if (Notification.permission === 'granted') {
    new Notification(notification.title, { body: notification.body, icon: '/favicon.ico' });
  }
}

// Izin notifikasi
if (Notification.permission !== 'granted' && Notification.permission !== 'denied') {
  Notification.requestPermission();
}

// Event socket
socket.on('rooms-update', (rooms) => {
  lastRoomsData = rooms;
  renderRooms(rooms);
});

socket.on('global-alert-update', (data) => {
  currentGlobalAlert = { level: data.level, sourceRoom: data.sourceRoom };
  if (lastRoomsData.length) renderRooms(lastRoomsData);
});

socket.on('push-notification', (notification) => {
  console.log('🔔 Notifikasi:', notification);
  showToast(notification);
});

// Tombol test dengan ruangan berbeda
document.getElementById('testNotif1')?.addEventListener('click', () => {
  socket.emit('test-notification', { level: 1, roomName: 'R102' });
});
document.getElementById('testNotif2')?.addEventListener('click', () => {
  socket.emit('test-notification', { level: 2, roomName: 'R103' });
});
document.getElementById('testNotif3')?.addEventListener('click', () => {
  socket.emit('test-notification', { level: 3, roomName: 'R201' });
});

socket.on('connect', () => console.log('✅ Terhubung ke server'));
socket.on('disconnect', () => console.log('❌ Koneksi terputus'));