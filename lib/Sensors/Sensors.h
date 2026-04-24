#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <AsyncTCP.h>
#include <DHT.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <freertos/FreeRTOS.h>
#include <functional>

#define DHT_TYPE DHT11

struct SensorData {
  float humidity;
  float temperature;
  float lux;
  bool raining;
};

class Sensors {
public:
  Sensors(uint8_t pinDHT, uint8_t pinLDR) : _pinDHT11(pinDHT), _pinLDR(pinLDR), _dht(0, DHT_TYPE) {
  }
  void begin();
  float getHumidity();
  float getTemperature();
  float getLux();

  bool isRaining();

  inline void onChange(std::function<void(const SensorData &)> cb) {
    _onChange = cb;
  };

  inline void onRaining(std::function<void(bool isRaining)> cb) {
    _onRaining = cb;
  }

private:
  std::function<void(const SensorData &)> _onChange = nullptr;
  std::function<void(bool isRaining)> _onRaining = nullptr;

  SensorData _lastData = {0};

  String _apiKey = API_KEY_WEATHER;
  float _lat = -10.81;
  float _lon = -55.45;

  DHT _dht;

  uint8_t _pinDHT11;
  uint8_t _pinLDR;

  bool _lastRainingEdge = false;
  bool _lastRainStatus = false;
  unsigned long _lastAPICheckMs = 0;
  u32_t _checkAPIIntervalMs = 5 * 60 * 1000;

  SemaphoreHandle_t _mutex;

  static void task(void *param);

  bool checkAPI();
  bool checkDHT();
};