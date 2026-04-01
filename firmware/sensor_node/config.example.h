#pragma once
#include <Arduino.h>

namespace Config {

// ---------- Wi-Fi ----------
static const char WIFI_SSID_1[] = "YOUR_WIFI_SSID_1";
static const char WIFI_PASSWORD_1[] = "YOUR_WIFI_PASSWORD_1";
static const char WIFI_SSID_2[] = "";
static const char WIFI_PASSWORD_2[] = "";
static const char WIFI_SSID_3[] = "";
static const char WIFI_PASSWORD_3[] = "";

// ---------- MQTT ----------
static const char MQTT_HOST[] = "broker.example.com";
static const uint16_t MQTT_PORT = 1883;
static const char MQTT_USERNAME[] = "";
static const char MQTT_PASSWORD[] = "";
static const char MODULE_NAME[] = "sensor-node";

// ---------- Pins ----------
static const uint8_t PIN_DHT = D1;
static const uint8_t PIN_PIR = D5;
static const uint8_t PIN_REED = D6;
static const uint8_t PIN_MQ2 = A0;

static const uint8_t PIR_ACTIVE_LEVEL = HIGH;
static const uint8_t REED_OPEN_LEVEL = HIGH;

// ---------- Timing ----------
static const unsigned long SENSOR_SAMPLE_MS = 5000UL;
static const unsigned long TELEMETRY_MS = 15000UL;
static const unsigned long INTRUSION_DEBOUNCE_MS = 5000UL;

// ---------- Fire logic ----------
static const float FIRE_PREALARM_THRESHOLD = 40.0f;
static const float FIRE_ALARM_THRESHOLD = 60.0f;
static const float MQ2_MAX_SCALE = 5.0f;

}
