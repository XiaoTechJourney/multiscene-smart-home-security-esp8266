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
static const char MODULE_NAME[] = "broadcast-node";

// ---------- Pins ----------
static const uint8_t PIN_WARNING_LIGHT = D1;
static const uint8_t WARNING_LIGHT_ACTIVE_LEVEL = HIGH;
static const uint8_t PIN_TTS_RX = D5;
static const uint8_t PIN_TTS_TX = D6;

// ---------- Fire thresholds ----------
static const float FIRE_PREALARM_THRESHOLD = 40.0f;
static const float FIRE_ALARM_THRESHOLD = 60.0f;
static const unsigned long ALERT_CONFIRM_TIMEOUT_MS = 120000UL;

// ---------- Push notification ----------
static const char GOTIFY_BASE_URL[] = "https://gotify.example.com";
static const char GOTIFY_TOKEN[] = "YOUR_GOTIFY_TOKEN";

// ---------- Weather API ----------
static const char WEATHER_API_URL[] = "http://apis.juhe.cn/simpleWeather/query?city=%E5%A4%A7%E8%BF%9E&key=YOUR_JUHE_API_KEY";

}
