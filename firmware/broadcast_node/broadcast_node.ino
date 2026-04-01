#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <PubSubClient.h>
#include <SoftwareSerial.h>
#include <WiFiClientSecure.h>

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
SoftwareSerial ttsSerial(Config::PIN_TTS_RX, Config::PIN_TTS_TX);

bool warningLightOn = false;
bool fireEmergencyActive = false;
bool pendingFireConfirmation = false;
unsigned long pendingFireStartedMs = 0;
float pendingFireScore = 0.0f;

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
    mqttClient.subscribe(Topics::INTRUSION_EVENT);
    mqttClient.subscribe(Topics::BROADCAST_EVENT);
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

void setWarningLight(bool on) {
  warningLightOn = on;
  digitalWrite(Config::PIN_WARNING_LIGHT, on ? Config::WARNING_LIGHT_ACTIVE_LEVEL : inactiveLevel(Config::WARNING_LIGHT_ACTIVE_LEVEL));
}

void speakText(const String& text) {
  Serial.print("[TTS] ");
  Serial.println(text);
  ttsSerial.println(text);
}

bool sendGotifyNotification(const String& title, const String& message, int priority) {
  if (Config::GOTIFY_BASE_URL[0] == '\0' || Config::GOTIFY_TOKEN[0] == '\0') {
    Serial.println("[Gotify] Skipped: token/url not configured");
    return false;
  }

  WiFiClientSecure secureClient;
  secureClient.setInsecure();

  HTTPClient http;
  String url = String(Config::GOTIFY_BASE_URL) + "/message?token=" + Config::GOTIFY_TOKEN;
  if (!http.begin(secureClient, url)) {
    Serial.println("[Gotify] HTTP begin failed");
    return false;
  }

  http.addHeader("Content-Type", "application/json");

  StaticJsonDocument<256> body;
  body["title"] = title;
  body["message"] = message;
  body["priority"] = priority;

  String payload;
  serializeJson(body, payload);

  int httpCode = http.POST(payload);
  Serial.print("[Gotify] HTTP code=");
  Serial.println(httpCode);
  http.end();

  return httpCode > 0 && httpCode < 400;
}

void publishEmergencyEscalation(float score) {
  StaticJsonDocument<320> doc;
  doc["module"] = Config::MODULE_NAME;
  doc["event"] = "fire";
  doc["source"] = "broadcast-timeout";
  doc["severity"] = "emergency";
  doc["score"] = score;
  doc["pir"] = false;
  doc["door_open"] = false;
  doc["armed"] = true;

  publishJson(Topics::FIRE_EVENT, doc);
}

void activateFireEmergency(const String& reason, float score, bool publishEscalation) {
  if (fireEmergencyActive) {
    return;
  }

  fireEmergencyActive = true;
  pendingFireConfirmation = false;
  setWarningLight(true);

  const String speech = "火警！火警！请保持冷静，尽快撤离！";
  const String pushText = "火灾指数=" + String(score, 1) + "。原因：" + reason;

  speakText(speech);
  sendGotifyNotification("火警告警", pushText, 8);

  if (publishEscalation) {
    publishEmergencyEscalation(score);
  }
}

bool isAdverseWeather(const String& weatherText) {
  static const char* keywords[] = {
      "雨", "雪", "雷", "雾", "霾", "风", "暴", "沙", "冰"
  };

  for (size_t i = 0; i < sizeof(keywords) / sizeof(keywords[0]); ++i) {
    if (weatherText.indexOf(keywords[i]) >= 0) {
      return true;
    }
  }
  return false;
}

bool fetchWeatherAndBroadcast(String* summaryOut = nullptr) {
  HTTPClient http;
  WiFiClient client;

  if (!http.begin(client, Config::WEATHER_API_URL)) {
    Serial.println("[Weather] HTTP begin failed");
    return false;
  }

  int httpCode = http.GET();
  Serial.print("[Weather] HTTP code=");
  Serial.println(httpCode);

  if (httpCode != HTTP_CODE_OK) {
    http.end();
    return false;
  }

  String response = http.getString();
  http.end();

  StaticJsonDocument<2048> doc;
  DeserializationError error = deserializeJson(doc, response);
  if (error) {
    Serial.println("[Weather] JSON parse failed");
    return false;
  }

  JsonObject realtime = doc["result"]["realtime"];
  String city = doc["result"]["city"] | "";
  String temperature = realtime["temperature"] | "";
  String weatherText = realtime["info"] | "";
  String windDirection = realtime["direct"] | "";
  String windPower = realtime["power"] | "";

  String summary = city + "，当前天气" + weatherText + "，气温" + temperature + "℃，" + windDirection + windPower + "。";

  if (summaryOut != nullptr) {
    *summaryOut = summary;
  }

  if (isAdverseWeather(weatherText)) {
    const String broadcastText = "天气提醒：" + summary;
    speakText(broadcastText);
    sendGotifyNotification("天气提醒", broadcastText, 5);
  }

  return true;
}

void handleFireEvent(const String& payload) {
  StaticJsonDocument<384> doc;
  DeserializationError error = deserializeJson(doc, payload);
  if (error) {
    Serial.println("[BROADCAST] Fire JSON parse failed");
    return;
  }

  const String severity = doc["severity"] | "normal";
  const float score = doc["score"] | 0.0f;

  if (severity == "emergency" || score >= Config::FIRE_ALARM_THRESHOLD) {
    activateFireEmergency("收到紧急火情消息", score, false);
    return;
  }

  if (score >= Config::FIRE_PREALARM_THRESHOLD) {
    pendingFireConfirmation = true;
    pendingFireStartedMs = millis();
    pendingFireScore = score;

    const String askText = "检测到疑似火情，请在两分钟内确认室内状态。";
    speakText(askText);
    sendGotifyNotification("火情求证", "检测到疑似火情，火灾指数=" + String(score, 1) + "。请确认室内状态。", 6);
  }
}

void handleIntrusionEvent(const String& payload) {
  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, payload);
  if (error) {
    Serial.println("[BROADCAST] Intrusion JSON parse failed");
    return;
  }

  const String source = doc["source"] | "unknown";
  setWarningLight(true);

  const String speech = "检测到入侵，请注意安全。";
  const String pushText = "安全防护场景触发，来源：" + source;

  speakText(speech);
  sendGotifyNotification("入侵告警", pushText, 8);
}

void handleBroadcastMessage(const String& payload) {
  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, payload);
  if (!error) {
    const String title = doc["title"] | "广播消息";
    const String text = doc["text"] | "";
    if (text.length() > 0) {
      speakText(text);
      sendGotifyNotification(title, text, 5);
      return;
    }
  }

  // 回退：当消息不是 JSON 时，直接按文本播报
  if (payload.length() > 0) {
    speakText(payload);
    sendGotifyNotification("广播消息", payload, 5);
  }
}

void handleControlCommand(const String& payload) {
  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, payload);
  if (error) {
    return;
  }

  const String command = doc["command"] | "";
  if (command == "reset") {
    fireEmergencyActive = false;
    pendingFireConfirmation = false;
    setWarningLight(false);
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
  } else if (strcmp(topic, Topics::INTRUSION_EVENT) == 0) {
    handleIntrusionEvent(message);
  } else if (strcmp(topic, Topics::BROADCAST_EVENT) == 0) {
    handleBroadcastMessage(message);
  } else if (strcmp(topic, Topics::CONTROL_COMMAND) == 0) {
    handleControlCommand(message);
  }
}

void sendJsonResponse(JsonDocument& doc, int statusCode = 200) {
  String output;
  serializeJson(doc, output);
  webServer.send(statusCode, "application/json; charset=utf-8", output);
}

void handleStatus() {
  StaticJsonDocument<256> doc;
  doc["module"] = Config::MODULE_NAME;
  doc["warning_light_on"] = warningLightOn;
  doc["fire_emergency_active"] = fireEmergencyActive;
  doc["pending_fire_confirmation"] = pendingFireConfirmation;
  doc["pending_fire_score"] = pendingFireScore;
  doc["ip"] = WiFi.localIP().toString();
  sendJsonResponse(doc);
}

void handleReset() {
  fireEmergencyActive = false;
  pendingFireConfirmation = false;
  pendingFireScore = 0.0f;
  setWarningLight(false);

  StaticJsonDocument<96> doc;
  doc["ok"] = true;
  doc["message"] = "broadcast node reset";
  sendJsonResponse(doc);
}

void handleConfirmSafe() {
  pendingFireConfirmation = false;
  pendingFireScore = 0.0f;

  StaticJsonDocument<128> doc;
  doc["ok"] = true;
  doc["message"] = "fire alert cleared by operator";
  sendJsonResponse(doc);
}

void handleAnnounce() {
  String text = webServer.arg("text");
  if (text.length() == 0) {
    StaticJsonDocument<96> doc;
    doc["ok"] = false;
    doc["error"] = "missing text";
    sendJsonResponse(doc, 400);
    return;
  }

  speakText(text);
  sendGotifyNotification("广播留言", text, 5);

  StaticJsonDocument<96> doc;
  doc["ok"] = true;
  sendJsonResponse(doc);
}

void handleMedication() {
  String text = webServer.arg("text");
  if (text.length() == 0) {
    text = "请按时服药。";
  }

  speakText(text);
  sendGotifyNotification("服药提醒", text, 5);

  StaticJsonDocument<96> doc;
  doc["ok"] = true;
  sendJsonResponse(doc);
}

void handleWeatherUpdate() {
  String summary;
  const bool ok = fetchWeatherAndBroadcast(&summary);

  StaticJsonDocument<256> doc;
  doc["ok"] = ok;
  doc["summary"] = summary;
  sendJsonResponse(doc, ok ? 200 : 500);
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
  webServer.on("/confirm-safe", HTTP_GET, handleConfirmSafe);
  webServer.on("/announce", HTTP_GET, handleAnnounce);
  webServer.on("/medication", HTTP_GET, handleMedication);
  webServer.on("/weather/update", HTTP_GET, handleWeatherUpdate);
  webServer.onNotFound(handleNotFound);
  webServer.begin();
}

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("[BOOT] broadcast_node");

  pinMode(Config::PIN_WARNING_LIGHT, OUTPUT);
  setWarningLight(false);

  ttsSerial.begin(9600);

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

  if (pendingFireConfirmation && !fireEmergencyActive) {
    if (millis() - pendingFireStartedMs >= Config::ALERT_CONFIRM_TIMEOUT_MS) {
      activateFireEmergency("2 分钟内未收到人工确认，自动升级为紧急火警。", pendingFireScore, true);
    }
  }
}
