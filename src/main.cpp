#include <Arduino.h>
#include <ArduinoJson.h>
#include <Door.h>
#include <Motor.h>
#include <OTA.h>
#include <Preferences.h>
#include <PsychicMqttClient.h>
#include <Sensors.h>
#include <WiFi.h>

#define PIN_LED 4
#define PIN_LAMP 13

const char *ssid = WIFI_SSID;
const char *password = WIFI_PASS;

Preferences prefs;

OTA ota;

Sensors sensors(2, 33);

Door curtainB(14, 27, 34);
Door curtainA(25, 26, 35);

String topic = "chicken/coop/" + String(ESP.getEfuseMac());
const char *mqttUrl = MQTT_URL;
const char *mqttUser = MQTT_USER;

const char *mqttPasswd = MQTT_PASS;
PsychicMqttClient mqtt;

bool automatic = true;
unsigned long lastCommandMs = 0;
uint32_t checkInterval = 1 * 60 * 60 * 1000UL;

void applyConfig(Door &curtain, JsonDocument &doc) {
  if (doc["timeToCloseMs"].is<int>()) {
    curtain.setTimeToClose(doc["timeToCloseMs"].as<int>());
  }

  if (doc["currentThreshold"].is<float>() || doc["currentThreshold"].is<int>()) {
    curtain.setCurrentThreshold(doc["currentThreshold"].as<float>());
  }
}

void saveConfig(const char *ns, JsonDocument &doc) {
  prefs.begin(ns, false);

  if (doc["timeToCloseMs"].is<int>()) {
    prefs.putInt("time", doc["timeToCloseMs"].as<int>());
  }

  if (doc["currentThreshold"].is<float>() || doc["currentThreshold"].is<int>()) {
    prefs.putFloat("current", doc["currentThreshold"].as<float>());
  }

  prefs.end();
}

void loadConfig(const char *ns, Door &curtain) {
  prefs.begin(ns, true);

  JsonDocument doc;

  doc["timeToCloseMs"] = prefs.getInt("time", 5000);
  doc["currentThreshold"] = prefs.getFloat("current", 1.0f);

  prefs.end();

  applyConfig(curtain, doc);
}

void onMqttMessage(char *topic_, char *payload, int retain, int qos, bool dup) {
  Serial.printf("[MQTT]: topic: %s | payload: %s | qos: %d | dup: %d | retain: %d \n", topic_, payload, qos, dup, retain);
  unsigned long now = millis();
  lastCommandMs = now;
  if (strcasecmp((topic + "/ota").c_str(), topic_) == 0) {

    if (ota.isRunning()) {
      mqtt.publish((topic + "/ota/status").c_str(), 0, 0, "BUSY");
      return;
    }

    JsonDocument doc;
    if (deserializeJson(doc, payload)) {
      mqtt.publish((topic + "/ota/status").c_str(), 0, 0, "INVALID_JSON");
      return;
    }
    if (!doc["firmware"].is<const char *>()) {
      mqtt.publish((topic + "/ota/status").c_str(), 0, 0, "INVALID_FIELD");
      return;
    }
    String url = doc["firmware"].as<String>();

    mqtt.publish((topic + "/ota/status").c_str(), 0, 0, "STARTING");
    ota.trigger(url);
  }

  if (strcasecmp((topic + "/cmd").c_str(), topic_) == 0) {
    if (strcasecmp(payload, "AUTO") == 0) {
      automatic = true;
      mqtt.publish((topic + "/cmd/state").c_str(), 0, 0, "AUTO");
    } else if (strcasecmp(payload, "MANUAL") == 0) {
      automatic = false;
      mqtt.publish((topic + "/cmd/state").c_str(), 0, 0, "MANUAL");
    } else {
      mqtt.publish((topic + "/cmd/info").c_str(), 0, 0, "Do not recognized last comand, [AUTO/MANUAL]");
    }
  }

  if (strcasecmp((topic + "/mode/set").c_str(), topic_) == 0) {
    if (strcasecmp(payload, "AUTO") == 0) {
      automatic = true;
      mqtt.publish((topic + "/mode/state").c_str(), 0, 0, "AUTO");
      return;
    }
    if (strcasecmp(payload, "MANUAL") == 0) {
      automatic = false;
      mqtt.publish((topic + "/mode/state").c_str(), 0, 0, "MANUAL");
      return;
    }
    mqtt.publish((topic + "/mode/info").c_str(), 0, 0, "Do not recognized last comand, [AUTO/MANUAL]");
    return;
  }

  if (strcasecmp((topic + "/lamp/set").c_str(), topic_) == 0) {
    if (strcasecmp(payload, "ON") == 0) {
      digitalWrite(PIN_LAMP, HIGH);
      mqtt.publish((topic + "/lamp/state").c_str(), 0, 0, "ON");
      return;
    }
    if (strcasecmp(payload, "OFF") == 0) {
      digitalWrite(PIN_LAMP, LOW);
      mqtt.publish((topic + "/lamp/state").c_str(), 0, 0, "OFF");
      return;
    }
    mqtt.publish((topic + "/lamp/info").c_str(), 0, 0, "Do not recognized last comand, [ON/OFF]");
    return;
  }

  if (strcasecmp((topic + "/door/cmd").c_str(), topic_) == 0) {
    if (strcasecmp(payload, "CLOSE") == 0) {
      curtainA.move(Move::CLOSE);
      curtainB.move(Move::CLOSE);
    }
    if (strcasecmp(payload, "OPEN") == 0) {
      curtainA.move(Move::OPEN);
      curtainB.move(Move::OPEN);
    }
    if (strcasecmp(payload, "STOP") == 0) {
      curtainA.move(Move::STOP);
      curtainB.move(Move::STOP);
    }
  }

  if (strcasecmp((topic + "/curtainA/cmd").c_str(), topic_) == 0) {
    if (strcasecmp(payload, "CLOSE") == 0) {
      curtainA.move(Move::CLOSE);
    }
    if (strcasecmp(payload, "OPEN") == 0) {
      curtainA.move(Move::OPEN);
    }
    if (strcasecmp(payload, "STOP") == 0) {
      curtainA.move(Move::STOP);
    }
  }

  if (strcasecmp((topic + "/curtainB/cmd").c_str(), topic_) == 0) {
    if (strcasecmp(payload, "CLOSE") == 0) {
      curtainB.move(Move::CLOSE);
    }
    if (strcasecmp(payload, "OPEN") == 0) {
      curtainB.move(Move::OPEN);
    }
    if (strcasecmp(payload, "STOP") == 0) {
      curtainB.move(Move::STOP);
    }
  }

  if (strcasecmp((topic + "/curtainA/set/config").c_str(), topic_) == 0) {
    JsonDocument doc;
    if (deserializeJson(doc, payload)) {
      mqtt.publish((topic + "/curtainA/config/status").c_str(), 0, 0, "INVALID_JSON");
      return;
    }
    applyConfig(curtainA, doc);
    saveConfig("curtainA", doc);
    mqtt.publish((topic + "/curtainA/config/status").c_str(), 0, 0, "OK");
    return;
  }

  if (strcasecmp((topic + "/curtainB/set/config").c_str(), topic_) == 0) {
    JsonDocument doc;
    if (deserializeJson(doc, payload)) {
      mqtt.publish((topic + "/curtainB/config/status").c_str(), 0, 0, "INVALID_JSON");
      return;
    }

    applyConfig(curtainB, doc);
    saveConfig("curtainB", doc);
    mqtt.publish((topic + "/curtainB/config/status").c_str(), 0, 0, "OK");
    return;
  }
}

void taskMqtt() {

  mqtt.setServer(mqttUrl);
  mqtt.setCredentials(mqttUser, mqttPasswd);

  mqtt.onConnect([](bool sessionPresent) {
    Serial.println("[MQTT]: Connected!");

    mqtt.subscribe((topic + "/ota").c_str(), 0);
    mqtt.subscribe((topic + "/cmd").c_str(), 0);
    mqtt.subscribe((topic + "/door/cmd").c_str(), 0);
    mqtt.subscribe((topic + "/lamp/set").c_str(), 0);
    mqtt.subscribe((topic + "/mode/set").c_str(), 0);
    mqtt.subscribe((topic + "/curtainA/set/config").c_str(), 0);
    mqtt.subscribe((topic + "/curtainB/set/config").c_str(), 0);

    mqtt.publish((topic + "/cmd/state").c_str(), 0, 0, "AUTO");
    mqtt.publish((topic + "/curtainA/state").c_str(), 0, 1, "UNKNOW");
    mqtt.publish((topic + "/curtainB/state").c_str(), 0, 1, "UNKNOW");
    mqtt.publish((topic + "/door/state").c_str(), 0, 1, "UNKNOW");

    mqtt.publish((topic + "/mode/state").c_str(), 0, 1, automatic ? "AUTO" : "MANUAL");

    mqtt.publish((topic + "/status").c_str(), 0, 0, "ONLINE");
  });

  mqtt.onDisconnect([](int reason) {
    Serial.printf("[MQTT]: Disconnected (%d)\n", reason);
  });

  mqtt.onMessage(onMqttMessage);

  mqtt.setWill((topic + "/status").c_str(), 1, 1, "OFFLINE");

  Serial.println("[MQTT]: Starting...");
  mqtt.connect();
}

void setup() {

  pinMode(PIN_LED, OUTPUT);
  pinMode(PIN_LAMP, OUTPUT);

  Serial.begin(115200);

  delay(500);
  Serial.printf("[APP]: Connecting in %s ", ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println("");

  ota.begin();

  taskMqtt();

  curtainA.begin();
  loadConfig("curtainA", curtainA);
  // curtainA.setCurrentThreshold(1.0f);
  // curtainA.setTimeToClose(5000);

  curtainB.begin();
  loadConfig("curtainB", curtainB);
  // curtainB.setCurrentThreshold(1.0f);
  // curtainB.setTimeToClose(5000);

  sensors.begin();

  sensors.onChange([](const SensorData &d) {
    mqtt.publish((topic + "/sensors/humidity").c_str(), 0, 0, String(d.humidity, 2).c_str());
    mqtt.publish((topic + "/sensors/temperature").c_str(), 0, 0, String(d.temperature, 2).c_str());
    mqtt.publish((topic + "/sensors/lux").c_str(), 0, 0, String(d.lux, 2).c_str());
    mqtt.publish((topic + "/sensors/rain").c_str(), 0, 0, d.raining ? "RAIN" : "NOT_RAIN");
  });

  sensors.onRaining([](bool raining) {
    if (raining) {
      curtainA.move(Move::CLOSE);
      curtainB.move(Move::CLOSE);
      return;
    }
    curtainA.move(Move::OPEN);
    curtainB.move(Move::OPEN);
  });

  curtainA.onState([](StateDoor state) {
    mqtt.publish((topic + "/curtainA/state").c_str(), 0, 1, Door::stateToString(state));
    mqtt.publish((topic + "/door/state").c_str(), 0, 1, Door::stateToString(state));
  });

  curtainB.onState([](StateDoor state) {
    mqtt.publish((topic + "/curtainB/state").c_str(), 0, 1, Door::stateToString(state));
    mqtt.publish((topic + "/door/state").c_str(), 0, 1, Door::stateToString(state));
  });
}

void loop() {
  curtainA.run();
  curtainB.run();

  unsigned long now = millis();
  if (!automatic && now - lastCommandMs > checkInterval) {
    automatic = true;
    lastCommandMs = now;
  }
  static unsigned long lastLapsedLedMs = 0;
  if (now - lastLapsedLedMs > 300) {
    digitalWrite(PIN_LED, !digitalRead(PIN_LED));
    lastLapsedLedMs = now;
  }
}
