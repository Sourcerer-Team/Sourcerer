// SSH_ASKPASS program of Sourcerer. ssh, started by Mercurial, runs it with a
// prompt (e.g. "Enter passphrase for key '...': ") when it needs a key
// passphrase. It hands the prompt to Sourcerer over the local socket named by
// SOURCERER_ASKPASS_SOCKET, which asks the user, and prints the answer for
// ssh. Exits with 1 when there is no answer, which ssh takes as cancelled.
//
// Protocol, one line each: the client sends SOURCERER_ASKPASS_TOKEN and the
// base64 of the prompt, the server answers "OK <base64 answer>" or "NO".

#include <QByteArray>
#include <QCoreApplication>
#include <QLocalSocket>
#include <QString>
#include <QStringList>

#include <cstdio>

int main(int argc, char *argv[]) {
  QCoreApplication app(argc, argv);

  const QString socketName = qEnvironmentVariable("SOURCERER_ASKPASS_SOCKET");
  const QByteArray token = qgetenv("SOURCERER_ASKPASS_TOKEN");
  if (socketName.isEmpty() || token.isEmpty()) {
    return 1;
  }

  const QStringList arguments = QCoreApplication::arguments();
  const QByteArray prompt =
      arguments.size() > 1 ? arguments.mid(1).join(' ').toUtf8() : QByteArray();

  QLocalSocket socket;
  socket.connectToServer(socketName);
  if (!socket.waitForConnected(5000)) {
    return 1;
  }

  socket.write(token + '\n' + prompt.toBase64() + '\n');
  if (!socket.waitForBytesWritten(5000)) {
    return 1;
  }

  // The user may take a while to answer.
  QByteArray reply;
  while (!reply.contains('\n')) {
    if (!socket.waitForReadyRead(-1)) {
      reply += socket.readAll();
      break;
    }
    reply += socket.readAll();
  }

  if (!reply.startsWith("OK ")) {
    return 1;
  }

  const QByteArray answer =
      QByteArray::fromBase64(reply.mid(3, reply.indexOf('\n') - 3));
  std::fwrite(answer.constData(), 1, static_cast<size_t>(answer.size()),
              stdout);
  std::fputc('\n', stdout);
  std::fflush(stdout);
  return 0;
}
