/*
 * FppClient.cpp
 *
 * See FppClient.h. Implements FPP device verification and video upload over
 * HTTP, mirroring the protocol xLights' FPP Connect uses.
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

#include "FppClient.h"

#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrl>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>

namespace mmp {

namespace {
// Extensions FPP stores under its "videos" media directory (matches xLights'
// FPP_VIDEO_EXT). Everything else we still send to "videos" for our purposes,
// but this is used to validate the user's selection.
const QStringList kVideoExts = {
  "mp4", "avi", "mov", "mkv", "mpg", "mpeg", "m4v"
};

// Read the HTTP status code from a finished reply (0 if the request never
// reached the server, e.g. connection refused / timeout).
int httpStatus(QNetworkReply* reply) {
  const QVariant v = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
  return v.isValid() ? v.toInt() : 0;
}

QNetworkRequest fppRequest(const QUrl& url) {
  QNetworkRequest req(url);
  // FPP gates some endpoints on this header; xLights sends the same.
  req.setRawHeader("X-Requested-With", "FPPConnect");
  req.setRawHeader("User-Agent", "MyMapMap-FPPConnect");
  return req;
}
} // namespace

FppClient::FppClient(QObject* parent)
  : QObject(parent), _nam(new QNetworkAccessManager(this))
{
}

FppClient::~FppClient()
{
  cleanupTransfer();
}

QString FppClient::normalizeHost(const QString& host)
{
  QString h = host.trimmed();
  if (h.startsWith("http://", Qt::CaseInsensitive))
    h = h.mid(7);
  else if (h.startsWith("https://", Qt::CaseInsensitive))
    h = h.mid(8);
  while (h.endsWith('/'))
    h.chop(1);
  return h;
}

bool FppClient::isVideoExtension(const QString& extNoDot)
{
  return kVideoExts.contains(extNoDot.toLower());
}

QString FppClient::baseUrl() const
{
  return QStringLiteral("http://") + _host;
}

void FppClient::cancel()
{
  if (_reply) {
    _reply->abort();
  }
  cleanupTransfer();
}

void FppClient::cleanupTransfer()
{
  if (_file) {
    _file->close();
    delete _file;
    _file = nullptr;
  }
  _busy = false;
  _offset = 0;
  _lastChunkSize = 0;
  _errorCount = 0;
  _triedLegacy = false;
}

// ---------------------------------------------------------------------------
// Verify
// ---------------------------------------------------------------------------

void FppClient::verify(const QString& host)
{
  _host = normalizeHost(host);
  if (_host.isEmpty()) {
    emit verifyFailed(tr("Enter the FPP device IP address or hostname."));
    return;
  }

  const QUrl url(baseUrl() + "/api/system/info");
  QNetworkReply* reply = _nam->get(fppRequest(url));
  connect(reply, &QNetworkReply::finished, this, [this, reply]() {
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError) {
      emit verifyFailed(tr("Could not reach %1: %2").arg(_host, reply->errorString()));
      return;
    }
    const QByteArray body = reply->readAll();
    QJsonParseError perr;
    const QJsonDocument doc = QJsonDocument::fromJson(body, &perr);
    if (perr.error != QJsonParseError::NoError || !doc.isObject()) {
      emit verifyFailed(tr("%1 did not return a valid FPP response.").arg(_host));
      return;
    }
    const QJsonObject o = doc.object();
    const QString name     = o.value("HostName").toString(_host);
    const QString version  = o.value("Version").toString();
    const QString platform = o.value("Platform").toString(o.value("Variant").toString());
    if (version.isEmpty() && !o.contains("HostName")) {
      emit verifyFailed(tr("%1 does not look like an FPP device.").arg(_host));
      return;
    }
    emit deviceVerified(name, version, platform);
  });
}

// ---------------------------------------------------------------------------
// Upload
// ---------------------------------------------------------------------------

void FppClient::uploadVideo(const QString& host, const QString& localFilePath)
{
  if (_busy) {
    emit uploadFailed(tr("An upload is already in progress."));
    return;
  }

  _host = normalizeHost(host);
  if (_host.isEmpty()) {
    emit uploadFailed(tr("Enter the FPP device IP address or hostname."));
    return;
  }

  _file = new QFile(localFilePath);
  if (!_file->open(QIODevice::ReadOnly)) {
    const QString err = _file->errorString();
    delete _file;
    _file = nullptr;
    emit uploadFailed(tr("Cannot open %1: %2").arg(localFilePath, err));
    return;
  }

  _remoteName  = QFileInfo(localFilePath).fileName();
  _fileSize    = _file->size();
  _offset      = 0;
  _errorCount  = 0;
  _triedLegacy = false;
  _busy        = true;

  if (_fileSize <= 0) {
    failUpload(tr("%1 is empty.").arg(_remoteName));
    return;
  }

  startV7Chunk();
}

void FppClient::startV7Chunk()
{
  if (!_file) {
    failUpload(tr("Upload source file was closed unexpectedly."));
    return;
  }

  _file->seek(_offset);
  const qint64 want = qMin<qint64>(kChunkSize, _fileSize - _offset);
  const QByteArray chunk = _file->read(want);
  if (chunk.size() != want) {
    failUpload(tr("Could not read %1.").arg(_remoteName));
    return;
  }
  _lastChunkSize = chunk.size();

  // FPP resumable upload: PATCH http://<host>/api/file/videos
  QNetworkRequest req = fppRequest(QUrl(baseUrl() + "/api/file/videos"));
  req.setHeader(QNetworkRequest::ContentTypeHeader, "application/offset+octet-stream");
  req.setRawHeader("Upload-Offset", QByteArray::number(_offset));
  req.setRawHeader("Upload-Length", QByteArray::number(_fileSize));
  req.setRawHeader("Upload-Name", _remoteName.toUtf8());

  _reply = _nam->sendCustomRequest(req, "PATCH", chunk);
  connect(_reply, &QNetworkReply::uploadProgress, this,
          [this](qint64 sent, qint64 /*total*/) {
            emit uploadProgress(_offset + sent, _fileSize);
          });
  connect(_reply, &QNetworkReply::finished, this, &FppClient::onV7ChunkFinished);
}

void FppClient::onV7ChunkFinished()
{
  if (!_reply)
    return;
  QNetworkReply* reply = _reply;
  _reply = nullptr;
  reply->deleteLater();

  const int status = httpStatus(reply);
  const bool netError = reply->error() != QNetworkReply::NoError;

  if (status == 200 && !netError) {
    _offset += _lastChunkSize;
    _errorCount = 0;
    emit uploadProgress(_offset, _fileSize);
    if (_offset >= _fileSize) {
      const QString name = _remoteName;
      cleanupTransfer();
      emit uploadFinished(name);
    } else {
      startV7Chunk();
    }
    return;
  }

  // The resumable endpoint is missing on older FPP — fall back to legacy upload.
  if (!_triedLegacy && _offset == 0 &&
      (status == 404 || status == 405 || status == 501)) {
    _triedLegacy = true;
    startLegacyUpload();
    return;
  }

  // Transient failure: restart from the beginning, up to three attempts
  // (matches xLights' retry behaviour).
  if (_errorCount < 3) {
    ++_errorCount;
    _offset = 0;
    startV7Chunk();
    return;
  }

  failUpload(netError
             ? tr("Upload failed: %1").arg(reply->errorString())
             : tr("Upload failed (HTTP %1).").arg(status));
}

void FppClient::startLegacyUpload()
{
  if (!_file) {
    failUpload(tr("Upload source file was closed unexpectedly."));
    return;
  }
  _file->seek(0);

  QNetworkRequest req =
      fppRequest(QUrl(baseUrl() + "/api/file/uploads/" + _remoteName));
  req.setHeader(QNetworkRequest::ContentTypeHeader, "application/octet-stream");

  _reply = _nam->post(req, _file); // streams the file
  connect(_reply, &QNetworkReply::uploadProgress, this,
          [this](qint64 sent, qint64 total) {
            emit uploadProgress(sent, total > 0 ? total : _fileSize);
          });
  connect(_reply, &QNetworkReply::finished, this, &FppClient::onLegacyUploadFinished);
}

void FppClient::onLegacyUploadFinished()
{
  if (!_reply)
    return;
  QNetworkReply* reply = _reply;
  _reply = nullptr;
  reply->deleteLater();

  if (reply->error() != QNetworkReply::NoError || httpStatus(reply) != 200) {
    failUpload(tr("Upload failed: %1").arg(reply->errorString()));
    return;
  }

  // Legacy upload lands in a staging dir; ask FPP to move it into place.
  const QUrl url(baseUrl() + "/api/file/move/" + _remoteName);
  _reply = _nam->get(fppRequest(url));
  connect(_reply, &QNetworkReply::finished, this, &FppClient::onMoveFinished);
}

void FppClient::onMoveFinished()
{
  if (!_reply)
    return;
  QNetworkReply* reply = _reply;
  _reply = nullptr;
  reply->deleteLater();

  // A failed move isn't fatal — the file uploaded — but report it if it errored.
  if (reply->error() != QNetworkReply::NoError) {
    failUpload(tr("Uploaded, but could not finalize on the device: %1")
               .arg(reply->errorString()));
    return;
  }
  const QString name = _remoteName;
  cleanupTransfer();
  emit uploadFinished(name);
}

void FppClient::failUpload(const QString& error)
{
  cleanupTransfer();
  emit uploadFailed(error);
}

} // namespace mmp
