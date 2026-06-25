/*
 * VideoExporter.cpp
 */

#include "VideoExporter.h"

#include <QMediaCaptureSession>
#include <QVideoFrameInput>
#include <QMediaRecorder>
#include <QAudioInput>
#include <QMediaDevices>
#include <QAudioDevice>
#include <QMediaFormat>
#include <QVideoFrame>
#include <QVideoFrameFormat>
#include <QUrl>
#include <QDebug>

namespace mmp {

VideoExporter::VideoExporter(QObject* parent) : QObject(parent)
{
  // _session / _frameInput / _recorder are constructed lazily inside start()
  // so their constructors (which trigger WMF/FFmpeg backend init) don't run
  // at app startup and don't freeze the main thread.
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

  // Lazy construction: allocate Qt Multimedia objects on first record call so
  // the WMF/FFmpeg backend initialisation happens here, not at app startup.
  if (!_recorder) {
    _session.reset(new QMediaCaptureSession);
    _frameInput.reset(new QVideoFrameInput);
    _recorder.reset(new QMediaRecorder);

    // Prefer a loopback / desktop-audio device (Stereo Mix, What U Hear …).
    // Fall back to the system default input so there is always an audio track.
    QAudioDevice chosenDev;
    const QStringList loopbackKw = {"mix", "stereo", "loopback", "what u hear", "wave out",
                                    "cable", "virtual", "vb-audio", "voicemeeter"};
    for (const QAudioDevice& dev : QMediaDevices::audioInputs()) {
      QString name = dev.description().toLower();
      for (const QString& kw : loopbackKw)
        if (name.contains(kw)) { chosenDev = dev; break; }
      if (!chosenDev.isNull()) break;
    }
    // Use default input if no loopback device was found.
    if (chosenDev.isNull())
      chosenDev = QMediaDevices::defaultAudioInput();

    if (!chosenDev.isNull()) {
      _audioDevice = chosenDev.description();
      _audioInput.reset(new QAudioInput(chosenDev));
      _session->setAudioInput(_audioInput.data());
    }

    _session->setVideoFrameInput(_frameInput.data());
    _session->setRecorder(_recorder.data());

    connect(_recorder.data(), &QMediaRecorder::durationChanged,
            this, &VideoExporter::durationChanged);
    connect(_recorder.data(), &QMediaRecorder::errorOccurred,
            this, [this](QMediaRecorder::Error, const QString& msg) {
      emit errorOccurred(msg);
    });
  }

  QMediaFormat mediaFormat;
  switch (format) {
  case H264_MP4:
    mediaFormat.setFileFormat(QMediaFormat::MPEG4);
    mediaFormat.setVideoCodec(QMediaFormat::VideoCodec::H264);
    if (_audioInput) mediaFormat.setAudioCodec(QMediaFormat::AudioCodec::AAC);
    break;
  case H265_MP4:
    mediaFormat.setFileFormat(QMediaFormat::MPEG4);
    mediaFormat.setVideoCodec(QMediaFormat::VideoCodec::H265);
    if (_audioInput) mediaFormat.setAudioCodec(QMediaFormat::AudioCodec::AAC);
    break;
  case MJPEG_AVI:
    mediaFormat.setFileFormat(QMediaFormat::AVI);
    mediaFormat.setVideoCodec(QMediaFormat::VideoCodec::MotionJPEG);
    if (_audioInput) mediaFormat.setAudioCodec(QMediaFormat::AudioCodec::MP3);
    break;
  }

  static const QMediaRecorder::Quality qualityMap[] = {
    QMediaRecorder::LowQuality,
    QMediaRecorder::NormalQuality,
    QMediaRecorder::HighQuality,
    QMediaRecorder::VeryHighQuality
  };

  _recorder->setMediaFormat(mediaFormat);
  _recorder->setOutputLocation(QUrl::fromLocalFile(filePath));
  _recorder->setVideoResolution(size);
  _recorder->setVideoFrameRate(fps);
  _recorder->setQuality(qualityMap[quality]);

  _recorder->record();

  if (_recorder->error() != QMediaRecorder::NoError) {
    emit errorOccurred(_recorder->errorString());
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

  if (!videoFrame.map(QVideoFrame::WriteOnly)) {
    return false;
  }

  // Copy row-by-row so mismatched strides (QImage padding vs QVideoFrame padding)
  // don't cause each row to shift and produce the diagonal-skew artifact.
  const int rowBytes = converted.width() * 4;
  for (int y = 0; y < converted.height(); ++y) {
    memcpy(videoFrame.bits(0) + y * static_cast<size_t>(videoFrame.bytesPerLine(0)),
           converted.constScanLine(y),
           static_cast<size_t>(rowBytes));
  }
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

  bool sent = _frameInput->sendVideoFrame(videoFrame);
  return sent;
}

void VideoExporter::stop()
{
  if (!_recording)
    return;

  _recorder->stop();
  _recording = false;
  emit recordingStopped(_filePath);
}

qint64 VideoExporter::duration() const
{
  return _recorder ? _recorder->duration() : 0;
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
