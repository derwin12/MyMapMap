/*
 * VideoExporter.h
 *
 * Encodes the output canvas to a video file using Qt Multimedia.
 * Uses QVideoFrameInput (Qt 6.8+) to push grabbed OpenGL frames
 * into a QMediaRecorder pipeline.
 */

#ifndef VIDEOEXPORTER_H
#define VIDEOEXPORTER_H

#include <QObject>
#include <QImage>
#include <QSize>
#include <QMediaCaptureSession>
#include <QVideoFrameInput>
#include <QMediaRecorder>

namespace mmp {

class VideoExporter : public QObject
{
  Q_OBJECT

public:
  // Matches SeqExportDialog-style labelling used in xLights for familiarity.
  enum Format {
    H264_MP4  = 0,   // H.264  — best compatibility, .mp4
    H265_MP4  = 1,   // H.265  — smaller files, Windows 10+, .mp4
    MJPEG_AVI = 2    // Motion JPEG — near-lossless, large files, .avi
  };
  Q_ENUM(Format)

  enum Quality {
    LowQuality      = 0,
    MediumQuality   = 1,
    HighQuality     = 2,
    VeryHighQuality = 3
  };
  Q_ENUM(Quality)

  explicit VideoExporter(QObject* parent = nullptr);
  ~VideoExporter();

  bool start(const QString& filePath, Format format, Quality quality,
             QSize size, qreal fps);
  bool sendFrame(const QImage& frame);
  void stop();

  bool      isRecording()     const { return _recording; }
  QString   currentFilePath() const { return _filePath; }
  qint64    duration()        const;

  // Human-readable label for each format (used in UI).
  static QString formatLabel(Format f);
  // File-dialog filter string for the given format.
  static QString formatFilter(Format f);
  // Default file extension for the given format.
  static QString formatExtension(Format f);

signals:
  void recordingStarted();
  void recordingStopped(const QString& filePath);
  void durationChanged(qint64 ms);
  void errorOccurred(const QString& errorString);

private:
  QMediaCaptureSession _session;
  QVideoFrameInput     _frameInput;
  QMediaRecorder       _recorder;
  bool                 _recording = false;
  QString              _filePath;
};

} // namespace mmp

#endif // VIDEOEXPORTER_H
