const socket = io();

// Fungsi render kartu ruangan
function renderRooms(rooms) {
  const grid = document.getElementById('roomsGrid');
  if (!rooms.length) {
    grid.innerHTML = '<div class="loading">Belum ada data ruangan</div>';
    return;
  }

  grid.innerHTML = rooms.map(room => {
    // Tentukan level class
    let levelClass = `level-${room.alertLevel}`;
    let alertText = 'AMAN';
    if (room.alertLevel === 1) alertText = '⚠️ LEVEL 1 (Waspada)';
    else if (room.alertLevel === 2) alertText = '🔥 LEVEL 2 (Siaga)';
    else if (room.alertLevel === 3) alertText = '🚨 LEVEL 3 (Evakuasi!)';

    // Status sensor
    const flameStatus = room.flame.detected ? 
      `<span class="flame-true">🔥 TERDETEKSI (${room.flame.intensity})</span>` : 
      `<span>✅ Aman</span>`;
    
    const gasStatus = room.gas.detected ?
      `<span class="gas-true">💨 TERDETEKSI (${room.gas.ppm.toFixed(0)} ppm - ${room.gas.gasType})</span>` :
      `<span>✅ Normal (${room.gas.ppm.toFixed(0)} ppm)</span>`;
    
    const tempStatus = room.temperature.overThreshold ?
      `<span class="temp-high">🌡️ ${room.temperature.value.toFixed(1)}°C (OVER THRESHOLD!)</span>` :
      `<span>🌡️ ${room.temperature.value.toFixed(1)}°C / ${room.temperature.threshold}°C</span>`;

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
        <div class="location">📍 ${room.location} | Terakhir: ${new Date(room.lastUpdate).toLocaleTimeString()}</div>
      </div>
    `;
  }).join('');
}

// Tampilkan notifikasi toast
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
  
  // Hapus setelah 6 detik
  setTimeout(() => {
    toast.remove();
  }, 6000);

  // Juga tampilkan notifikasi browser jika diizinkan
  if (Notification.permission === 'granted') {
    new Notification(notification.title, { body: notification.body, icon: '/favicon.ico' });
  }
}

// Minta izin notifikasi browser
if (Notification.permission !== 'granted' && Notification.permission !== 'denied') {
  Notification.requestPermission();
}

// Event dari server
socket.on('rooms-update', (rooms) => {
  renderRooms(rooms);
});

socket.on('push-notification', (notification) => {
  console.log('🔔 Notifikasi:', notification);
  showToast(notification);
});

// Koneksi status
socket.on('connect', () => console.log('✅ Terhubung ke server monitoring'));
socket.on('disconnect', () => console.log('❌ Koneksi server terputus'));