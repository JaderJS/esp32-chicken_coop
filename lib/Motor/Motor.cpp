#include "Motor.h"

uint8_t Motor::_nextFreeChannel = 0;

void Motor::begin(uint8_t pinA, uint8_t pinB, uint8_t pinCurrent) {
  _pinA = pinA;
  _pinB = pinB;
  _pinCurrent = pinCurrent;

  pinMode(_pinA, OUTPUT);
  pinMode(_pinB, OUTPUT);

  digitalWrite(_pinA, HIGH);
  digitalWrite(_pinB, HIGH);

  if (_pinCurrent != 255) {
    pinMode(_pinCurrent, INPUT);
  }

  _pwmChannelA = _nextFreeChannel;
  _pwmChannelB = _nextFreeChannel + 1;
  _nextFreeChannel += 2;

  ledcSetup(_pwmChannelA, 10000, 8);
  ledcAttachPin(_pinA, _pwmChannelA);

  ledcSetup(_pwmChannelB, 10000, 8);
  ledcAttachPin(_pinB, _pwmChannelB);

  for (uint8_t i = 0; i < NUM_CURRENT_SAMPLES; i++) {
    _currentBuffer[i] = 0;
  }

  _currentIndex = 0;
  _numCurrentSamples = 0;

  ledcWrite(_pwmChannelA, 255);
  ledcWrite(_pwmChannelB, 255);

  _state = State::IDLE;
  _currentSpeed = 0;
  _dir = Dir::STOP;
}

void Motor::setCurrentCalibration(float vRef, float gain_A_per_Volt, uint8_t adcBits) {
  _vRef = vRef;
  _gain = gain_A_per_Volt;
  _adcBits = adcBits;
}

float Motor::getCurrent() {
  if (_pinCurrent == 255) {
    return 0.0f;
  }

  uint16_t raw = analogRead(_pinCurrent);

  _currentBuffer[_currentIndex] = raw;
  _currentIndex = (_currentIndex + 1) % NUM_CURRENT_SAMPLES;

  if (_numCurrentSamples < NUM_CURRENT_SAMPLES) _numCurrentSamples++;

  uint32_t sum = 0;
  for (uint8_t i = 0; i < _numCurrentSamples; i++) {
    sum += _currentBuffer[i];
  }

  float avg_adc = static_cast<float>(sum) / _numCurrentSamples;
  float max_adc = (1 << _adcBits) - 1.0f;
  float voltage = avg_adc * (_vRef / max_adc);
  float current = voltage * _gain;

  // Serial.printf("RAW: %d GAIN: %.3f VOLTAGE: %.3f CURRENT: %.4f THRESHOLD:%.3f\n", raw, _gain, voltage, current, _currentThreshold);

  return current;
}

bool Motor::checkCurrent() {
  if (_pinCurrent == 255 || _currentThreshold < 0) return false;

  float current = getCurrent();

  if (current > _currentThreshold) {
    return true;
  }

  return false;
}

void Motor::move(Dir dir) {

  setDir(dir);

  if (dir == Dir::STOP) {
    stop();
    return;
  }

  unsigned long now = millis();

  _useTimeout = false;
  _lastRampUpdateMs = now;
  _runStartMs = now;
  _moveStartMs = now;

  setState(State::RUNNING);

  bool clockwise = (dir == Dir::UP);
  applyPWM(clockwise, _minStartSpeed);
  setEvent(Event::START);
}

void Motor::move(Dir dir, uint32_t timeMs) {
  setDir(dir);

  if (dir == Dir::STOP) {
    stop();
    return;
  }

  unsigned long now = millis();
  _moveStartMs = now;
  _moveTimeoutMs = timeMs;
  _useTimeout = true;
  _runStartMs = now;
  _lastRampUpdateMs = now;

  setState(State::RUNNING);

  bool clockwise = (dir == Dir::UP);
  applyPWM(clockwise, _minStartSpeed);
  setEvent(Event::START);
}

void Motor::move(Dir dir, uint32_t timeMs, std::function<void(bool success)> onComplete) {
  _onComplete = onComplete;
  move(dir, timeMs);
}

void Motor::applyPWM(bool clockwise, uint8_t speed) {
  bool actual = _invert ? !clockwise : clockwise;
  uint8_t pwm = 255 - speed;
  ledcWrite(_pwmChannelA, actual ? pwm : 255);
  ledcWrite(_pwmChannelB, actual ? 255 : pwm);
  _currentSpeed = speed;
}

void Motor::run() {

  if (_dir == Dir::STOP) return;

  // =========================
  // 🔥 OVERCURRENT
  // =========================
  if (checkCurrent()) {
    stop();
    setEvent(Event::OVERCURRENT);
    if (_onComplete) {
      _onComplete(false);
      _onComplete = nullptr;
    }
    return;
  }

  unsigned long now = millis();
  // =========================
  // 🔥 FAIL-SAFE GLOBAL
  // =========================
  if (_maxRunTimeMs > 0 && (now - _runStartMs >= _maxRunTimeMs)) {
    stop();
    setEvent(Event::TIMEOUT);
    if (_onComplete) {
      _onComplete(false);
      _onComplete = nullptr;
    }
    return;
  }

  // =========================
  // 🔥 TEMPO DO MOVIMENTO
  // =========================
  unsigned long elapsedMs = now - _moveStartMs;
  unsigned long remainingMs = 0;
  if (_useTimeout) {
    remainingMs = (_moveTimeoutMs > elapsedMs) ? (_moveTimeoutMs - elapsedMs) : 0;
  }

  if (_useTimeout && _decelTimeMs > 0 && remainingMs <= _decelTimeMs) {

    uint8_t targetSpeed = map(remainingMs, 0, _decelTimeMs, 0, _maxSpeed);

    if (_currentSpeed != targetSpeed) {
      bool clockwise = (_dir == Dir::UP);
      applyPWM(clockwise, targetSpeed);
    }

    if (remainingMs == 0) {
      if (_onComplete) {
        _onComplete(true);
        _onComplete = nullptr;
      }
      stop();
    }

    return;
  }

  // =========================
  // 🔥 ACELERAÇÃO BASEADA EM TEMPO (novo)
  // =========================

  if (_accelTimeMs > 0 && elapsedMs <= _accelTimeMs) {

    uint8_t targetSpeed = map(elapsedMs, 0, _accelTimeMs, _minStartSpeed, _maxSpeed);

    if (_currentSpeed != targetSpeed) {
      bool clockwise = (_dir == Dir::UP);
      applyPWM(clockwise, targetSpeed);
    }

    return;
  }

  // =========================
  // 🔥 CRUZEIRO
  // =========================

  if (_currentSpeed != _maxSpeed) {
    bool clockwise = (_dir == Dir::UP);
    applyPWM(clockwise, _maxSpeed);
  }
}

void Motor::stop() {
  ledcWrite(_pwmChannelA, 255);
  ledcWrite(_pwmChannelB, 255);

  _currentSpeed = 0;
  _useTimeout = false;
  setState(State::IDLE);
  setDir(Dir::STOP);
  setEvent(Event::STOP);
}

void Motor::setDir(Dir dir) {
  if (dir == _dir) return;

  _dir = dir;
}

void Motor::setState(State state) {
  if (state == _state) return;

  _state = state;
  if (_onChangeState) {
    _onChangeState(_state);
  }
}

void Motor::setEvent(Event event) {
  if (_onEvent) {
    _onEvent(event);
  }
}

void Motor::setMaxSpeed(uint8_t speed) {
  if (speed > 255 || speed < 0) {
    return;
  }
  _maxSpeed = speed;
}

void Motor::setMinStartSpeed(uint8_t speed) {
  if (speed > 250 || speed < 0) {
    return;
  }
  _minStartSpeed = speed;
}

void Motor::setMaxRunTime(unsigned long timeS) {
  _maxRunTimeMs = timeS * 1000UL;
}

const char *Motor::stateToString(State s) {
  switch (s) {
  case State::IDLE:
    return "IDLE";
  case State::RUNNING:
    return "RUNNING";
  case State::ERROR:
    return "ERROR";
  default:
    return "UNKNOWN";
  }
}

const char *Motor::eventToString(Event e) {
  switch (e) {
  case Event::START:
    return "START";
  case Event::STOP:
    return "STOP";
  case Event::TIMEOUT:
    return "TIMEOUT";
  case Event::OVERCURRENT:
    return "OVERCURRENT";
  default:
    return "UNKNOWN";
  }
}