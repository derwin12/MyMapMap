/*
 * VideoFrameConverter.cpp
 *
 * (c) 2026 MapMap contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "VideoFrameConverter.h"

namespace mmp {

void VideoFrameConverter::convertFrame(const QVideoFrame& frame)
{
  if (!frame.isValid())
    return;

  QImage img = frame.toImage().convertToFormat(QImage::Format_RGBA8888);
  if (img.isNull())
    return;

  emit frameReady(img);
}

}
