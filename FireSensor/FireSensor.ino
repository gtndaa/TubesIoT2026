#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <ArduinoJson.h>
#include "config.h"

// ── Objek ────────────────────────────────────────────────────
OneWire           oneWire(PIN_DS18B20);
DallasTemperature ds18b20(&oneWire);
WiFiClientSecure  wifiClient;
PubSubClient      mqtt(wifiClient);

// ── State ────────────────────────────────────────────────────
int           level          = 0;
bool          buzzer_override = false;
unsigned long lastKirim      = 0;
unsigned long lastBlink      = 0;
bool          blinkState     = false;

// ════════════════════════════════════════════════════════════
//  AKTUATOR (Normal mode berdasarkan level sensor)
// ════════════════════════════════════════════════════════════
void setAktuator(int lvl) {
    if (buzzer_override) return; // Jika override aktif, jangan ubah apapun

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

// ════════════════════════════════════════════════════════════
//  MQTT CALLBACK (Menangani perintah dari server)
// ════════════════════════════════════════════════════════════
void mqttCallback(char* topic, byte* payload, unsigned int length) {
    String msg = "";
    for (unsigned int i = 0; i < length; i++) msg += (char)payload[i];

    StaticJsonDocument<256> doc;
    if (deserializeJson(doc, msg) != DeserializationError::Ok) return;

    // Perintah buzzer (override)
    if (doc.containsKey("buzzer")) {
        int buz = doc["buzzer"];
        buzzer_override = (buz == 1);
        
        if (buzzer_override) {
            // Override aktif: LED hijau tetap menyala (indikasi peringatan dari ruang lain)
            digitalWrite(PIN_LED_HIJAU, HIGH);
            digitalWrite(PIN_LED_MERAH, HIGH);
            digitalWrite(PIN_BUZZER,    HIGH);
        } else {
            // Matikan override, kembalikan ke mode normal berdasarkan level sensor
            digitalWrite(PIN_BUZZER, LOW);
            digitalWrite(PIN_LED_MERAH, LOW);
            setAktuator(level); // LED hijau akan diatur ulang oleh fungsi ini
        }
    }

    // Perintah reset
    if (doc.containsKey("reset") && (int)doc["reset"] == 1) {
        buzzer_override = false;
        level = 0;
        digitalWrite(PIN_BUZZER,    LOW);
        digitalWrite(PIN_LED_MERAH, LOW);
        digitalWrite(PIN_LED_HIJAU, HIGH);
        lastBlink = 0;
        blinkState = false;
    }
}

// ════════════════════════════════════════════════════════════
//  KONEKSI WiFi
// ════════════════════════════════════════════════════════════
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

// ════════════════════════════════════════════════════════════
//  KONEKSI MQTT
// ════════════════════════════════════════════════════════════
void connectMQTT() {
    wifiClient.setInsecure(); // Untuk HiveMQ dengan TLS
    mqtt.setServer(MQTT_HOST, MQTT_PORT);
    mqtt.setCallback(mqttCallback);
    mqtt.setKeepAlive(60);

    Serial.print("Connecting MQTT...");

    for (int i = 0; i < 5; i++) {
        if (mqtt.connect(MQTT_CLIENT, MQTT_USER, MQTT_PASS)) {
            Serial.println("MQTT OK!");
            mqtt.subscribe(TOPIC_AKTUATOR);
            return;
        }
        Serial.printf("Gagal rc=%d, retry %d/5\n", mqtt.state(), i + 1);
        delay(2000);
    }
    Serial.println("MQTT GAGAL!");
}

// ════════════════════════════════════════════════════════════
//  SETUP
// ════════════════════════════════════════════════════════════
void setup() {
    Serial.begin(115200);

    pinMode(PIN_MQ2_DO,    INPUT);
    pinMode(PIN_KY026_DO,  INPUT);
    pinMode(PIN_BUZZER,    OUTPUT);
    pinMode(PIN_LED_MERAH, OUTPUT);
    pinMode(PIN_LED_HIJAU, OUTPUT);

    // Kondisi awal: matikan buzzer & LED merah, nyalakan LED hijau
    digitalWrite(PIN_BUZZER,    LOW);
    digitalWrite(PIN_LED_MERAH, LOW);
    digitalWrite(PIN_LED_HIJAU, HIGH);

    ds18b20.begin();

    connectWiFi();
    connectMQTT();

    Serial.println("System ready!");
}

// ════════════════════════════════════════════════════════════
//  LOOP UTAMA
// ════════════════════════════════════════════════════════════
void loop() {
    // Reconnect MQTT jika putus
    if (!mqtt.connected() && WiFi.status() == WL_CONNECTED) {
        static unsigned long lastReconnect = 0;
        if (millis() - lastReconnect > 5000) {
            lastReconnect = millis();
            if (mqtt.connect(MQTT_CLIENT, MQTT_USER, MQTT_PASS)) {
                mqtt.subscribe(TOPIC_AKTUATOR);
                Serial.println("MQTT Reconnected!");
            }
        }
    }
    mqtt.loop();

    // ── Baca sensor ──────────────────────────────────────────
    int  mq2_ao = analogRead(PIN_MQ2_AO);
    bool mq2_do = !digitalRead(PIN_MQ2_DO);
    bool ky_do  = !digitalRead(PIN_KY026_DO);

    ds18b20.requestTemperatures();
    float suhu = ds18b20.getTempCByIndex(0);
    if (suhu == DEVICE_DISCONNECTED) suhu = 0.0f;

    // ── Logika level bahaya ─────────────────────────────────
    bool gas_tinggi  = (mq2_ao > MQ2_THRESHOLD) || mq2_do;
    bool api_aktif   = (ky_do);
    bool suhu_tinggi = (suhu > SUHU_THRESHOLD);

    if      (gas_tinggi && api_aktif && suhu_tinggi) level = 3;
    else if (gas_tinggi && api_aktif)                level = 2;
    else if (gas_tinggi || api_aktif || suhu_tinggi) level = 1;
    else                                             level = 0;

    // ── Aktuator (hanya jika tidak dalam override) ───────────
    if (!buzzer_override) {
        setAktuator(level);
    }

    // ── Serial debug ─────────────────────────────────────────
    Serial.printf("MQ2_AO: %d | MQ2_DO: %d | KY026: %d | Suhu: %.1f°C | Level: %d | Override: %d\n",
        mq2_ao, mq2_do, api_aktif, suhu, level, buzzer_override);

    // ── Kirim data sensor ke server MQTT tiap 5 detik ────────
    if (millis() - lastKirim >= INTERVAL_KIRIM) {
        lastKirim = millis();

        StaticJsonDocument<256> doc;
        doc["lantai"] = LANTAI;
        doc["ruang"]  = RUANG;
        doc["level"]  = level;
        doc["gas"]    = mq2_ao;
        doc["api"]    = api_aktif;
        doc["suhu"]   = suhu;
        doc["status"] = level == 0 ? "AMAN"    :
                        level == 1 ? "WASPADA" :
                        level == 2 ? "BAHAYA"  : "DARURAT";

        char buf[256];
        serializeJson(doc, buf);

        if (mqtt.connected()) {
            mqtt.publish(TOPIC_SENSOR, buf);
            Serial.println("Published: " + String(buf));
        } else {
            Serial.println("MQTT tidak konek, skip kirim");
        }
    }

    delay(100);
}