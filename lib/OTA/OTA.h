#pragma once
#include <Arduino.h>
#include <HTTPClient.h>
#include <Update.h>
#include <WiFi.h>
#include <functional>

class OTA {
public:
  void begin();

  void trigger(const String &url);

  inline void onStart(std::function<void()> cb) {
    _onStart = cb;
  }
  inline void onProgress(std::function<void(size_t, size_t)> cb) {
    _onProgress = cb;
  }
  inline void onEnd(std::function<void(bool)> cb) {
    _onEnd = cb;
  }

  inline bool isRunning() {
    return _running;
  }

private:
  static void task(void *param);

  void run();

  String _url;

  bool _running = false;

  std::function<void()> _onStart = nullptr;
  std::function<void(size_t, size_t)> _onProgress = nullptr;
  std::function<void(bool)> _onEnd = nullptr;
};