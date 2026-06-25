/*
 * VideoPlayerImpl.cpp
 *
 * (c) 2024 MapMap contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "VideoPlayerImpl.h"
#include <QDebug>
#include <QUrl>

namespace mmp {

VideoPlayerImpl::VideoPlayerImpl()
  : _player(nullptr),
    _audioOutput(nullptr),
    _videoSink(nullptr),
    _eos(false)
{
  // Worker thread + converter live for the lifetime of this object;
  // only the QVideoSink -> converter connection is redone per loadMovie().
  _converterThread = new QThread();
  _converter = new VideoFrameConverter();
  _converter->moveToThread(_converterThread);
  connect(_converterThread, &QThread::finished, _converter, &QObject::deleteLater);
  connect(_converter, &VideoFrameConverter::frameReady,
          this, &VideoPlayerImpl::onFrameConverted);
  _converterThread->start();
}

VideoPlayerImpl::~VideoPlayerImpl()
{
  freeResources();
  _converterThread->quit();
  _converterThread->wait();
  delete _converterThread;
}

void VideoPlayerImpl::freeResources()
{
  if (_player) {
    _player->stop();
    delete _player;
    _player = nullptr;
  }
  delete _audioOutput;
  _audioOutput = nullptr;
  delete _videoSink;
  _videoSink = nullptr;
  _eos = false;
  VideoImpl::freeResources();
}

bool VideoPlayerImpl::loadMovie(const QString& path)
{
  // Store URI in base class.
  VideoImpl::loadMovie(path);

  // Tear down any previous player and reset frame state.
  freeResources();

  _player      = new QMediaPlayer(this);
  _audioOutput = new QAudioOutput(this);
  _videoSink   = new QVideoSink(this);

  _player->setAudioOutput(_audioOutput);
  _player->setVideoSink(_videoSink);

  connect(_videoSink, &QVideoSink::videoFrameChanged,
          _converter, &VideoFrameConverter::convertFrame);
  connect(_player, &QMediaPlayer::mediaStatusChanged,
          this, &VideoPlayerImpl::onMediaStatusChanged);

  _player->setSource(QUrl::fromLocalFile(path));

  if (_playInLoop)
    _player->setLoops(QMediaPlayer::Infinite);

  // Seek is always enabled for file sources in Qt 6.
  _seekEnabled = true;
  _videoIsConnected = true;
  _audioIsConnected = true;

  _player->setPlaybackRate(_rate);
  _audioOutput->setVolume(_muted ? 0.0 : _volume);
  _player->play();
  _playState = true;

  return true;
}

void VideoPlayerImpl::onFrameConverted(QImage img)
{
  if (img.isNull())
    return;

  lockMutex();

  _currentFrame = img;
  _width  = _currentFrame.width();
  _height = _currentFrame.height();
  _data   = _currentFrame.constBits();
  _bitsChanged = true;
  _videoIsConnected = true;

  unlockMutex();
}

void VideoPlayerImpl::onMediaStatusChanged(QMediaPlayer::MediaStatus status)
{
  switch (status) {
  case QMediaPlayer::LoadedMedia:
    _duration = _player->duration();
    _setMovieReady(true);
    break;
  case QMediaPlayer::EndOfMedia:
    _eos = true;
    break;
  case QMediaPlayer::InvalidMedia:
    qWarning() << "Invalid media:" << _player->errorString();
    break;
  default:
    break;
  }
}

bool VideoPlayerImpl::setPlayState(bool play)
{
  if (!_player)
    return false;
  if (play)
    _player->play();
  else
    _player->pause();
  _playState = play;
  return true;
}

bool VideoPlayerImpl::seekTo(qint64 positionMs)
{
  if (!_player || !_seekEnabled)
    return false;
  _player->setPosition(positionMs);
  return true;
}

void VideoPlayerImpl::setRate(double rate)
{
  _rate = rate;
  if (_player)
    _player->setPlaybackRate(rate);
}

void VideoPlayerImpl::setVolume(double volume)
{
  _volume = volume;
  if (_audioOutput)
    _audioOutput->setVolume(_muted ? 0.0 : volume);
}

void VideoPlayerImpl::setPlayInLoop(bool loop)
{
  _playInLoop = loop;
  if (_player)
    _player->setLoops(loop ? QMediaPlayer::Infinite : 1);
}

void VideoPlayerImpl::update()
{
  if (_eos) {
    _eos = false;
    if (_playInLoop)
      resetMovie();
  }
}

}
