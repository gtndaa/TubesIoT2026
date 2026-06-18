# 🔥 FireDetect IoT

**Sistem Deteksi Kebakaran Berbasis IoT Menggunakan ESP32-S2, MQTT, dan Firebase Cloud Messaging**

---

## 📖 Deskripsi

FireDetect IoT merupakan sistem pendeteksi kebakaran berbasis Internet of Things yang dirancang untuk memantau kondisi ruangan secara real-time menggunakan sensor api, sensor gas, dan sensor suhu. Data sensor dikirimkan oleh ESP32-S2 ke broker MQTT melalui koneksi TLS/SSL dan diproses oleh backend Node.js untuk menentukan tindakan lanjutan serta mengirimkan notifikasi kepada pengguna.

Sistem ini dikembangkan sebagai bagian dari **Tugas Besar EL4044 Perancangan Sistem IoT**.

---

## 👥 Tim

| NIM      | Nama                  |
| -------- | --------------------- |
| 13223002 | Agita Trinanda I.     |
| 13223034 | Muhammad Dzaki F.     |
| 13223073 | Fahrian Maulana F. N. |
| 13223096 | Ramadhan Abhinawa H.  |

---

## ✨ Fitur Utama

* Monitoring kondisi ruangan secara real-time
* Deteksi api menggunakan sensor KY-026
* Deteksi gas/asap menggunakan sensor MQ-2
* Monitoring suhu menggunakan sensor DS18B20
* Komunikasi MQTT melalui TLS/SSL (port 8883)
* Sistem klasifikasi tingkat bahaya (Level 0–3)
* Aktuator lokal berupa buzzer dan LED indikator
* Kendali perangkat jarak jauh melalui MQTT
* Integrasi backend Node.js dan Firebase Cloud Messaging (FCM)

---

## 🏗 Arsitektur Sistem

```text
┌──────────────────────────────────┐
│            ESP32-S2              │
│                                  │
│  KY-026   MQ-2    DS18B20        │
│    │        │         │          │
│    └────────┴─────────┘          │
│       Logika Deteksi Bahaya      │
└───────────────┬──────────────────┘
                │ MQTT over TLS
                ▼
┌──────────────────────────────────┐
│          HiveMQ Cloud            │
│                                  │
│ gedung/lantai1/ruang101/sensor   │
│ gedung/lantai1/ruang101/aktuator │
└───────────────┬──────────────────┘
                │
                ▼
┌──────────────────────────────────┐
│         Backend Node.js          │
│                                  │
│ - Monitoring                     │
│ - Logging                        │
│ - Notifikasi FCM                 │
└───────────────┬──────────────────┘
                │
                ▼
┌──────────────────────────────────┐
│      Website / Mobile User       │
└──────────────────────────────────┘
```

---

## 🔧 Komponen Hardware

| Komponen            | Fungsi                   |
| ------------------- | ------------------------ |
| ESP32-S2            | Mikrokontroler utama     |
| KY-026 Flame Sensor | Deteksi keberadaan api   |
| MQ-2 Gas Sensor     | Deteksi gas dan asap     |
| DS18B20             | Pengukuran suhu          |
| Buzzer              | Alarm lokal              |
| LED Hijau           | Indikator kondisi normal |
| LED Merah           | Indikator kondisi bahaya |

---

## 🔌 Konfigurasi Pin

| Perangkat             | GPIO    |
| --------------------- | ------- |
| MQ-2 Analog Output    | GPIO 34 |
| MQ-2 Digital Output   | GPIO 35 |
| KY-026 Digital Output | GPIO 33 |
| DS18B20               | GPIO 4  |
| Buzzer                | GPIO 26 |
| LED Merah             | GPIO 27 |
| LED Hijau             | GPIO 14 |

---

## 📡 MQTT Topics

### Publish

```text
gedung/lantai1/ruang101/sensor
```

Digunakan untuk mengirim data sensor dan status bahaya dari ESP32 ke server.

### Subscribe

```text
gedung/lantai1/ruang101/aktuator
```

Digunakan untuk menerima perintah dari backend ke perangkat.

---

## 📦 Payload Sensor

ESP32 mengirimkan data setiap 5 detik.

Contoh payload:

```json
{
  "lantai": 1,
  "ruang": "R101",
  "level": 2,
  "gas": 2841,
  "api": true,
  "suhu": 68.5,
  "status": "BAHAYA"
}
```

### Penjelasan Field

| Field  | Tipe    | Keterangan              |
| ------ | ------- | ----------------------- |
| lantai | Integer | Nomor lantai            |
| ruang  | String  | Identitas ruangan       |
| level  | Integer | Tingkat bahaya          |
| gas    | Integer | Nilai ADC MQ-2 (0–4095) |
| api    | Boolean | Status deteksi api      |
| suhu   | Float   | Suhu dalam °C           |
| status | String  | Status kondisi ruangan  |

---

## 🚨 Klasifikasi Tingkat Bahaya

### Level 0 — AMAN

Tidak ada sensor yang mendeteksi kondisi berbahaya.

```json
{
  "level": 0,
  "status": "AMAN"
}
```

---

### Level 1 — WASPADA

Salah satu kondisi berikut terdeteksi:

* Gas tinggi
* Api terdeteksi
* Suhu melebihi ambang batas

```json
{
  "level": 1,
  "status": "WASPADA"
}
```

---

### Level 2 — BAHAYA

Gas tinggi dan api terdeteksi secara bersamaan.

```json
{
  "level": 2,
  "status": "BAHAYA"
}
```

---

### Level 3 — DARURAT

Gas tinggi, api terdeteksi, dan suhu tinggi secara bersamaan.

```json
{
  "level": 3,
  "status": "DARURAT"
}
```

---

## ⚙️ Threshold Sensor

Nilai ambang yang digunakan pada firmware:

```cpp
#define MQ2_THRESHOLD   2000
#define SUHU_THRESHOLD  50.0f
```

### MQ-2

Kondisi gas dianggap tinggi jika:

```cpp
mq2_ao > 2000
```

atau output digital MQ-2 aktif.

### DS18B20

Kondisi suhu dianggap tinggi jika:

```cpp
suhu > 50°C
```

---

## 🔊 Aktuator Lokal

### LED Hijau

Menunjukkan kondisi normal.

### LED Merah

Menyala saat Level 1–3.

### Buzzer

| Level | Pola                      |
| ----- | ------------------------- |
| 0     | Mati                      |
| 1     | Berkedip lambat (1 detik) |
| 2     | Berkedip cepat (300 ms)   |
| 3     | Menyala terus             |

---

## 🎛 MQTT Control Command

Backend dapat mengirim perintah ke:

```text
gedung/lantai1/ruang101/aktuator
```

### Mengaktifkan Override Buzzer

```json
{
  "buzzer": 1
}
```

### Menonaktifkan Override Buzzer

```json
{
  "buzzer": 0
}
```

### Reset Sistem

```json
{
  "reset": 1
}
```

---

## 🔐 Konfigurasi MQTT

| Parameter  | Nilai        |
| ---------- | ------------ |
| Protocol   | MQTT v3.1.1  |
| Security   | TLS/SSL      |
| Port       | 8883         |
| Keep Alive | 60 detik     |
| Broker     | HiveMQ Cloud |

---

## 🚀 Alur Kerja Sistem

1. ESP32 membaca sensor MQ-2, KY-026, dan DS18B20.
2. Sistem menentukan level bahaya berdasarkan kondisi sensor.
3. Aktuator lokal (LED dan buzzer) diperbarui sesuai level.
4. Data sensor dikirim ke broker MQTT setiap 5 detik.
5. Backend menerima dan memproses data.
6. Backend mengirimkan notifikasi ke pengguna menggunakan Firebase Cloud Messaging.
7. Backend dapat mengirim perintah kembali ke perangkat melalui topic aktuator.

---

## 📚 Mata Kuliah

**EL4044 – Perancangan Sistem IoT**

Institut Teknologi Bandung

Semester II 2025/2026
