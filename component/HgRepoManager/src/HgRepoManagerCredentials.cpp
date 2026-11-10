// MIT License
// Copyright (c) 2026 Sourcerer

#include "HgRepoManagerCredentials.h"

#include <EventManager.h>
#include <GitCredentialStorage.h>
#include <GitKeyStorage.h>
#include <GitRepo.h>
#include <Log.h>

#include <QHash>

#include <set>

#define THREAD_ID "HgRepoManagerCredentials"

namespace HgRepoManager::Workers {

namespace {

void sendPromptSignal(const EventManager::EventType type,
                      const std::size_t id) {
  EventManager::Event event(type);
  event.addData(id);
  EventManager::EventManager::getInstance().send(event);
}

} // namespace

bool EventCredentialsProvider::credentials(const HgRepo::HgAuthRequest &request,
                                           HgRepo::HgCredentials *credentials) {
  if (request.attempt == 0) {
    _id = qHash(QStringLiteral("hg|%1|%2").arg(request.repoPath, request.url));
    _prompted = false;
    _fromStorage = false;

    // Stored credentials are tried first, the user is asked when they are
    // rejected.
    QString username;
    QString password;
    if (GitRepo::GitCredentialStorage::getInstance().getCredentials(
            request.repoPath, username, password)) {
      LOG_INFO("Using stored credentials for {}", request.url.toStdString());
      _fromStorage = true;
      *credentials = HgRepo::HgCredentials{username, password};
      return true;
    }
  }

  return prompt(request, credentials);
}

bool EventCredentialsProvider::prompt(const HgRepo::HgAuthRequest &request,
                                      HgRepo::HgCredentials *credentials) {
  EventManager::EventManager &eventManager =
      EventManager::EventManager::getInstance();

  // Waiting starts before the overlay can answer.
  const int token = eventManager.startWait();
  if (_prompted) {
    sendPromptSignal(
        EventManager::EventType::GitRepoManagerGetCredentialsInvalid, _id);
  } else {
    GitRepo::GitCredentialsRequest prompt;
    prompt.id = _id;
    prompt.url = request.url;
    // The overlay fills in the stored credentials itself.
    prompt.username = QString();
    prompt.allowedTypes = 0;
    prompt.repoPath = request.repoPath;
    prompt.needsPassphrase = false;

    EventManager::Event event(
        EventManager::EventType::GitRepoManagerGetCredentials);
    event.addData(prompt);
    eventManager.send(event);
    _prompted = true;
  }
  _fromStorage = false;

  const std::size_t id = _id;
  const auto predicate = [id](EventManager::Event event) -> bool {
    switch (event.type()) {
    case EventManager::EventType::GitRepoManagerGetCredentialsCancel:
      return event.data<GitRepo::GitCredentialsErrorResponse>().id == id;
    case EventManager::EventType::GitRepoManagerGetCredentialsSubmit:
      return event.data<GitRepo::GitCredentialsResponse>().id == id;
    default:
      return false;
    }
  };

  const std::set<EventManager::EventType> types{
      EventManager::EventType::GitRepoManagerGetCredentialsCancel,
      EventManager::EventType::GitRepoManagerGetCredentialsSubmit};
  const auto &[success, event] =
      eventManager.waitOneOf(token, types, predicate);
  if (!success ||
      event.type() ==
          EventManager::EventType::GitRepoManagerGetCredentialsCancel) {
    // The overlay closes itself on Cancel.
    _prompted = false;
    return false;
  }

  const auto response = event.data<GitRepo::GitCredentialsResponse>();
  _answer = HgRepo::HgCredentials{response.username, response.password};
  _remember = response.rememberCredentials;
  *credentials = _answer;
  return true;
}

void EventCredentialsProvider::finished(const HgRepo::HgAuthRequest &request,
                                        const HgRepo::HgAuthResult result) {
  switch (result) {
  case HgRepo::HgAuthResult::Accepted:
    if (_prompted) {
      sendPromptSignal(
          EventManager::EventType::GitRepoManagerGetCredentialsFinished, _id);
      if (_remember) {
        GitRepo::GitCredentialStorage::getInstance().storeCredentials(
            request.repoPath, _answer.username, _answer.password, true);
      }
      _prompted = false;
    }
    break;
  case HgRepo::HgAuthResult::Rejected:
    // The next credentials() call tells the overlay.
    break;
  case HgRepo::HgAuthResult::GaveUp:
    if (_prompted) {
      sendPromptSignal(
          EventManager::EventType::GitRepoManagerGetCredentialsFinished, _id);
      _prompted = false;
    }
    break;
  }
}

QVector<HgRepo::HgSshKey> EventCredentialsProvider::sshKeys() {
  QVector<HgRepo::HgSshKey> keys;
  for (const GitRepo::GitKeyPair &pair :
       GitRepo::GitKeyStorage::getInstance().getAllKeyPairs()) {
    keys.append(HgRepo::HgSshKey{pair.host, pair.privateKeyPath});
  }
  return keys;
}

} // namespace HgRepoManager::Workers
