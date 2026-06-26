/*
 * VideoPlayerImpl.h
 *
 * (c) 2024 MapMap contributors
 *
 * Qt 6 Multimedia video playback using QMediaPlayer + QVideoSink + QAudioOutput.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef VIDEO_PLAYER_IMPL_H_
#define VIDEO_PLAYER_IMPL_H_

#include "VideoImpl.h"
#include "VideoFrameConverter.h"

#include <QObject>
#include <QThread>
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QVideoSink>
#include <QVideoFrame>

namespace mmp {

/**
 * File/URI video playback using Qt 6 QMediaPlayer.
 * Delivers RGBA frames via QVideoSink into VideoImpl::_data so that
 * callers (ShapeGraphicsItem etc.) can upload them to OpenGL unchanged.
 */
class VideoPlayerImpl : public QObject, public VideoImpl
{
  Q_OBJECT

public:
  VideoPlayerImpl();
  ~VideoPlayerImpl() override;

  bool loadMovie(const QString& path) override;
  bool isLive() override { return false; }

  bool setPlayState(bool play);
  bool seekTo(qint64 positionMs) override;

  qint64 getPosition() const override;
  void setRate(double rate) override;
  void setVolume(double volume) override;
  void setPlayInLoop(bool loop) override;

  void update() override;

protected:
  // Releases the player/audio/sink (called by unloadMovie() and the destructor).
  void freeResources() override;

private slots:
  void onFrameConverted(QImage img);
  void onMediaStatusChanged(QMediaPlayer::MediaStatus status);

private:
  QMediaPlayer *_player;
  QAudioOutput *_audioOutput;
  QVideoSink   *_videoSink;
  bool          _eos;

  // Runs VideoFrameConverter off the GUI thread so the per-frame
  // QVideoFrame -> QImage pixel conversion never blocks UI/input.
  QThread             *_converterThread;
  VideoFrameConverter *_converter;
};

}

#endif // VIDEO_PLAYER_IMPL_H_
