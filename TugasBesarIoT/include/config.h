#pragma once

// ── WiFi ────────────────────────────────────────────────────
#define WIFI_SSID     "NAMA_WIFI_KAMU"
#define WIFI_PASS     "PASSWORD_WIFI_KAMU"

// ── HiveMQ Cloud ────────────────────────────────────────────
#define MQTT_HOST     "XXXXX.s1.eu.hivemq.cloud"
#define MQTT_PORT     8883
#define MQTT_USER     "USERNAME_HIVEMQ"
#define MQTT_PASS     "PASSWORD_HIVEMQ"
#define MQTT_CLIENT   "FireSensor_Lt1_R101"

// ── MQTT Topics ─────────────────────────────────────────────
#define TOPIC_SENSOR    "gedung/lantai1/ruang101/sensor"
#define TOPIC_AKTUATOR  "gedung/lantai1/ruang101/aktuator"

// ── Pin ─────────────────────────────────────────────────────
#define PIN_MQ2_AO      34
#define PIN_MQ2_DO      35
#define PIN_KY026_AO    32
#define PIN_KY026_DO    33
#define PIN_DS18B20     4
#define PIN_BUZZER      26
#define PIN_LED_MERAH   27
#define PIN_LED_HIJAU   14

// ── Threshold ───────────────────────────────────────────────
#define MQ2_THRESHOLD   2000
#define SUHU_THRESHOLD  50.0f
#define INTERVAL_KIRIM  5000

// ── Identitas ruangan ───────────────────────────────────────
#define LANTAI          1
#define RUANG           "R101"