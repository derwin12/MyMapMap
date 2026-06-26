/*
 * SourceGui.h
 *
 * (c) 2014 Sofian Audry -- info(@)sofianaudry(.)com
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

#ifndef SOURCEGUI_H_
#define SOURCEGUI_H_


#include <QtGlobal>
#include <QSlider>
#include <QLabel>
#include <QToolButton>
#include <QButtonGroup>
#include <QTimer>

#if __APPLE__
#include <OpenGL/gl.h>
#else
#ifdef _WIN32
#include <windows.h>
#endif
#include <GL/gl.h>
#endif

#include "MM.h"

#include "Source.h"

#ifdef HAVE_SYPHON
#include "Syphon.h"
class QTimer;
#endif

#include "qtpropertymanager.h"
#include "qtvariantproperty.h"
#include "qttreepropertybrowser.h"
#include "qtbuttonpropertybrowser.h"
#include "qtgroupboxpropertybrowser.h"

#include "variantmanager.h"
#include "variantfactory.h"

namespace mmp {

/**
 * The view components corresponding to a Source (which is the model) in the interface.
 * Mainly manages the property browser for the Source.
 *
 * In other words the SourceGui is to Source what MappingGui is to Mapping.
 */
class SourceGui : public QObject {
  Q_OBJECT

public:
  typedef QSharedPointer<SourceGui> ptr;

public:
  // TODO: should be protected
  /// Constructor. A source gui applies to a source.
  SourceGui(Source::ptr source);
  virtual ~SourceGui();

public:
  /// Returns a pointer to the properties editor for that mapper.
  virtual QWidget* getPropertiesEditor();

public slots:
  virtual void setValue(QtProperty* property, const QVariant& value);
  virtual void setValue(QString propertyName, QVariant value);

signals:
  void valueChanged(Source::ptr);

protected:
  Source::ptr _source;
  QtAbstractPropertyBrowser* _propertyBrowser;
  QtVariantEditorFactory* _variantFactory;
  QtVariantPropertyManager* _variantManager;
  QWidget* _compositeWidget = nullptr; // wraps slider rows + property browser when set

  QtVariantProperty* _idItem;
  QtVariantProperty* _opacityItem;
};

class ColorGui : public SourceGui {
  Q_OBJECT

public:
  ColorGui(Source::ptr source);
  virtual ~ColorGui() {}

public slots:
  virtual void setValue(QtProperty* property, const QVariant& value);
  virtual void setValue(QString propertyName, QVariant value);

protected:
  QSharedPointer<Color> color;
  QtVariantProperty* _colorItem;
};

class TextGui : public SourceGui {
  Q_OBJECT

public:
  TextGui(Source::ptr source);
  virtual ~TextGui() {}

public slots:
  virtual void setValue(QtProperty* property, const QVariant& value);
  virtual void setValue(QString propertyName, QVariant value);

protected:
  QSharedPointer<Text> textSource;
  QtVariantProperty* _textItem;
  QtVariantProperty* _textColorItem;
  QtVariantProperty* _bgColorItem;
  QtVariantProperty* _fontFamilyItem;
  QtVariantProperty* _fontSizeItem;
  QtVariantProperty* _boldItem;
  QtVariantProperty* _italicItem;
  QtVariantProperty* _alignmentItem;
};

class TextureGui : public SourceGui {
  Q_OBJECT

public:
  TextureGui(Source::ptr source);
  virtual ~TextureGui() {}
};

class ImageGui : public TextureGui {
  Q_OBJECT

public:
  ImageGui(Source::ptr source);
  virtual ~ImageGui() {}

public slots:
  virtual void setValue(QtProperty* property, const QVariant& value);
  virtual void setValue(QString propertyName, QVariant value);

protected:
  void _refreshImageSize();
  QSharedPointer<Image> image;
  QtVariantProperty* _imageFileItem;
  QtVariantProperty* _imageRateItem;
  QtVariantProperty* _imageWidthItem  = nullptr;
  QtVariantProperty* _imageHeightItem = nullptr;
  QSlider* _speedSlider    = nullptr;
  QLabel*  _speedValueLbl  = nullptr;
};

class FolderGui : public TextureGui {
  Q_OBJECT

public:
  FolderGui(Source::ptr source);
  virtual ~FolderGui() {}

public slots:
  virtual void setValue(QtProperty* property, const QVariant& value);
  virtual void setValue(QString propertyName, QVariant value);

protected:
  QSharedPointer<FolderSource> folder;
  QtVariantProperty* _folderPathItem;
  QtVariantProperty* _fileCountItem;
  QtVariantProperty* _rateItem;
  QSlider* _speedSlider    = nullptr;
  QLabel*  _speedValueLbl  = nullptr;
};

class VideoGui : public TextureGui {
  Q_OBJECT

public:
  VideoGui(Source::ptr source);
  virtual ~VideoGui() {}

public slots:
  virtual void setValue(QtProperty* property, const QVariant& value);
  virtual void setValue(QString propertyName, QVariant value);

private slots:
  void _refreshMetadata();

protected:
  QSharedPointer<Video> media;
  QtVariantProperty* _mediaFileItem;
  QtVariantProperty* _mediaRateItem;
  QtVariantProperty* _mediaVolumeItem;
  QSlider* _speedSlider    = nullptr;
  QLabel*  _speedValueLbl  = nullptr;
  QSlider* _volumeSlider   = nullptr;
  QLabel*  _volumeValueLbl = nullptr;

  // Info display
  QLabel* _infoNameLbl  = nullptr;
  QLabel* _infoResLbl   = nullptr;
  QLabel* _infoDurLbl   = nullptr;
  QLabel* _infoFpsLbl   = nullptr;
  QLabel* _infoCodecLbl = nullptr;

  // Transport buttons (left group)
  QToolButton* _btnStepBack = nullptr;
  QToolButton* _btnPause    = nullptr;
  QToolButton* _btnPlay     = nullptr;
  // Transport buttons (right group)
  QToolButton* _btnToStart  = nullptr;
  QToolButton* _btnSeekBack = nullptr;
  QToolButton* _btnSeekFwd  = nullptr;

  // Mode buttons
  QToolButton* _btnModeLoop    = nullptr;
  QToolButton* _btnModeForward = nullptr;
  QToolButton* _btnModeReverse = nullptr;
  QToolButton* _btnModeRevLoop = nullptr;

  QTimer* _metadataTimer = nullptr;
};

#ifdef HAVE_SYPHON
/**
 * Property editor for a Syphon source (macOS only). Exposes a live-refreshed
 * "Server" dropdown so the source can be re-pointed at any available Syphon
 * server, plus a read-only connection status.
 */
class SyphonGui : public TextureGui {
  Q_OBJECT

public:
  SyphonGui(Source::ptr source);
  virtual ~SyphonGui() {}

public slots:
  virtual void setValue(QtProperty* property, const QVariant& value);
  virtual void setValue(QString propertyName, QVariant value);

private slots:
  /// Polls the Syphon directory and refreshes the dropdown/status.
  void refreshServers();

protected:
  void _rebuildServerEnum();
  void _updateStatus();

  QSharedPointer<Syphon> syphon;
  QtVariantProperty* _serverItem;
  QtVariantProperty* _statusItem;
  QtVariantProperty* _alphaItem;

  // _servers[i] corresponds to dropdown index i+1 (index 0 is "(none)").
  QList<SyphonServerDescription> _servers;
  QStringList _lastSignature;
  bool _updatingEnum;
  QTimer* _refreshTimer;
};
#endif // HAVE_SYPHON

}

#endif /* SOURCEGUI_H_ */
