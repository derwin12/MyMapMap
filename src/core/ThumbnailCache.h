/*
 * ThumbnailCache.h
 *
 * Async video thumbnail generator. Extracts NUM_FRAMES evenly-spaced
 * PNG frames per video and stores them in:
 *   <cacheBaseDir>/<sha1-of-path>/frame_NNN.png
 *
 * If frames already exist they are returned immediately (via queued signal).
 */

#ifndef THUMBNAILCACHE_H_
#define THUMBNAILCACHE_H_

#include <QObject>
#include <QStringList>
#include <QQueue>

namespace mmp {

class VideoFrameExtractor;

class ThumbnailCache : public QObject
{
  Q_OBJECT

public:
  static const int NUM_FRAMES = 8;

  explicit ThumbnailCache(QObject* parent = nullptr);

  /// Request thumbnails for videoPath. Emits ready() when done (may be immediate if cached).
  void request(const QString& videoPath, const QString& cacheBaseDir);

  /// Returns frame paths if fully cached, empty list otherwise.
  static QStringList cachedFrames(const QString& videoPath, const QString& cacheBaseDir);

  /// Returns the per-video cache subdirectory (named by path hash).
  static QString cacheDir(const QString& videoPath, const QString& cacheBaseDir);

signals:
  void ready(const QString& videoPath, const QStringList& framePaths);
  void failed(const QString& videoPath);

private slots:
  void onExtractorDone(const QString& videoPath, const QStringList& frames);
  void onExtractorFailed(const QString& videoPath);
  void processQueue();

private:
  struct Job { QString videoPath; QString cacheBaseDir; };
  QQueue<Job>          _queue;
  VideoFrameExtractor* _active = nullptr;
};

} // namespace mmp

#endif // THUMBNAILCACHE_H_
