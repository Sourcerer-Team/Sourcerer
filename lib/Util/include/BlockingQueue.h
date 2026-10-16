#ifndef BLOCKING_QUEUE_H
#define BLOCKING_QUEUE_H

#include <QObject>

#include <chrono>
#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>

using namespace std::chrono_literals;

template <typename T> class BlockingQueue : public QObject {
public:
  BlockingQueue() {}
  ~BlockingQueue() override {}

  T pop() {
    std::unique_lock<std::mutex> guard(myMutex);

    while (myQueue.empty()) {
      myCondition.wait(guard);
    }

    auto item = myQueue.front();
    myQueue.pop_front();

    return item;
  }

  void pop(T &item) {
    std::unique_lock<std::mutex> guard(myMutex);

    while (myQueue.empty()) {
      myCondition.wait(guard);
    }

    item = myQueue.front();
    myQueue.pop_front();
  }

  T pop(ulong timeout, const T &defaultValue) {
    std::unique_lock<std::mutex> guard(myMutex);

    const auto start = std::chrono::system_clock::now();
    auto current = start;

    while (
        myQueue.empty() &&
        std::chrono::duration_cast<std::chrono::milliseconds>(current - start)
                .count() <= timeout) {
      myCondition.wait_for(guard, timeout * 1ms);
      current = std::chrono::system_clock::now();
    }

    if (!myQueue.empty()) {
      auto item = myQueue.front();
      myQueue.pop_front();

      return item;
    } else {
      return defaultValue;
    }
  }

  void push(const T &item) {
    std::unique_lock<std::mutex> guard(myMutex);
    myQueue.push_back(item);
    guard.unlock();
    myCondition.notify_one();
  }

  void push(T &&item) {
    std::unique_lock<std::mutex> guard(myMutex);
    myQueue.push_back(std::move(item));
    guard.unlock();
    myCondition.notify_one();
  }

  void push_front(const T &item) {
    std::unique_lock<std::mutex> guard(myMutex);
    myQueue.push_front(item);
    guard.unlock();
    myCondition.notify_one();
  }

  void push_front(T &&item) {
    std::unique_lock<std::mutex> guard(myMutex);
    myQueue.push_front(std::move(item));
    guard.unlock();
    myCondition.notify_one();
  }

  void push(const T &item, std::function<bool(const std::deque<T> &)> pred) {
    std::unique_lock<std::mutex> guard(myMutex);
    if (!pred(myQueue)) {
      return;
    }

    myQueue.push_back(item);
    guard.unlock();
    myCondition.notify_one();
  }

  bool empty() const { return myQueue.empty(); }

  size_t size() const { return myQueue.size(); }

  T last(const T &defaultValue) {
    std::unique_lock<std::mutex> guard(myMutex);

    if (empty()) {
      return defaultValue;
    } else {
      return myQueue.front();
    }
  }

  void transform(std::function<void(T &)> transformFunc) {
    std::unique_lock<std::mutex> guard(myMutex);
    for (auto &t : myQueue) {
      transformFunc(t);
    }
  }

  // Removes all queued items and returns how many were removed.
  size_t clear() {
    std::unique_lock<std::mutex> guard(myMutex);
    const size_t removed = myQueue.size();
    myQueue.clear();
    return removed;
  }

  T operator[](size_t index) const {
    std::unique_lock<std::mutex> guard(myMutex);
    if (index < 0 || index >= myQueue.size()) {
      throw std::out_of_range("Index out of range");
    }

    return myQueue[index];
  }

private:
  std::deque<T> myQueue;
  mutable std::mutex myMutex;
  std::condition_variable myCondition;
};

#endif // BLOCKING_QUEUE_H
