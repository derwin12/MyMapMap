/*
 * VideoFrameConverter.h
 *
 * (c) 2026 MapMap contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef VIDEO_FRAME_CONVERTER_H_
#define VIDEO_FRAME_CONVERTER_H_

#include <QObject>
#include <QImage>
#include <QVideoFrame>

namespace mmp {

/**
 * Converts a QVideoFrame to a Format_RGBA8888 QImage off the GUI thread.
 * Move an instance to a worker QThread and connect a QVideoSink's
 * videoFrameChanged signal directly to convertFrame() — the cross-thread
 * connection queues automatically, keeping frame.toImage()'s software
 * pixel conversion off the UI thread.
 */
class VideoFrameConverter : public QObject
{
  Q_OBJECT

public slots:
  void convertFrame(const QVideoFrame& frame);

signals:
  void frameReady(QImage image);
};

}

#endif // VIDEO_FRAME_CONVERTER_H_
