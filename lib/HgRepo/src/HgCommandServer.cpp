#include "HgCommandServer.h"

#include <HgRepo.h>

#include <QMutex>
#include <QMutexLocker>
#include <QtEndian>

namespace HgRepo {

namespace {

QMutex gAskPassMutex;
QString gAskPassProgram;
QString gAskPassSocket;
QString gAskPassToken;

} // namespace

void setAskPass(const QString &program, const QString &socketName,
                const QString &token) {
  QMutexLocker locker(&gAskPassMutex);
  gAskPassProgram = program;
  gAskPassSocket = socketName;
  gAskPassToken = token;
}

} // namespace HgRepo

namespace HgRepo::Private {

namespace {

// Wait slice between checks of the quit flag.
constexpr int kReadSliceMs = 100;
constexpr int kWriteTimeoutMs = 30 * 1000;
constexpr int kStartTimeoutMs = 30 * 1000;
constexpr int kStopTimeoutMs = 3 * 1000;

// Header of every message the server sends: the channel and the length of
// the data, or of the data requested for an input channel.
constexpr qint64 kHeaderSize = 5;

QByteArray uint32BigEndian(quint32 value) {
  QByteArray bytes(4, Qt::Uninitialized);
  qToBigEndian(value, bytes.data());
  return bytes;
}

} // namespace

QProcessEnvironment hgEnvironment() {
  QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
  env.insert(QStringLiteral("HGPLAIN"), QStringLiteral("1"));
  env.insert(QStringLiteral("HGENCODING"), QStringLiteral("UTF-8"));
  env.insert(QStringLiteral("HGENCODINGMODE"), QStringLiteral("strict"));
  env.remove(QStringLiteral("HGPLAINEXCEPT"));
  // Mercurial runs git for Git subrepos with URLs taken from .hgsub. Without
  // a limit set by the user, git may only use the plain transports, so a
  // repository can't make it run commands through "ext::".
  if (!env.contains(QStringLiteral("GIT_ALLOW_PROTOCOL"))) {
    env.insert(QStringLiteral("GIT_ALLOW_PROTOCOL"),
               QStringLiteral("file:git:http:https:ssh"));
  }

  // ssh asks Sourcerer for key passphrases, without a terminal it would
  // fail.
  QMutexLocker locker(&gAskPassMutex);
  if (!gAskPassProgram.isEmpty()) {
    env.insert(QStringLiteral("SSH_ASKPASS"), gAskPassProgram);
    env.insert(QStringLiteral("SSH_ASKPASS_REQUIRE"), QStringLiteral("force"));
    env.insert(QStringLiteral("SOURCERER_ASKPASS_SOCKET"), gAskPassSocket);
    env.insert(QStringLiteral("SOURCERER_ASKPASS_TOKEN"), gAskPassToken);
  }
  return env;
}

bool hasAskPass() {
  QMutexLocker locker(&gAskPassMutex);
  return !gAskPassProgram.isEmpty();
}

void hideConsoleWindow(QProcess &process) {
#ifdef Q_OS_WIN
  process.setCreateProcessArgumentsModifier(
      [](QProcess::CreateProcessArguments *arguments) {
        // CREATE_NO_WINDOW
        arguments->flags |= 0x08000000;
      });
#else
  Q_UNUSED(process);
#endif
}

HgCommandServer::HgCommandServer(const QString &executable,
                                 const QString &workingDirectory,
                                 const QStringList &configArgs)
    : _executable(executable), _workingDirectory(workingDirectory),
      _configArgs(configArgs) {}

HgCommandServer::~HgCommandServer() {
  stop();
}

bool HgCommandServer::start() {
  if (isRunning()) {
    return true;
  }

  _errorMessage.clear();

  QStringList arguments = _configArgs;
  arguments << QStringLiteral("serve") << QStringLiteral("--cmdserver")
            << QStringLiteral("pipe");

  _process.setProgram(_executable);
  _process.setArguments(arguments);
  _process.setWorkingDirectory(_workingDirectory);
  _process.setProcessEnvironment(hgEnvironment());
  _process.setProcessChannelMode(QProcess::SeparateChannels);
  hideConsoleWindow(_process);
  _process.start(QIODevice::ReadWrite);

  if (!_process.waitForStarted(kStartTimeoutMs)) {
    _errorMessage = QStringLiteral("Failed to start %1: %2")
                        .arg(_executable, _process.errorString());
    kill();
    return false;
  }

  // The hello message: "capabilities: ...\nencoding: ...\n...".
  bool cancelled = false;
  QByteArray header;
  if (!readExactly(&header, kHeaderSize, nullptr, &cancelled) ||
      header.at(0) != 'o') {
    const QByteArray stderrOutput = _process.readAllStandardError().trimmed();
    _errorMessage =
        stderrOutput.isEmpty()
            ? QStringLiteral("The Mercurial command server did not start.")
            : QString::fromUtf8(stderrOutput);
    kill();
    return false;
  }

  QByteArray hello;
  const quint32 length = qFromBigEndian<quint32>(header.constData() + 1);
  if (!readExactly(&hello, length, nullptr, &cancelled)) {
    kill();
    return false;
  }

  bool runCommand = false;
  QString encoding;
  for (const QByteArray &line : hello.split('\n')) {
    if (line.startsWith("capabilities:")) {
      runCommand = line.mid(13).split(' ').contains("runcommand");
    } else if (line.startsWith("encoding:")) {
      encoding = QString::fromLatin1(line.mid(9).trimmed());
    }
  }

  if (!runCommand) {
    _errorMessage = QStringLiteral(
        "The Mercurial command server does not support runcommand.");
    kill();
    return false;
  }

  if (encoding.compare(QStringLiteral("UTF-8"), Qt::CaseInsensitive) != 0) {
    _errorMessage =
        QStringLiteral("The Mercurial command server uses the %1 encoding "
                       "instead of UTF-8.")
            .arg(encoding);
    kill();
    return false;
  }

  return true;
}

void HgCommandServer::stop() {
  if (_process.state() == QProcess::NotRunning) {
    return;
  }

  // The server exits at the end of its input.
  _process.closeWriteChannel();
  if (!_process.waitForFinished(kStopTimeoutMs)) {
    kill();
  }
}

bool HgCommandServer::isRunning() const {
  return _process.state() == QProcess::Running;
}

HgCommandResult HgCommandServer::run(const QStringList &args,
                                     const QByteArray &input,
                                     const bool *quitFlag) {
  HgCommandResult result;
  _errorMessage.clear();

  if (!isRunning()) {
    _errorMessage = QStringLiteral("The Mercurial command server is not "
                                   "running.");
    return result;
  }

  QByteArray payload;
  for (int i = 0; i < args.size(); ++i) {
    if (i > 0) {
      payload += '\0';
    }
    payload += args.at(i).toUtf8();
  }

  if (!writeAll(QByteArrayLiteral("runcommand\n") +
                uint32BigEndian(static_cast<quint32>(payload.size())) +
                payload)) {
    return result;
  }

  qint64 inputOffset = 0;

  while (true) {
    QByteArray header;
    if (!readExactly(&header, kHeaderSize, quitFlag, &result.cancelled)) {
      return result;
    }

    const char channel = header.at(0);
    const quint32 length = qFromBigEndian<quint32>(header.constData() + 1);

    switch (channel) {
    case 'o':
    case 'e':
    case 'd': {
      QByteArray data;
      if (!readExactly(&data, length, quitFlag, &result.cancelled)) {
        return result;
      }
      if (channel == 'o') {
        result.out += data;
      } else if (channel == 'e') {
        result.err += data;
      }
      break;
    }
    case 'r': {
      QByteArray data;
      if (!readExactly(&data, length, quitFlag, &result.cancelled) ||
          data.size() < 4) {
        return result;
      }
      result.exitCode = qFromBigEndian<qint32>(data.constData());
      result.finished = true;
      return result;
    }
    case 'I':
    case 'L': {
      // Input requested: at most length bytes, a single line for 'L'. An
      // empty answer is the end of the input.
      QByteArray chunk = input.mid(inputOffset, length);
      if (channel == 'L') {
        const int newline = chunk.indexOf('\n');
        if (newline >= 0) {
          chunk = chunk.left(newline + 1);
        }
      }
      inputOffset += chunk.size();
      if (!writeAll(uint32BigEndian(static_cast<quint32>(chunk.size())) +
                    chunk)) {
        return result;
      }
      break;
    }
    default:
      if (channel >= 'A' && channel <= 'Z') {
        // Required channels must be understood.
        _errorMessage =
            QStringLiteral("Unknown Mercurial command server channel '%1'.")
                .arg(QChar::fromLatin1(channel));
        kill();
        return result;
      }
      QByteArray ignored;
      if (!readExactly(&ignored, length, quitFlag, &result.cancelled)) {
        return result;
      }
      break;
    }
  }
}

bool HgCommandServer::readExactly(QByteArray *data, qint64 size,
                                  const bool *quitFlag, bool *cancelled) {
  data->clear();
  data->reserve(static_cast<int>(size));

  while (data->size() < size) {
    if (quitFlag && *quitFlag) {
      // A running command can't be interrupted through the protocol.
      *cancelled = true;
      _errorMessage = QStringLiteral("The operation was cancelled.");
      kill();
      return false;
    }

    if (_process.bytesAvailable() == 0 &&
        !_process.waitForReadyRead(kReadSliceMs)) {
      if (_process.state() != QProcess::Running) {
        const QByteArray stderrOutput =
            _process.readAllStandardError().trimmed();
        _errorMessage =
            stderrOutput.isEmpty()
                ? QStringLiteral("The Mercurial command server exited.")
                : QString::fromUtf8(stderrOutput);
        return false;
      }
      continue;
    }

    *data += _process.read(size - data->size());
  }

  return true;
}

bool HgCommandServer::writeAll(const QByteArray &data) {
  if (_process.write(data) != data.size()) {
    _errorMessage = QStringLiteral("Failed to write to the Mercurial command "
                                   "server: %1")
                        .arg(_process.errorString());
    return false;
  }

  while (_process.bytesToWrite() > 0) {
    if (!_process.waitForBytesWritten(kWriteTimeoutMs)) {
      _errorMessage = QStringLiteral("Failed to write to the Mercurial command "
                                     "server: %1")
                          .arg(_process.errorString());
      return false;
    }
  }

  return true;
}

void HgCommandServer::kill() {
  if (_process.state() != QProcess::NotRunning) {
    _process.kill();
    _process.waitForFinished(kStopTimeoutMs);
  }
}

} // namespace HgRepo::Private
