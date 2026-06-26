/*
 * MapperGLCanvasToolbar.h
 *
 * (c) 2016 Sofian Audry -- info(@)sofianaudry(.)com
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef MAPPERGLCANVASTOOLBAR_H_
#define MAPPERGLCANVASTOOLBAR_H_

#include "MM.h"
#include "MapperGLCanvas.h"

#include <QLabel>
#include <QToolButton>
#include <QComboBox>
#include <QSlider>

class QHBoxLayout;

namespace mmp {

class MapperGLCanvasToolbar : public QWidget {
  Q_OBJECT
public:
  MapperGLCanvasToolbar(MapperGLCanvas* parent = nullptr);
  virtual ~MapperGLCanvasToolbar();

  // Create zoom tool buttons
  void createZoomToolsLayout();

  void enableZoomToolBar(bool enabled);

  // Re-applies themed icons (call after a light/dark theme switch).
  void refreshIcons();

  // Return enable statut
  bool buttonsAreEnable() { return _areEnable; }

  void setToolbarTitle(const QString &title) { _titleLabel->setText(title); }

  // Background reference photo controls (destination canvas only).
  void setupBackgroundPhotoControls();
  void setBackgroundPhotoControlsVisible(bool visible);
  void setBackgroundOpacityValue(int value); // 0–100

signals:
  void backgroundPhotoToggled(bool visible);
  void backgroundOpacityChanged(int value);

public slots:
  // Update and feedback zoom level
  void updateDropdownMenu(qreal factor = 1);
  // Show/Hide zoom tool buttons
  void showZoomToolBar(bool visible);

protected:
  MapperGLCanvas* _canvas;
  // Labels
  QLabel *_titleLabel;
  // Buttons for toolbox layout
  QToolButton* _zoomInButton;
  QToolButton* _zoomOutButton;
  QToolButton* _resetZoomButton;
  QToolButton* _fitToViewButton;
  // Dropdown menu
  QComboBox* _dropdownMenu;

  bool _areEnable;

  QToolButton* _bgPhotoToggleButton = nullptr;
  QLabel*      _bgOpacityLabel      = nullptr;
  QSlider*     _bgOpacitySlider     = nullptr;
};

}

#endif /* MAPPERGLCANVASTOOLBAR_H_ */
