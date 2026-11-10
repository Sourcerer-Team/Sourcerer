// MIT License
// Copyright (c) 2026 Sourcerer

#pragma once

#include <QString>
#include <QVector>

// SSH host keys in the user's known_hosts file, read and written with the
// OpenSSH tools (ssh-keygen, ssh-keyscan). Mercurial's ssh refuses hosts whose
// key isn't there, the SSH key manager adds them.
namespace GitRepo::SshKnownHosts {

struct HostKey {
  // The known_hosts line, "host type key".
  QString line;
  // e.g. "ED25519".
  QString type;
  // e.g. "SHA256:...".
  QString fingerprint;
};

// ~/.ssh/known_hosts.
QString defaultFile();

// Whether knownHostsFile has a key for host, hashed entries included.
bool isKnown(const QString &host,
             const QString &knownHostsFile = defaultFile());

// The host keys host offers on port, with their fingerprints. Connects to the
// host, so it blocks for up to timeoutSeconds. Empty and error set on failure.
QVector<HostKey> scan(const QString &host, QString *error, int port = 22,
                      int timeoutSeconds = 10);

// Appends keys to knownHostsFile, creating it and its directory.
bool add(const QVector<HostKey> &keys, QString *error,
         const QString &knownHostsFile = defaultFile());

} // namespace GitRepo::SshKnownHosts
