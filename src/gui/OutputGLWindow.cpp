/*
 * OutputGLWindow.cpp
 *
 * (c) 2014 Sofian Audry -- info(@)sofianaudry(.)com
 * (c) 2014 Alexandre Quessy -- alexandre(@)quessy(.)net
 * (c) 2014 Dame Diongue -- baydamd(@)gmail(.)com
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

#include "OutputGLWindow.h"

#include "MainWindow.h"

#include <QGuiApplication>
#include <QScreen>
#include <QOpenGLWidget>

namespace mmp {

OutputGLWindow:: OutputGLWindow(QWidget* parent, const MapperGLCanvas* canvas_) : QDialog(parent)
{
  // Qt::Window makes this a proper top-level window (not constrained as a child dialog),
  // which is required for reliable multi-monitor fullscreen on Windows.
  setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
  resize(MainWindow::OUTPUT_WINDOW_MINIMUM_WIDTH, MainWindow::OUTPUT_WINDOW_MINIMUM_HEIGHT);

  canvas = new OutputGLCanvas(canvas_->getMainWindow(), this, qobject_cast<QOpenGLWidget*>(canvas_->viewport()), canvas_->scene());
  canvas->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  canvas->setMinimumSize(MainWindow::OUTPUT_WINDOW_MINIMUM_WIDTH, MainWindow::OUTPUT_WINDOW_MINIMUM_HEIGHT);

  QLayout* layout = new QVBoxLayout;
  layout->setContentsMargins(0,0,0,0); // remove margin
  layout->addWidget(canvas);
  setLayout(layout);

  setCanvasDisplayCrosshair(false); // default

  _isFullScreen = false;
  _preferredScreen = QApplication::screens().size() - 1;
}

//OutputGLWindow::OutputGLWindow(MainWindow* mainWindow, QWidget* parent, const QGLWidget * shareWidget) : QDialog(parent)
//{
//  resize(MainWindow::OUTPUT_WINDOW_MINIMUM_WIDTH, MainWindow::OUTPUT_WINDOW_MINIMUM_HEIGHT);
//
//  canvas = new DestinationGLCanvas(mainWindow, this, shareWidget);
//  canvas->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
//  canvas->setMinimumSize(MainWindow::OUTPUT_WINDOW_MINIMUM_WIDTH, MainWindow::OUTPUT_WINDOW_MINIMUM_HEIGHT);
//
//  QLayout* layout = new QVBoxLayout;
//  layout->setContentsMargins(0,0,0,0); // remove margin
//  layout->addWidget(canvas);
//  setLayout(layout);
//
//  // Save window geometry.
//  _geometry = saveGeometry();
//
//  _pointerIsVisible = true;
//
//}

void OutputGLWindow::setFullScreen(bool fullscreen)
{
  _setFullScreen(fullscreen);
  _isFullScreen = fullscreen;

  _resetCursor(_isFullScreen);
}

void OutputGLWindow::_resetCursor(bool fullscreen)
{
  setCursor(fullscreen ? Qt::BlankCursor : Qt::ArrowCursor);
}

void OutputGLWindow::setCanvasDisplayCrosshair(bool crosshair)
{
  canvas->setDisplayCrosshair(crosshair);
  _resetCursor(_isFullScreen);
}

void OutputGLWindow::setDisplayTestSignal(bool displayTestSignal)
{
  canvas->setDisplayTestSignal(displayTestSignal);

  // Force fullscreen if needed.
  if (!_isFullScreen)
    _setFullScreen(displayTestSignal);

  canvas->update();
}

void OutputGLWindow::setPreferredScreen(int screen)
{
  _preferredScreen = qBound(screen, 0, QApplication::screens().size() - 1);
}


void OutputGLWindow::_updateToPreferredScreen()
{
  int screen = getPreferredScreen();
  const QList<QScreen*> screens = QGuiApplication::screens();
  if (screen >= screens.size())
    return;
  QScreen* targetScreen = screens.at(screen);
  // Position the window on the target screen.
  setGeometry(targetScreen->geometry());
  // Qt 6: QWindow::setScreen() is required for reliable multi-monitor fullscreen.
  // WA_NativeWindow forces the native window handle to be created (windowHandle()
  // returns null until the first show() or until the handle is explicitly created).
  setAttribute(Qt::WA_NativeWindow);
  if (QWindow* wh = windowHandle())
    wh->setScreen(targetScreen);
}

void OutputGLWindow::_setFullScreen(bool fullscreen)
{
  if (fullscreen)
  {
    _updateToPreferredScreen();
    showFullScreen();
  }
  else
  {
    hide();
  }
}

}
