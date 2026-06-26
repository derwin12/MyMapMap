/*
 * ThumbnailCache.cpp
 */

#include "ThumbnailCache.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QMediaPlayer>
#include <QTimer>
#include <QUrl>
#include <QVideoFrame>
#include <QVideoSink>

namespace mmp {

// ─────────────────────────────────────────────────────────────────────────────
// VideoFrameExtractor — internal async worker (defined here, not in the header)
// ─────────────────────────────────────────────────────────────────────────────

class VideoFrameExtractor : public QObject
{
  Q_OBJECT

public:
  explicit VideoFrameExtractor(QObject* parent = nullptr);
  void extract(const QString& videoPath, const QString& outDir,
               int numFrames = ThumbnailCache::NUM_FRAMES);

signals:
  void done(const QString& videoPath, const QStringList& frames);
  void failed(const QString& videoPath);

private slots:
  void onMediaStatusChanged(QMediaPlayer::MediaStatus status);
  void onVideoFrameChanged(const QVideoFrame& frame);
  void onTimeout();

private:
  void _seekToNext();

  QMediaPlayer* _player   = nullptr;
  QVideoSink*   _sink     = nullptr;
  QTimer*       _timeout  = nullptr;

  QString     _videoPath;
  QString     _outDir;
  int         _numFrames  = ThumbnailCache::NUM_FRAMES;
  int         _frameIndex = 0;
  qint64      _duration   = 0;
  bool        _capturing  = false;
  QStringList _saved;
};

VideoFrameExtractor::VideoFrameExtractor(QObject* parent) : QObject(parent) {}

void VideoFrameExtractor::extract(const QString& videoPath, const QString& outDir, int numFrames)
{
  _videoPath  = videoPath;
  _outDir     = outDir;
  _numFrames  = numFrames;
  _frameIndex = 0;
  _capturing  = false;
  _saved.clear();

  QDir().mkpath(outDir);

  _timeout = new QTimer(this);
  _timeout->setSingleShot(true);
  _timeout->setInterval(30000); // 30 s overall cap
  connect(_timeout, &QTimer::timeout, this, &VideoFrameExtractor::onTimeout);
  _timeout->start();

  _player = new QMediaPlayer(this);
  _sink   = new QVideoSink(this);
  _player->setVideoSink(_sink);

  connect(_player, &QMediaPlayer::mediaStatusChanged,
          this, &VideoFrameExtractor::onMediaStatusChanged);
  connect(_sink, &QVideoSink::videoFrameChanged,
          this, &VideoFrameExtractor::onVideoFrameChanged);

  _player->setSource(QUrl::fromLocalFile(videoPath));
  _player->play();
}

void VideoFrameExtractor::onMediaStatusChanged(QMediaPlayer::MediaStatus status)
{
  using S = QMediaPlayer::MediaStatus;
  if (status == S::LoadedMedia || status == S::BufferedMedia) {
    _duration = _player->duration();
    if (_duration > 0 && _frameIndex == 0)
      _seekToNext();
  } else if (status == S::InvalidMedia) {
    emit failed(_videoPath);
    deleteLater();
  }
}

void VideoFrameExtractor::onVideoFrameChanged(const QVideoFrame& frame)
{
  if (!_capturing || !frame.isValid())
    return;

  _capturing = false;

  QImage img = frame.toImage();
  if (!img.isNull()) {
    QString path = QString("%1/frame_%2.png")
                       .arg(_outDir)
                       .arg(_frameIndex, 3, 10, QChar('0'));
    img.scaled(320, 200, Qt::KeepAspectRatio, Qt::SmoothTransformation)
       .save(path, "PNG");
    _saved << path;
  }

  _frameIndex++;
  _seekToNext();
}

void VideoFrameExtractor::_seekToNext()
{
  if (_frameIndex >= _numFrames || _duration <= 0) {
    _timeout->stop();
    if (!_saved.isEmpty())
      emit done(_videoPath, _saved);
    else
      emit failed(_videoPath);
    deleteLater();
    return;
  }

  qint64 pos = (_frameIndex * _duration) / _numFrames;
  _player->setPosition(pos);

  // Brief delay after seek before enabling capture, so buffered
  // pre-seek frames don't get recorded at the wrong position.
  QTimer::singleShot(300, this, [this]() { _capturing = true; });
}

void VideoFrameExtractor::onTimeout()
{
  if (!_saved.isEmpty())
    emit done(_videoPath, _saved);
  else
    emit failed(_videoPath);
  deleteLater();
}

// ─────────────────────────────────────────────────────────────────────────────
// ThumbnailCache
// ─────────────────────────────────────────────────────────────────────────────

QString ThumbnailCache::cacheDir(const QString& videoPath, const QString& cacheBaseDir)
{
  QString hash = QCryptographicHash::hash(
                     videoPath.toUtf8(), QCryptographicHash::Sha1)
                     .toHex()
                     .left(16);
  return cacheBaseDir + "/" + hash;
}

QStringList ThumbnailCache::cachedFrames(const QString& videoPath, const QString& cacheBaseDir)
{
  QString dir = cacheDir(videoPath, cacheBaseDir);
  QStringList frames;
  for (int i = 0; i < NUM_FRAMES; ++i) {
    QString path = QString("%1/frame_%2.png").arg(dir).arg(i, 3, 10, QChar('0'));
    if (!QFileInfo::exists(path))
      return {};
    frames << path;
  }
  return frames;
}

ThumbnailCache::ThumbnailCache(QObject* parent) : QObject(parent) {}

void ThumbnailCache::request(const QString& videoPath, const QString& cacheBaseDir)
{
  QStringList cached = cachedFrames(videoPath, cacheBaseDir);
  if (!cached.isEmpty()) {
    QTimer::singleShot(0, this, [this, videoPath, cached]() {
      emit ready(videoPath, cached);
    });
    return;
  }

  // Avoid duplicate queued requests for the same path
  for (const Job& j : _queue) {
    if (j.videoPath == videoPath)
      return;
  }
  if (_active) {
    // Check if active extractor is already working on this path — no easy way
    // to check, so just queue it; cachedFrames will short-circuit on the next call.
  }

  _queue.enqueue({videoPath, cacheBaseDir});
  if (!_active)
    processQueue();
}

void ThumbnailCache::processQueue()
{
  if (_queue.isEmpty() || _active)
    return;

  Job job = _queue.dequeue();

  // Double-check cache before starting extraction (might have been filled
  // by a previous run between enqueue and now).
  QStringList cached = cachedFrames(job.videoPath, job.cacheBaseDir);
  if (!cached.isEmpty()) {
    emit ready(job.videoPath, cached);
    processQueue();
    return;
  }

  auto* extractor = new VideoFrameExtractor(this);
  _active = extractor;

  connect(extractor, &VideoFrameExtractor::done,
          this, &ThumbnailCache::onExtractorDone);
  connect(extractor, &VideoFrameExtractor::failed,
          this, &ThumbnailCache::onExtractorFailed);

  extractor->extract(job.videoPath, cacheDir(job.videoPath, job.cacheBaseDir));
}

void ThumbnailCache::onExtractorDone(const QString& videoPath, const QStringList& frames)
{
  _active = nullptr;
  emit ready(videoPath, frames);
  processQueue();
}

void ThumbnailCache::onExtractorFailed(const QString& videoPath)
{
  _active = nullptr;
  emit failed(videoPath);
  processQueue();
}

} // namespace mmp

// VideoFrameExtractor is defined in this .cpp file so MOC must be included here.
#include "ThumbnailCache.moc"
