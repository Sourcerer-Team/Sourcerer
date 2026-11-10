// MIT License
// Copyright (c) 2026 Sourcerer

#pragma once

#include <HgRepo.h>

#include <QString>
#include <QVector>

#include <cstddef>

namespace HgRepoManager::Workers {

// Answers the HTTP credential requests of HgRepo with the credentials stored
// for the repository, then with the Git credentials overlay: it sends
// GitRepoManagerGetCredentials, waits for ...Submit or ...Cancel, and closes
// the overlay with ...Finished or asks again with ...Invalid. Gives ssh the
// keys chosen in the SSH key manager. One per thread.
class EventCredentialsProvider final : public HgRepo::HgCredentialsProvider {
public:
  bool credentials(const HgRepo::HgAuthRequest &request,
                   HgRepo::HgCredentials *credentials) override;
  void finished(const HgRepo::HgAuthRequest &request,
                HgRepo::HgAuthResult result) override;
  // The keys chosen per host in the SSH key manager.
  QVector<HgRepo::HgSshKey> sshKeys() override;

private:
  // Shows the overlay, or tells the open one the last answer was rejected,
  // and waits for the user.
  bool prompt(const HgRepo::HgAuthRequest &request,
              HgRepo::HgCredentials *credentials);

  // Overlay of the current request, open while _prompted.
  std::size_t _id = 0;
  bool _prompted = false;
  // The last credentials came from the storage, not from the user.
  bool _fromStorage = false;
  bool _remember = false;
  HgRepo::HgCredentials _answer;
};

} // namespace HgRepoManager::Workers
