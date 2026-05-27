#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <TFT_eSPI.h>
#include <ArduinoJson.h>
#include "config.h"

// ── Objek ────────────────────────────────────────────────────
OneWire           oneWire(PIN_DS18B20);
DallasTemperature ds18b20(&oneWire);
TFT_eSPI          tft = TFT_eSPI();
WiFiClientSecure  wifiClient;
PubSubClient      mqtt(wifiClient);

// ── State ────────────────────────────────────────────────────
int           level          = 0;
bool          buzzer_override = false;
unsigned long lastKirim      = 0;
unsigned long lastBlink      = 0;
bool          blinkState     = false;

// ── Warna ────────────────────────────────────────────────────
#define CLR_BG      TFT_BLACK
#define CLR_HIJAU   0x07E0
#define CLR_KUNING  0xFFE0
#define CLR_MERAH   0xF800
#define CLR_PUTIH   TFT_WHITE
#define CLR_ABU     0x8410
#define CLR_NAVY    0x000F

// ════════════════════════════════════════════════════════════
//  DISPLAY FUNCTIONS
// ════════════════════════════════════════════════════════════

void displayHeader() {
    tft.fillRect(0, 0, 240, 38, 0x1082);
    tft.setTextColor(CLR_PUTIH, 0x1082);
    tft.setTextSize(2);
    tft.setCursor(8, 8);
    tft.print("FIRE DETECTOR");
    tft.setTextSize(1);
    tft.setCursor(8, 26);
    tft.printf("Lt.%d - %s", LANTAI, RUANG);
}

void displayStatus(int lvl) {
    uint16_t warna;
    const char* label;
    if      (lvl == 0) { warna = CLR_HIJAU;  label = "   AMAN   "; }
    else if (lvl == 1) { warna = CLR_KUNING; label = " WASPADA  "; }
    else if (lvl == 2) { warna = CLR_MERAH;  label = "  BAHAYA  "; }
    else               { warna = CLR_MERAH;  label = " DARURAT! "; }

    tft.fillRect(0, 38, 240, 44, warna);
    tft.setTextColor(CLR_BG, warna);
    tft.setTextSize(3);
    tft.setCursor(10, 50);
    tft.print(label);
}

void displaySensor(float suhu, int gas, bool api, bool mqttOk) {
    tft.fillRect(0, 85, 240, 120, CLR_BG);

    // Label
    tft.setTextSize(1);
    tft.setTextColor(CLR_ABU, CLR_BG);
    tft.setCursor(8, 90);
    tft.print("--- SENSOR READINGS ---");

    // Suhu
    tft.setTextSize(2);
    tft.setTextColor(suhu > SUHU_THRESHOLD ? CLR_MERAH : CLR_PUTIH, CLR_BG);
    tft.setCursor(8, 108);
    tft.printf("Suhu : %.1f C  ", suhu);

    // Gas
    tft.setTextColor(gas > MQ2_THRESHOLD ? CLR_MERAH : CLR_PUTIH, CLR_BG);
    tft.setCursor(8, 134);
    tft.printf("Gas  : %4d   ", gas);

    // Api
    tft.setTextColor(api ? CLR_MERAH : CLR_HIJAU, CLR_BG);
    tft.setCursor(8, 160);
    tft.printf("Api  : %s", api ? "TERDETEKSI" : "Aman      ");

    // MQTT status
    tft.setTextSize(1);
    tft.setTextColor(CLR_ABU, CLR_BG);
    tft.setCursor(8, 192);
    tft.print("MQTT : ");
    tft.setTextColor(mqttOk ? CLR_HIJAU : CLR_MERAH, CLR_BG);
    tft.print(mqttOk ? "Connected    " : "Disconnected ");
}

void displayLevelBar(int lvl) {
    tft.fillRect(0, 205, 240, 35, CLR_BG);
    tft.setTextSize(1);
    tft.setTextColor(CLR_ABU, CLR_BG);
    tft.setCursor(8, 208);
    tft.print("LEVEL:");

    const char* labels[] = {"L1", "L2", "L3"};
    uint16_t    colors[] = {CLR_KUNING, 0xFC00, CLR_MERAH};
    for (int i = 0; i < 3; i++) {
        uint16_t bg = (i < lvl) ? colors[i] : 0x2104;
        tft.fillRect(60 + i * 60, 204, 50, 16, bg);
        tft.setTextColor(CLR_PUTIH, bg);
        tft.setCursor(78 + i * 60, 208);
        tft.print(labels[i]);
    }

    // Timestamp (uptime)
    tft.setTextColor(CLR_ABU, CLR_BG);
    tft.setCursor(8, 228);
    tft.printf("Uptime: %lus", millis() / 1000);
}

// ════════════════════════════════════════════════════════════
//  AKTUATOR
// ════════════════════════════════════════════════════════════

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
            // Buzzer blink tiap 1 detik
            if (millis() - lastBlink > 1000) {
                lastBlink  = millis();
                blinkState = !blinkState;
                digitalWrite(PIN_BUZZER, blinkState);
            }
            break;

        case 2:
            digitalWrite(PIN_LED_HIJAU, LOW);
            digitalWrite(PIN_LED_MERAH, HIGH);
            // Buzzer blink cepat tiap 300ms
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
//  MQTT CALLBACK
// ════════════════════════════════════════════════════════════

void mqttCallback(char* topic, byte* payload, unsigned int length) {
    String msg = "";
    for (unsigned int i = 0; i < length; i++) msg += (char)payload[i];

    StaticJsonDocument<256> doc;
    if (deserializeJson(doc, msg) != DeserializationError::Ok) return;

    // {"buzzer": 1}  → paksa buzzer nyala
    // {"buzzer": 0}  → matikan buzzer override
    // {"reset": 1}   → reset semua
    if (doc.containsKey("buzzer")) {
        int buz = doc["buzzer"];
        buzzer_override = (buz == 1);
        digitalWrite(PIN_BUZZER, buz ? HIGH : LOW);
    }
    if (doc.containsKey("reset") && (int)doc["reset"] == 1) {
        buzzer_override = false;
        level = 0;
    }
}

// ════════════════════════════════════════════════════════════
//  KONEKSI
// ════════════════════════════════════════════════════════════

void connectWiFi() {
    tft.fillScreen(CLR_BG);
    tft.setTextColor(CLR_PUTIH, CLR_BG);
    tft.setTextSize(1);
    tft.setCursor(8, 8);
    tft.print("Connecting WiFi...");
    tft.setCursor(8, 22);
    tft.print(WIFI_SSID);

    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);

    int tries = 0;
    while (WiFi.status() != WL_CONNECTED && tries < 40) {
        delay(500);
        tft.setCursor(8 + tries * 4, 40);
        tft.print(".");
        tries++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        tft.setCursor(8, 60);
        tft.setTextColor(CLR_HIJAU, CLR_BG);
        tft.print("WiFi OK! IP:");
        tft.setCursor(8, 75);
        tft.setTextColor(CLR_PUTIH, CLR_BG);
        tft.print(WiFi.localIP());
    } else {
        tft.setCursor(8, 60);
        tft.setTextColor(CLR_MERAH, CLR_BG);
        tft.print("WiFi GAGAL - cek SSID/PASS");
    }
    delay(1500);
}

void connectMQTT() {
    wifiClient.setInsecure();
    mqtt.setServer(MQTT_HOST, MQTT_PORT);
    mqtt.setCallback(mqttCallback);
    mqtt.setKeepAlive(60);

    tft.fillScreen(CLR_BG);
    tft.setTextColor(CLR_PUTIH, CLR_BG);
    tft.setTextSize(1);
    tft.setCursor(8, 8);
    tft.print("Connecting MQTT...");
    tft.setCursor(8, 22);
    tft.print(MQTT_HOST);

    for (int i = 0; i < 5; i++) {
        tft.setCursor(8, 45 + i * 14);
        if (mqtt.connect(MQTT_CLIENT, MQTT_USER, MQTT_PASS)) {
            tft.setTextColor(CLR_HIJAU, CLR_BG);
            tft.print("MQTT OK!");
            mqtt.subscribe(TOPIC_AKTUATOR);
            break;
        } else {
            tft.setTextColor(CLR_MERAH, CLR_BG);
            tft.printf("Gagal (rc=%d) retry %d/5", mqtt.state(), i+1);
            delay(2000);
        }
    }
    delay(1000);
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

    digitalWrite(PIN_BUZZER,    LOW);
    digitalWrite(PIN_LED_MERAH, LOW);
    digitalWrite(PIN_LED_HIJAU, HIGH);

    tft.init();
    tft.setRotation(0);
    tft.fillScreen(CLR_BG);

    ds18b20.begin();

    connectWiFi();
    connectMQTT();

    tft.fillScreen(CLR_BG);
    displayHeader();
}

// ════════════════════════════════════════════════════════════
//  LOOP
// ════════════════════════════════════════════════════════════

void loop() {
    // Reconnect MQTT kalau putus
    if (!mqtt.connected() && WiFi.status() == WL_CONNECTED) {
        static unsigned long lastReconnect = 0;
        if (millis() - lastReconnect > 5000) {
            lastReconnect = millis();
            if (mqtt.connect(MQTT_CLIENT, MQTT_USER, MQTT_PASS)) {
                mqtt.subscribe(TOPIC_AKTUATOR);
            }
        }
    }
    mqtt.loop();

    // ── Baca sensor ─────────────────────────────────────────
    int   mq2_ao = analogRead(PIN_MQ2_AO);
    bool  mq2_do = !digitalRead(PIN_MQ2_DO);
    bool  ky_do  = !digitalRead(PIN_KY026_DO);

    ds18b20.requestTemperatures();
    float suhu = ds18b20.getTempCByIndex(0);
    if (suhu == DEVICE_DISCONNECTED_C) suhu = 0.0f;

    // ── Logika level ─────────────────────────────────────────
    bool gas_tinggi  = (mq2_ao > MQ2_THRESHOLD) || mq2_do;
    bool api_aktif   = ky_do;
    bool suhu_tinggi = (suhu > SUHU_THRESHOLD);

    if      (gas_tinggi && api_aktif && suhu_tinggi) level = 3;
    else if (gas_tinggi && api_aktif)                level = 2;
    else if (gas_tinggi || api_aktif || suhu_tinggi) level = 1;
    else                                             level = 0;

    // ── Aktuator ─────────────────────────────────────────────
    setAktuator(level);

    // ── Update display ───────────────────────────────────────
    displayHeader();
    displayStatus(level);
    displaySensor(suhu, mq2_ao, api_aktif, mqtt.connected());
    displayLevelBar(level);

    // ── Kirim MQTT ───────────────────────────────────────────
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
        }
        Serial.println(buf);
    }

    delay(100);
}