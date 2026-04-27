#include <Arduino.h>
#include <WiFi.h>

#define PIN_LED 4

enum NetState {
  NO_WIFI,
  WIFI_OK,
  MQTT_OK
};

NetState netState = NO_WIFI;

void wiFiStationConnected(WiFiEvent_t event, WiFiEventInfo_t info) {
  Serial.println("[WIFI] Connected to AP successfully");
}

void wiFiGotIP(WiFiEvent_t event, WiFiEventInfo_t info) {
  Serial.printf("[WIFI] Connected with IP %s \n", WiFi.localIP());
  netState = WIFI_OK;
}

void wiFiStationDisconnected(WiFiEvent_t event, WiFiEventInfo_t info) {
  Serial.printf("\n[WIFI] Disconnected from WiFi AP.");
  Serial.printf("\n[WIFI] Reason: %d.", info.wifi_sta_disconnected.reason);
  netState = NO_WIFI;
  if (info.wifi_sta_disconnected.reason == WIFI_REASON_AUTH_FAIL) {
    Serial.printf("\n[WIFI] Error pass, aborting reconnecting.");
    return;
  }
  Serial.printf("\n[WIFI] Trying to reconnect");
  WiFi.begin(WIFI_SSID, WIFI_PASS);
}

void updateLed() {
  static unsigned long lastLapsedLedMs = 0;
  static bool state = false;

  unsigned long now = millis();

  switch (netState) {
  case NO_WIFI:
    if (now - lastLapsedLedMs > 1000) {
      state = !state;
      digitalWrite(PIN_LED, state);
      lastLapsedLedMs = now;
    }
    break;
  case WIFI_OK:
    if (now - lastLapsedLedMs > 500) { 
      state = !state;
      digitalWrite(PIN_LED, state);
      lastLapsedLedMs = now;
    }
    break;
  case MQTT_OK:
    digitalWrite(PIN_LED, LOW); 
    break;
  }
}