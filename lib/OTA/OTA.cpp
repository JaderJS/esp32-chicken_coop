#include "OTA.h"

void OTA::begin() {
}

void OTA::trigger(const String &url) {
  if (_running) return;

  _url = url;
  _running = true;

  xTaskCreatePinnedToCore(
      task,
      "OTATask",
      10000,
      this,
      1,
      NULL,
      1);
}

void OTA::task(void *param) {
  OTA *self = (OTA *)param;
  self->run();
  vTaskDelete(NULL);
}

void OTA::run() {

  if (_onStart) _onStart();

  if (WiFi.status() != WL_CONNECTED) {
    if (_onEnd) _onEnd(false);
    _running = false;
    return;
  }

  HTTPClient http;
  http.begin(_url);
  http.setTimeout(10000);

  int httpCode = http.GET();

  if (httpCode != HTTP_CODE_OK) {
    http.end();
    if (_onEnd) _onEnd(false);
    _running = false;
    return;
  }

  int contentLength = http.getSize();
  WiFiClient *stream = http.getStreamPtr();

  if (!Update.begin(contentLength)) {
    http.end();
    if (_onEnd) _onEnd(false);
    _running = false;
    return;
  }

  uint8_t buff[1024];
  size_t written = 0;

  while (http.connected() && written < contentLength) {

    size_t available = stream->available();

    if (available) {
      int len = stream->readBytes(buff, min(available, sizeof(buff)));

      Update.write(buff, len);
      written += len;

      if (_onProgress) {
        _onProgress(written, contentLength);
      }
    }

    vTaskDelay(1);
  }

  bool success = Update.end();

  http.end();

  if (_onEnd) _onEnd(success);

  _running = false;

  if (success) {
    delay(1000);
    ESP.restart();
  }
}