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
#include <QGridLayout>
#include <QFrame>
#include <QFileInfo>
#include <QPushButton>
#include <QColorDialog>

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

  // Color swatch button — replaces the property browser entry for color.
  _colorButton = new QPushButton;
  _colorButton->setMinimumHeight(48);
  _colorButton->setCursor(Qt::PointingHandCursor);
  _colorButton->setToolTip(QObject::tr("Click to change color"));
  _colorButton->setFlat(true);
  updateColorButton();
  connect(_colorButton, &QPushButton::clicked, this, &ColorGui::openColorDialog);

  auto* swatchWidget = new QWidget;
  auto* vl = new QVBoxLayout(swatchWidget);
  vl->setContentsMargins(8, 8, 8, 4);
  vl->addWidget(_colorButton);

  _compositeWidget = makeComposite(swatchWidget, _propertyBrowser);
}

void ColorGui::setValue(QString propertyName, QVariant value)
{
  if (propertyName == "color") {
    color->setColor(value.value<QColor>());
    updateColorButton();
  } else {
    SourceGui::setValue(propertyName, value);
  }
}

void ColorGui::updateColorButton()
{
  QColor c = color->getColor();
  QString hex = c.name(QColor::HexArgb).toUpper();
  _colorButton->setText(hex);
  // Choose contrasting text color.
  double luminance = 0.299*c.redF() + 0.587*c.greenF() + 0.114*c.blueF();
  QString textColor = (luminance > 0.5) ? "#000000" : "#ffffff";
  _colorButton->setStyleSheet(QString(
    "QPushButton { background: %1; border: 1px solid #555; border-radius: 4px;"
    "  color: %2; font-weight: bold; }"
    "QPushButton:hover { border: 2px solid #aaa; }").arg(hex, textColor));
}

void ColorGui::openColorDialog()
{
  QColor c = QColorDialog::getColor(color->getColor(), _colorButton,
                                    QObject::tr("Choose Color"),
                                    QColorDialog::ShowAlphaChannel);
  if (c.isValid()) {
    color->setColor(c);
    updateColorButton();
    emit valueChanged(_source);
  }
}

// Shared helper used by TextGui.
static void applyColorToButton(QPushButton* btn, const QColor& c)
{
  QString hex = c.name(QColor::HexArgb).toUpper();
  double luminance = 0.299*c.redF() + 0.587*c.greenF() + 0.114*c.blueF();
  QString textColor = (luminance > 0.5) ? "#000000" : "#ffffff";
  btn->setStyleSheet(QString(
    "QPushButton { background: %1; border: 1px solid #555; border-radius: 4px;"
    "  color: %2; font-weight: bold; }"
    "QPushButton:hover { border: 2px solid #aaa; }").arg(hex, textColor));
}

void TextGui::updateColorButton(QPushButton* btn, const QColor& c)
{
  applyColorToButton(btn, c);
}

TextGui::TextGui(Source::ptr source)
  : SourceGui(source)
{
  textSource = qSharedPointerCast<Text>(source);
  Q_CHECK_PTR(textSource);

  _textItem = _variantManager->addProperty(QMetaType::QString, tr("Text"));
  _textItem->setValue(textSource->getText());

  _fontFamilyItem = _variantManager->addProperty(QMetaType::QString, tr("Font family"));
  _fontFamilyItem->setValue(textSource->getFontFamily());

  _fontSizeItem = _variantManager->addProperty(QMetaType::Int, tr("Font size (pt)"));
  _fontSizeItem->setValue(textSource->getFontSize());
  _fontSizeItem->setAttribute("minimum", 6);
  _fontSizeItem->setAttribute("maximum", 200);

  _boldItem = _variantManager->addProperty(QMetaType::Bool, tr("Bold"));
  _boldItem->setValue(textSource->isBold());

  _italicItem = _variantManager->addProperty(QMetaType::Bool, tr("Italic"));
  _italicItem->setValue(textSource->isItalic());

  _alignmentItem = _variantManager->addProperty(QtVariantPropertyManager::enumTypeId(), tr("Alignment"));
  QStringList alignNames;
  alignNames << tr("Left") << tr("Center") << tr("Right");
  _alignmentItem->setAttribute("enumNames", alignNames);
  int alignIdx = 1;
  if (textSource->getAlignment() & Qt::AlignLeft)        alignIdx = 0;
  else if (textSource->getAlignment() & Qt::AlignHCenter) alignIdx = 1;
  else if (textSource->getAlignment() & Qt::AlignRight)   alignIdx = 2;
  _alignmentItem->setValue(alignIdx);

  _propertyBrowser->addProperty(_textItem);
  _propertyBrowser->addProperty(_fontFamilyItem);
  _propertyBrowser->addProperty(_fontSizeItem);
  _propertyBrowser->addProperty(_boldItem);
  _propertyBrowser->addProperty(_italicItem);
  _propertyBrowser->addProperty(_alignmentItem);

  // Color swatch panel — replaces the color property browser rows.
  auto* colorPanel = new QWidget;
  auto* colorLayout = new QHBoxLayout(colorPanel);
  colorLayout->setContentsMargins(6, 6, 6, 4);
  colorLayout->setSpacing(6);

  auto makeColorBtn = [this](const QString& label, const QColor& initColor,
                             std::function<QColor()> getter,
                             std::function<void(const QColor&)> setter) -> QPushButton* {
    auto* btn = new QPushButton;
    btn->setMinimumHeight(36);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setToolTip(label);
    btn->setFlat(true);
    updateColorButton(btn, initColor);
    connect(btn, &QPushButton::clicked, this, [this, btn, label, getter, setter]() {
      QColor c = QColorDialog::getColor(getter(), btn, label,
                                        QColorDialog::ShowAlphaChannel);
      if (c.isValid()) {
        setter(c);
        updateColorButton(btn, c);
        emit valueChanged(_source);
      }
    });
    return btn;
  };

  _textColorButton = makeColorBtn(tr("Text color"),
    textSource->getTextColor(),
    [this]() { return textSource->getTextColor(); },
    [this](const QColor& c) { textSource->setTextColor(c); });

  _bgColorButton = makeColorBtn(tr("Background color"),
    textSource->getBgColor(),
    [this]() { return textSource->getBgColor(); },
    [this](const QColor& c) { textSource->setBgColor(c); });

  auto* textLbl = new QLabel(tr("Text"));
  textLbl->setAlignment(Qt::AlignCenter);
  auto* bgLbl = new QLabel(tr("BG"));
  bgLbl->setAlignment(Qt::AlignCenter);

  auto* col1 = new QVBoxLayout; col1->setSpacing(2);
  col1->addWidget(textLbl); col1->addWidget(_textColorButton);
  auto* col2 = new QVBoxLayout; col2->setSpacing(2);
  col2->addWidget(bgLbl); col2->addWidget(_bgColorButton);

  colorLayout->addLayout(col1);
  colorLayout->addLayout(col2);

  _compositeWidget = makeComposite(colorPanel, _propertyBrowser);
}

void TextGui::setValue(QtProperty* property, const QVariant& value)
{
  static const Qt::Alignment kAlignments[3] = { Qt::AlignLeft, Qt::AlignHCenter, Qt::AlignRight };

  if (property == _textItem) {
    textSource->setText(value.toString());
    emit valueChanged(_source);
  } else if (property == _fontFamilyItem) {
    textSource->setFontFamily(value.toString());
    emit valueChanged(_source);
  } else if (property == _fontSizeItem) {
    textSource->setFontSize(value.toInt());
    emit valueChanged(_source);
  } else if (property == _boldItem) {
    textSource->setBold(value.toBool());
    emit valueChanged(_source);
  } else if (property == _italicItem) {
    textSource->setItalic(value.toBool());
    emit valueChanged(_source);
  } else if (property == _alignmentItem) {
    int idx = qBound(0, value.toInt(), 2);
    textSource->setAlignment(static_cast<int>(kAlignments[idx]));
    emit valueChanged(_source);
  } else {
    SourceGui::setValue(property, value);
  }
}

void TextGui::setValue(QString propertyName, QVariant value)
{
  if      (propertyName == "text")        setValue(_textItem, value);
  else if (propertyName == "textColor") {
    textSource->setTextColor(value.value<QColor>());
    if (_textColorButton) updateColorButton(_textColorButton, value.value<QColor>());
  } else if (propertyName == "bgColor") {
    textSource->setBgColor(value.value<QColor>());
    if (_bgColorButton) updateColorButton(_bgColorButton, value.value<QColor>());
  } else if (propertyName == "fontFamily")  setValue(_fontFamilyItem, value);
  else if (propertyName == "fontSize")    setValue(_fontSizeItem, value);
  else if (propertyName == "bold")        setValue(_boldItem, value);
  else if (propertyName == "italic")      setValue(_italicItem, value);
  else if (propertyName == "alignment")   setValue(_alignmentItem, value);
  else SourceGui::setValue(propertyName, value);
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

  // --- Property items (file picker + numeric fields) ---
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

  // --- Info section (Name / Resolution / Duration / FPS / Codec) ---
  auto* infoFrame = new QFrame;
  infoFrame->setFrameShape(QFrame::StyledPanel);
  auto* infoGrid = new QGridLayout(infoFrame);
  infoGrid->setContentsMargins(8, 6, 8, 6);
  infoGrid->setVerticalSpacing(2);
  infoGrid->setHorizontalSpacing(8);

  auto addInfoRow = [&](const QString& key, QLabel*& valueOut, int row) {
    auto* kl = new QLabel(key);
    kl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    kl->setStyleSheet("color: palette(mid);");
    valueOut = new QLabel(tr("\xe2\x80\x94")); // em-dash
    valueOut->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    infoGrid->addWidget(kl, row, 0);
    infoGrid->addWidget(valueOut, row, 1);
  };
  addInfoRow(tr("Name"),       _infoNameLbl,  0);
  addInfoRow(tr("Resolution"), _infoResLbl,   1);
  addInfoRow(tr("Duration"),   _infoDurLbl,   2);
  addInfoRow(tr("FPS"),        _infoFpsLbl,   3);
  addInfoRow(tr("Codec"),      _infoCodecLbl, 4);
  infoGrid->setColumnStretch(1, 1);

  // --- Transport controls ---
  auto makeBtn = [](const QString& text, bool checkable = false) {
    auto* b = new QToolButton;
    b->setText(text);
    b->setFixedSize(34, 34);
    b->setAutoRaise(false);
    b->setCheckable(checkable);
    b->setStyleSheet(
      "QToolButton {"
      "  border: 1px solid palette(mid);"
      "  border-radius: 0px;"
      "  padding: 0px;"
      "}"
      "QToolButton:hover {"
      "  border-color: palette(highlight);"
      "  background: palette(highlight);"
      "}"
      "QToolButton:pressed {"
      "  background: palette(dark);"
      "}"
      "QToolButton:checked {"
      "  border: 2px solid palette(highlight);"
      "  background: palette(midlight);"
      "}");
    QFont f = b->font();
    f.setPointSize(17);
    b->setFont(f);
    return b;
  };

  _btnStepBack = makeBtn(QString::fromUtf8("\xe2\x97\x80")); // ◀
  _btnPause    = makeBtn(QString::fromUtf8("\xe2\x8f\xb8")); // ⏸
  _btnPlay     = makeBtn(QString::fromUtf8("\xe2\x96\xb6")); // ▶
  _btnToStart  = makeBtn(QString::fromUtf8("\xe2\x8f\xae")); // ⏮
  _btnSeekBack = makeBtn(QString::fromUtf8("\xe2\x8f\xaa")); // ⏪
  _btnSeekFwd  = makeBtn(QString::fromUtf8("\xe2\x8f\xa9")); // ⏩

  auto* transpRow = new QWidget;
  auto* transpHl  = new QHBoxLayout(transpRow);
  transpHl->setContentsMargins(6, 4, 6, 8);
  transpHl->setSpacing(2);
  auto* playLbl = new QLabel(tr("Play"));
  playLbl->setMinimumWidth(36);
  transpHl->addWidget(playLbl);
  transpHl->addWidget(_btnStepBack);
  transpHl->addWidget(_btnPause);
  transpHl->addWidget(_btnPlay);
  transpHl->addStretch();
  transpHl->addWidget(_btnToStart);
  transpHl->addWidget(_btnSeekBack);
  transpHl->addWidget(_btnSeekFwd);

  // --- Mode controls ---
  _btnModeLoop    = makeBtn(QString::fromUtf8("\xe2\x88\x9e"), true); // ∞
  _btnModeForward = makeBtn(QString::fromUtf8("\xe2\x86\x92"), true); // →
  _btnModeReverse = makeBtn(QString::fromUtf8("\xe2\x86\x90"), true); // ←
  _btnModeRevLoop = makeBtn(QString::fromUtf8("\xe2\x86\xba"), true); // ↺

  auto* modeGroup = new QButtonGroup(this);
  modeGroup->setExclusive(true);
  modeGroup->addButton(_btnModeLoop,    0);
  modeGroup->addButton(_btnModeForward, 1);
  modeGroup->addButton(_btnModeReverse, 2);
  modeGroup->addButton(_btnModeRevLoop, 3);

  // Set initial mode
  bool looping  = media->getPlayInLoop();
  bool reversed = media->getRate() < 0.0;
  if      ( looping && !reversed) _btnModeLoop->setChecked(true);
  else if (!looping && !reversed) _btnModeForward->setChecked(true);
  else if (!looping &&  reversed) _btnModeReverse->setChecked(true);
  else                            _btnModeRevLoop->setChecked(true);

  auto* modeRow = new QWidget;
  auto* modeHl  = new QHBoxLayout(modeRow);
  modeHl->setContentsMargins(6, 4, 6, 8);
  modeHl->setSpacing(2);
  auto* modeLbl = new QLabel(tr("Mode"));
  modeLbl->setMinimumWidth(36);
  modeHl->addWidget(modeLbl);
  modeHl->addWidget(_btnModeLoop);
  modeHl->addWidget(_btnModeForward);
  modeHl->addWidget(_btnModeReverse);
  modeHl->addWidget(_btnModeRevLoop);
  modeHl->addStretch();

  // --- Speed / Volume sliders ---
  auto* sliders = new QWidget;
  auto* svl = new QVBoxLayout(sliders);
  svl->setContentsMargins(0, 2, 0, 2);
  svl->setSpacing(0);
  svl->addWidget(makeSliderRow(tr("Speed (%)"),  0, 200, (int)rate,   _speedSlider,  _speedValueLbl));
  svl->addWidget(makeSliderRow(tr("Volume (%)"), 0, 100, (int)volume, _volumeSlider, _volumeValueLbl));

  // --- Build composite widget ---
  auto addSep = [](QVBoxLayout* vl) {
    auto* sep = new QFrame;
    sep->setFrameShape(QFrame::HLine);
    sep->setFrameShadow(QFrame::Sunken);
    vl->addWidget(sep);
  };

  auto* composite = new QWidget;
  auto* vl = new QVBoxLayout(composite);
  vl->setContentsMargins(0, 0, 0, 0);
  vl->setSpacing(0);
  vl->addWidget(infoFrame);
  addSep(vl);
  vl->addWidget(transpRow);
  addSep(vl);
  vl->addWidget(modeRow);
  addSep(vl);
  vl->addWidget(sliders);
  addSep(vl);
  vl->addWidget(_propertyBrowser, 1);
  _compositeWidget = composite;

  // --- Slider connections ---
  connect(_speedSlider, &QSlider::valueChanged, this, [this](int v) {
    _speedValueLbl->setText(QString::number(v) + "%");
    _mediaRateItem->setValue(double(v));
  });
  connect(_volumeSlider, &QSlider::valueChanged, this, [this](int v) {
    _volumeValueLbl->setText(QString::number(v) + "%");
    _mediaVolumeItem->setValue(double(v));
  });

  // --- Transport connections ---
  connect(_btnPlay, &QToolButton::clicked, this, [this]() {
    media->play();
  });
  connect(_btnPause, &QToolButton::clicked, this, [this]() {
    media->pause();
  });
  connect(_btnToStart, &QToolButton::clicked, this, [this]() {
    media->seekTo(0.0);
  });
  connect(_btnStepBack, &QToolButton::clicked, this, [this]() {
    qreal fps = media->getFrameRate();
    qint64 step = fps > 0.0 ? qint64(1000.0 / fps) : 33;
    media->seekToMs(qMax(0LL, media->getPosition() - step));
  });
  connect(_btnSeekBack, &QToolButton::clicked, this, [this]() {
    media->seekToMs(qMax(0LL, media->getPosition() - 5000));
  });
  connect(_btnSeekFwd, &QToolButton::clicked, this, [this]() {
    qint64 dur = media->getDuration();
    qint64 pos = media->getPosition() + 5000;
    media->seekToMs(dur > 0 ? qMin(dur, pos) : pos);
  });

  // --- Mode connections ---
  connect(_btnModeLoop, &QToolButton::clicked, this, [this]() {
    media->setPlayInLoop(true);
    if (media->getRate() < 0.0) { media->setRate(-media->getRate()); _mediaRateItem->setValue(media->getRate()*100); }
    emit valueChanged(_source);
  });
  connect(_btnModeForward, &QToolButton::clicked, this, [this]() {
    media->setPlayInLoop(false);
    if (media->getRate() < 0.0) { media->setRate(-media->getRate()); _mediaRateItem->setValue(media->getRate()*100); }
    emit valueChanged(_source);
  });
  connect(_btnModeReverse, &QToolButton::clicked, this, [this]() {
    media->setPlayInLoop(false);
    if (media->getRate() > 0.0) { media->setRate(-media->getRate()); _mediaRateItem->setValue(media->getRate()*100); }
    emit valueChanged(_source);
  });
  connect(_btnModeRevLoop, &QToolButton::clicked, this, [this]() {
    media->setPlayInLoop(true);
    if (media->getRate() > 0.0) { media->setRate(-media->getRate()); _mediaRateItem->setValue(media->getRate()*100); }
    emit valueChanged(_source);
  });

  // --- Metadata polling (stops once fps+codec arrive) ---
  _metadataTimer = new QTimer(this);
  _metadataTimer->setInterval(400);
  connect(_metadataTimer, &QTimer::timeout, this, &VideoGui::_refreshMetadata);
  QTimer::singleShot(0, this, &VideoGui::_refreshMetadata);
  _metadataTimer->start();
}

void VideoGui::_refreshMetadata()
{
  const QString uri = media->getUri();
  _infoNameLbl->setText(uri.isEmpty() ? tr("\xe2\x80\x94") : QFileInfo(uri).fileName());

  int w = media->getWidth(), h = media->getHeight();
  _infoResLbl->setText((w > 0 && h > 0) ? QString("%1\xc3\x97%2").arg(w).arg(h) : tr("\xe2\x80\x94"));

  qint64 dur = media->getDuration();
  if (dur > 0) {
    int ms  = (int)(dur % 1000);
    int sec = (int)((dur / 1000) % 60);
    int min = (int)((dur / 60000) % 60);
    int hr  = (int)(dur / 3600000);
    _infoDurLbl->setText(QString("%1:%2:%3.%4")
      .arg(hr,  2, 10, QChar('0'))
      .arg(min, 2, 10, QChar('0'))
      .arg(sec, 2, 10, QChar('0'))
      .arg(ms,  3, 10, QChar('0')));
  } else {
    _infoDurLbl->setText(tr("\xe2\x80\x94"));
  }

  qreal fps = media->getFrameRate();
  _infoFpsLbl->setText(fps > 0.0 ? QString::number(fps, 'f', 4) : tr("\xe2\x80\x94"));

  QString codec = media->getVideoCodec();
  _infoCodecLbl->setText(codec.isEmpty() ? tr("\xe2\x80\x94") : codec);

  // Stop polling once we have complete metadata
  if (dur > 0 && fps > 0.0 && !codec.isEmpty() && _metadataTimer)
    _metadataTimer->stop();
}

void VideoGui::setValue(QtProperty* property, const QVariant& value)
{
  if (property == _mediaFileItem)
  {
    QString newUri = value.toString();
    if (newUri != media->getUri()) {
      media->setUri(newUri);
      // Restart metadata polling for the new file
      if (_metadataTimer) _metadataTimer->start();
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
