/*
 * PreferenceDialog.cpp
 *
 * (c) 2014 Sofian Audry -- info(@)sofianaudry(.)com
 * (c) 2014 Alexandre Quessy -- alexandre(@)quessy(.)net
 * (c) 2016 Dame Diongue -- baydamd(@)gmail(.)com
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

#include "PreferenceDialog.h"
#include "MainApplication.h"
#include "VideoExporter.h"

namespace mmp {

PreferenceDialog::PreferenceDialog(QWidget* parent) :
    QDialog(parent)
{
  // configure size
  resize(PREFERENCES_WINDOW_WIDTH, PREFERENCES_WINDOW_HEIGHT);
  setMinimumSize(PREFERENCES_WINDOW_WIDTH, PREFERENCES_WINDOW_HEIGHT);

  // Create different pages
  createInterfacePage();
  createMappingPage();
  createOutputPage();
  createControlsPage();
  createAdvancedPage();

  // | Cancel || OK |
  _buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok
                                    | QDialogButtonBox::Cancel);

  // Preferences list
  createPreferencesList();

  // Rearrange position
  QGridLayout *mainLayout = new QGridLayout;
  mainLayout->setColumnStretch(0, 1);
  mainLayout->setColumnStretch(1, 3);
  mainLayout->addWidget(_listWidget, 0, 0);
  mainLayout->addLayout(_stackedLayout, 0, 1);
  mainLayout->addWidget(_buttonBox, 1, 0, 1, 2);
  setLayout(mainLayout);

  connect(_buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
  connect(_buttonBox, SIGNAL(rejected()), this, SLOT(reject()));

  setWindowTitle(tr("Preferences"));
  _listWidget->setCurrentRow(0);
}

PreferenceDialog::~PreferenceDialog()
{

}

void PreferenceDialog::showDialog()
{
  exec();
}

void PreferenceDialog::closeEvent(QCloseEvent *event)
{
  emit closed();
  event->accept();
}

void PreferenceDialog::accept()
{
  // Save settings
  applySettings();
  // send signal for changes
  emit settingSaved();
  QDialog::accept();
}

void PreferenceDialog::reject()
{
  close();
}

int PreferenceDialog::exec()
{
  // Make sure settings is loaded
  if (!loadSettings())
    return QDialog::Rejected;

  return QDialog::exec();
}

bool PreferenceDialog::loadSettings()
{
  QSettings settings;
  // Listening port
  _listenPortNumber->setValue(settings.value("oscListeningPort", MM::DEFAULT_OSC_PORT).toInt());
  // Sticky vertices enabled
  _stickyVerticesBox->setChecked(settings.value("stickyVertices", MM::STICKY_VERTICES).toBool());
  // Sticky vertices radius
  _stickyRadiusBox->setCurrentText(settings.value("vertexStickRadius", MM::VERTEX_STICK_RADIUS).toString());
  // Output resolution preference
  _outputResolutionBox->setCurrentIndex(
    _outputResolutionBox->findData(settings.value("outputResolution", "1920x1080").toString()));
  if (_outputResolutionBox->currentIndex() < 0)
    _outputResolutionBox->setCurrentIndex(0);
  // Show screen resolution on output
  _showResolutionBox->setChecked(settings.value("showResolution", MM::SHOW_OUTPUT_RESOLUTION).toBool());
  // Show control on mouse hover
  _showControlOnOverBox->setChecked(settings.value("showControlOnMouseOver", MM::SHOW_OUTPUT_ON_MOUSE_HOVER).toBool());
  // Set preferred test signal pattern
  _radioGroup.at(settings.value("signalTestCard", MM::DEFAULT_TEST_CARD).toInt())->setChecked(true);
  // Set toolbar icon size
  _toolbarIconSizeBox->setCurrentIndex(_toolbarIconSizeBox->findData(
                                         settings.value("toolbarIconSize", MM::TOOLBAR_ICON_SIZE)));
  // Set language
  _languageBox->setCurrentIndex(_languageBox->findData(settings.value("language", MM::DEFAULT_LANGUAGE)));
  // Set theme
  _themeBox->setCurrentIndex(_themeBox->findData(settings.value("theme", "dark")));

  // Allow OSC message with same media source
  _oscSameMediaSourceBox->setChecked(settings.value("oscSameMediaSource", MM::OSC_SAME_MEDIA_SOURCE).toBool());
#ifdef HAVE_MCP
  // MCP port
  _mcpPortNumber->setValue(settings.value("mcpListeningPort", MM::DEFAULT_MCP_PORT).toInt());
#endif
  // Play in loop
  _playInLoopBox->setChecked(settings.value("playInLoop", MM::PLAY_IN_LOOP).toBool());

  // Video recording
  _videoFormatBox->setCurrentIndex(settings.value("videoFormat", (int)VideoExporter::H264_MP4).toInt());
  _videoQualityBox->setCurrentIndex(settings.value("videoQuality", (int)VideoExporter::HighQuality).toInt());
  _maxRecordLengthBox->setValue(settings.value("maxRecordLengthSec", 30).toInt());

  return true;
}

void PreferenceDialog::applySettings()
{
  QSettings settings;
  MainWindow *mainWindow = MainWindow::window();
  // Listen port
  settings.setValue("oscListeningPort", _listenPortNumber->value());
  mainWindow->setOscPort(settings.value("oscListeningPort").toInt());
  // Sticky vertices enabled
  settings.setValue("stickyVertices", _stickyVerticesBox->isChecked());
  // Sticky vertices radius
  settings.setValue("vertexStickRadius", _stickyRadiusBox->currentText());
  // Output resolution preference (apply immediately if no projector is connected)
  settings.setValue("outputResolution", _outputResolutionBox->currentData().toString());
  mainWindow->applyOutputResolutionFromPref();
  // Show screen resolution on output
  settings.setValue("showResolution", _showResolutionBox->isChecked());
  // Show control on mouse hover
  settings.setValue("showControlOnMouseOver", _showControlOnOverBox->isChecked());
  // Set preferred test signal pattern
  for (QRadioButton *radio: _radioGroup) {
    if (radio->isChecked()) {
      settings.setValue("signalTestCard", _radioGroup.indexOf(radio));
      break;
    }
  }
  // Set toolbar icon size
  settings.setValue("toolbarIconSize", _toolbarIconSizeBox->currentData());
  // Set language
  settings.setValue("language", _languageBox->currentData());
  // Set theme and apply immediately
  QString theme = _themeBox->currentData().toString();
  settings.setValue("theme", theme);
  MainApplication::applyTheme(theme);
  mainWindow->refreshIcons();
  // Allow OSC message with same media source
  settings.setValue("oscSameMediaSource", _oscSameMediaSourceBox->isChecked());
#ifdef HAVE_MCP
  // MCP port
  settings.setValue("mcpListeningPort", _mcpPortNumber->value());
  mainWindow->setMcpPort(_mcpPortNumber->value());
#endif
  // Play in loop
  settings.setValue("playInLoop", _playInLoopBox->isChecked());

  // Video recording
  settings.setValue("videoFormat",        _videoFormatBox->currentIndex());
  settings.setValue("videoQuality",       _videoQualityBox->currentIndex());
  settings.setValue("maxRecordLengthSec", _maxRecordLengthBox->value());
}

void PreferenceDialog::refreshCurrentIP()
{
  for (const QHostAddress &address: QNetworkInterface::allAddresses()) {
    if (address.protocol() == QAbstractSocket::IPv4Protocol
        && address != QHostAddress(QHostAddress::LocalHost)) {
      _machineAddressLabel->setText(address.toString());
      break;
    }
  }
}

void PreferenceDialog::createInterfacePage()
{
  _interfacePage = new QWidget;

  // Create language box
  createLanguageList();

  _toolbarIconSizeBox = new QComboBox;
  _toolbarIconSizeBox->addItem(tr("Large"), 64);
  _toolbarIconSizeBox->addItem(tr("Medium"), 48);
  _toolbarIconSizeBox->addItem(tr("Small"), 32);

  _themeBox = new QComboBox;
  _themeBox->addItem(tr("Dark"), "dark");
  _themeBox->addItem(tr("Light"), "light");

  QFormLayout *interfaceForm = new QFormLayout;
  interfaceForm->setFieldGrowthPolicy(QFormLayout::FieldsStayAtSizeHint);
  interfaceForm->setSpacing(10);
  interfaceForm->setContentsMargins(12, 12, 12, 12);
  interfaceForm->addRow(tr("Language (requires restart)"), _languageBox);
  interfaceForm->addRow(tr("Toolbar icon size (requires restart)"), _toolbarIconSizeBox);
  interfaceForm->addRow(tr("Theme"), _themeBox);

  QVBoxLayout *interfaceLayout = new QVBoxLayout;
  interfaceLayout->addLayout(interfaceForm);
  interfaceLayout->addStretch();

  _interfacePage->setLayout(interfaceLayout);
}

void PreferenceDialog::createMappingPage()
{
  _mappingPage = new QWidget;
  QMargins marginLeft(40, 0, 0, 0);

  // Vertex
  _stickyVerticesBox = new QCheckBox(tr("Enable Sticky vertices"));

  _stickyRadiusBox = new QComboBox;
  QStringList stickList;
  for (int i(1); i <= 10; ++i)
    stickList.append(QString::number(i * 10));
  _stickyRadiusBox->addItems(stickList);
  // Enable box only if sticky vertices is enabled
  connect(_stickyVerticesBox, SIGNAL(toggled(bool)), _stickyRadiusBox, SLOT(setEnabled(bool)));

  QFormLayout *stickRadiusForm = new QFormLayout;
  stickRadiusForm->setFieldGrowthPolicy(QFormLayout::FieldsStayAtSizeHint);
  stickRadiusForm->setContentsMargins(marginLeft);
  stickRadiusForm->addRow(tr("Sensitivity"), _stickyRadiusBox);

  QGroupBox *vertexGroupBox = new QGroupBox(tr("Vertices"));

  QVBoxLayout *vertexLayout = new QVBoxLayout;
  vertexLayout->addWidget(_stickyVerticesBox);
  vertexLayout->addLayout(stickRadiusForm);

  vertexGroupBox->setLayout(vertexLayout);

  // // Shape
  // QGroupBox *shapeGroupBox = new QGroupBox(tr("Shapes"));
  //
  // // Mapping
  // QGroupBox *layerGroupBox = new QGroupBox(tr("Layers"));

  QVBoxLayout *pageLayout = new QVBoxLayout;
  pageLayout->addWidget(vertexGroupBox);
  // pageLayout->addWidget(shapeGroupBox);
  // pageLayout->addWidget(layerGroupBox);

  _mappingPage->setLayout(pageLayout);
}

void PreferenceDialog::createOutputPage()
{
  _outputPage = new QWidget;

  // Output resolution preference (used when no projector is detected)
  _outputResolutionBox = new QComboBox;
  _outputResolutionBox->addItem(tr("720p HD (1280×720)"),    QString("1280x720"));
  _outputResolutionBox->addItem(tr("1080p HD (1920×1080)"), QString("1920x1080"));
  _outputResolutionBox->addItem(tr("2K (2560×1440)"),        QString("2560x1440"));
  _outputResolutionBox->addItem(tr("4K (3840×2160)"),        QString("3840x2160"));

  QFormLayout *resolutionForm = new QFormLayout;
  resolutionForm->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
  resolutionForm->setSpacing(8);
  resolutionForm->setContentsMargins(12, 8, 12, 8);
  resolutionForm->addRow(tr("Output resolution"), _outputResolutionBox);

  QGroupBox *resolutionGroupBox = new QGroupBox(tr("Output Resolution"));
  resolutionGroupBox->setLayout(resolutionForm);

  _showControlOnOverBox = new QCheckBox(tr("Only show output controls on mouse over"));

  QVBoxLayout *outputLayout = new QVBoxLayout;
  outputLayout->addWidget(_showControlOnOverBox);

  QGroupBox *outputGroupBox = new QGroupBox(tr("Output Layers"));
  outputGroupBox->setLayout(outputLayout);


  _showResolutionBox = new QCheckBox(tr("Show resolution on output test cards"));

  _classicRadio = new QRadioButton(tr("Classic test card"));
  _palTestRadio = new QRadioButton(tr("PAL test card"));
  _ntscTestRadio = new QRadioButton(tr("NTSC test card"));

  _radioGroup.append(_classicRadio);
  _radioGroup.append(_palTestRadio);
  _radioGroup.append(_ntscTestRadio);

  _classicTestImg = new QLabel;
  _palTestImg = new QLabel;
  _ntscTestImg = new QLabel;

  const QSize thumbSize(160, 90);
  auto thumb = [&](const QString &res) {
    return QPixmap(res).scaled(thumbSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
  };
  _classicTestImg->setPixmap(thumb(":/classic-test"));
  _palTestImg->setPixmap(thumb(":/pal-test"));
  _ntscTestImg->setPixmap(thumb(":/ntsc-test"));
  for (QLabel *lbl : {_classicTestImg, _palTestImg, _ntscTestImg}) {
    lbl->setFixedSize(thumbSize);
    lbl->setAlignment(Qt::AlignCenter);
  }

  QGridLayout *testLayout = new QGridLayout;
  testLayout->setContentsMargins(0, 0, 0, 0);
  testLayout->addWidget(_classicTestImg, 0, 0);
  testLayout->addWidget(_palTestImg, 0, 1);
  testLayout->addWidget(_ntscTestImg, 0, 2);
  testLayout->addWidget(_classicRadio, 1, 0);
  testLayout->addWidget(_palTestRadio, 1, 1);
  testLayout->addWidget(_ntscTestRadio, 1, 2);

  QVBoxLayout *testCardLayout = new QVBoxLayout;
  testCardLayout->addWidget(_showResolutionBox);
  testCardLayout->addSpacing(30);
  testCardLayout->addLayout(testLayout);
  testCardLayout->addStretch();

  QGroupBox *testCardGroupbox = new QGroupBox(tr("Test Card"));
  testCardGroupbox->setLayout(testCardLayout);

  // Recording group box
  _videoFormatBox = new QComboBox;
  for (int i = 0; i <= (int)VideoExporter::H264_MOV; ++i)
    _videoFormatBox->addItem(VideoExporter::formatLabel((VideoExporter::Format)i), i);

  _videoQualityBox = new QComboBox;
  _videoQualityBox->addItem(tr("Low"),       (int)VideoExporter::LowQuality);
  _videoQualityBox->addItem(tr("Medium"),    (int)VideoExporter::MediumQuality);
  _videoQualityBox->addItem(tr("High"),      (int)VideoExporter::HighQuality);
  _videoQualityBox->addItem(tr("Very High"), (int)VideoExporter::VeryHighQuality);

  _maxRecordLengthBox = new QSpinBox;
  _maxRecordLengthBox->setRange(1, 3600);
  _maxRecordLengthBox->setSuffix(tr(" sec"));
  _maxRecordLengthBox->setToolTip(tr("Used when all media is set to loop, or when only images are present."));

  QFormLayout *recordingForm = new QFormLayout;
  recordingForm->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
  recordingForm->setSpacing(8);
  recordingForm->setContentsMargins(12, 8, 12, 8);
  recordingForm->addRow(tr("Format / Codec"),        _videoFormatBox);
  recordingForm->addRow(tr("Quality"),               _videoQualityBox);
  recordingForm->addRow(tr("Max default length"),    _maxRecordLengthBox);

  QGroupBox *recordingGroupBox = new QGroupBox(tr("Recording"));
  recordingGroupBox->setLayout(recordingForm);

  QVBoxLayout *outputPageLayout = new QVBoxLayout;
  outputPageLayout->addWidget(resolutionGroupBox);
  outputPageLayout->addWidget(outputGroupBox);
  outputPageLayout->addWidget(testCardGroupbox);
  outputPageLayout->addWidget(recordingGroupBox);

  _outputPage->setLayout(outputPageLayout);
}

void PreferenceDialog::createControlsPage()
{
  _controlsPage = new QTabWidget;

  // Osc Tab
  _oscWidget = new QWidget;

  QFont bold;
  bold.setWeight(QFont::Bold);
  // Set IP Address validator
  // QString octet("(?:[0-1]?[0-9]?[0-9]|2[0-4][0-9]|25[0-5])");
  // QRegularExpression rx(octet + "\\." + octet + "\\." + octet + "\\." + octet);
  // QRegularExpressionValidator *ipValidator = new QRegularExpressionValidator(rx);
  QMargins margins(40, 0, 0, 0);

  // _sendMessageBox = new QCheckBox;
  // _sendMessageBox->setFont(bold);
  // _sendMessageBox->setText(tr("Send OSC messages"));
  // _sendMessageBox->setEnabled(false); // Disable until supported
  //
  // _sendAddressEdit = new QLineEdit;
  // _sendAddressEdit->setValidator(ipValidator);
  // _sendAddressEdit->setEnabled(false); // Disable until supported
  //
  // _sendPortNumber = new QSpinBox;
  // _sendPortNumber->setRange(1024, 65534);
  // _sendPortNumber->setFixedWidth(120);
  // _sendPortNumber->setEnabled(false); // Disable until supported
  //
  // QFormLayout *sendMessageForm = new QFormLayout;
  // sendMessageForm->setFieldGrowthPolicy(QFormLayout::FieldsStayAtSizeHint);
  // sendMessageForm->setContentsMargins(margins);
  // sendMessageForm->addRow(tr("to machine"), _sendAddressEdit);
  // sendMessageForm->addRow(tr("at port"), _sendPortNumber);

  _listenMessageBox = new QCheckBox;
  _listenMessageBox->setFont(bold);
  _listenMessageBox->setText(tr("Listen to OSC messages"));
  _listenMessageBox->setChecked(true); // TODO: Give the choice to the user

  _listenPortNumber = new QSpinBox;
  _listenPortNumber->setRange(1024, 65534);
  _listenPortNumber->setFixedWidth(120);

  _oscSameMediaSourceBox = new QCheckBox(tr("Allow message with existing media source"));
  _oscSameMediaSourceBox->setChecked(false);

  QFormLayout *listenPortForm = new QFormLayout;
  listenPortForm->setContentsMargins(margins);
  listenPortForm->addRow(tr("on port"), _listenPortNumber);

  _machineAddressLabel = new QLabel;
  _ipRefreshButton = new QPushButton(tr("Refresh"));
  connect(_ipRefreshButton, SIGNAL(clicked()), this, SLOT(refreshCurrentIP()));

  QHBoxLayout *listenAddressLayout = new QHBoxLayout;
  listenAddressLayout->addWidget(_machineAddressLabel);
  listenAddressLayout->addWidget(_ipRefreshButton);

  QFormLayout *listenAddressForm = new QFormLayout;
  listenAddressForm->setFieldGrowthPolicy(QFormLayout::FieldsStayAtSizeHint);
  listenAddressForm->addRow(tr("Local IP"), listenAddressLayout);

  QVBoxLayout *oscLayout = new QVBoxLayout;
  oscLayout->setSpacing(8);
  oscLayout->setContentsMargins(12, 12, 12, 12);
  oscLayout->addWidget(_listenMessageBox);
  oscLayout->addLayout(listenPortForm);
  oscLayout->addSpacing(6);
  oscLayout->addWidget(_oscSameMediaSourceBox);
  oscLayout->addSpacing(12);
  oscLayout->addLayout(listenAddressForm);
  oscLayout->addStretch();

  _oscWidget->setLayout(oscLayout);

  _controlsPage->addTab(_oscWidget, tr("OSC Setup"));

#ifdef HAVE_MCP
  // MCP Tab
  _mcpWidget = new QWidget;

  _mcpPortNumber = new QSpinBox;
  _mcpPortNumber->setRange(0, 65534);
  _mcpPortNumber->setFixedWidth(120);
  _mcpPortNumber->setSpecialValueText(tr("Disabled"));

  QFormLayout *mcpPortForm = new QFormLayout;
  mcpPortForm->setFieldGrowthPolicy(QFormLayout::FieldsStayAtSizeHint);
  mcpPortForm->addRow(tr("MCP port (0 to disable)"), _mcpPortNumber);

  QVBoxLayout *mcpLayout = new QVBoxLayout;
  mcpLayout->addLayout(mcpPortForm);
  mcpLayout->addStretch();

  _mcpWidget->setLayout(mcpLayout);

  _controlsPage->addTab(_mcpWidget, tr("MCP Setup"));
#endif

  refreshCurrentIP();
}

void PreferenceDialog::createAdvancedPage()
{
  _advancedPage = new QTabWidget;

  // Playback tab
  _playbackWidget = new QWidget;

  // Play in loop
  _playInLoopBox = new QCheckBox(tr("Play in loop (requires restart)"));
  _playInLoopBox->setChecked(true); // Loop by default

  QVBoxLayout *playbackLayout = new QVBoxLayout;
  playbackLayout->addWidget(_playInLoopBox, 1, Qt::AlignTop);

  _playbackWidget->setLayout(playbackLayout);

  _advancedPage->addTab(_playbackWidget, tr("Playback"));
}

void PreferenceDialog::createPreferencesList()
{
  // Apparence Item
  QListWidgetItem *interfaceItem = new QListWidgetItem(QIcon(":/pref-interface"), tr("Interface"));

  // Mapping & Shape Item
  QListWidgetItem *shapeItem = new QListWidgetItem(QIcon(":/control-points"), tr("Layers"));

  // Output Item
  QListWidgetItem *outputItem = new QListWidgetItem(QIcon(":/output-window"), tr("Output"));

  // Controls Item
  QListWidgetItem *controlsItem = new QListWidgetItem(QIcon(":/pref-controls"), tr("Controls"));

  // Advanced Item
  QListWidgetItem *advancedItem = new QListWidgetItem(QIcon(":/pref-advanced"), tr("Advanced"));

  // Add items in the list
  _listWidget = new QListWidget;
  _listWidget->setFixedWidth(140);
  _listWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  _listWidget->addItem(interfaceItem);
  _listWidget->addItem(shapeItem);
  _listWidget->addItem(outputItem);
  _listWidget->addItem(controlsItem);
  _listWidget->addItem(advancedItem);

  for (int i = 0; i < _listWidget->count(); i++) {
    QListWidgetItem *item = _listWidget->item(i);
    item->setSizeHint(QSize(140, PreferenceDialog::LIST_ITEM_HEIGHT));
  }

  // Add pages in the stacked layout
  _stackedLayout = new QStackedLayout;
  _stackedLayout->addWidget(_interfacePage);
  _stackedLayout->addWidget(_mappingPage);
  _stackedLayout->addWidget(_outputPage);
  _stackedLayout->addWidget(_controlsPage);
  _stackedLayout->addWidget(_advancedPage);
  // Sync list and pages
  connect(_listWidget, SIGNAL(currentRowChanged(int)), _stackedLayout, SLOT(setCurrentIndex(int)));
}

void PreferenceDialog::createLanguageList()
{
  _languageBox = new QComboBox;

  // format systems language
  QString defaultLocale = QLocale::system().name();
  defaultLocale.truncate(defaultLocale.lastIndexOf('_'));

  _languageBox->addItem("System default", defaultLocale);

  QString translationPath = QApplication::applicationDirPath().append("/translations");
  QDir dir(translationPath);
  QStringList translations = dir.entryList(QStringList("mapmap_*.qm"));

  for (int i(0); i < translations.size(); i++) {
    // get locale extracted by filename
    QString locale;
    locale = translations[i]; // mapmap_fr.qm
    locale.truncate(locale.lastIndexOf('.')); // mapmap_fr
    locale.remove(0, locale.indexOf('_') + 1); // fr

    QString lang = QLocale::languageToString(QLocale(locale).language());

    _languageBox->addItem(QString("%1 (%2)").arg(lang).arg(locale), locale);
  }
}


}
