/*
 * VideoExporter.cpp
 */

#include "VideoExporter.h"

#include <QMediaCaptureSession>
#include <QVideoFrameInput>
#include <QAudioBufferInput>
#include <QMediaRecorder>
#include <QAudioSource>
#include <QAudioBuffer>
#include <QAudioFormat>
#include <QAudioDevice>
#include <QMediaDevices>
#include <QMediaFormat>
#include <QVideoFrame>
#include <QVideoFrameFormat>
#include <QIODevice>
#include <QUrl>
#include <QDebug>

namespace mmp {

VideoExporter::VideoExporter(QObject* parent) : QObject(parent)
{
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

  if (!_recorder) {
    _session.reset(new QMediaCaptureSession);
    _frameInput.reset(new QVideoFrameInput);
    _recorder.reset(new QMediaRecorder);

    _session->setVideoFrameInput(_frameInput.data());
    _session->setRecorder(_recorder.data());

    // Search for a loopback / desktop-audio device.
    // Do NOT fall back to the microphone — that records the wrong source.
    QAudioDevice loopbackDev;
    const QStringList loopbackKw = {"mix", "stereo", "loopback", "what u hear",
                                    "wave out", "cable", "virtual", "vb-audio", "voicemeeter"};
    for (const QAudioDevice& dev : QMediaDevices::audioInputs()) {
      QString name = dev.description().toLower();
      for (const QString& kw : loopbackKw)
        if (name.contains(kw)) { loopbackDev = dev; break; }
      if (!loopbackDev.isNull()) break;
    }

    if (!loopbackDev.isNull()) {
      // Use QAudioSource + QAudioBufferInput so we push audio the same way
      // we push video — both streams share the same session clock and the
      // FFmpeg muxer can interleave them correctly.
      QAudioFormat fmt;
      fmt.setSampleRate(44100);
      fmt.setChannelCount(2);
      fmt.setSampleFormat(QAudioFormat::Int16);

      _audioBufferInput.reset(new QAudioBufferInput(fmt));
      _session->setAudioBufferInput(_audioBufferInput.data());

      _audioSource.reset(new QAudioSource(loopbackDev, fmt));
      QIODevice* iodev = _audioSource->start(); // pull-mode: returns QIODevice*
      _audioDevice = iodev;

      if (iodev) {
        connect(iodev, &QIODevice::readyRead, this, [this, fmt]() {
          if (!_audioDevice || !_audioBufferInput) return;
          const QByteArray data = _audioDevice->readAll();
          if (!data.isEmpty())
            _audioBufferInput->sendAudioBuffer(QAudioBuffer(data, fmt));
        });
        _audioDeviceName = loopbackDev.description();
      } else {
        _audioBufferInput.reset();
        _audioSource.reset();
      }
    }

    connect(_recorder.data(), &QMediaRecorder::durationChanged,
            this, &VideoExporter::durationChanged);
    connect(_recorder.data(), &QMediaRecorder::errorOccurred,
            this, [this](QMediaRecorder::Error, const QString& msg) {
      emit errorOccurred(msg);
    });
  }

  QMediaFormat mediaFormat;
  const bool hasAudio = !_audioDeviceName.isEmpty();
  switch (format) {
  case H264_MP4:
    mediaFormat.setFileFormat(QMediaFormat::MPEG4);
    mediaFormat.setVideoCodec(QMediaFormat::VideoCodec::H264);
    if (hasAudio) mediaFormat.setAudioCodec(QMediaFormat::AudioCodec::AAC);
    break;
  case H265_MP4:
    mediaFormat.setFileFormat(QMediaFormat::MPEG4);
    mediaFormat.setVideoCodec(QMediaFormat::VideoCodec::H265);
    if (hasAudio) mediaFormat.setAudioCodec(QMediaFormat::AudioCodec::AAC);
    break;
  case MJPEG_AVI:
    mediaFormat.setFileFormat(QMediaFormat::AVI);
    mediaFormat.setVideoCodec(QMediaFormat::VideoCodec::MotionJPEG);
    if (hasAudio) mediaFormat.setAudioCodec(QMediaFormat::AudioCodec::MP3);
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

  QImage converted = frame.convertToFormat(QImage::Format_RGBA8888);

  QVideoFrameFormat vff(converted.size(), QVideoFrameFormat::Format_RGBA8888);
  QVideoFrame videoFrame(vff);

  if (!videoFrame.map(QVideoFrame::WriteOnly))
    return false;

  const int rowBytes = converted.width() * 4;
  for (int y = 0; y < converted.height(); ++y) {
    memcpy(videoFrame.bits(0) + y * static_cast<size_t>(videoFrame.bytesPerLine(0)),
           converted.constScanLine(y),
           static_cast<size_t>(rowBytes));
  }
  videoFrame.unmap();

  if (_fps > 0.0) {
    qint64 frameDurationUs = qRound64(1000000.0 / _fps);
    videoFrame.setStartTime(_frameCount * frameDurationUs);
    videoFrame.setEndTime((_frameCount + 1) * frameDurationUs);
  }
  ++_frameCount;

  return _frameInput->sendVideoFrame(videoFrame);
}

void VideoExporter::stop()
{
  if (!_recording)
    return;

  if (_audioSource)
    _audioSource->stop();

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
