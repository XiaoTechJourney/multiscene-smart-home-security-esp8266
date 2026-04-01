#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <PubSubClient.h>
#include <RCSwitch.h>
#include <Servo.h>

#if __has_include("config.h")
#include "config.h"
#else
#include "config.example.h"
#endif

#include "app_topics.h"

ESP8266WiFiMulti wifiMulti;
ESP8266WebServer webServer(80);
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);
RCSwitch rfSwitch;
Servo gasValveServo;

bool fireEmergencyActive = false;

uint8_t inactiveLevel(uint8_t activeLevel) {
  return activeLevel == HIGH ? LOW : HIGH;
}

void addAccessPointIfConfigured(const char* ssid, const char* password) {
  if (ssid != nullptr && ssid[0] != '\0') {
    wifiMulti.addAP(ssid, password);
  }
}

void configureWiFiNetworks() {
  WiFi.mode(WIFI_STA);
  addAccessPointIfConfigured(Config::WIFI_SSID_1, Config::WIFI_PASSWORD_1);
  addAccessPointIfConfigured(Config::WIFI_SSID_2, Config::WIFI_PASSWORD_2);
  addAccessPointIfConfigured(Config::WIFI_SSID_3, Config::WIFI_PASSWORD_3);
}

void connectWiFi() {
  Serial.print("[WiFi] Connecting");
  while (wifiMulti.run() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.print("[WiFi] Connected to: ");
  Serial.println(WiFi.SSID());
  Serial.print("[WiFi] IP: ");
  Serial.println(WiFi.localIP());
}

bool connectMqtt() {
  if (mqttClient.connected()) {
    return true;
  }

  String clientId = String(Config::MODULE_NAME) + "-" + WiFi.macAddress();
  bool connected = false;

  if (Config::MQTT_USERNAME[0] != '\0') {
    connected = mqttClient.connect(clientId.c_str(), Config::MQTT_USERNAME, Config::MQTT_PASSWORD);
  } else {
    connected = mqttClient.connect(clientId.c_str());
  }

  if (connected) {
    Serial.println("[MQTT] Connected");
    mqttClient.subscribe(Topics::FIRE_EVENT);
    mqttClient.subscribe(Topics::CONTROL_COMMAND);
  } else {
    Serial.print("[MQTT] Connect failed, state=");
    Serial.println(mqttClient.state());
  }

  return connected;
}

template <typename TDoc>
bool publishJson(const char* topic, TDoc& doc) {
  char payload[512];
  const size_t payloadLength = serializeJson(doc, payload, sizeof(payload));
  return mqttClient.publish(topic, payload, payloadLength);
}

void sendControlState(const char* stateText) {
  StaticJsonDocument<192> doc;
  doc["module"] = Config::MODULE_NAME;
  doc["state"] = stateText;
  publishJson(Topics::TELEMETRY_EVENT, doc);
}

void setMainPowerCut(bool cutPower) {
  digitalWrite(Config::PIN_RELAY, cutPower ? Config::RELAY_ACTIVE_LEVEL : inactiveLevel(Config::RELAY_ACTIVE_LEVEL));
}

void openGasValve() {
  gasValveServo.write(Config::SERVO_OPEN_ANGLE);
}

void closeGasValve() {
  gasValveServo.write(Config::SERVO_CLOSE_ANGLE);
}

void sendRfCodeIfPresent(const char* code) {
  if (code != nullptr && code[0] != '\0') {
    rfSwitch.sendTriState(code);
    delay(100);
  }
}

void openShutter() {
  for (int i = 0; i < Config::RF_REPEAT; ++i) {
    sendRfCodeIfPresent(Config::RF_CODE_OPEN_1);
    sendRfCodeIfPresent(Config::RF_CODE_OPEN_2);
  }
}

void closeShutter() {
  for (int i = 0; i < Config::RF_REPEAT; ++i) {
    sendRfCodeIfPresent(Config::RF_CODE_CLOSE_1);
    sendRfCodeIfPresent(Config::RF_CODE_CLOSE_2);
  }
}

void enterFireEmergency(float score) {
  if (fireEmergencyActive) {
    return;
  }

  fireEmergencyActive = true;

  Serial.print("[CONTROL] Enter fire emergency, score=");
  Serial.println(score, 2);

  openShutter();
  closeGasValve();
  setMainPowerCut(true);

  sendControlState("fire_emergency");
}

void resetActuators() {
  Serial.println("[CONTROL] Reset actuators");
  fireEmergencyActive = false;
  openGasValve();
  setMainPowerCut(false);
  closeShutter();
  sendControlState("reset");
}

void handleFireEvent(const String& payload) {
  StaticJsonDocument<384> doc;
  DeserializationError error = deserializeJson(doc, payload);
  if (error) {
    Serial.println("[CONTROL] Fire JSON parse failed");
    return;
  }

  const String severity = doc["severity"] | "normal";
  const float score = doc["score"] | 0.0f;

  if (severity == "emergency" || score >= Config::FIRE_ALARM_THRESHOLD) {
    enterFireEmergency(score);
  }
}

void handleCommand(const String& payload) {
  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, payload);
  if (error) {
    Serial.println("[CONTROL] Command JSON parse failed");
    return;
  }

  const String command = doc["command"] | "";

  if (command == "reset") {
    resetActuators();
  } else if (command == "test_shutter_open") {
    openShutter();
  } else if (command == "test_shutter_close") {
    closeShutter();
  } else if (command == "relay_off") {
    setMainPowerCut(true);
  } else if (command == "relay_on") {
    setMainPowerCut(false);
  } else if (command == "valve_open") {
    openGasValve();
  } else if (command == "valve_close") {
    closeGasValve();
  }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String message;
  message.reserve(length);
  for (unsigned int i = 0; i < length; ++i) {
    message += static_cast<char>(payload[i]);
  }

  Serial.print("[MQTT] Topic=");
  Serial.print(topic);
  Serial.print(" Payload=");
  Serial.println(message);

  if (strcmp(topic, Topics::FIRE_EVENT) == 0) {
    handleFireEvent(message);
  } else if (strcmp(topic, Topics::CONTROL_COMMAND) == 0) {
    handleCommand(message);
  }
}

void sendJsonResponse(JsonDocument& doc, int statusCode = 200) {
  String output;
  serializeJson(doc, output);
  webServer.send(statusCode, "application/json; charset=utf-8", output);
}

void handleStatus() {
  StaticJsonDocument<192> doc;
  doc["module"] = Config::MODULE_NAME;
  doc["fire_emergency_active"] = fireEmergencyActive;
  doc["ip"] = WiFi.localIP().toString();
  sendJsonResponse(doc);
}

void handleReset() {
  resetActuators();

  StaticJsonDocument<128> doc;
  doc["ok"] = true;
  doc["message"] = "actuators reset";
  sendJsonResponse(doc);
}

void handleOpenShutter() {
  openShutter();

  StaticJsonDocument<96> doc;
  doc["ok"] = true;
  doc["message"] = "shutter open signal sent";
  sendJsonResponse(doc);
}

void handleCloseShutter() {
  closeShutter();

  StaticJsonDocument<96> doc;
  doc["ok"] = true;
  doc["message"] = "shutter close signal sent";
  sendJsonResponse(doc);
}

void handleNotFound() {
  StaticJsonDocument<96> doc;
  doc["ok"] = false;
  doc["error"] = "not found";
  sendJsonResponse(doc, 404);
}

void setupWebServer() {
  webServer.on("/status", HTTP_GET, handleStatus);
  webServer.on("/reset", HTTP_GET, handleReset);
  webServer.on("/test/open-shutter", HTTP_GET, handleOpenShutter);
  webServer.on("/test/close-shutter", HTTP_GET, handleCloseShutter);
  webServer.onNotFound(handleNotFound);
  webServer.begin();
}

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("[BOOT] control_node");

  pinMode(Config::PIN_RELAY, OUTPUT);
  setMainPowerCut(false);

  rfSwitch.enableTransmit(Config::PIN_RF_TX);

  gasValveServo.attach(Config::PIN_SERVO);
  openGasValve();

  configureWiFiNetworks();
  connectWiFi();

  mqttClient.setServer(Config::MQTT_HOST, Config::MQTT_PORT);
  mqttClient.setCallback(mqttCallback);
  connectMqtt();

  setupWebServer();
}

void loop() {
  if (wifiMulti.run() != WL_CONNECTED) {
    connectWiFi();
  }

  if (!mqttClient.connected()) {
    connectMqtt();
  }

  mqttClient.loop();
  webServer.handleClient();
}
