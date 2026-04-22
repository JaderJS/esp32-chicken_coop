#pragma once
#include <Arduino.h>
#include <Motor.h>
#include <functional>

enum class Move {
  OPEN,
  CLOSE,
  STOP
};

enum class StateDoor {
  UNKNOW,
  HOMING,
  OPEN,
  OPENING,
  CLOSE,
  CLOSING,
};

class Door {
public:
  Door(uint8_t pinOpen, uint8_t pinClose, uint8_t pinCurrent, float currentThreshold = 0.7f) : _currentThreshold(currentThreshold) {
    _motor.begin(pinOpen, pinClose, pinCurrent);
    _motor.setCurrentCalibration(3.3f, 1.257f, (uint8_t)12);
    _motor.setCurrentThreshold(_currentThreshold);
    _motor.setMaxRunTime(60);
    _motor.setMinStartSpeed(50);
    _motor.setMaxSpeed(200);

    _motor.onEvent([this](Event event) {
      Serial.printf("DOOR @onEvent: %s", Motor::eventToString(event));
      if (event == Event::OVERCURRENT) {
        if (_state == StateDoor::HOMING || _state == StateDoor::OPENING) {
          setState(StateDoor::OPEN);
          if (_pendingClose) {
            _pendingClose = false;
            setState(StateDoor::CLOSING);
            _motor.move(Dir::DOWN, _timeToCloseMs, [this](bool success) {
              setState(success ? StateDoor::CLOSE : StateDoor::UNKNOW);
            });
            return;
          }
        } else if (_state == StateDoor::CLOSING) {
          setState(StateDoor::CLOSE);
        }
      }
      if (event == Event::TIMEOUT) {
        setState(StateDoor::UNKNOW);
      }
    });
  }

  inline void begin() {
    if (_onState) {
      _onState(StateDoor::UNKNOW);
    }
  }

  inline void goToHome() {

    setState(StateDoor::HOMING);

    _motor.setCurrentThreshold(_currentThreshold * 0.33f);
    _motor.move(Dir::UP);
  }

  inline void move(Move move) {
    if (move == Move::OPEN) {
      _pendingClose = false;
      setState(StateDoor::OPENING);
      _motor.setCurrentThreshold(_currentThreshold);
      _motor.move(Dir::UP);
    } else if (move == Move::CLOSE) {

      _pendingClose = true;
      goToHome();
      return;

      // Optional feature
      //   if (_state != StateDoor::OPEN) {
      //     goToHome(); // mantém pendingClose = true
      //   } else {

      //     _pendingClose = false;

      //     setState(StateDoor::CLOSING);

      //     _motor.setCurrentThreshold(_currentThreshold);

      //     _motor.move(Dir::DOWN, 25, [this](bool success) {
      //       setState(success ? StateDoor::CLOSE : StateDoor::UNKNOW);
      //     });
      //   }
    } else if (move == Move::STOP) {
      _pendingClose = false;
      setState(StateDoor::UNKNOW);
      _motor.move(Dir::STOP);
    }
  }

  inline void run() {
    _motor.run();
  }

  inline void onState(std::function<void(StateDoor)> cb) {
    _onState = cb;
  }

  static const char *stateToString(StateDoor s) {
    switch (s) {
    case StateDoor::UNKNOW:
      return "UNKNOW";
    case StateDoor::HOMING:
      return "HOMING";
    case StateDoor::OPEN:
      return "OPEN";
    case StateDoor::OPENING:
      return "OPENING";
    case StateDoor::CLOSE:
      return "CLOSE";
    case StateDoor::CLOSING:
      return "CLOSING";
    default:
      return "__UNKNOWN__";
    }
  }

  inline void setMaxSpeed(uint8_t percentage) {
    percentage = constrain(percentage, 0, 100);
    _maxSpeed = map(percentage, 0, 100, 75, 255);
  }

  inline void setTimeToClose(unsigned long timeMs) {
    _timeToCloseMs = timeMs;
  }

  inline void setCurrentThreshold(float current) {
    _currentThreshold = current;
  }

private:
  Motor _motor;
  StateDoor _state = StateDoor::UNKNOW;

  unsigned long _timeToOpenMs = 1000;
  unsigned long _timeToCloseMs = 1000;

  uint8_t _maxSpeed = 255;
  float _currentThreshold = 1;

  bool _pendingClose = false;

  std::function<void(StateDoor)> _onState = nullptr;

  inline void setState(StateDoor state) {
    if (_state == state) return;

    _state = state;

    if (_onState) {
      _onState(_state);
    }
  }
};