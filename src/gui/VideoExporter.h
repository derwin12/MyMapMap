/*
 * VideoExporter.h
 *
 * Encodes the output canvas to a video file using Qt Multimedia.
 * Uses QVideoFrameInput + QAudioBufferInput (Qt 6.8+) to push grabbed
 * frames and loopback audio into a QMediaRecorder pipeline.
 */

#ifndef VIDEOEXPORTER_H
#define VIDEOEXPORTER_H

#include <QObject>
#include <QAudioFormat>
#include <QImage>
#include <QSize>
#include <QScopedPointer>
class QMediaCaptureSession;
class QVideoFrameInput;
class QAudioBufferInput;
class QMediaRecorder;
class QAudioSource;
class QIODevice;

namespace mmp {

class VideoExporter : public QObject
{
  Q_OBJECT

public:
  enum Format {
    H264_MP4  = 0,
    H265_MP4  = 1,
    MJPEG_AVI = 2
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

  bool      isRecording()       const { return _recording; }
  QString   currentFilePath()   const { return _filePath; }
  QString   audioDeviceName()   const { return _audioDeviceName; }
  qint64    duration()          const;

  static QString formatLabel(Format f);
  static QString formatFilter(Format f);
  static QString formatExtension(Format f);

signals:
  void recordingStarted();
  void recordingStopped(const QString& filePath);
  void durationChanged(qint64 ms);
  void errorOccurred(const QString& errorString);

private:
  QScopedPointer<QMediaCaptureSession> _session;
  QScopedPointer<QVideoFrameInput>     _frameInput;
  QScopedPointer<QAudioBufferInput>    _audioBufferInput;
  QScopedPointer<QMediaRecorder>       _recorder;
  QScopedPointer<QAudioSource>         _audioSource;
  QAudioFormat                         _audioFormat;
  QIODevice*                           _audioDevice = nullptr; // valid only while recording
  bool                                 _recording   = false;
  QString                              _filePath;
  QString                              _audioDeviceName;
  qreal                                _fps        = 0.0;
  qint64                               _frameCount = 0;
};

} // namespace mmp

#endif // VIDEOEXPORTER_H
