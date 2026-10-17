#pragma once

// Stands in for GitCredentialsOverlayWidget: answers the credential and
// passphrase prompts from a list, cancels when the list is used up, and
// records the prompt events ("prompt", "invalid", "finished").

#include <GitRepo.h>

#include <EventManager.h>

#include <QMutex>
#include <QMutexLocker>
#include <QPair>
#include <QString>
#include <QStringList>
#include <QVector>

class FakeCredentialsOverlay {
public:
  FakeCredentialsOverlay() {
    auto &eventManager = EventManager::EventManager::getInstance();
    for (const EventManager::EventType type : kTypes) {
      eventManager.subscribe({type, this, _handler});
    }
  }

  ~FakeCredentialsOverlay() {
    auto &eventManager = EventManager::EventManager::getInstance();
    for (const EventManager::EventType type : kTypes) {
      eventManager.unsubscribe({type, this, _handler});
    }
  }

  FakeCredentialsOverlay(const FakeCredentialsOverlay &) = delete;
  FakeCredentialsOverlay &operator=(const FakeCredentialsOverlay &) = delete;

  // Username and password (or passphrase) per prompt, in order.
  void setAnswers(const QVector<QPair<QString, QString>> &answers) {
    QMutexLocker locker(&_mutex);
    _answers = answers;
    _events.clear();
    _requests.clear();
    _finishedIds.clear();
  }

  QStringList events() const {
    QMutexLocker locker(&_mutex);
    return _events;
  }

  QVector<GitRepo::GitCredentialsRequest> requests() const {
    QMutexLocker locker(&_mutex);
    return _requests;
  }

  // The prompt id of every "finished" event, in order.
  QVector<size_t> finishedIds() const {
    QMutexLocker locker(&_mutex);
    return _finishedIds;
  }

private:
  static constexpr EventManager::EventType kTypes[] = {
      EventManager::EventType::GitRepoManagerGetCredentials,
      EventManager::EventType::GitRepoManagerGetCredentialsInvalid,
      EventManager::EventType::GitRepoManagerGetCredentialsFinished};

  void onEvent(const EventManager::Event &event) {
    QMutexLocker locker(&_mutex);
    if (event.type() == EventManager::EventType::GitRepoManagerGetCredentialsInvalid) {
      _events << QStringLiteral("invalid");
      return;
    }
    if (event.type() ==
        EventManager::EventType::GitRepoManagerGetCredentialsFinished) {
      _events << QStringLiteral("finished");
      _finishedIds << event.data<size_t>();
      return;
    }

    const auto request = event.data<GitRepo::GitCredentialsRequest>();
    _requests << request;
    _events << QStringLiteral("prompt");

    auto &eventManager = EventManager::EventManager::getInstance();
    if (_answers.isEmpty()) {
      EventManager::Event cancel(
          EventManager::EventType::GitRepoManagerGetCredentialsCancel);
      cancel.addData(GitRepo::GitCredentialsErrorResponse{request.id});
      eventManager.send(cancel);
      return;
    }
    const auto [username, secret] = _answers.takeFirst();
    GitRepo::GitCredentialsResponse response;
    response.id = request.id;
    response.username = username;
    if (request.needsPassphrase) {
      response.passphrase = secret;
      response.needPassphrase = true;
    } else {
      response.password = secret;
    }
    response.rememberCredentials = false;
    EventManager::Event submit(
        EventManager::EventType::GitRepoManagerGetCredentialsSubmit);
    submit.addData(response);
    eventManager.send(submit);
  }

  // One callback for subscribe and unsubscribe: EventManager tells
  // subscriptions apart by the callback.
  const EventManager::CallbackType _handler =
      [this](EventManager::Event event) { onEvent(event); };
  mutable QMutex _mutex;
  QVector<QPair<QString, QString>> _answers;
  QStringList _events;
  QVector<GitRepo::GitCredentialsRequest> _requests;
  QVector<size_t> _finishedIds;
};
