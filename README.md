<div align="center">

```
███████╗██╗██████╗ ███████╗    ██████╗ ███████╗████████╗███████╗ ██████╗████████╗
██╔════╝██║██╔══██╗██╔════╝    ██╔══██╗██╔════╝╚══██╔══╝██╔════╝██╔════╝╚══██╔══╝
█████╗  ██║██████╔╝█████╗      ██║  ██║█████╗     ██║   █████╗  ██║        ██║
██╔══╝  ██║██╔══██╗██╔══╝      ██║  ██║██╔══╝     ██║   ██╔══╝  ██║        ██║
██║     ██║██║  ██║███████╗    ██████╔╝███████╗   ██║   ███████╗╚██████╗   ██║
╚═╝     ╚═╝╚═╝  ╚═╝╚══════╝    ╚═════╝ ╚══════╝   ╚═╝   ╚══════╝ ╚═════╝   ╚═╝
```

# 🔥 FireDetect IoT: Smart Fire Detection System

**Sistem deteksi kebakaran berbasis IoT real-time menggunakan ESP32-S2, MQTT, dan Firebase FCM**

---

[![Platform](https://img.shields.io/badge/Platform-ESP32--S2-red?style=for-the-badge&logo=espressif)](https://www.espressif.com/)
[![Protocol](https://img.shields.io/badge/Protocol-MQTT%20v3.1.1-blue?style=for-the-badge&logo=mqtt)](https://mqtt.org/)
[![Broker](https://img.shields.io/badge/Broker-HiveMQ-orange?style=for-the-badge)](https://www.hivemq.com/)
[![Backend](https://img.shields.io/badge/Backend-Node.js-green?style=for-the-badge&logo=node.js)](https://nodejs.org/)
[![Notification](https://img.shields.io/badge/Notifikasi-Firebase%20FCM-yellow?style=for-the-badge&logo=firebase)](https://firebase.google.com/)
[![License](https://img.shields.io/badge/License-MIT-purple?style=for-the-badge)](LICENSE)

</div>

---

## 👥 Tim

| NIM | Nama |
|-----|------|
| 13223002 | Agita Trinanda I. |
| 13223034 | Muhammad Dzaki F. |
| 13223073 | Fahrian Maulana F. N. |
| 13223096 | Ramadhan Abhinawa H. |



## 🔎 About

**FireDetect IoT** adalah sistem pendeteksi kebakaran berbasis Internet of Things yang dirancang untuk memantau kondisi ruangan secara real-time. Sistem ini mengintegrasikan tiga sensor fisik (api, gas, suhu) pada mikrokontroler **ESP32-S2**, dikomunikasikan melalui protokol **MQTT over TLS** ke cloud broker **HiveMQ**, dan memproses data di backend **Node.js** sebelum mengirimkan notifikasi push ke perangkat melalui **Firebase Cloud Messaging (FCM)**.

### ✨ Fitur Utama

- Monitoring 3 sensor secara **real-time** (setiap 2 detik)
- Komunikasi aman via **MQTT over TLS/SSL** (port 8883)
- **3 level alert** adaptif berdasarkan kombinasi sensor
- **Heartbeat / status online** perangkat otomatis
- Notifikasi push ke HP & website via **Firebase FCM**
- Pesan **Last Will & Testament** untuk deteksi perangkat offline
- **Auto-broadcast** evakuasi pada kondisi darurat (Level 3)

---

## 🏗 Arsitektur Sistem

```
┌─────────────────────────────────────────────────────────────────┐
│                        ESP32-S2 (Edge Device)                   │
│                                                                 │
│   ┌──────────┐   ┌──────────┐   ┌──────────────┐                │
│   │  KY-026  │   │   MQ-2   │   │   DS18B20    │                │
│   │  (Flame) │   │  (Gas)   │   │   (Suhu)     │                │
│   └────┬─────┘   └────┬─────┘   └──────┬───────┘                │
│        │ GPIO 35       │ GPIO 14         │ GPIO 33              │
│        └───────────────┴─────────────────┘                      │
│                         │ Baca tiap 2 detik                     │
│                    ┌────▼─────┐                                 │
│                    │ JSON     │ Struktur data payload           │
│                    │ Payload  │                                 │
│                    └────┬─────┘                                 │
└─────────────────────────┼───────────────────────────────────────┘
                           │ WiFi → MQTT over TLS (port 8883)
                           ▼
┌──────────────────────────────────┐
│        HiveMQ Cloud Broker       │
│  firedetect/sensor/flame         │
│  firedetect/sensor/gas           │
│  firedetect/sensor/temperature   │
│  firedetect/alert/event          │
│  firedetect/device/status        │
└──────────────┬───────────────────┘
               │ Subscribe firedetect/#
               ▼
┌──────────────────────────────────┐
│      Backend Node.js             │
│  - Proses logika alert level 1-3 │
│  - Tentukan tindakan             │
└──────────────┬───────────────────┘
               │ Firebase FCM
               ▼
┌──────────────────────────────────┐
│   📱 Website / HP Pengguna      │
│   Push Notification Real-time    │
└──────────────────────────────────┘
```

---

## 🔧 Komponen

| Komponen | Spesifikasi | Fungsi |
|----------|------------|--------|
| **ESP32-S2** | Mikrokontroler utama | Membaca sensor, koneksi WiFi/MQTT |
| **KY-026** | Flame Sensor, GPIO 35 (ADC) | Mendeteksi keberadaan api |
| **MQ-2** | Gas Sensor, GPIO 14 (ADC) | Mendeteksi gas LPG/Smoke/CO |
| **DS18B20** | Temperature Sensor, GPIO 33 (1-Wire) | Mengukur suhu ruangan |

---

## 📡 Sensor & MQTT Topics

| Sensor | Model | GPIO | MQTT Topic | QoS | Retain |
|--------|-------|------|-----------|-----|--------|
| Flame Sensor | KY-026 | 35 (ADC) | `firedetect/sensor/flame` | 1 | ✅ |
| Gas Sensor | MQ-2 | 14 (ADC) | `firedetect/sensor/gas` | 1 | ✅ |
| Temperature | DS18B20 | 33 (1-Wire) | `firedetect/sensor/temperature` | 1 | ✅ |
| Alert Event | ESP32 Logic | Software | `firedetect/alert/event` | 2 | ❌ |
| Device Status | ESP32 WiFi | Software | `firedetect/device/status` | 1 | ✅ |

---

## 📦 Struktur Payload JSON

### 🔥 Flame Sensor — `firedetect/sensor/flame`

```json
{
  "device_id": "ESP32-FIRESYS-01",
  "sensor": "flame",
  "model": "KY-026",
  "timestamp": "2025-05-14T09:23:11.452Z",
  "gpio_pin": 35,
  "digital_value": 0,
  "analog_value": 312,
  "flame_detected": true,
  "intensity": "HIGH",
  "alert_level": 2,
  "location": "Ruang Server Lt.2"
}
```

| Field | Tipe | Keterangan |
|-------|------|-----------|
| `digital_value` | Integer (0/1) | 0 = api terdeteksi, 1 = aman (aktif LOW) |
| `analog_value` | Integer (0–4095) | Nilai ADC 12-bit, makin kecil = makin terang |
| `flame_detected` | Boolean | `true` jika `digital_value == 0` |
| `intensity` | String | `LOW` / `MEDIUM` / `HIGH` |
| `alert_level` | Integer (1–3) | Level bahaya yang ditentukan ESP32 |

---

### 💨 Gas Sensor — `firedetect/sensor/gas`

```json
{
  "device_id": "ESP32-FIRESYS-01",
  "sensor": "gas",
  "model": "MQ-2",
  "timestamp": "2025-05-14T09:23:09.118Z",
  "gpio_pin": 14,
  "analog_value": 2841,
  "voltage_v": 2.30,
  "ppm": 487.3,
  "threshold_ppm": 300,
  "gas_detected": true,
  "gas_type": "LPG/Smoke",
  "alert_level": 2,
  "location": "Ruang Server Lt.2"
}
```

| Field | Tipe | Keterangan |
|-------|------|-----------|
| `ppm` | Float | Konsentrasi gas hasil kalibrasi |
| `threshold_ppm` | Integer | Batas aman (default: 300 PPM) |
| `gas_detected` | Boolean | `true` jika `ppm > threshold_ppm` |
| `gas_type` | String | `LPG` / `Smoke` / `CO` |

---

### 🌡️ Temperature Sensor — `firedetect/sensor/temperature`

```json
{
  "device_id": "ESP32-FIRESYS-01",
  "sensor": "temperature",
  "model": "DS18B20",
  "timestamp": "2025-05-14T09:23:08.776Z",
  "gpio_pin": 33,
  "temperature_c": 68.5,
  "threshold_c": 60.0,
  "over_threshold": true,
  "resolution_bits": 12,
  "alert_level": 1,
  "location": "Ruang Server Lt.2"
}
```

---

### 🚨 Alert Event — `firedetect/alert/event`

```json
{
  "device_id": "ESP32-FIRESYS-01",
  "alert_id": "ALT-20250514-0923-L3",
  "timestamp": "2025-05-14T09:23:15.000Z",
  "alert_level": 3,
  "alert_label": "KEBAKARAN BESAR - EVAKUASI SEGERA",
  "sensors_triggered": ["flame", "gas", "temperature"],
  "readings": {
    "flame_detected": true,
    "gas_ppm": 487.3,
    "temperature_c": 68.5
  },
  "location": "Ruang Server Lt.2",
  "action": "BROADCAST_ALL",
  "evacuation_route": "Tangga Darurat A - Lantai 2",
  "auto_broadcast": true
}
```

---

### 💚 Heartbeat (Semua Sensor Normal) — `firedetect/device/status`

```json
{
  "device_id": "ESP32-FIRESYS-01",
  "sensor": "all",
  "timestamp": "2025-05-14T08:00:00.000Z",
  "status": "NORMAL",
  "alert_level": 0,
  "readings": {
    "flame_detected": false,
    "gas_ppm": 42.1,
    "temperature_c": 27.3
  },
  "uptime_seconds": 86400,
  "wifi_rssi_dbm": -58,
  "location": "Ruang Server Lt.2"
}
```

---

## 🚦 Logika Alert Level

```
  ┌─────────────────────────────────────────────────────────────────┐
  │                      ALERT LEVEL MATRIX                         │
  ├──────────┬──────────────────────────────┬───────────────────────┤
  │  LEVEL   │  KONDISI SENSOR              │  AKSI SISTEM          │
  ├──────────┼──────────────────────────────┼───────────────────────┤
  │  🟡  1   │  1 sensor aktif              │  Tunggu konfirmasi    │
  │          │  Suhu ATAU Gas ATAU Api      │  admin 60 detik       │
  ├──────────┼──────────────────────────────┼───────────────────────┤
  │  🟠  2   │  2 sensor aktif              │  Alarm berbunyi,      │
  │          │  Suhu+Gas ATAU Suhu+Api      │  broadcast tim        │
  │          │                              │  evakuasi lantai      │
  ├──────────┼──────────────────────────────┼───────────────────────┤
  │  🔴  3   │  3 sensor aktif              │  AUTO BROADCAST       │
  │          │  + Api besar terdeteksi      │  tanpa konfirmasi,    │
  │          │                              │  tampilkan jalur      │
  │          │                              │  evakuasi             │
  └──────────┴──────────────────────────────┴───────────────────────┘
```

| Level | Notifikasi Dikirim ke |
|-------|-----------------------|
| 🟡 Level 1 | Admin & Security |
| 🟠 Level 2 | Semua staf lantai terdampak |
| 🔴 Level 3 | **Seluruh karyawan** via FCM Broadcast |

---

## ⚙️ Konfigurasi MQTT

| Parameter | Nilai |
|-----------|-------|
| **Broker Host** | `*.hivemq.cloud` |
| **Port MQTT (TLS)** | `8883` |
| **Protocol** | MQTT v3.1.1 over TLS/SSL |
| **Client ID** | `ESP32-FIRESYS-01` |
| **Username** | `firesys_device` |
| **Keep Alive** | 60 detik |
| **QoS Level** | 1 (At Least Once) |
| **Clean Session** | `false` (persistent) |
| **Will Topic** | `firedetect/device/status` |
| **Will Message** | `{"status":"OFFLINE","device_id":"ESP32-FIRESYS-01"}` |



<div align="center">

**Tugas Besar EL4044 Perancangan Sistem IoT**

*Sistem ini dirancang untuk meningkatkan keselamatan gedung melalui deteksi kebakaran dini yang cepat dan akurat.*

</div>