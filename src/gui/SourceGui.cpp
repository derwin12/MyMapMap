/*
 * SourceGui.cpp
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

#include <SourceGui.h>

#include <QTimer>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>

namespace mmp {

// Helper: one labelled horizontal slider row with a live value label.
static QWidget* makeSliderRow(const QString& label, int min, int max, int initValue,
                               QSlider*& sliderOut, QLabel*& valueLblOut)
{
  auto* row = new QWidget;
  auto* hl  = new QHBoxLayout(row);
  hl->setContentsMargins(6, 2, 6, 2);
  hl->setSpacing(6);

  auto* lbl = new QLabel(label);
  lbl->setMinimumWidth(70);

  sliderOut = new QSlider(Qt::Horizontal);
  sliderOut->setRange(min, max);
  sliderOut->setValue(initValue);

  valueLblOut = new QLabel(QString::number(initValue) + "%");
  valueLblOut->setFixedWidth(38);
  valueLblOut->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

  hl->addWidget(lbl);
  hl->addWidget(sliderOut, 1);
  hl->addWidget(valueLblOut);
  return row;
}

// Build a composite widget: slider rows on top, a thin separator, then the property browser.
static QWidget* makeComposite(QWidget* sliderPanel, QWidget* browser)
{
  auto* composite = new QWidget;
  auto* vl = new QVBoxLayout(composite);
  vl->setContentsMargins(0, 0, 0, 0);
  vl->setSpacing(0);
  vl->addWidget(sliderPanel);
  auto* sep = new QFrame;
  sep->setFrameShape(QFrame::HLine);
  sep->setFrameShadow(QFrame::Sunken);
  vl->addWidget(sep);
  vl->addWidget(browser, 1);
  return composite;
}

SourceGui::SourceGui(Source::ptr source)
  : _source(source)
{
  // Create editor.
  _propertyBrowser = new QtTreePropertyBrowser;
  _variantManager = new VariantManager;
  _variantFactory = new VariantFactory;

  _propertyBrowser->setFactoryForManager(_variantManager, _variantFactory);

  connect(_variantManager, SIGNAL(valueChanged(QtProperty*, const QVariant&)),
          this,            SLOT(setValue(QtProperty*, const QVariant&)));

  // Mapping UID.
  _idItem = _variantManager->addProperty(QMetaType::Int, QObject::tr("ID"));
  _idItem->setEnabled(false);
  _idItem->setValue(_source->getId());
  _propertyBrowser->addProperty(_idItem);

  // Source basic properties.
  _opacityItem = _variantManager->addProperty(QMetaType::Double, QObject::tr("Opacity (%)"));
  _opacityItem->setAttribute("minimum", 0.0);
  _opacityItem->setAttribute("maximum", 100.0);
  _opacityItem->setAttribute("decimals", 1);
  _opacityItem->setValue(_source->getOpacity()*100.0);
  _propertyBrowser->addProperty(_opacityItem);
}

SourceGui::~SourceGui()
{
  if (_compositeWidget)
    delete _compositeWidget; // owns _propertyBrowser as a child
  else
    delete _propertyBrowser;
}

QWidget* SourceGui::getPropertiesEditor()
{
  return _compositeWidget ? _compositeWidget : _propertyBrowser;
}

void SourceGui::setValue(QtProperty* property, const QVariant& value)
{
  if (property == _opacityItem)
  {
    double opacity = qBound(value.toDouble() / 100.0, 0.0, 1.0);
    if (opacity != _source->getOpacity())
    {
      _source->setOpacity(opacity);
      emit valueChanged(_source);
    }
  }
}

void SourceGui::setValue(QString propertyName, QVariant value)
{
  if (propertyName == "opacity")
    _opacityItem->setValue(value.toDouble() * 100);
}

ColorGui::ColorGui(Source::ptr source)
  : SourceGui(source)
{
  color = qSharedPointerCast<Color>(source);
  Q_CHECK_PTR(color);

  _colorItem = _variantManager->addProperty(QMetaType::QColor,
                                            QObject::tr("Color"));

  _colorItem->setValue(color->getColor());

  _propertyBrowser->addProperty(_colorItem);
}

void ColorGui::setValue(QtProperty* property, const QVariant& value) {
  if (property == _colorItem) {
    QColor newColor = value.value<QColor>();
    if (newColor != color->getColor()) {
      color->setColor(newColor);
      emit valueChanged(_source);
    }
  }
  else
    SourceGui::setValue(property, value);
}

void ColorGui::setValue(QString propertyName, QVariant value)
{
  if (propertyName == "color")
    setValue(_colorItem, value);
  else
    SourceGui::setValue(propertyName, value);
}

TextureGui::TextureGui(Source::ptr source) : SourceGui(source) {
}

ImageGui::ImageGui(Source::ptr source)
  : TextureGui(source)
{
  image = qSharedPointerCast<Image>(source);
  Q_CHECK_PTR(image);

  _imageFileItem = _variantManager->addProperty(VariantManager::filePathTypeId(),
                                                tr("Image file"));
  _imageFileItem->setAttribute("filter", tr("Image files (%1);;All files (*)").arg(MM::IMAGE_FILES_FILTER));
  _imageFileItem->setValue(image->getUri());

  _imageRateItem = _variantManager->addProperty(QMetaType::Double, tr("Speed (%)"));
  double rate = image->getRate() * 100;
  _imageRateItem->setAttribute("decimals", 1);
  _imageRateItem->setValue(rate);

  _imageWidthItem = _variantManager->addProperty(QMetaType::Int, tr("Width (px)"));
  _imageWidthItem->setEnabled(false);
  _imageHeightItem = _variantManager->addProperty(QMetaType::Int, tr("Height (px)"));
  _imageHeightItem->setEnabled(false);

  _propertyBrowser->addProperty(_imageFileItem);
  _propertyBrowser->addProperty(_imageRateItem);
  _propertyBrowser->addProperty(_imageWidthItem);
  _propertyBrowser->addProperty(_imageHeightItem);

  // Defer size read — project loader resolves relative URIs after ImageGui is constructed.
  QTimer::singleShot(0, this, [this]() { _refreshImageSize(); });

  // Slider panel
  auto* sliders = new QWidget;
  auto* svl = new QVBoxLayout(sliders);
  svl->setContentsMargins(0, 2, 0, 2);
  svl->setSpacing(0);
  svl->addWidget(makeSliderRow(tr("Speed (%)"), 0, 200, (int)rate, _speedSlider, _speedValueLbl));

  _compositeWidget = makeComposite(sliders, _propertyBrowser);

  connect(_speedSlider, &QSlider::valueChanged, this, [this](int v) {
    _speedValueLbl->setText(QString::number(v) + "%");
    _imageRateItem->setValue(double(v)); // flows through setValue(QtProperty*, ...)
  });
}

void ImageGui::_refreshImageSize()
{
  if (_imageWidthItem)  _imageWidthItem->setValue(image->getWidth());
  if (_imageHeightItem) _imageHeightItem->setValue(image->getHeight());
}

void ImageGui::setValue(QtProperty* property, const QVariant& value) {
  if (property == _imageFileItem) {
    QString newUri = value.toString();
    if (newUri != image->getUri()) {
      image->setUri(newUri);
      _refreshImageSize();
      emit valueChanged(_source);
    }
  }
  else if (property == _imageRateItem)
  {
    double newRate = value.toDouble() / 100.0;
    if (newRate != image->getRate()) {
      image->setRate(newRate);
      emit valueChanged(_source);
    }
    if (_speedSlider) {
      _speedSlider->blockSignals(true);
      _speedSlider->setValue((int)qRound(value.toDouble()));
      _speedValueLbl->setText(QString::number(_speedSlider->value()) + "%");
      _speedSlider->blockSignals(false);
    }
  }
  else
    TextureGui::setValue(property, value);
}

void ImageGui::setValue(QString propertyName, QVariant value)
{
  if (propertyName == "uri")
    _imageFileItem->setValue(value);
  else if (propertyName == "rate")
    _imageRateItem->setValue(value.toDouble()*100);
  else
    TextureGui::setValue(propertyName, value);
}

FolderGui::FolderGui(Source::ptr source)
  : TextureGui(source)
{
  folder = qSharedPointerCast<FolderSource>(source);
  Q_CHECK_PTR(folder);

  _folderPathItem = _variantManager->addProperty(QMetaType::QString, tr("Folder"));
  _folderPathItem->setEnabled(false);
  _folderPathItem->setValue(folder->getUri());
  _propertyBrowser->addProperty(_folderPathItem);

  _fileCountItem = _variantManager->addProperty(QMetaType::Int, tr("Images"));
  _fileCountItem->setEnabled(false);
  _fileCountItem->setValue(folder->imageCount());
  _propertyBrowser->addProperty(_fileCountItem);

  _rateItem = _variantManager->addProperty(QMetaType::Double, tr("Speed (%)"));
  _rateItem->setAttribute("decimals", 1);
  double rate = folder->getRate() * 100.0;
  _rateItem->setValue(rate);
  _propertyBrowser->addProperty(_rateItem);

  // Slider panel
  auto* sliders = new QWidget;
  auto* svl = new QVBoxLayout(sliders);
  svl->setContentsMargins(0, 2, 0, 2);
  svl->setSpacing(0);
  svl->addWidget(makeSliderRow(tr("Speed (%)"), 0, 200, (int)rate, _speedSlider, _speedValueLbl));

  _compositeWidget = makeComposite(sliders, _propertyBrowser);

  connect(_speedSlider, &QSlider::valueChanged, this, [this](int v) {
    _speedValueLbl->setText(QString::number(v) + "%");
    _rateItem->setValue(double(v));
  });
}

void FolderGui::setValue(QtProperty* property, const QVariant& value)
{
  if (property == _rateItem) {
    double newRate = value.toDouble() / 100.0;
    if (newRate != folder->getRate()) {
      folder->setRate(newRate);
      emit valueChanged(_source);
    }
    if (_speedSlider) {
      _speedSlider->blockSignals(true);
      _speedSlider->setValue((int)qRound(value.toDouble()));
      _speedValueLbl->setText(QString::number(_speedSlider->value()) + "%");
      _speedSlider->blockSignals(false);
    }
  } else {
    TextureGui::setValue(property, value);
  }
}

void FolderGui::setValue(QString propertyName, QVariant value)
{
  if (propertyName == "uri")
    _folderPathItem->setValue(value);
  else if (propertyName == "rate")
    _rateItem->setValue(value.toDouble() * 100.0);
  else
    TextureGui::setValue(propertyName, value);
}

VideoGui::VideoGui(Source::ptr source)
: TextureGui(source)
{
  media = qSharedPointerCast<Video>(source);
  Q_CHECK_PTR(media);

  _mediaFileItem = _variantManager->addProperty(VariantManager::filePathTypeId(), tr("Source"));
  _mediaFileItem->setAttribute("filter", tr("Video files (%1);;All files (*)").arg(MM::VIDEO_FILES_FILTER));
  _mediaFileItem->setValue(media->getUri());

  _mediaRateItem = _variantManager->addProperty(QMetaType::Double, tr("Speed (%)"));
  double rate = media->getRate() * 100;
  _mediaRateItem->setAttribute("decimals", 1);
  _mediaRateItem->setValue(rate);

  _mediaVolumeItem = _variantManager->addProperty(QMetaType::Double, tr("Volume (%)"));
  double volume = media->getVolume() * 100;
  _mediaVolumeItem->setAttribute("minimum", 0.0);
  _mediaVolumeItem->setAttribute("maximum", 100.0);
  _mediaVolumeItem->setAttribute("decimals", 1);
  _mediaVolumeItem->setValue(volume);

  _propertyBrowser->addProperty(_mediaFileItem);
  _propertyBrowser->addProperty(_mediaRateItem);
  _propertyBrowser->addProperty(_mediaVolumeItem);

  // Slider panel
  auto* sliders = new QWidget;
  auto* svl = new QVBoxLayout(sliders);
  svl->setContentsMargins(0, 2, 0, 2);
  svl->setSpacing(0);
  svl->addWidget(makeSliderRow(tr("Speed (%)"),  0, 200, (int)rate,   _speedSlider,  _speedValueLbl));
  svl->addWidget(makeSliderRow(tr("Volume (%)"), 0, 100, (int)volume, _volumeSlider, _volumeValueLbl));

  _compositeWidget = makeComposite(sliders, _propertyBrowser);

  connect(_speedSlider, &QSlider::valueChanged, this, [this](int v) {
    _speedValueLbl->setText(QString::number(v) + "%");
    _mediaRateItem->setValue(double(v));
  });
  connect(_volumeSlider, &QSlider::valueChanged, this, [this](int v) {
    _volumeValueLbl->setText(QString::number(v) + "%");
    _mediaVolumeItem->setValue(double(v));
  });
}

void VideoGui::setValue(QtProperty* property, const QVariant& value)
{
  if (property == _mediaFileItem)
  {
    QString newUri = value.toString();
    if (newUri != media->getUri()) {
      media->setUri(newUri);
      emit valueChanged(_source);
    }
  }
  else if (property == _mediaRateItem)
  {
    double newRate = value.toDouble() / 100.0;
    if (newRate != media->getRate()) {
      media->setRate(newRate);
      emit valueChanged(_source);
    }
    if (_speedSlider) {
      _speedSlider->blockSignals(true);
      _speedSlider->setValue((int)qRound(value.toDouble()));
      _speedValueLbl->setText(QString::number(_speedSlider->value()) + "%");
      _speedSlider->blockSignals(false);
    }
  }
  else if (property == _mediaVolumeItem)
  {
    double newVolume = value.toDouble() / 100.0;
    if (newVolume != media->getVolume()) {
      media->setVolume(newVolume);
      emit valueChanged(_source);
    }
    if (_volumeSlider) {
      _volumeSlider->blockSignals(true);
      _volumeSlider->setValue((int)qRound(value.toDouble()));
      _volumeValueLbl->setText(QString::number(_volumeSlider->value()) + "%");
      _volumeSlider->blockSignals(false);
    }
  }
  else
    TextureGui::setValue(property, value);
}

void VideoGui::setValue(QString propertyName, QVariant value)
{
  if (propertyName == "uri")
    _mediaFileItem->setValue(value);
  if (propertyName == "rate")
    _mediaRateItem->setValue(value.toDouble()*100);
  if (propertyName == "volume")
    _mediaVolumeItem->setValue(value.toDouble()*100);
  else
    TextureGui::setValue(propertyName, value);
}

#ifdef HAVE_SYPHON
SyphonGui::SyphonGui(Source::ptr source)
  : TextureGui(source),
    _updatingEnum(false),
    _refreshTimer(nullptr)
{
  syphon = qSharedPointerCast<Syphon>(source);
  Q_CHECK_PTR(syphon);

  _serverItem = _variantManager->addProperty(QtVariantPropertyManager::enumTypeId(),
                                              tr("Server"));
  _statusItem = _variantManager->addProperty(QMetaType::QString, tr("Status"));
  _statusItem->setEnabled(false);
  _alphaItem = _variantManager->addProperty(QMetaType::Bool, tr("Respect source alpha"));
  _alphaItem->setValue(syphon->getRespectAlpha());

  _propertyBrowser->addProperty(_serverItem);
  _propertyBrowser->addProperty(_statusItem);
  _propertyBrowser->addProperty(_alphaItem);

  // Populate the dropdown and status now, then keep them current. Syphon has
  // no Qt-native change signal, so we poll the shared directory on a timer.
  refreshServers();

  _refreshTimer = new QTimer(this);
  connect(_refreshTimer, SIGNAL(timeout()), this, SLOT(refreshServers()));
  _refreshTimer->start(1000);
}

void SyphonGui::_rebuildServerEnum()
{
  _updatingEnum = true;

  _servers = Syphon::availableServers();

  const QString boundUuid = syphon->getServerUUID();
  const QString boundName = syphon->getServerName();
  const QString boundApp  = syphon->getAppName();
  const bool hasBound = !(boundUuid.isEmpty() && boundName.isEmpty() && boundApp.isEmpty());

  auto matchesBound = [&](const SyphonServerDescription& s) {
    if (!boundUuid.isEmpty())
      return s.uuid == boundUuid;
    return s.name == boundName && s.appName == boundApp;
  };

  // Make sure the bound server is always selectable, even if currently offline.
  bool boundPresent = false;
  for (const SyphonServerDescription& s : _servers)
    if (matchesBound(s)) { boundPresent = true; break; }
  if (hasBound && !boundPresent)
  {
    SyphonServerDescription bound;
    bound.uuid = boundUuid;
    bound.name = boundName;
    bound.appName = boundApp;
    _servers.prepend(bound);
  }

  QStringList names;
  names << tr("(none)");
  int currentIndex = 0;
  for (int i = 0; i < _servers.size(); ++i)
  {
    QString label = _servers[i].displayName();
    if (label.isEmpty())
      label = tr("Unknown server");
    names << label;
    if (hasBound && matchesBound(_servers[i]))
      currentIndex = i + 1;
  }

  _serverItem->setAttribute("enumNames", names);
  _serverItem->setValue(currentIndex);

  _updatingEnum = false;
}

void SyphonGui::_updateStatus()
{
  QString status;
  if (syphon->isConnected())
    status = tr("Connected");
  else if (syphon->getServerUUID().isEmpty() &&
           syphon->getServerName().isEmpty() &&
           syphon->getAppName().isEmpty())
    status = tr("No server selected");
  else
    status = tr("Waiting for server…");
  _statusItem->setValue(status);
}

void SyphonGui::refreshServers()
{
  // Only rebuild the dropdown when the set of available servers changes, so we
  // do not disrupt the user (e.g. close an open combo box) every tick.
  QStringList signature;
  const QList<SyphonServerDescription> servers = Syphon::availableServers();
  for (const SyphonServerDescription& s : servers)
    signature << (s.uuid + "|" + s.appName + "|" + s.name);
  signature.sort();

  if (signature != _lastSignature)
  {
    _lastSignature = signature;
    _rebuildServerEnum();
  }

  _updateStatus();
}

void SyphonGui::setValue(QtProperty* property, const QVariant& value)
{
  if (property == _serverItem)
  {
    if (_updatingEnum)
      return; // Programmatic rebuild, not a user choice.

    const int index = value.toInt();
    if (index <= 0)
      syphon->connectToServer(QString(), QString(), QString()); // "(none)"
    else if (index - 1 < _servers.size())
    {
      const SyphonServerDescription& s = _servers[index - 1];
      syphon->connectToServer(s.uuid, s.name, s.appName);
    }
    emit valueChanged(_source);
    _updateStatus();
  }
  else if (property == _alphaItem)
  {
    syphon->setRespectAlpha(value.toBool());
    emit valueChanged(_source);
  }
  else
    TextureGui::setValue(property, value);
}

void SyphonGui::setValue(QString propertyName, QVariant value)
{
  Q_UNUSED(value);
  if (propertyName == "serverUUID" || propertyName == "serverName" || propertyName == "appName")
    _rebuildServerEnum();
  else
    TextureGui::setValue(propertyName, value);
}
#endif // HAVE_SYPHON

}
