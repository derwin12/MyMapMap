/*
 * VideoExporter.cpp
 */

#include "VideoExporter.h"

#include <QMediaFormat>
#include <QVideoFrame>
#include <QVideoFrameFormat>
#include <QUrl>

namespace mmp {

VideoExporter::VideoExporter(QObject* parent) : QObject(parent)
{
  _session.setVideoFrameInput(&_frameInput);
  _session.setRecorder(&_recorder);

  connect(&_recorder, &QMediaRecorder::durationChanged,
          this, &VideoExporter::durationChanged);

  connect(&_recorder, &QMediaRecorder::errorOccurred,
          this, [this](QMediaRecorder::Error, const QString& msg) {
    emit errorOccurred(msg);
  });
}

VideoExporter::~VideoExporter()
{
  if (_recording)
    stop();
}

bool VideoExporter::start(const QString& filePath, Format format,
                          Quality quality, QSize size, qreal fps)
{
  if (_recording)
    return false;

  QMediaFormat mediaFormat;
  switch (format) {
  case H264_MP4:
    mediaFormat.setFileFormat(QMediaFormat::MPEG4);
    mediaFormat.setVideoCodec(QMediaFormat::VideoCodec::H264);
    break;
  case H265_MP4:
    mediaFormat.setFileFormat(QMediaFormat::MPEG4);
    mediaFormat.setVideoCodec(QMediaFormat::VideoCodec::H265);
    break;
  case MJPEG_AVI:
    mediaFormat.setFileFormat(QMediaFormat::AVI);
    mediaFormat.setVideoCodec(QMediaFormat::VideoCodec::MotionJPEG);
    break;
  }

  static const QMediaRecorder::Quality qualityMap[] = {
    QMediaRecorder::LowQuality,
    QMediaRecorder::NormalQuality,
    QMediaRecorder::HighQuality,
    QMediaRecorder::VeryHighQuality
  };

  _recorder.setMediaFormat(mediaFormat);
  _recorder.setOutputLocation(QUrl::fromLocalFile(filePath));
  _recorder.setVideoResolution(size);
  _recorder.setVideoFrameRate(fps);
  _recorder.setQuality(qualityMap[quality]);

  _recorder.record();

  if (_recorder.error() != QMediaRecorder::NoError) {
    emit errorOccurred(_recorder.errorString());
    return false;
  }

  _recording   = true;
  _filePath    = filePath;
  _fps         = fps;
  _frameCount  = 0;
  emit recordingStarted();
  return true;
}

bool VideoExporter::sendFrame(const QImage& frame)
{
  if (!_recording)
    return false;

  // QVideoFrameInput expects RGBA8888 on Windows Media Foundation.
  QImage converted = frame.convertToFormat(QImage::Format_RGBA8888);

  QVideoFrameFormat vff(converted.size(),
                        QVideoFrameFormat::Format_RGBA8888);
  QVideoFrame videoFrame(vff);

  if (!videoFrame.map(QVideoFrame::WriteOnly))
    return false;

  memcpy(videoFrame.bits(0), converted.constBits(),
         static_cast<size_t>(converted.sizeInBytes()));
  videoFrame.unmap();

  // Stamp presentation time so Qt Multimedia sees a non-zero frame rate.
  // Without timestamps QVideoFrameInput reports frameRate=0 and the encoder
  // may drop frames or produce a zero-duration file.
  if (_fps > 0.0) {
    qint64 frameDurationUs = qRound64(1000000.0 / _fps);
    videoFrame.setStartTime(_frameCount * frameDurationUs);
    videoFrame.setEndTime((_frameCount + 1) * frameDurationUs);
  }
  ++_frameCount;

  return _frameInput.sendVideoFrame(videoFrame);
}

void VideoExporter::stop()
{
  if (!_recording)
    return;

  _recorder.stop();
  _recording = false;
  emit recordingStopped(_filePath);
}

qint64 VideoExporter::duration() const
{
  return _recorder.duration();
}

QString VideoExporter::formatLabel(Format f)
{
  switch (f) {
  case H264_MP4:  return tr("H.264 (MP4)  — best compatibility");
  case H265_MP4:  return tr("H.265 (MP4)  — smaller files, Windows 10+");
  case MJPEG_AVI: return tr("Motion JPEG (AVI)  — near-lossless");
  }
  return {};
}

QString VideoExporter::formatFilter(Format f)
{
  switch (f) {
  case H264_MP4:
  case H265_MP4:  return tr("MPEG-4 Video (*.mp4)");
  case MJPEG_AVI: return tr("AVI Video (*.avi)");
  }
  return {};
}

QString VideoExporter::formatExtension(Format f)
{
  switch (f) {
  case H264_MP4:
  case H265_MP4:  return "mp4";
  case MJPEG_AVI: return "avi";
  }
  return "mp4";
}

} // namespace mmp
