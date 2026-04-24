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

enum class DoorEvent {
  CMD_OPEN,
  CMD_CLOSE,
  CMD_STOP,
  MOTOR_OVERCURRENT,
  MOTOR_TIMEOUT,
  MOTOR_DONE,
  MOTOR_STOP
};

class Door {
public:
  Door(uint8_t pinOpen, uint8_t pinClose, uint8_t pinCurrent, float currentThreshold = 0.7f)
      : _currentThreshold(currentThreshold) {

    _motor.begin(pinOpen, pinClose, pinCurrent);
    _motor.setCurrentCalibration(3.3f, 1.257f, 12);
    _motor.setMaxRunTime(60);
    _motor.setMinStartSpeed(50);
    _motor.setMaxSpeed(255);

    _motor.onEvent([this](Event e) {
      switch (e) {
      case Event::OVERCURRENT:
        pushEvent(DoorEvent::MOTOR_OVERCURRENT);
        break;
      case Event::TIMEOUT:
        pushEvent(DoorEvent::MOTOR_TIMEOUT);
        break;
      case Event::STOP:
        pushEvent(DoorEvent::MOTOR_STOP);
        break;
      default:
        break;
      }
    });
  }

  void begin() {
    _motor.move(Dir::STOP);
    setState(StateDoor::UNKNOW);
  }

  void move(Move m) {
    if (m == Move::OPEN) pushEvent(DoorEvent::CMD_OPEN);
    if (m == Move::CLOSE) pushEvent(DoorEvent::CMD_CLOSE);
    if (m == Move::STOP) pushEvent(DoorEvent::CMD_STOP);
  }

  void run() {
    _motor.run();

    DoorEvent ev;

    while (popEvent(ev)) {

      Serial.printf("[FSM] STATE: %s | EVENT: %d\n", stateToString(_state), (int)ev);

      switch (_state) {

      case StateDoor::UNKNOW:

        if (ev == DoorEvent::CMD_OPEN) {
          setState(StateDoor::OPENING);
          _motor.setCurrentThreshold(_currentThreshold);
          _motor.move(Dir::UP);
        }

        if (ev == DoorEvent::CMD_CLOSE) {
          _pendingClose = true;
          setState(StateDoor::HOMING);
          _motor.setCurrentThreshold(_currentThreshold * 0.66f);
          _motor.move(Dir::UP);
        }

        break;

      case StateDoor::HOMING:

        if (ev == DoorEvent::MOTOR_OVERCURRENT) {
          setState(StateDoor::OPEN);

          if (_pendingClose) {
            _pendingClose = false;
            setState(StateDoor::CLOSING);

            _motor.setCurrentThreshold(-1); // 🔥 desativa corrente no fechamento

            _motor.move(Dir::DOWN, _timeToCloseMs, [this](bool success) {
              pushEvent(DoorEvent::MOTOR_DONE);
            });
          }
        }

        break;

      case StateDoor::OPENING:

        if (ev == DoorEvent::MOTOR_OVERCURRENT) {
          setState(StateDoor::OPEN);
        }
        if (ev == DoorEvent::MOTOR_TIMEOUT) {
          setState(StateDoor::UNKNOW);
        }

        break;

      case StateDoor::CLOSING:

        if (ev == DoorEvent::MOTOR_DONE) {
          setState(StateDoor::CLOSE);
        }

        if (ev == DoorEvent::MOTOR_TIMEOUT) {
          setState(StateDoor::UNKNOW);
        }

        break;

      case StateDoor::OPEN:

        if (ev == DoorEvent::CMD_CLOSE) {
          _pendingClose = true;
          setState(StateDoor::HOMING);
          _motor.setCurrentThreshold(_currentThreshold * 0.66f);
          _motor.move(Dir::UP);

          continue;
        }

        break;

      case StateDoor::CLOSE:

        if (ev == DoorEvent::CMD_OPEN) {
          setState(StateDoor::OPENING);
          _motor.setCurrentThreshold(_currentThreshold);
          _motor.move(Dir::UP);
        }

        break;

      default:
        break;
      }
    }

    if (_stateChanged) {
      _stateChanged = false;
      if (_onState) _onState(_state);
    }
  }

  void onState(std::function<void(StateDoor)> cb) {
    _onState = cb;
  }

  void setTimeToClose(unsigned long timeMs) {
    _timeToCloseMs = timeMs;
  }

  void setCurrentThreshold(float c) {
    _currentThreshold = c;
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

private:
  Motor _motor;

  StateDoor _state = StateDoor::UNKNOW;
  bool _stateChanged = false;

  float _currentThreshold = 1;
  unsigned long _timeToCloseMs = 10;

  bool _pendingClose = false;

  std::function<void(StateDoor)> _onState = nullptr;

  static const int QSIZE = 10;
  DoorEvent _queue[QSIZE];
  volatile int _head = 0;
  volatile int _tail = 0;

  void pushEvent(DoorEvent e) {
    int next = (_head + 1) % QSIZE;
    if (next != _tail) {
      _queue[_head] = e;
      _head = next;
    }
  }

  bool popEvent(DoorEvent &e) {
    if (_tail == _head) return false;
    e = _queue[_tail];
    _tail = (_tail + 1) % QSIZE;
    return true;
  }

  void setState(StateDoor s) {
    if (_state == s) return;

    Serial.printf("[FSM] %s → %s\n", stateToString(_state), stateToString(s));

    _state = s;
    _stateChanged = true;
  }
};