/*
 * FppConnectDialog.cpp
 *
 * See FppConnectDialog.h.
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

#include "FppConnectDialog.h"

#include "MainWindow.h"
#include "FppClient.h"
#include "VideoExporter.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QLabel>
#include <QProgressBar>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QDir>
#include <QSettings>
#include <QMessageBox>

namespace mmp {

FppConnectDialog::FppConnectDialog(MainWindow* mainWindow, QWidget* parent)
  : QDialog(parent),
    _mainWindow(mainWindow),
    _fpp(new FppClient(this))
{
  setWindowTitle(tr("FPP Connect"));
  setModal(false);
  setMinimumWidth(440);

  // --- Device group ---------------------------------------------------------
  QGroupBox* deviceBox = new QGroupBox(tr("FPP device"), this);
  QFormLayout* deviceForm = new QFormLayout(deviceBox);

  QHBoxLayout* hostRow = new QHBoxLayout;
  _hostEdit = new QLineEdit(this);
  _hostEdit->setPlaceholderText(tr("IP address or hostname (e.g. 192.168.1.50)"));
  _verifyButton = new QPushButton(tr("Verify"), this);
  hostRow->addWidget(_hostEdit);
  hostRow->addWidget(_verifyButton);
  deviceForm->addRow(tr("Address:"), hostRow);

  _deviceLabel = new QLabel(tr("Not connected."), this);
  _deviceLabel->setWordWrap(true);
  deviceForm->addRow(QString(), _deviceLabel);

  // --- Source group ---------------------------------------------------------
  QGroupBox* sourceBox = new QGroupBox(tr("Video to upload"), this);
  QVBoxLayout* sourceLayout = new QVBoxLayout(sourceBox);

  _existingRadio = new QRadioButton(tr("Use an existing video file"), this);
  _exportRadio   = new QRadioButton(tr("Export the current project, then upload"), this);
  _existingRadio->setChecked(true);

  QHBoxLayout* fileRow = new QHBoxLayout;
  _fileEdit = new QLineEdit(this);
  _fileEdit->setPlaceholderText(tr("Path to an .mp4 file"));
  _browseButton = new QPushButton(tr("Browse..."), this);
  fileRow->addWidget(_fileEdit);
  fileRow->addWidget(_browseButton);

  sourceLayout->addWidget(_existingRadio);
  sourceLayout->addLayout(fileRow);
  sourceLayout->addWidget(_exportRadio);
  QLabel* exportNote = new QLabel(
      tr("Exporting records the output in real time, as the project plays."), this);
  exportNote->setWordWrap(true);
  exportNote->setStyleSheet("color: gray;");
  sourceLayout->addWidget(exportNote);

  // --- Progress / status ----------------------------------------------------
  _progress = new QProgressBar(this);
  _progress->setRange(0, 100);
  _progress->setValue(0);
  _progress->hide();

  _statusLabel = new QLabel(this);
  _statusLabel->setWordWrap(true);

  // --- Buttons --------------------------------------------------------------
  _uploadButton = new QPushButton(tr("Upload"), this);
  _uploadButton->setDefault(true);
  _closeButton  = new QPushButton(tr("Close"), this);

  QHBoxLayout* buttonRow = new QHBoxLayout;
  buttonRow->addStretch();
  buttonRow->addWidget(_uploadButton);
  buttonRow->addWidget(_closeButton);

  // --- Assemble -------------------------------------------------------------
  QVBoxLayout* root = new QVBoxLayout(this);
  root->addWidget(deviceBox);
  root->addWidget(sourceBox);
  root->addWidget(_progress);
  root->addWidget(_statusLabel);
  root->addLayout(buttonRow);

  // --- Wiring ---------------------------------------------------------------
  connect(_verifyButton, &QPushButton::clicked, this, &FppConnectDialog::onVerify);
  connect(_browseButton, &QPushButton::clicked, this, &FppConnectDialog::onBrowse);
  connect(_existingRadio, &QRadioButton::toggled, this, &FppConnectDialog::onModeChanged);
  connect(_uploadButton, &QPushButton::clicked, this, &FppConnectDialog::onUploadClicked);
  connect(_closeButton, &QPushButton::clicked, this, &QDialog::close);

  connect(_fpp, &FppClient::deviceVerified, this, &FppConnectDialog::onDeviceVerified);
  connect(_fpp, &FppClient::verifyFailed, this, &FppConnectDialog::onVerifyFailed);
  connect(_fpp, &FppClient::uploadProgress, this, &FppConnectDialog::onUploadProgress);
  connect(_fpp, &FppClient::uploadFinished, this, &FppConnectDialog::onUploadFinished);
  connect(_fpp, &FppClient::uploadFailed, this, &FppConnectDialog::onUploadFailed);

  // Restore last device and default the file to the last recording.
  QSettings settings;
  _hostEdit->setText(settings.value("fppConnectHost").toString());
  const QString lastDir = settings.value("lastRecordingDir").toString();
  if (!lastDir.isEmpty())
    _fileEdit->setText(QDir(lastDir).absolutePath());

  onModeChanged();
}

FppConnectDialog::~FppConnectDialog() = default;

void FppConnectDialog::onModeChanged()
{
  const bool existing = _existingRadio->isChecked();
  _fileEdit->setEnabled(existing);
  _browseButton->setEnabled(existing);
  _uploadButton->setText(existing ? tr("Upload") : tr("Export && Upload"));
}

void FppConnectDialog::onBrowse()
{
  QSettings settings;
  QString startDir = settings.value("lastRecordingDir").toString();
  const QString current = _fileEdit->text().trimmed();
  if (!current.isEmpty())
    startDir = QFileInfo(current).absolutePath();

  const QString path = QFileDialog::getOpenFileName(
      this, tr("Choose video file"), startDir,
      tr("Video files (*.mp4 *.mov *.avi *.mkv *.mpg *.mpeg *.m4v);;All files (*)"));
  if (!path.isEmpty())
    _fileEdit->setText(path);
}

void FppConnectDialog::onVerify()
{
  const QString host = _hostEdit->text().trimmed();
  if (host.isEmpty()) {
    setStatus(tr("Enter the FPP device address first."), true);
    return;
  }
  QSettings().setValue("fppConnectHost", host);
  _deviceLabel->setText(tr("Verifying %1...").arg(host));
  _verifyButton->setEnabled(false);
  _fpp->verify(host);
}

void FppConnectDialog::onDeviceVerified(const QString& hostName,
                                        const QString& version,
                                        const QString& platform)
{
  _verifyButton->setEnabled(true);
  QString text = tr("Connected to %1").arg(hostName);
  if (!version.isEmpty())
    text += tr(" — FPP %1").arg(version);
  if (!platform.isEmpty())
    text += tr(" (%1)").arg(platform);
  _deviceLabel->setText(text);
}

void FppConnectDialog::onVerifyFailed(const QString& error)
{
  _verifyButton->setEnabled(true);
  _deviceLabel->setText(tr("Not connected."));
  setStatus(error, true);
}

void FppConnectDialog::onUploadClicked()
{
  const QString host = _hostEdit->text().trimmed();
  if (host.isEmpty()) {
    setStatus(tr("Enter the FPP device address first."), true);
    return;
  }
  QSettings().setValue("fppConnectHost", host);

  if (_exportRadio->isChecked()) {
    startExport();
    return;
  }

  const QString file = _fileEdit->text().trimmed();
  if (file.isEmpty() || !QFileInfo::exists(file)) {
    setStatus(tr("Choose a video file to upload."), true);
    return;
  }
  beginUpload(file);
}

// ---------------------------------------------------------------------------
// Export then upload
// ---------------------------------------------------------------------------

void FppConnectDialog::startExport()
{
  VideoExporter* exporter = _mainWindow->videoExporter();
  if (!exporter) {
    setStatus(tr("Video recorder is still initializing — try again in a moment."), true);
    return;
  }
  if (_mainWindow->isRecording()) {
    setStatus(tr("A recording is already in progress."), true);
    return;
  }

  // Build a temp path with the user's configured recording format extension.
  QSettings s;
  const auto format =
      (VideoExporter::Format) s.value("videoFormat", (int)VideoExporter::H264_MP4).toInt();
  const QString ext = VideoExporter::formatExtension(format);
  _pendingTempExport = QDir(QDir::tempPath()).filePath("mymapmap-fpp-export." + ext);

  // Listen for completion only while exporting.
  connect(exporter, &VideoExporter::recordingStopped,
          this, &FppConnectDialog::onExportStopped, Qt::UniqueConnection);
  connect(exporter, &VideoExporter::errorOccurred,
          this, &FppConnectDialog::onExportError, Qt::UniqueConnection);

  setBusy(true);
  _exporting = true;
  _progress->setRange(0, 0); // busy/indeterminate during real-time record
  setStatus(tr("Exporting project (recording in real time)..."));

  if (!_mainWindow->startRecordingToFile(_pendingTempExport)) {
    disconnectExportSignals();
    _exporting = false;
    _pendingTempExport.clear();
    setBusy(false);
    setStatus(tr("Could not start export."), true);
  }
}

void FppConnectDialog::onExportStopped(const QString& filePath)
{
  if (!_exporting)
    return;
  disconnectExportSignals();
  _exporting = false;
  setStatus(tr("Export complete. Uploading..."));
  beginUpload(filePath.isEmpty() ? _pendingTempExport : filePath);
}

void FppConnectDialog::onExportError(const QString& error)
{
  if (!_exporting)
    return;
  disconnectExportSignals();
  _exporting = false;
  _pendingTempExport.clear();
  setBusy(false);
  setStatus(tr("Export failed: %1").arg(error), true);
}

void FppConnectDialog::disconnectExportSignals()
{
  VideoExporter* exporter = _mainWindow->videoExporter();
  if (exporter)
    exporter->disconnect(this);
}

// ---------------------------------------------------------------------------
// Upload
// ---------------------------------------------------------------------------

void FppConnectDialog::beginUpload(const QString& filePath)
{
  setBusy(true);
  _progress->setRange(0, 100);
  _progress->setValue(0);
  setStatus(tr("Uploading %1...").arg(QFileInfo(filePath).fileName()));
  _fpp->uploadVideo(_hostEdit->text().trimmed(), filePath);
}

void FppConnectDialog::onUploadProgress(qint64 sent, qint64 total)
{
  if (total <= 0)
    return;
  _progress->setRange(0, 100);
  _progress->setValue(int((sent * 100) / total));
}

void FppConnectDialog::onUploadFinished(const QString& remoteName)
{
  setBusy(false);
  _progress->setValue(100);
  setStatus(tr("Upload complete: %1 is now on the FPP device.").arg(remoteName));

  // Remove the temporary export, if we made one.
  if (!_pendingTempExport.isEmpty()) {
    QFile::remove(_pendingTempExport);
    _pendingTempExport.clear();
  }
}

void FppConnectDialog::onUploadFailed(const QString& error)
{
  setBusy(false);
  setStatus(error, true);
  if (!_pendingTempExport.isEmpty()) {
    QFile::remove(_pendingTempExport);
    _pendingTempExport.clear();
  }
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

void FppConnectDialog::setBusy(bool busy)
{
  _hostEdit->setEnabled(!busy);
  _verifyButton->setEnabled(!busy);
  _existingRadio->setEnabled(!busy);
  _exportRadio->setEnabled(!busy);
  _fileEdit->setEnabled(!busy && _existingRadio->isChecked());
  _browseButton->setEnabled(!busy && _existingRadio->isChecked());
  _uploadButton->setEnabled(!busy);
  _progress->setVisible(busy);
}

void FppConnectDialog::setStatus(const QString& text, bool error)
{
  _statusLabel->setStyleSheet(error ? "color: #c0392b;" : QString());
  _statusLabel->setText(text);
}

} // namespace mmp
