/*
 * FppClient.h
 *
 * Uploads a local video file to a Falcon Player (FPP) device over HTTP,
 * mirroring xLights' "FPP Connect" media upload.
 *
 * Transfer uses FPP's resumable (tus-style) chunked protocol: repeated
 * HTTP PATCH requests to http://<host>/api/file/videos carrying
 * Upload-Offset / Upload-Length / Upload-Name headers, 16 MB at a time.
 * If that endpoint is unavailable (older FPP), it falls back to the legacy
 * POST http://<host>/api/file/uploads/<name> + GET /api/file/move/<name>.
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

#ifndef FPPCLIENT_H
#define FPPCLIENT_H

#include <QObject>
#include <QString>
#include <QPointer>

QT_BEGIN_NAMESPACE
class QNetworkAccessManager;
class QNetworkReply;
class QFile;
QT_END_NAMESPACE

namespace mmp {

/// Talks to a single FPP device: identity check + video file upload.
class FppClient : public QObject {
  Q_OBJECT

public:
  explicit FppClient(QObject* parent = nullptr);
  ~FppClient() override;

  /// GET http://<host>/api/system/info to confirm the device is an FPP and
  /// report its name/version. Emits deviceVerified or verifyFailed.
  void verify(const QString& host);

  /// Upload a local video file to the device's "videos" media directory.
  /// Emits uploadProgress / uploadFinished / uploadFailed.
  void uploadVideo(const QString& host, const QString& localFilePath);

  /// Abort an in-flight verify or upload.
  void cancel();

  bool isBusy() const { return _busy; }

  /// Strip any scheme and trailing slash from a user-entered host.
  static QString normalizeHost(const QString& host);
  /// True if the extension (no dot) maps to FPP's "videos" media directory.
  static bool isVideoExtension(const QString& extNoDot);

signals:
  void deviceVerified(const QString& hostName, const QString& version,
                      const QString& platform);
  void verifyFailed(const QString& error);

  void uploadProgress(qint64 sent, qint64 total);
  void uploadFinished(const QString& remoteName);
  void uploadFailed(const QString& error);

private:
  void startV7Chunk();                       // PATCH one chunk
  void onV7ChunkFinished();                  // handle a chunk reply
  void startLegacyUpload();                  // POST the whole file
  void onLegacyUploadFinished();
  void onMoveFinished();
  void failUpload(const QString& error);
  void cleanupTransfer();

  QString baseUrl() const;                   // http://<host>

  static constexpr qint64 kChunkSize = 16 * 1024 * 1024; // 16 MB, matches xLights

  QNetworkAccessManager* _nam;
  QPointer<QNetworkReply> _reply;
  QFile*  _file       = nullptr;
  QString _host;                             // normalized, no scheme
  QString _remoteName;                       // file name on the device
  qint64  _fileSize       = 0;
  qint64  _offset         = 0;
  qint64  _lastChunkSize  = 0;
  int     _errorCount     = 0;
  bool    _busy           = false;
  bool    _triedLegacy    = false;
};

} // namespace mmp

#endif // FPPCLIENT_H
