#include <Arduino.h>
#include <ArduinoJson.h>
#include <DHTesp.h>
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <PubSubClient.h>
#include <Ticker.h>

#if __has_include("config.h")
#include "config.h"
#else
#include "config.example.h"
#endif

#include "app_topics.h"
#include "fuzzy_fire_controller.h"

ESP8266WiFiMulti wifiMulti;
ESP8266WebServer webServer(80);
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);
DHTesp dht;
Ticker sensorTicker;
Ticker telemetryTicker;

FuzzyFireController fuzzyController;

volatile bool sensorSampleRequested = false;
volatile bool telemetryRequested = false;

bool armingEnabled = false;
unsigned long lastIntrusionPublishMs = 0;

float lastGasLevel = 0.0f;
float lastTemperatureC = 0.0f;
float lastFireScore = 0.0f;
bool lastPirDetected = false;
bool lastDoorOpen = false;

void requestSensorSample() {
  sensorSampleRequested = true;
}

void requestTelemetryPublish() {
  telemetryRequested = true;
}

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
  const bool ok = mqttClient.publish(topic, payload, payloadLength);

  Serial.print("[MQTT] Publish ");
  Serial.print(topic);
  Serial.print(" -> ");
  Serial.println(ok ? "OK" : "FAILED");

  return ok;
}

void publishBroadcastMessage(const String& title, const String& text, const String& kind) {
  StaticJsonDocument<256> doc;
  doc["module"] = Config::MODULE_NAME;
  doc["title"] = title;
  doc["text"] = text;
  doc["kind"] = kind;
  publishJson(Topics::BROADCAST_EVENT, doc);
}

float readMq2Level() {
  unsigned long sum = 0;
  const int sampleCount = 10;

  for (int i = 0; i < sampleCount; ++i) {
    sum += analogRead(Config::PIN_MQ2);
    delay(5);
  }

  const float rawAverage = static_cast<float>(sum) / sampleCount;
  float scaled = rawAverage * Config::MQ2_MAX_SCALE / 1023.0f;

  if (scaled < 0.0f) {
    scaled = 0.0f;
  }
  if (scaled > Config::MQ2_MAX_SCALE) {
    scaled = Config::MQ2_MAX_SCALE;
  }

  return scaled;
}

float readTemperatureC() {
  TempAndHumidity data = dht.getTempAndHumidity();
  if (isnan(data.temperature)) {
    Serial.println("[DHT11] Invalid temperature, using last value");
    return lastTemperatureC;
  }
  return data.temperature;
}

bool isPirTriggered() {
  return digitalRead(Config::PIN_PIR) == Config::PIR_ACTIVE_LEVEL;
}

bool isDoorOpen() {
  return digitalRead(Config::PIN_REED) == Config::REED_OPEN_LEVEL;
}

const char* fireSeverity(float score) {
  if (score >= Config::FIRE_ALARM_THRESHOLD) {
    return "emergency";
  }
  if (score >= Config::FIRE_PREALARM_THRESHOLD) {
    return "alert";
  }
  return "normal";
}

void publishFireEvent(float gasLevel, float temperatureC, float fireScore, bool pirDetected, bool doorOpen) {
  StaticJsonDocument<320> doc;
  doc["module"] = Config::MODULE_NAME;
  doc["event"] = "fire";
  doc["source"] = "sensor";
  doc["severity"] = fireSeverity(fireScore);
  doc["score"] = fireScore;
  doc["gas_level"] = gasLevel;
  doc["temperature_c"] = temperatureC;
  doc["pir"] = pirDetected;
  doc["door_open"] = doorOpen;
  doc["armed"] = armingEnabled;
  publishJson(Topics::FIRE_EVENT, doc);
}

void publishIntrusionEvent(bool pirDetected, bool doorOpen) {
  const unsigned long nowMs = millis();
  if (nowMs - lastIntrusionPublishMs < Config::INTRUSION_DEBOUNCE_MS) {
    return;
  }

  lastIntrusionPublishMs = nowMs;

  StaticJsonDocument<256> doc;
  doc["module"] = Config::MODULE_NAME;
  doc["event"] = "intrusion";
  doc["source"] = pirDetected && doorOpen ? "pir_and_door" : (pirDetected ? "pir" : "door");
  doc["armed"] = armingEnabled;
  doc["pir"] = pirDetected;
  doc["door_open"] = doorOpen;

  publishJson(Topics::INTRUSION_EVENT, doc);
}

void publishTelemetry() {
  StaticJsonDocument<320> doc;
  doc["module"] = Config::MODULE_NAME;
  doc["event"] = "telemetry";
  doc["gas_level"] = lastGasLevel;
  doc["temperature_c"] = lastTemperatureC;
  doc["fire_score"] = lastFireScore;
  doc["pir"] = lastPirDetected;
  doc["door_open"] = lastDoorOpen;
  doc["armed"] = armingEnabled;
  doc["wifi_rssi"] = WiFi.RSSI();

  publishJson(Topics::TELEMETRY_EVENT, doc);
}

void sampleAndProcessSensors() {
  lastGasLevel = readMq2Level();
  lastTemperatureC = readTemperatureC();
  lastPirDetected = isPirTriggered();
  lastDoorOpen = isDoorOpen();
  lastFireScore = fuzzyController.evaluate(lastGasLevel, lastTemperatureC);

  Serial.print("[Sensor] MQ-2=");
  Serial.print(lastGasLevel, 2);
  Serial.print("  Temp=");
  Serial.print(lastTemperatureC, 2);
  Serial.print("  FireScore=");
  Serial.println(lastFireScore, 2);

  if (armingEnabled && (lastPirDetected || lastDoorOpen)) {
    publishIntrusionEvent(lastPirDetected, lastDoorOpen);
  }

  if (lastFireScore >= Config::FIRE_PREALARM_THRESHOLD) {
    publishFireEvent(lastGasLevel, lastTemperatureC, lastFireScore, lastPirDetected, lastDoorOpen);
  }
}

void sendJsonResponse(JsonDocument& doc, int statusCode = 200) {
  String output;
  serializeJson(doc, output);
  webServer.send(statusCode, "application/json; charset=utf-8", output);
}

void handleArm() {
  armingEnabled = true;
  publishBroadcastMessage("系统状态", "安全防护已布防。", "system");

  StaticJsonDocument<96> doc;
  doc["ok"] = true;
  doc["armed"] = true;
  sendJsonResponse(doc);
}

void handleDisarm() {
  armingEnabled = false;
  publishBroadcastMessage("系统状态", "安全防护已撤防。", "system");

  StaticJsonDocument<96> doc;
  doc["ok"] = true;
  doc["armed"] = false;
  sendJsonResponse(doc);
}

void handleStatus() {
  StaticJsonDocument<256> doc;
  doc["module"] = Config::MODULE_NAME;
  doc["armed"] = armingEnabled;
  doc["gas_level"] = lastGasLevel;
  doc["temperature_c"] = lastTemperatureC;
  doc["fire_score"] = lastFireScore;
  doc["pir"] = lastPirDetected;
  doc["door_open"] = lastDoorOpen;
  doc["ip"] = WiFi.localIP().toString();
  sendJsonResponse(doc);
}

void handleResetFire() {
  lastFireScore = 0.0f;

  StaticJsonDocument<96> doc;
  doc["ok"] = true;
  doc["message"] = "fire score reset";
  sendJsonResponse(doc);
}

void handleNotFound() {
  StaticJsonDocument<96> doc;
  doc["ok"] = false;
  doc["error"] = "not found";
  sendJsonResponse(doc, 404);
}

void setupWebServer() {
  webServer.on("/arm", HTTP_GET, handleArm);
  webServer.on("/disarm", HTTP_GET, handleDisarm);
  webServer.on("/status", HTTP_GET, handleStatus);
  webServer.on("/reset-fire", HTTP_GET, handleResetFire);
  webServer.onNotFound(handleNotFound);
  webServer.begin();
}

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("[BOOT] sensor_node");

  pinMode(Config::PIN_PIR, INPUT);
  pinMode(Config::PIN_REED, INPUT_PULLUP);

  dht.setup(Config::PIN_DHT, DHTesp::DHT11);

  configureWiFiNetworks();
  connectWiFi();

  mqttClient.setServer(Config::MQTT_HOST, Config::MQTT_PORT);
  connectMqtt();

  setupWebServer();

  sensorTicker.attach_ms(Config::SENSOR_SAMPLE_MS, requestSensorSample);
  telemetryTicker.attach_ms(Config::TELEMETRY_MS, requestTelemetryPublish);

  requestSensorSample();
  telemetryRequested = true;
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

  if (sensorSampleRequested) {
    sensorSampleRequested = false;
    sampleAndProcessSensors();
  }

  if (telemetryRequested) {
    telemetryRequested = false;
    publishTelemetry();
  }
}
