// MIT License
// Copyright (c) 2026 Sourcerer

#pragma once

#include "HgRepoManagerStructs.h"

#include <Events.h>

namespace HgRepoManager::Workers {

// The repository a worker thread runs commands for.
struct Session {
  QString uuid;
  HgRepo::Repository *repo = nullptr;
  const bool *quit = nullptr;
};

struct Outcome {
  bool success = false;
  QVariant value;
  Structs::HgRepoError error;
};

// Runs command on the current thread. Open and close change session.repo.
Outcome execute(Session &session, const Structs::HgRepoCommand &command);

EventManager::EventType resultEventType(Structs::HgRepoCommandType type);
EventManager::EventType errorEventType(Structs::HgRepoCommandType type);

// Sends the result or error event answering command.
void reply(const Structs::HgRepoCommand &command, const Outcome &outcome);

// Prepares the current thread for HgRepo, GitRepo and SvnRepo, which the
// Git and Subversion subrepos are handled with.
void initThread(const bool *quit);
void shutdownThread();

} // namespace HgRepoManager::Workers
