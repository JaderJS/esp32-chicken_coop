#include <Arduino.h>
#include <Door.h>
#include <Motor.h>
#include <PsychicMqttClient.h>
#include <WiFi.h>

const char *ssid = "LinkRadio";
const char *password = "62724654";

Door curtainB(14, 27, 34);
Door curtainA(25, 26, 35);

String topic = "chicken/coop/" + String(ESP.getEfuseMac());
const char *mqttUrl = "ws://mqtt.fieldlink.net.br";
const char *mqttUser = "esp32@chicken_coop";
;
const char *mqttPasswd = "pipa_papa3427";
PsychicMqttClient mqtt;

void onMqttMessage(char *topic_, char *payload, int retain, int qos, bool dup) {
  Serial.printf("[MQTT]: topic: %s | payload: %s | qos: %d | dup: %d | retain: %d \n", topic_, payload, qos, dup, retain);

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
}

void taskMqtt(void *pvParameters) {
  vTaskDelay(pdMS_TO_TICKS(2000));

  mqtt.setServer(mqttUrl);
  mqtt.setCredentials(mqttUser, mqttPasswd);

  mqtt.onConnect([](bool sessionPresent) {
    Serial.println("[MQTT]: Connected!");

    mqtt.subscribe((topic + "/cmd").c_str(), 0);
    mqtt.subscribe((topic + "/ota").c_str(), 0);
    mqtt.subscribe((topic + "/door/cmd").c_str(), 0);

    mqtt.publish((topic + "/status").c_str(), 0, 0, "ONLINE");
  });

  mqtt.onDisconnect([](int reason) {
    Serial.printf("[MQTT]: Disconnected (%d)\n", reason);
  });

  mqtt.onMessage(onMqttMessage);

  mqtt.setWill((topic + "/status").c_str(), 1, true, "OFFLINE");

  Serial.println("[MQTT]: Starting...");
  mqtt.connect();

  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

void setup() {
  Serial.begin(115200);

  delay(1000);
  Serial.printf("[APP]: Connecting in %s ", ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println("");

  xTaskCreatePinnedToCore(taskMqtt, "TaskMQTT", 8192, NULL, 2, NULL, 0);

  curtainA.begin();
  curtainA.setCurrentThreshold(2.2);
  curtainA.setTimeToClose(10000);

  curtainB.begin();
  curtainB.setCurrentThreshold(1);
  curtainB.setTimeToClose(4000);
  // curtainB.goToHome();

  curtainA.onState([](StateDoor state) {
    mqtt.publish((topic + "/curtainA/state").c_str(), 0, 0, Door::stateToString(state));
  });

  curtainB.onState([](StateDoor state) {
    mqtt.publish((topic + "/curtainB/state").c_str(), 0, 0, Door::stateToString(state));
  });
}

void loop() {
  curtainA.run();
  curtainB.run();
}
