/*
 * FreePolygon.h
 *
 * A variable-vertex convex polygon built by click-to-place in the canvas.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef FREEPOLYGON_H_
#define FREEPOLYGON_H_

#include "Polygon.h"

namespace mmp {

/**
 * A free-form polygon with a variable number of vertices.
 * Vertices are placed one at a time by the user (click-to-add, auto-close).
 */
class FreePolygon : public Polygon
{
  Q_OBJECT
public:
  Q_INVOKABLE FreePolygon() {}
  explicit FreePolygon(const QVector<QPointF>& pts) : Polygon(pts) {}
  virtual ~FreePolygon() {}

  virtual ShapeType getType() const override { return ShapeType::Polygon; }

protected:
  virtual MShape* _create() const override { return new FreePolygon(); }
};

} // namespace mmp

#endif /* FREEPOLYGON_H_ */
