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
static const char MODULE_NAME[] = "control-node";

// ---------- Pins ----------
static const uint8_t PIN_RF_TX = D1;
static const uint8_t PIN_SERVO = D2;
static const uint8_t PIN_RELAY = D4;

// ---------- Relay ----------
static const uint8_t RELAY_ACTIVE_LEVEL = LOW;   // 常见继电器模块多为低电平触发

// ---------- Servo ----------
static const int SERVO_OPEN_ANGLE = 0;
static const int SERVO_CLOSE_ANGLE = 90;

// ---------- 433 ----------
static const char RF_CODE_OPEN_1[] = "00000FFF0F0F";
static const char RF_CODE_OPEN_2[] = "00000FFF0FF0";
static const char RF_CODE_CLOSE_1[] = "";
static const char RF_CODE_CLOSE_2[] = "";
static const int RF_REPEAT = 3;

// ---------- Fire thresholds ----------
static const float FIRE_ALARM_THRESHOLD = 60.0f;

}
