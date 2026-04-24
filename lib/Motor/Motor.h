#pragma once
#include <Arduino.h>
#include <functional>

enum class Event {
  START,
  STOP,
  TIMEOUT,
  OVERCURRENT
};

enum class State {
  IDLE,
  RUNNING,
  ERROR,
};

enum class Rotation {
  CLOCKWISE,
  COUNTERCLOCKWISE,
  STOP,
};

enum class Dir {
  UP,
  DOWN,
  STOP
};

class Motor {
public:
  Motor() = default;

  void begin(uint8_t pinA, uint8_t pinB, uint8_t pinCurrent = 255);

  void run();

  void move(Dir dir);
  void move(Dir dir, uint32_t timeMs);
  void move(Dir dir, uint32_t timeMs, std::function<void(bool success)> onComplete);

  void rotation(Rotation rot);

  inline void setInvert() {
    _invert = !_invert;
  };
  inline void setCurrentThreshold(float current) {
    _currentThreshold = current;
  };

  void setMaxSpeed(uint8_t speed);
  void setMinStartSpeed(uint8_t speed);
  void setCurrentCalibration(float vRef, float gain_A_per_Volt, uint8_t adcBits);
  void setMaxRunTime(unsigned long timeS);

  // CALLBACKS
  inline void onChangeState(std::function<void(const State &)> cb) {
    _onChangeState = cb;
  }

  inline void onEvent(std::function<void(const Event &)> cb) {
    _onEvent = cb;
  }

  static const char *stateToString(State s);
  static const char *eventToString(Event e);

private:
  float _currentThreshold = -1.0f;
  bool _invert = false;

  State _state = State::IDLE;
  Dir _dir = Dir::STOP;
  Rotation _rot = Rotation::STOP;

  std::function<void(const State &)> _onChangeState = nullptr;
  std::function<void(const Event &)> _onEvent = nullptr;
  std::function<void(bool success)> _onComplete = nullptr;

  uint8_t _pinA;
  uint8_t _pinB;
  uint8_t _pinCurrent = 255;

  static uint8_t _nextFreeChannel;
  uint8_t _pwmChannelA;
  uint8_t _pwmChannelB;

  uint8_t _currentSpeed = 0;
  unsigned long _lastRampUpdateMs = 0;

  unsigned long _moveStartMs = 0;
  unsigned long _moveTimeoutMs = 1000;
  unsigned long _decelTimeMs = 1500;
  unsigned long _accelTimeMs = 1500;
  unsigned long _runStartMs = 0;
  unsigned long _maxRunTimeMs = 0;

  uint8_t _maxSpeed = 255;
  uint8_t _minStartSpeed = 30;
  bool _useTimeout = false;

  void applyPWM(bool clockwise, uint8_t speed);

  static const uint8_t NUM_CURRENT_SAMPLES = 30;
  uint16_t _currentBuffer[NUM_CURRENT_SAMPLES];
  uint8_t _currentIndex = 0;
  uint8_t _numCurrentSamples = 0;
  float _vRef = 3.3f;
  float _gain = 1.0f;
  uint8_t _adcBits = 12;

  bool checkCurrent();
  float getCurrent();

  void setState(State state);
  void setDir(Dir dir);
  void setEvent(Event event);
  void stop();
};