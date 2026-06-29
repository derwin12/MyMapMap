/*
 * FppConnectDialog.h
 *
 * "FPP Connect" dialog: export the current MyMapMap project to a video file
 * (or pick an existing one) and upload it to a Falcon Player (FPP) device,
 * the same workflow xLights offers under Tools > FPP Connect.
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

#ifndef FPPCONNECTDIALOG_H
#define FPPCONNECTDIALOG_H

#include <QDialog>
#include <QString>

QT_BEGIN_NAMESPACE
class QLineEdit;
class QPushButton;
class QRadioButton;
class QLabel;
class QProgressBar;
QT_END_NAMESPACE

namespace mmp {

class MainWindow;
class FppClient;

class FppConnectDialog : public QDialog {
  Q_OBJECT

public:
  explicit FppConnectDialog(MainWindow* mainWindow, QWidget* parent = nullptr);
  ~FppConnectDialog() override;

private slots:
  void onVerify();
  void onBrowse();
  void onModeChanged();
  void onUploadClicked();

  // FppClient feedback.
  void onDeviceVerified(const QString& hostName, const QString& version,
                        const QString& platform);
  void onVerifyFailed(const QString& error);
  void onUploadProgress(qint64 sent, qint64 total);
  void onUploadFinished(const QString& remoteName);
  void onUploadFailed(const QString& error);

  // Export (record) feedback, connected only while exporting.
  void onExportStopped(const QString& filePath);
  void onExportError(const QString& error);

private:
  void beginUpload(const QString& filePath);
  void startExport();
  void setBusy(bool busy);
  void setStatus(const QString& text, bool error = false);
  void disconnectExportSignals();

  MainWindow*   _mainWindow;
  FppClient*    _fpp;

  QLineEdit*    _hostEdit;
  QPushButton*  _verifyButton;
  QLabel*       _deviceLabel;

  QRadioButton* _existingRadio;
  QRadioButton* _exportRadio;
  QLineEdit*    _fileEdit;
  QPushButton*  _browseButton;

  QProgressBar* _progress;
  QLabel*       _statusLabel;
  QPushButton*  _uploadButton;
  QPushButton*  _closeButton;

  QString _pendingTempExport;  // temp file we created and should delete after
  bool    _exporting = false;
};

} // namespace mmp

#endif // FPPCONNECTDIALOG_H
