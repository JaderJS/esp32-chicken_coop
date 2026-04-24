#include "Sensors.h"

void Sensors::begin() {

  _dht = DHT(_pinDHT11, DHT_TYPE);
  _dht.begin();

  pinMode(_pinLDR, INPUT);

  _mutex = xSemaphoreCreateMutex();

  xTaskCreatePinnedToCore(
      task,
      "SensorsTask",
      8192,
      this,
      1,
      NULL,
      1);
}

// ==========================
// TASK
// ==========================
void Sensors::task(void *param) {

  Sensors *self = (Sensors *)param;

  for (;;) {

    SensorData data;

    // ===== leitura única do DHT =====
    float h = self->_dht.readHumidity();
    float t = self->_dht.readTemperature();

    if (isnan(h) || isnan(t)) {
      vTaskDelay(pdMS_TO_TICKS(3000));
      continue;
    }

    data.humidity = h;
    data.temperature = t;
    data.lux = self->getLux();

    // ===== estado atual de chuva =====
    bool rainingNow;

    xSemaphoreTake(self->_mutex, portMAX_DELAY);
    rainingNow = self->_lastRainStatus;
    xSemaphoreGive(self->_mutex);

    // ===== fallback (opcional) =====
    if (!rainingNow && data.humidity > 85.0f) {
      rainingNow = true;
    }

    data.raining = rainingNow;

    // =========================
    // 🔥 DETECÇÃO DE BORDA
    // =========================
    if (rainingNow != self->_lastRainingEdge) {

      self->_lastRainingEdge = rainingNow;

      if (self->_onRaining) {
        self->_onRaining(rainingNow);
      }
    }

    // =========================
    // 🔥 CALLBACK DE MUDANÇA
    // =========================
    bool changed = false;

    if (abs(data.humidity - self->_lastData.humidity) > 0.5f) changed = true;
    if (abs(data.temperature - self->_lastData.temperature) > 0.5f) changed = true;
    if (abs(data.lux - self->_lastData.lux) > 5.0f) changed = true;
    if (data.raining != self->_lastData.raining) changed = true;

    if (changed) {

      self->_lastData = data;

      if (self->_onChange) {
        self->_onChange(data);
      }
    }

    // =========================
    // 🔥 API
    // =========================
    if (WiFi.status() == WL_CONNECTED) {

      unsigned long now = millis();

      if (now - self->_lastAPICheckMs > self->_checkAPIIntervalMs) {

        bool raining = self->checkAPI();

        xSemaphoreTake(self->_mutex, portMAX_DELAY);
        self->_lastRainStatus = raining;
        self->_lastAPICheckMs = now;
        xSemaphoreGive(self->_mutex);
      }
    }

    vTaskDelay(pdMS_TO_TICKS(4000));
  }
}

// ==========================
// API
// ==========================
bool Sensors::checkAPI() {

  HTTPClient http;

  String url =
      "http://api.openweathermap.org/data/2.5/weather?lat=" +
      String(_lat, 6) +
      "&lon=" + String(_lon, 6) +
      "&appid=" + _apiKey;

  http.begin(url);
  http.setTimeout(3000);

  int httpCode = http.GET();

  if (httpCode != 200) {
    http.end();
    return false;
  }

  String payload = http.getString();
  http.end();

  JsonDocument doc;

  if (deserializeJson(doc, payload)) {
    return false;
  }

//   serializeJsonPretty(doc, Serial);

  if (doc["rain"].is<JsonVariant>()) {
    return true;
  }
  return false;
}

// ==========================
// SENSORES
// ==========================
float Sensors::getHumidity() {
  float h = _dht.readHumidity();
  if (isnan(h)) return -1;
  return h;
}

float Sensors::getTemperature() {
  float t = _dht.readTemperature();
  if (isnan(t)) return -1;
  return t;
}

float Sensors::getLux() {
  uint16_t raw = analogRead(_pinLDR);
  float voltage = raw * (3.3f / 4095.0f);
  return (3.3f - voltage) * 1000.0f;
}

// ==========================
// CHUVA
// ==========================
bool Sensors::isRaining() {

  bool rain;

  xSemaphoreTake(_mutex, portMAX_DELAY);
  rain = _lastRainStatus;
  xSemaphoreGive(_mutex);

  if (!rain) {
    float humidity = getHumidity();
    if (humidity > 85.0f) return true;
  }

  return rain;
}