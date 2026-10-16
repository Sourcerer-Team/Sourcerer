#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>

namespace Util {

class Timer {
public:
  Timer() = default;
  ~Timer() { stop(); }

  template <typename Func> void setTimeout(Func &&function, int delay) {
    start(std::forward<Func>(function), std::chrono::milliseconds(delay),
          false);
  }

  template <typename Func> void setInterval(Func &&function, int interval) {
    start(std::forward<Func>(function), std::chrono::milliseconds(interval),
          true);
  }

  void stop();
  bool isRunning() const;

private:
  Timer(const Timer &) = delete;
  Timer &operator=(const Timer &) = delete;

  struct SharedState {
    std::mutex mutex;
    std::condition_variable condition;
    std::atomic_bool stopRequested{false};
    std::atomic_bool finished{false};
  };

  template <typename Func>
  void start(Func &&function, std::chrono::milliseconds delay, bool repeating) {
    stop();

    auto state = std::make_shared<SharedState>();
    {
      std::lock_guard<std::mutex> lock(_stateMutex);
      _state = state;
    }

    _timerThread =
        std::thread([state, delay, repeating,
                     callback = std::forward<Func>(function)]() mutable {
          while (true) {
            std::unique_lock<std::mutex> lock(state->mutex);
            if (state->condition.wait_for(lock, delay, [&]() {
                  return state->stopRequested.load();
                })) {
              break;
            }

            lock.unlock();
            callback();

            if (!repeating) {
              break;
            }
          }

          state->stopRequested = true;
          state->finished = true;
        });
  }

private:
  mutable std::mutex _stateMutex;
  std::shared_ptr<SharedState> _state;
  std::thread _timerThread;
};

inline void Timer::stop() {
  std::shared_ptr<SharedState> state;
  {
    std::lock_guard<std::mutex> lock(_stateMutex);
    state = _state;
  }

  if (state) {
    state->stopRequested = true;
    state->condition.notify_all();
  }

  if (_timerThread.joinable()) {
    if (_timerThread.get_id() == std::this_thread::get_id()) {
      _timerThread.detach();
    } else {
      _timerThread.join();
    }
  }

  {
    std::lock_guard<std::mutex> lock(_stateMutex);
    _state.reset();
  }
}

inline bool Timer::isRunning() const {
  std::lock_guard<std::mutex> lock(_stateMutex);
  return _state && !_state->finished;
}

} // namespace Util
