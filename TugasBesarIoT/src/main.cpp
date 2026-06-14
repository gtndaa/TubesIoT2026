#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <ArduinoJson.h>
#include "config.h"

// ========== KONFIGURASI PERANGKAT ==========
// Pastikan deviceId unik sesuai daftar di server
#ifndef DEVICE_ID
#define DEVICE_ID "ESP32-FIRESYS-01"   // Ganti sesuai perangkat (01-06)
#endif

// Topik MQTT
#define TOPIC_SENSOR_FLAME   "firedetect/sensor/flame"
#define TOPIC_SENSOR_GAS     "firedetect/sensor/gas"
#define TOPIC_SENSOR_TEMP    "firedetect/sensor/temperature"
#define TOPIC_ALERT_EVENT    "firedetect/alert/event"
#define TOPIC_CMD            "firedetect/device/" DEVICE_ID "/cmd"

// Threshold
#define MQ2_THRESHOLD       500      // Analog threshold untuk gas
#define SUHU_THRESHOLD      60.0f    // °C
#define INTENSITY_LOW       400
#define INTENSITY_MEDIUM    700

// Objek
OneWire           oneWire(PIN_DS18B20);
DallasTemperature ds18b20(&oneWire);
WiFiClientSecure  wifiClient;
PubSubClient      mqtt(wifiClient);

// State
int           level          = 0;
bool          buzzer_override = false;
unsigned long lastKirim      = 0;
unsigned long lastBlink      = 0;
bool          blinkState     = false;
int           lastAlertLevel = -1;

// ========== AKTUATOR ==========
void setAktuator(int lvl) {
    if (buzzer_override) return;

    switch (lvl) {
        case 0:
            digitalWrite(PIN_BUZZER,    LOW);
            digitalWrite(PIN_LED_MERAH, LOW);
            digitalWrite(PIN_LED_HIJAU, HIGH);
            break;
        case 1:
            digitalWrite(PIN_LED_HIJAU, LOW);
            digitalWrite(PIN_LED_MERAH, HIGH);
            if (millis() - lastBlink > 1000) {
                lastBlink  = millis();
                blinkState = !blinkState;
                digitalWrite(PIN_BUZZER, blinkState);
            }
            break;
        case 2:
            digitalWrite(PIN_LED_HIJAU, LOW);
            digitalWrite(PIN_LED_MERAH, HIGH);
            if (millis() - lastBlink > 300) {
                lastBlink  = millis();
                blinkState = !blinkState;
                digitalWrite(PIN_BUZZER, blinkState);
            }
            break;
        case 3:
            digitalWrite(PIN_LED_HIJAU, LOW);
            digitalWrite(PIN_LED_MERAH, HIGH);
            digitalWrite(PIN_BUZZER,    HIGH);
            break;
    }
}

// ========== MQTT CALLBACK (menerima perintah) ==========
void mqttCallback(char* topic, byte* payload, unsigned int length) {
    String msg = "";
    for (unsigned int i = 0; i < length; i++) msg += (char)payload[i];

    StaticJsonDocument<128> doc;
    if (deserializeJson(doc, msg) != DeserializationError::Ok) return;

    if (doc.containsKey("buzzer")) {
        buzzer_override = (doc["buzzer"] == 1);
        digitalWrite(PIN_BUZZER, buzzer_override ? HIGH : LOW);
        Serial.printf("Buzzer override: %d\n", buzzer_override);
    }
    if (doc.containsKey("reset") && doc["reset"] == 1) {
        buzzer_override = false;
        level = 0;
        digitalWrite(PIN_BUZZER,    LOW);
        digitalWrite(PIN_LED_MERAH, LOW);
        digitalWrite(PIN_LED_HIJAU, HIGH);
        Serial.println("System reset by server");
    }
}

// ========== KONEKSI WiFi ==========
void connectWiFi() {
    Serial.print("Connecting WiFi: ");
    Serial.println(WIFI_SSID);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);

    int tries = 0;
    while (WiFi.status() != WL_CONNECTED && tries < 40) {
        delay(500);
        Serial.print(".");
        tries++;
    }
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nWiFi OK! IP: " + WiFi.localIP().toString());
    } else {
        Serial.println("\nWiFi GAGAL!");
    }
}

// ========== KONEKSI MQTT ==========
void connectMQTT() {
    wifiClient.setInsecure();   // Untuk broker tanpa sertifikat (Mosquitto lokal)
    mqtt.setServer(MQTT_HOST, MQTT_PORT);
    mqtt.setCallback(mqttCallback);
    mqtt.setKeepAlive(60);

    Serial.print("Connecting MQTT...");
    for (int i = 0; i < 5; i++) {
        if (mqtt.connect(DEVICE_ID, MQTT_USER, MQTT_PASS)) {
            Serial.println("MQTT OK!");
            mqtt.subscribe(TOPIC_CMD);
            return;
        }
        Serial.printf("Gagal rc=%d, retry %d/5\n", mqtt.state(), i + 1);
        delay(2000);
    }
    Serial.println("MQTT GAGAL!");
}

// ========== KIRIM DATA SENSOR ==========
void publishSensorData(int mq2_ao, bool api_aktif, float suhu) {
    StaticJsonDocument<192> doc;
    doc["device_id"] = DEVICE_ID;
    doc["timestamp"] = millis();

    // 1. Flame sensor
    bool flame_detected = api_aktif;
    String intensity = "LOW";
    if (api_aktif) {
        // Estimasi intensitas berdasarkan analog (jika ada) atau dummy
        int analogFlame = analogRead(PIN_KY026_AO); // jika pin AO tersedia
        if (analogFlame > INTENSITY_MEDIUM) intensity = "HIGH";
        else if (analogFlame > INTENSITY_LOW) intensity = "MEDIUM";
        else intensity = "LOW";
    }
    JsonObject flame = doc.createNestedObject("flame");
    flame["detected"] = flame_detected;
    flame["intensity"] = intensity;
    flame["analog"] = flame_detected ? analogRead(PIN_KY026_AO) : 0;

    // 2. Gas sensor
    bool gas_detected = (mq2_ao > MQ2_THRESHOLD);
    JsonObject gas = doc.createNestedObject("gas");
    gas["detected"] = gas_detected;
    gas["ppm"] = mq2_ao;  // konversi sederhana, bisa disesuaikan
    gas["gasType"] = gas_detected ? "SMOKE" : "NONE";

    // Kirim ke topik masing-masing
    char buffer[256];
    serializeJson(doc, buffer);

    if (mqtt.connected()) {
        // Kirim flame
        JsonObject onlyFlame = doc["flame"];
        char flameBuf[128];
        serializeJson(onlyFlame, flameBuf);
        mqtt.publish(TOPIC_SENSOR_FLAME, flameBuf);

        // Kirim gas
        JsonObject onlyGas = doc["gas"];
        char gasBuf[128];
        serializeJson(onlyGas, gasBuf);
        mqtt.publish(TOPIC_SENSOR_GAS, gasBuf);
    }
}

void publishTemperature(float suhu) {
    StaticJsonDocument<128> doc;
    doc["device_id"] = DEVICE_ID;
    doc["timestamp"] = millis();
    doc["temperature_c"] = suhu;
    doc["over_threshold"] = (suhu > SUHU_THRESHOLD);
    doc["threshold_c"] = SUHU_THRESHOLD;

    char buf[128];
    serializeJson(doc, buf);
    if (mqtt.connected()) {
        mqtt.publish(TOPIC_SENSOR_TEMP, buf);
    }
}

void publishAlert(int newLevel) {
    StaticJsonDocument<256> doc;
    doc["device_id"] = DEVICE_ID;
    doc["alert_level"] = newLevel;
    doc["timestamp"] = millis();
    doc["alert_label"] = (newLevel == 1) ? "WASPADA" :
                         (newLevel == 2) ? "BAHAYA" : "DARURAT";
    doc["evacuation_route"] = (newLevel == 3) ? "Gunakan tangga darurat keluar gedung" : "Pantau terus kondisi";
    
    char buf[256];
    serializeJson(doc, buf);
    if (mqtt.connected()) {
        mqtt.publish(TOPIC_ALERT_EVENT, buf);
        Serial.printf("Alert event: level %d\n", newLevel);
    }
}

// ========== SETUP ==========
void setup() {
    Serial.begin(115200);
    pinMode(PIN_MQ2_DO,    INPUT);
    pinMode(PIN_KY026_DO,  INPUT);
    pinMode(PIN_KY026_AO,  INPUT);   // jika ada pin analog flame
    pinMode(PIN_BUZZER,    OUTPUT);
    pinMode(PIN_LED_MERAH, OUTPUT);
    pinMode(PIN_LED_HIJAU, OUTPUT);

    digitalWrite(PIN_BUZZER,    LOW);
    digitalWrite(PIN_LED_MERAH, LOW);
    digitalWrite(PIN_LED_HIJAU, HIGH);

    ds18b20.begin();
    connectWiFi();
    connectMQTT();
    Serial.println("System ready!");
}

// ========== LOOP ==========
void loop() {
    if (!mqtt.connected() && WiFi.status() == WL_CONNECTED) {
        static unsigned long lastReconnect = 0;
        if (millis() - lastReconnect > 5000) {
            lastReconnect = millis();
            if (mqtt.connect(DEVICE_ID, MQTT_USER, MQTT_PASS)) {
                mqtt.subscribe(TOPIC_CMD);
                Serial.println("MQTT Reconnected!");
            }
        }
    }
    mqtt.loop();

    // Baca sensor
    int  mq2_ao = analogRead(PIN_MQ2_AO);
    bool mq2_do = !digitalRead(PIN_MQ2_DO);
    bool ky_do  = !digitalRead(PIN_KY026_DO);

    ds18b20.requestTemperatures();
    float suhu = ds18b20.getTempCByIndex(0);
    if (suhu == DEVICE_DISCONNECTED_C) suhu = 0.0f;

    // Logika level (sama seperti sebelumnya)
    bool gas_tinggi  = (mq2_ao > MQ2_THRESHOLD) || mq2_do;
    bool api_aktif   = ky_do;
    bool suhu_tinggi = (suhu > SUHU_THRESHOLD);

    int newLevel = 0;
    if      (gas_tinggi && api_aktif && suhu_tinggi) newLevel = 3;
    else if (gas_tinggi && api_aktif)                newLevel = 2;
    else if (gas_tinggi || api_aktif || suhu_tinggi) newLevel = 1;
    else                                             newLevel = 0;

    if (newLevel != level) {
        level = newLevel;
        if (level != 0) publishAlert(level);
    }

    setAktuator(level);

    // Kirim data periodik setiap 5 detik
    if (millis() - lastKirim >= INTERVAL_KIRIM) {
        lastKirim = millis();
        publishSensorData(mq2_ao, api_aktif, suhu);
        publishTemperature(suhu);
        Serial.printf("MQ2_AO: %d | Gas: %d | Api: %d | Suhu: %.1f | Level: %d\n",
                      mq2_ao, gas_tinggi, api_aktif, suhu, level);
    }
    delay(100);
}