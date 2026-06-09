#pragma once

#include <chrono>
#include <thread>
#include <vector>

class Timer {
public:
  Timer();
  ~Timer();

  template <typename Func> void setTimeout(Func function, int delay);

  template <typename Func> void setInterval(Func function, int interval);

  void stop();

  bool isRunning() const;

private:
  Timer(const Timer &) = delete;
  Timer &operator=(const Timer &) = delete;

private:
  bool _clear;
  std::thread _timerThread;
};

inline Timer::Timer() {}

inline Timer::~Timer() {
  stop();
}

template <typename Func> void Timer::setTimeout(Func function, int delay) {
  stop();

  _clear = false;

  _timerThread = std::thread([=]() {
    if (_clear) {
      return;
    }

    int high = delay / 5;
    const int low = delay % 5;

    while (high > 0 && !_clear) {
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
      --high;
    }

    if (_clear) {
      return;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(low));

    if (_clear) {
      return;
    }

    function();
  });
}

template <typename Func> void Timer::setInterval(Func function, int interval) {
  stop();

  _clear = false;

  _timerThread = std::thread([=]() {
    while (true) {
      if (_clear) {
        return;
      }

      int high = interval / 5;
      const int low = interval % 5;

      while (high > 0 && !_clear) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        --high;
      }

      if (_clear) {
        return;
      }

      std::this_thread::sleep_for(std::chrono::milliseconds(low));

      if (_clear) {
        return;
      }

      function();
    }
  });
}

inline void Timer::stop() {
  _clear = true;

  if (_timerThread.joinable()) {
    _timerThread.join();
  }
}

inline bool Timer::isRunning() const {
  return _timerThread.joinable();
}
