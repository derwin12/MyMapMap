/*
 * FppMultiSyncListener.h
 *
 * (c) 2026 MyMapMap contributors
 *
 * Listens for Falcon Player (FPP) MultiSync UDP control packets and emits
 * high-level transport signals so MyMapMap can chase an FPP show clock.
 *
 * FPP broadcasts small "FPPD" control packets on UDP port 32320. The sync
 * packets carry the currently playing media/sequence filename plus an elapsed
 * time, which lets a follower keep its own playback locked to the master.
 * See the FPP source (fppd MultiSync.{h,cpp}) for the wire format.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef FPP_MULTISYNC_LISTENER_H_
#define FPP_MULTISYNC_LISTENER_H_

#include <QObject>
#include <QString>

class QUdpSocket;

namespace mmp {

/**
 * Receives FPP MultiSync packets on a UDP port (default 32320) and turns the
 * SYNC control packets into Qt signals. The listener is purely a protocol
 * decoder; deciding what to do with the signals (play/seek which source) is
 * left to whoever connects to them (see MainWindow).
 *
 * Lives in the GUI thread, so connected slots run on the GUI thread and may
 * touch the model / players directly.
 */
class FppMultiSyncListener : public QObject
{
  Q_OBJECT

public:
  /// FPP's well-known MultiSync control port.
  static const quint16 DEFAULT_PORT = 32320;

  explicit FppMultiSyncListener(quint16 port = DEFAULT_PORT, QObject* parent = nullptr);
  ~FppMultiSyncListener() override;

  /// Binds the UDP socket and begins listening. Returns false if the bind
  /// failed (e.g. another process holds the port without address sharing).
  bool start();

  /// Stops listening and releases the socket.
  void stop();

  bool isListening() const;
  quint16 port() const { return _port; }

  void setVerbose(bool verbose) { _verbose = verbose; }

signals:
  /// Master started playing a file at the given elapsed position (seconds).
  void mediaStart(const QString& filename, double secondsElapsed);
  /// Master stopped the given file.
  void mediaStop(const QString& filename);
  /// Periodic position update — followers should drift-correct to this.
  void mediaSync(const QString& filename, double secondsElapsed, quint32 frameNumber);
  /// Master is opening/preparing a file (sent before playback starts).
  void mediaOpen(const QString& filename);

private slots:
  void onReadyRead();

private:
  void parseDatagram(const char* data, int len);

  QUdpSocket* _socket;
  quint16     _port;
  bool        _verbose;
};

}

#endif /* FPP_MULTISYNC_LISTENER_H_ */
