/*
 * MainWindow.cpp
 *
 * (c) 2013 Sofian Audry -- info(@)sofianaudry(.)com
 * (c) 2013 Alexandre Quessy -- alexandre(@)quessy(.)net
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

#include "MainWindow.h"
#include "Mesh.h"
#include "Maths.h"
#include "PreferenceDialog.h"
#include "AboutDialog.h"
#include "ShortcutWindow.h"
#include "Commands.h"
#include "ProjectWriter.h"
#include "ProjectReader.h"
#ifdef Q_OS_MAC
#include "Syphon.h"
#include "SyphonServerDialog.h"
#endif
#include <sstream>
#include <string>
#include <QOpenGLWidget>
#include <QGuiApplication>
#include <QScreen>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDesktopServices>
#include <QDir>
#include <QDirIterator>
#include <QStandardPaths>

namespace mmp {

// Auto-scaling, optionally animated QLabel for the source preview panel.
class SourcePreviewLabel : public QLabel {
public:
  explicit SourcePreviewLabel(QWidget* parent = nullptr) : QLabel(parent) {
    setAlignment(Qt::AlignCenter);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMinimumHeight(80);
    connect(&_animTimer, &QTimer::timeout, this, [this]() {
      if (_animFrames.isEmpty()) return;
      _animIdx = (_animIdx + 1) % _animFrames.size();
      _showFrame(_animIdx);
    });
  }

  void setSourcePixmap(const QPixmap& px) {
    _animTimer.stop();
    _animFrames.clear();
    _original = px;
    _updateScaled();
  }

  void setAnimationFrames(const QVector<QPixmap>& frames, int fps = 4) {
    _animTimer.stop();
    _original = QPixmap();
    _animFrames = frames;
    _animIdx = 0;
    _fps = fps;
    if (frames.isEmpty()) { clear(); return; }
    _showFrame(0);
    if (frames.size() > 1) {
      _animTimer.setInterval(1000 / qMax(1, _fps));
      _animTimer.start();
    }
  }

  void setAnimationFps(int fps) {
    _fps = qMax(1, fps);
    if (_animTimer.isActive())
      _animTimer.setInterval(1000 / _fps);
  }

  void clearAll() {
    _animTimer.stop();
    _animFrames.clear();
    _original = QPixmap();
    clear();
  }

protected:
  void resizeEvent(QResizeEvent* e) override {
    QLabel::resizeEvent(e);
    if (!_animFrames.isEmpty())
      _showFrame(_animIdx);
    else
      _updateScaled();
  }

private:
  QPixmap        _original;
  QVector<QPixmap> _animFrames;
  int            _animIdx = 0;
  int            _fps = 4;
  QTimer         _animTimer;

  void _showFrame(int idx) {
    if (idx < 0 || idx >= _animFrames.size()) return;
    QLabel::setPixmap(_animFrames[idx].scaled(size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
  }
  void _updateScaled() {
    if (_original.isNull()) { clear(); return; }
    QLabel::setPixmap(_original.scaled(size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
  }
};

MainWindow::MainWindow()
{
  // Create model.
#if QT_VERSION >= 0x050500
  QMessageLogger(__FILE__, __LINE__, nullptr).info() << "Video support: " <<
      (Video::hasVideoSupport() ? "yes" : "no");
#else
  QMessageLogger(__FILE__, __LINE__, 0).debug() << "Video support: " <<
      (Video::hasVideoSupport() ? "yes" : "no");
#endif

  mappingManager = new MappingManager;

  // Initialize internal variables.
  currentSourceId = NULL_UID;
  currentLayerId = NULL_UID;
  // TODO: not sure we need this anymore since we have NULL_UID
  _hasCurrentSource = false;
  _hasCurrentLayer = false;
  currentSelectedItem = nullptr;

  // Frames per second.
  _framesPerSecond = (-1);

  // Play state.
  _isPlaying = false;

  // Editing toggles.
  _displayControls = true;
  _displaySourceControls = true;
  _stickyVertices = true;
  _audioMuted = false;
  _displayUndoStack = false;
  _showMenuBar = true; // Show menubar by default

  // UndoStack
  undoStack = new QUndoStack(this);
  connect(undoStack, &QUndoStack::indexChanged, this, &MainWindow::windowModified);

  createLayout();
  createActions();
  createMenus();
  createLayerContextMenu();
  createSourceContextMenu();
  createToolBars();
  createStatusBar();
  updateRecentFileActions();
  updateRecentVideoActions();

  readSettings();

  // Defer all UI state restoration to after the singleton is assigned and
  // the event loop starts. Any restoreGeometry/restoreState/setChecked called
  // during construction triggers a resize → canvas repaint → MainWindow::window()
  // re-entry, causing infinite recursive construction.
  QTimer::singleShot(0, this, [this]() {
    QSettings s;

    // Window / pane geometry
    restoreGeometry(s.value("geometry").toByteArray());
    restoreState(s.value("windowState").toByteArray());
    mainSplitter->restoreState(s.value("mainSplitter").toByteArray());
    sourceSplitter->restoreState(s.value("sourceSplitter").toByteArray());
    layerSplitter->restoreState(s.value("layerSplitter").toByteArray());
    canvasSplitter->restoreState(s.value("canvasSplitter").toByteArray());
    outputWindow->restoreGeometry(s.value("outputWindow").toByteArray());
    int savedScreen = s.value("outputScreen", QApplication::screens().size() - 1).toInt();
    outputWindow->setPreferredScreen(savedScreen);
    // Sync the "Output screen" menu checkmarks to the restored value.
    if (savedScreen >= 0 && savedScreen < screenActions.size())
      screenActions.at(savedScreen)->setChecked(true);

    // Action toggle states
    outputFullScreenAction->setChecked(false); // always start windowed
    displayTestSignalAction->setChecked(s.value("displayTestSignal", MM::DISPLAY_TEST_SIGNAL).toBool());
    // Always start with controls ON — persisting this setting lets a stale
    // "false" (e.g. from a crashed recording session) permanently hide handles.
    displayControlsAction->setChecked(true);
    outputWindow->setCanvasDisplayCrosshair(true);
    displayUndoHistoryAction->setChecked(s.value("displayUndoStack", MM::DISPLAY_UNDO_HISTORY).toBool());
    displayZoomToolAction->setChecked(s.value("zoomToolBar", MM::DISPLAY_ZOOM_TOOLBAR).toBool());
    showMenuBarAction->setChecked(s.value("showMenuBar", MM::DISPLAY_MENU_BAR).toBool());
    displaySourceControlsAction->setChecked(s.value("displayAllControls", MM::DISPLAY_ALL_CONTROLS).toBool());
    stickyVerticesAction->setChecked(s.value("stickyVertices", MM::STICKY_VERTICES).toBool());
    muteAllAction->setChecked(s.value("audioMuted", false).toBool());
    int toolBarIconSize = s.value("toolbarIconSize", MM::TOOLBAR_ICON_SIZE).toInt();
    mainToolBar->setIconSize(QSize(toolBarIconSize, toolBarIconSize));
    int srcIconSize = s.value("sourceListIconSize", MainWindow::PAINT_LIST_ICON_SIZE).toInt();
    _sourceListIconSize = srcIconSize;
    for (int i = 0; i < 3; ++i) {
      static const int kSizes[3] = { 24, 32, 48 };
      if (_thumbSizeBtns[i]) _thumbSizeBtns[i]->setChecked(kSizes[i] == srcIconSize);
    }
    bool thumbMode = s.value("sourceListThumbnailMode", true).toBool();
    setSourceListThumbnailMode(thumbMode);

    // Recent file/video menus
    updateRecentFileActions();
    updateRecentVideoActions();

    // VideoExporter (Qt Multimedia backend init also deferred)
    _videoExporter = new VideoExporter(this);
    connect(_videoExporter, &VideoExporter::recordingStopped,
            this, &MainWindow::onRecordingStopped);
    connect(_videoExporter, &VideoExporter::errorOccurred, this, [this](const QString& msg) {
      statusBar()->showMessage(tr("Recording error: %1").arg(msg), 5000);
      recordAction->setChecked(false);
    });
  });

  // Thumbnail cache (deferred start — needs event loop for async QMediaPlayer).
  _thumbnailCache = new ThumbnailCache(this);
  connect(_thumbnailCache, &ThumbnailCache::ready,
          this, &MainWindow::onThumbnailReady);

  // Start osc.
  startOscReceiver();

#ifdef HAVE_MCP
  // Start MCP server.
  startMcpServer();
#endif

  // Defaults.
  setWindowIcon(QIcon(":/mapmap-logo"));
  setCurrentFile("");

  // Allow drag n drop
  setAcceptDrops(true);

  // Create and start timer.
  videoTimer = new QTimer(this);
  connect(videoTimer, SIGNAL(timeout()), this, SLOT(processFrame()));
  setFramesPerSecond(MM::DEFAULT_FRAMES_PER_SECOND);
  videoTimer->start();

  // Create elapsed timer.
  systemTimer = new QElapsedTimer;
  systemTimer->start();

  // Start playing by default.
  play();
}

MainWindow::~MainWindow()
{
  delete mappingManager;
  //  delete _facade;
  delete osc_timer;
  delete systemTimer;
}

void MainWindow::handleSourceItemSelectionChanged()
{
  // Set current source.
  QListWidgetItem* item = sourceList->currentItem();

  // Ignore clicks on section-header items (they are not selectable sources).
  if (item == _sourceSectionImages || item == _sourceSectionVideos ||
      item == _sourceSectionGenerated || item == _sourceSectionFolders) {
    sourceList->clearSelection();
    return;
  }

  currentSelectedItem = item;

  // Is a source item selected?
  bool sourceItemSelected = (item ? true : false);

  if (sourceItemSelected)
  {
    // Set current source.
    uid sourceId = getItemId(*item);
    // Unselect current mapping.
    if (currentSourceId != sourceId)
      removeCurrentLayer();
    // Set current source.
    setCurrentSource(sourceId);
  }
  else
    removeCurrentSource();

  // Enable/disable creation of mappings depending on whether a source is selected.
  addMeshAction->setEnabled(sourceItemSelected);
  addTriangleAction->setEnabled(sourceItemSelected);
  addEllipseAction->setEnabled(sourceItemSelected);
  if (addPolygonAction) addPolygonAction->setEnabled(sourceItemSelected);
  deleteSourceAction->setEnabled(sourceItemSelected);
  renameSourceAction->setEnabled(sourceItemSelected);

  // Update canvases.
  updateCanvases();
}

void MainWindow::handleLayerItemSelectionChanged(const QModelIndex &index)
{
  // Set current source and mappings.
  uid layerId = layerListModel->getItemId(index);
  Layer::ptr layer = mappingManager->getLayerById(layerId);
  uid sourceId = layer->getSource()->getId();
  // Set current mapping and source
  setCurrentLayer(layerId);
  setCurrentSource(sourceId);
  // Enable destination zoom toolbar buttons and avoid loop
  if (!destinationCanvasToolbar->buttonsAreEnable()) {
    // Enable destination toolbar
   destinationCanvasToolbar->enableZoomToolBar(true);
   // Enable source toolbar
   sourceCanvasToolbar->enableZoomToolBar(true);
   // Enable source and mapping edit action
   duplicateLayerAction->setEnabled(true);
   deleteLayerAction->setEnabled(true);
   renameLayerAction->setEnabled(true);
   layerLockedAction->setEnabled(true);
   layerHideAction->setEnabled(true);
   layerSoloAction->setEnabled(true);
   layerRotate90CWAction->setEnabled(true);
   layerRotate90CCWAction->setEnabled(true);
   layerRotate180Action->setEnabled(true);
   layerHorizontalFlipAction->setEnabled(true);
   layerVerticalFlipAction->setEnabled(true);
   layerRaiseAction->setEnabled(true);
   layerLowerAction->setEnabled(true);
   layerRaiseToTopAction->setEnabled(true);
   layerLowerToBottomAction->setEnabled(true);
   // Enable zoom action
   zoomInAction->setEnabled(true);
   zoomOutAction->setEnabled(true);
   resetZoomAction->setEnabled(true);
   fitToViewAction->setEnabled(true);
  }

  // Update canvases.
  updateCanvases();
  updateLayerListColumnWidth();
}

void MainWindow::handleLayerItemChanged(const QModelIndex &index)
{
  // Get item.
  uid layerId = layerListModel->getItemId(index);

  // Sync name.
  Layer::ptr layer = mappingManager->getLayerById(layerId);
  Q_CHECK_PTR(layer);

  // Change properties.
  layer->setName(index.data(Qt::EditRole).toString());
  layer->setVisible(index.data(Qt::CheckStateRole).toBool());
  layer->setSolo(index.data(Qt::CheckStateRole + 1).toBool());
  layer->setLocked(index.data(Qt::CheckStateRole + 2).toBool());

  // Update model (important to make sure icons get updated in the interface).
  layerListModel->updateModel();

  updatePlayingState();
 }

void MainWindow::handleLayerIndexesMoved()
{
  // Resync mapping manager.
  syncLayerManager();

  // Update canvases according to new order.
  updateCanvases();

  // Update playing state.
  updatePlayingState();
}

void MainWindow::handleSourceItemSelected(QListWidgetItem* item)
{
  Q_UNUSED(item);
  // Change currently selected item.
  currentSelectedItem = item;
}

void MainWindow::handleSourceChanged(Source::ptr source)
{
  // Change currently selected item.
  uid curLayerId = getCurrentLayerId();
  removeCurrentLayer();
  removeCurrentSource();

  uid sourceId = mappingManager->getSourceId(source);

//  QSharedPointer<Texture> texture;

  if (source->getSourceType() == SourceType::Video)
  {
    QSharedPointer<Video> media = qSharedPointerCast<Video>(source);
    Q_CHECK_PTR(media);
    updateSourceItem(sourceId, getSourceIcon(source), strippedName(media->getUri()));
  }
  if (source->getSourceType() == SourceType::Image)
  {
    QSharedPointer<Image> image = qSharedPointerCast<Image>(source);
    Q_CHECK_PTR(image);
    updateSourceItem(sourceId, getSourceIcon(source), strippedName(image->getUri()));
  }
  else if (source->getSourceType() == SourceType::Color)
  {
    // Pop-up color-choosing dialog to choose color source.
    QSharedPointer<Color> color = qSharedPointerCast<Color>(source);
    Q_CHECK_PTR(color);
    updateSourceItem(sourceId, getSourceIcon(source), strippedName(color->getColor().name()));
  }
#ifdef Q_OS_MAC
  else if (source->getSourceType() == SourceType::Syphon)
  {
    // Keep the list item label/icon in sync (e.g. after re-pointing the server).
    updateSourceItem(sourceId, getSourceIcon(source), source->getName());
  }
#endif

  setCurrentSource(sourceId);

  if (curLayerId != NULL_UID)
  {
    setCurrentLayer(curLayerId);
  }

//  updatePlayingState();
}

void MainWindow::layerPropertyChanged(uid id, QString propertyName, QVariant value)
{
  // Retrieve mapping.
  Layer::ptr layer = mappingManager->getLayerById(id);
  Q_CHECK_PTR(layer);

  // Send to mapping gui.
  LayerGui::ptr layerGui = getLayerGuiByLayerId(id);
  Q_CHECK_PTR(layerGui);
  layerGui->setValue(propertyName, value);

  // Send to actions.
  if (layer == getCurrentLayer())
  {
    if (propertyName == "visible")
    {
      layerHideAction->setChecked(!value.toBool());
      updatePlayingState();
    }
    else if (propertyName == "solo")
    {
      layerSoloAction->setChecked(value.toBool());
      updatePlayingState();
    }
    else if (propertyName == "locked")
    {
      layerLockedAction->setChecked(value.toBool());
    }
    else if (propertyName == "sourceId")
    {
      layerGui->updateSources();
      updatePlayingState();
    }
  }

  // Send to list items.
  const QModelIndex& index = layerListModel->getIndexFromId(layer->getId());
  if (propertyName == "name")
  {
    layerListModel->setData(index, layer->getName(), Qt::EditRole);
  }
  else if (propertyName == "visible")
  {
    layerListModel->setData(index, layer->isVisible(), Qt::CheckStateRole);
  }
  else if (propertyName == "solo")
  {
    layerListModel->setData(index, layer->isSolo(), Qt::CheckStateRole + 1);
  }
  else if (propertyName == "locked")
  {
    layerListModel->setData(index, layer->isLocked(), Qt::CheckStateRole + 2);
  }
}

void MainWindow::sourcePropertyChanged(uid id, QString propertyName, QVariant value)
{
  // Retrieve source.
  Source::ptr source = mappingManager->getSourceById(id);
  Q_CHECK_PTR(source);

  // Send to source gui.
  SourceGui::ptr sourceGui = getSourceGuiBySourceId(id);
  Q_CHECK_PTR(sourceGui);

  sourceGui->setValue(propertyName, value);

  // Send to list items.
  QListWidgetItem* sourceItem = getItemFromId(*sourceList, id);
  if (propertyName == "name")
    sourceItem->setText(source->getName());
}

void MainWindow::closeEvent(QCloseEvent *event)
{
  // Stop video playback to avoid lags. XXX Hack
  pause(false);

  // Popup dialog allowing the user to save before closing.
  if (okToContinue())
  {
    // Save settings
    writeSettings();
//    _preferenceDialog->saveSettings();
    // Close all top level widgets
    for (QWidget *widget: QApplication::topLevelWidgets()) {
      if (widget != this) { // Avoid recursion
        widget->close();
      }
    }
    event->accept();
  }
  else
  {
    event->ignore();
    // We're not actually closing after all: restart video playback. XXX Hack
    play(false);
  }
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
#ifdef Q_OS_MACOS // On Mac OS X
  Q_UNUSED(event);
  // Do nothing
#endif

#ifdef Q_OS_LINUX // On Linux
  if (event->modifiers() & Qt::AltModifier) {
    QString currentDesktop = QString(getenv("XDG_CURRENT_DESKTOP")).toLower();
    if (currentDesktop != "unity" && !_showMenuBar) {
      menuBar()->setHidden(!menuBar()->isHidden());
      menuBar()->setFocus(Qt::MenuBarFocusReason);
    }
  }
#endif
#ifdef Q_OS_WIN32
  if (event->modifiers() & Qt::AltModifier) {
    if (!_showMenuBar) {
      menuBar()->setHidden(!menuBar()->isHidden());
      menuBar()->setFocus(Qt::MenuBarFocusReason);
    }
  }
#endif
}

bool MainWindow::eventFilter(QObject *object, QEvent *event)
{
  QMenu *menu = static_cast<QMenu*>(object);

  if (menu && (event->type() == QEvent::MouseButtonPress
      || event->type() == QEvent::MouseButtonDblClick))
  {
    QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
    // Disable right click on context menu actions
    if (mouseEvent->buttons() & Qt::RightButton) {
      mouseEvent->ignore();
      return true;
    }
    return false;
  }

  return QMainWindow::eventFilter(object, event);
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
  const QMimeData *mimeData = event->mimeData();
  bool allowDrag = true;

  if (mimeData->hasUrls()) {
    for (const QUrl& url : mimeData->urls()) {
      QString fileName = url.toLocalFile();
      // Don't allow drag if file is not supported
      if (!fileSupported(fileName, MM::FILE_EXTENSION) &&
          !fileSupported(fileName, MM::IMAGE_FILES_FILTER) &&
          !fileSupported(fileName, MM::VIDEO_FILES_FILTER)) {
        allowDrag = false;
      }
    }
  }

  if (allowDrag)
    event->acceptProposedAction();
}

void MainWindow::dragMoveEvent(QDragMoveEvent *event)
{
  event->acceptProposedAction();
}

void MainWindow::dragLeaveEvent(QDragLeaveEvent *event)
{
  event->accept();
}

void MainWindow::dropEvent(QDropEvent *event)
{
  const QMimeData *mimeData = event->mimeData();

  if (mimeData->hasUrls()) {
    // In case that dragged many files
    for (const QUrl& url : mimeData->urls()) {
      QString fileName = url.toLocalFile();

      if (!fileName.isEmpty()) {
        // Test if is mmp file and exit loop
        if (fileSupported(fileName, MM::FILE_EXTENSION)) {
          if (okToContinue()) {
            loadFile(fileName);
          }
          // Exit for prevent drag to many project files
          break;
        }
        // Allow to drag too many videos or images
        else {
          // Check if file is image or not
          // according to file extension
          if (fileSupported(fileName, MM::IMAGE_FILES_FILTER))
            importMediaFile(fileName, true);
          else
            importMediaFile(fileName, false);
        }
      }
    }
  }
  event->acceptProposedAction();
}

void MainWindow::setOutputWindowFullScreen(bool enable)
{
  outputWindow->setFullScreen(enable);
  // setCheckState
  displayControlsAction->setChecked(enable);
  displaySourceControlsAction->setChecked(enable);
 }

void MainWindow::newFile()
{
  // Stop video playback to avoid lags. XXX Hack
  pause(false);

  // Popup dialog allowing the user to save before creating a new file.
  if (okToContinue())
  {
    clearWindow();
    setCurrentFile("");
    undoStack->clear();
  }

  // Restart video playback. XXX Hack
  play(false);
}

void MainWindow::open()
{
  // Stop video playback to avoid lags. XXX Hack
  pause(false);

  // Popup dialog allowing the user to save before opening a new file.
  if (okToContinue())
  {
// Temporary fix of QFileDialog on GTK
#ifdef Q_OS_LINUX
    QString fileName = QFileDialog::getOpenFileName(this,
                                                    tr("Open project"),
                                                    settings.value("defaultProjectDir").toString(),
                                                    tr("MyMapMap files (*.%1)").arg(MM::FILE_EXTENSION),
                                                    nullptr, QFileDialog::DontUseNativeDialog);
#else
    QString fileName = QFileDialog::getOpenFileName(this,
                                                    tr("Open project"),
                                                    settings.value("defaultProjectDir").toString(),
                                                    tr("MyMapMap files (*.%1)").arg(MM::FILE_EXTENSION));
#endif

    if (! fileName.isEmpty())
      loadFile(fileName);
  }

  // Restart video playback. XXX Hack
  play(false);
}

bool MainWindow::save()
{
  // Popup save-as dialog if file has never been saved.
  if (curFile.isEmpty())
  {
    return saveAs();
  }
  else
  {
    return saveFile(curFile);
  }
}

bool MainWindow::saveAs()
{
  // Stop video playback to avoid lags. XXX Hack
  pause(false);

  // Suggest the current project's name, or "untitled" for a new project.
  QString defaultName = curFile.isEmpty() ? tr("untitled") : QFileInfo(curFile).completeBaseName();
  QString defaultPath = QDir(settings.value("defaultProjectDir").toString())
                           .filePath(QString("%1.%2").arg(defaultName, MM::FILE_EXTENSION));

#ifdef Q_OS_LINUX
  QString fileName = QFileDialog::getSaveFileName(this,
                                                  tr("Save project"), defaultPath,
                                                  tr("MyMapMap files (*.%1)").arg(MM::FILE_EXTENSION),
                                                  nullptr, QFileDialog::DontUseNativeDialog);
#else
  // Popul file dialog to choose filename.
  QString fileName = QFileDialog::getSaveFileName(this,
                                                  tr("Save project"), defaultPath,
                                                  tr("MyMapMap files (*.%1)").arg(MM::FILE_EXTENSION));
#endif

  // Restart video playback. XXX Hack
  play(false);

  if (fileName.isEmpty())
    return false;

  if (! fileName.endsWith(MM::FILE_EXTENSION))
  {
    std::cout << "filename doesn't end with expected extension: " <<
                 fileName.toStdString() << std::endl;
    fileName.append(".");
    fileName.append(MM::FILE_EXTENSION);
  }

  // Save to filename.
  return saveFile(fileName);
}

void MainWindow::importMedia()
{
  // Stop video playback, if it is playing, to avoid lags. XXX Hack
  pause(!_isPlaying);

  // Pop-up file-choosing dialog to choose media file.
  // TODO: restrict the type of files that can be imported
#ifdef Q_OS_LINUX
  QString fileName = QFileDialog::getOpenFileName(this,
                                                  tr("Import media source file"),
                                                  settings.value("defaultVideoDir").toString(),
                                                  tr("Media files (%1 %2);;All files (*)")
                                                  .arg(MM::VIDEO_FILES_FILTER)
                                                  .arg(MM::IMAGE_FILES_FILTER),
                                                  nullptr, QFileDialog::DontUseNativeDialog);
#else
  QString fileName = QFileDialog::getOpenFileName(this,
                                                  tr("Import media source file"),
                                                  settings.value("defaultVideoDir").toString(),
                                                  tr("Media files (%1 %2);;All files (*)")
                                                  .arg(MM::VIDEO_FILES_FILTER)
                                                  .arg(MM::IMAGE_FILES_FILTER));
#endif
  // Restart video playback if it was previously playing. XXX Hack
  play(!_isPlaying);

  // Check if file is image or not
  // according to file extension
  if (!fileName.isEmpty()) {
    if (!QFileInfo(fileName).suffix().isEmpty() && MM::IMAGE_FILES_FILTER.contains(QFileInfo(fileName).suffix(), Qt::CaseInsensitive))
      importMediaFile(fileName, true);
    else
      importMediaFile(fileName, false);
  }
}

void MainWindow::importFolder()
{
  QString startDir = QFileInfo(settings.value("defaultVideoDir").toString()).absolutePath();
  QString dirPath = QFileDialog::getExistingDirectory(
      this, tr("Import Files From Folder"),
      startDir);
  if (dirPath.isEmpty())
    return;

  const QStringList allExts = (MM::IMAGE_FILES_FILTER + " " + MM::VIDEO_FILES_FILTER)
                                .split(' ', Qt::SkipEmptyParts);

  QDirIterator it(dirPath, allExts, QDir::Files);
  int imported = 0;
  while (it.hasNext()) {
    QString filePath = it.next();
    QString ext = QFileInfo(filePath).suffix();
    bool isImage = MM::IMAGE_FILES_FILTER.contains(ext, Qt::CaseInsensitive);
    if (importMediaFile(filePath, isImage))
      ++imported;
  }

  if (imported > 0)
    statusBar()->showMessage(tr("Imported %1 file(s) from folder").arg(imported), 4000);
  else
    statusBar()->showMessage(tr("No supported media files found in folder"), 4000);
}

void MainWindow::importFolderAsSource()
{
  QString dirPath = QFileDialog::getExistingDirectory(
    this, tr("Add Image Folder"),
    settings.value("defaultImageDir").toString());

  if (dirPath.isEmpty())
    return;

  QApplication::setOverrideCursor(Qt::WaitCursor);
  uid id = createFolderSource(NULL_UID, dirPath);
  QApplication::restoreOverrideCursor();

  if (id != NULL_UID) {
    settings.setValue("defaultImageDir", dirPath);
    statusBar()->showMessage(tr("Folder added: %1").arg(QDir(dirPath).dirName()), 3000);
  } else {
    QMessageBox::warning(this, tr("Empty Folder"),
      tr("No image files found in the selected folder."));
  }
}

void MainWindow::openCameraDevice()
{
#if QT_VERSION >= 0x050300
  // Stop video playback, if it is playing, to avoid lags. XXX Hack
  pause(!_isPlaying);

  QString device;
  QList<QCameraDevice> cameras = QMediaDevices::videoInputs();

  if (cameras.count() > 1)
  {
    QStringList devicesList;
    QMap<QString, QString> devices;

    for (const QCameraDevice &cameraInfo: cameras)
    {
      devicesList << cameraInfo.description();
      devices.insert(cameraInfo.description(), QString::fromUtf8(cameraInfo.id()));
    }

    bool ok;
    QString deviceName = QInputDialog::getItem(this, tr("Camera device"),
                                               tr("Select camera"), devicesList, 0, false, &ok);

    if (ok && !deviceName.isEmpty())
    {
      if (devices.contains(deviceName))
        device = devices.value(deviceName);
    }

  }

  else
  {
    if (QMediaDevices::defaultVideoInput().isNull())
    {
      QMessageBox::warning(this, tr("No camera available"), tr("You can not use this feature!\nNo camera available in your system"));

    }
    else
    {
      device = QString::fromUtf8(QMediaDevices::defaultVideoInput().id());
    }
  }

  // Restart video playback if it was previously playing. XXX Hack
  play(!_isPlaying);

  if (!device.isEmpty())
    importMediaFile(device, false, true);
#else
    QMessageBox::warning(this, tr("No camera available"), tr("You can not use this feature!\nNo camera available in your system"));
#endif
}

void MainWindow::addColor()
{
  bool wasPlaying = _isPlaying;
  if (wasPlaying) pause(false);

  static QColor color = QColor(0, 255, 0, 255);
#ifdef Q_OS_LINUX
  color = QColorDialog::getColor(color, this, tr("Select Color"),
                                  QColorDialog::DontUseNativeDialog |
                                 QColorDialog::ShowAlphaChannel);
#else
  color = QColorDialog::getColor(color, this, tr("Select Color"),
                                 QColorDialog::ShowAlphaChannel);
#endif
  if (color.isValid())
    addColorSource(color);

  if (wasPlaying) play(false);
}

void MainWindow::addText()
{
  bool wasPlaying = _isPlaying;
  if (wasPlaying) pause(false);

  bool ok = false;
  QString text = QInputDialog::getText(this, tr("Add Text Source"),
                                       tr("Text:"), QLineEdit::Normal,
                                       tr("Text"), &ok);
  if (ok && !text.isEmpty())
    addTextSource(text);

  if (wasPlaying) play(false);
}

void MainWindow::addSyphon()
{
#ifdef Q_OS_MAC
  bool wasPlaying = _isPlaying;
  if (wasPlaying) pause(false);

  SyphonServerDialog dialog(this);
  if (dialog.exec() == QDialog::Accepted)
  {
    if (dialog.hasSelection())
    {
      const SyphonServerDescription server = dialog.selectedServer();
      createSyphonSource(NULL_UID, server.uuid, server.name, server.appName);
    }
    else
    {
      // Create an unbound source; the user can pick a server later.
      createSyphonSource(NULL_UID, QString(), QString(), QString());
    }
    statusBar()->showMessage(tr("Syphon source added"), 2000);
  }

  if (wasPlaying) play(false);
#endif
}

void MainWindow::autoFitSyphonInputShapes(int sourceId, int width, int height)
{
#ifdef HAVE_SYPHON
  if (width <= 0 || height <= 0)
    return;

  Source::ptr source = mappingManager->getSourceById(sourceId);
  if (source.isNull() || source->getSourceType() != SourceType::Syphon)
    return;

  const qreal defW = Syphon::DEFAULT_WIDTH;
  const qreal defH = Syphon::DEFAULT_HEIGHT;
  const qreal sx = width / defW;
  const qreal sy = height / defH;
  const qreal eps = 1.0;

  bool changed = false;
  QMap<uid, Layer::ptr> layers = mappingManager->getSourceLayers(source);
  for (QMap<uid, Layer::ptr>::const_iterator it = layers.constBegin();
       it != layers.constEnd(); ++it)
  {
    Layer::ptr layer = it.value();
    if (layer.isNull() || !layer->hasInputShape())
      continue;

    MShape::ptr input = layer->getInputShape();
    QVector<QPointF> verts = input->getVertices();
    if (verts.isEmpty())
      continue;

    // Only rescale shapes still at the untouched default size, so we never
    // clobber a shape the user has already adjusted.
    qreal minX = verts[0].x(), maxX = verts[0].x();
    qreal minY = verts[0].y(), maxY = verts[0].y();
    for (const QPointF& v : verts)
    {
      minX = qMin(minX, v.x()); maxX = qMax(maxX, v.x());
      minY = qMin(minY, v.y()); maxY = qMax(maxY, v.y());
    }
    if (qAbs((maxX - minX) - defW) > eps || qAbs((maxY - minY) - defH) > eps)
      continue;

    for (QPointF& v : verts)
      v = QPointF(v.x() * sx, v.y() * sy);
    input->setVertices(verts);
    input->build();
    changed = true;
  }

  if (changed)
  {
    updateMappers();
    updateCanvases();
  }
#else
  Q_UNUSED(sourceId); Q_UNUSED(width); Q_UNUSED(height);
#endif
}

void MainWindow::addMesh()
{
  // A source must be selected to add a mapping.
  if (getCurrentSourceId() == NULL_UID)
    return;

  // Retrieve current source (as texture).
  Source::ptr source = getMappingManager().getSourceById(getCurrentSourceId());
  Q_CHECK_PTR(source);

  // Create input and output quads.
  Layer* layerPtr;
  if (source->getSourceType() == SourceType::Color)
  {
    MShape::ptr outputQuad = MShape::ptr(Util::createMeshForColor(sourceCanvas->width(), sourceCanvas->height()));
    layerPtr = new ColorLayer(source, outputQuad);
  }
  else
  {
    QSharedPointer<Texture> texture = qSharedPointerCast<Texture>(source);
    Q_CHECK_PTR(texture);

    MShape::ptr outputQuad = MShape::ptr(Util::createMeshForTexture(texture.data(), sourceCanvas->width(), sourceCanvas->height()));
    MShape::ptr  inputQuad = MShape::ptr(Util::createMeshForTexture(texture.data(), sourceCanvas->width(), sourceCanvas->height()));
    layerPtr = new TextureLayer(source, outputQuad, inputQuad);
  }

  // Create texture mapping.
  Layer::ptr layer(layerPtr);
  uint layerId = mappingManager->addLayer(layer);

  // Lets the undo-stack handle Undo/Redo the adding of mapping item.
  undoStack->push(new AddLayerCommand(this, layerId));
}

void MainWindow::addTriangle()
{
  // A source must be selected to add a mapping.
  if (getCurrentSourceId() == NULL_UID)
    return;

  // Retrieve current source (as texture).
  Source::ptr source = getMappingManager().getSourceById(getCurrentSourceId());
  Q_CHECK_PTR(source);

  // Create input and output quads.
  Layer* layerPtr;
  if (source->getSourceType() == SourceType::Color)
  {
    MShape::ptr outputTriangle = MShape::ptr(Util::createTriangleForColor(sourceCanvas->width(), sourceCanvas->height()));
    layerPtr = new ColorLayer(source, outputTriangle);
  }
  else
  {
    QSharedPointer<Texture> texture = qSharedPointerCast<Texture>(source);
    Q_CHECK_PTR(texture);

    MShape::ptr outputTriangle = MShape::ptr(Util::createTriangleForTexture(texture.data(), sourceCanvas->width(), sourceCanvas->height()));
    MShape::ptr inputTriangle = MShape::ptr(Util::createTriangleForTexture(texture.data(), sourceCanvas->width(), sourceCanvas->height()));
    layerPtr = new TextureLayer(source, inputTriangle, outputTriangle);
  }

  // Create mapping.
  Layer::ptr layer(layerPtr);
  uint layerId = mappingManager->addLayer(layer);

  // Lets undo-stack handle Undo/Redo the adding of mapping item.
  undoStack->push(new AddLayerCommand(this, layerId));
}

void MainWindow::addEllipse()
{
  // A source must be selected to add a mapping.
  if (getCurrentSourceId() == NULL_UID)
    return;

  // Retrieve current source (as texture).
  Source::ptr source = getMappingManager().getSourceById(getCurrentSourceId());
  Q_CHECK_PTR(source);

  // Create input and output ellipses.
  Layer* layerPtr;
  if (source->getSourceType() == SourceType::Color)
  {
    MShape::ptr outputEllipse = MShape::ptr(Util::createEllipseForColor(sourceCanvas->width(), sourceCanvas->height()));
    layerPtr = new ColorLayer(source, outputEllipse);
  }
  else
  {
    QSharedPointer<Texture> texture = qSharedPointerCast<Texture>(source);
    Q_CHECK_PTR(texture);

    MShape::ptr outputEllipse = MShape::ptr(Util::createEllipseForTexture(texture.data(), sourceCanvas->width(), sourceCanvas->height()));
    MShape::ptr inputEllipse = MShape::ptr(Util::createEllipseForTexture(texture.data(), sourceCanvas->width(), sourceCanvas->height()));
    layerPtr = new TextureLayer(source, inputEllipse, outputEllipse);
  }

  // Create mapping.
  Layer::ptr layer(layerPtr);
  uint layerId = mappingManager->addLayer(layer);

  // Lets undo-stack handle Undo/Redo the adding of mapping item.
  undoStack->push(new AddLayerCommand(this, layerId));
}

void MainWindow::addPolygon()
{
  // A source must be selected to add a mapping.
  if (getCurrentSourceId() == NULL_UID)
    return;
  startPolygonDrawMode();
}

void MainWindow::startPolygonDrawMode()
{
  _polygonDrawMode = true;
  _polygonPoints.clear();
  _polygonCursorPos = QPointF();

  // For texture sources, draw on the source/input canvas so the user selects
  // which region of the source image to use.
  Source::ptr src = getMappingManager().getSourceById(getCurrentSourceId());
  _polygonDrawOnSource = (src && src->getSourceType() != SourceType::Color);

  MapperGLCanvas* canvas = _polygonDrawOnSource ? sourceCanvas : destinationCanvas;
  canvas->setCursor(Qt::CrossCursor);
  canvas->setFocus();   // ensure keyboard focus so Esc/Enter reach this canvas
  statusBar()->showMessage(tr("Click to add polygon vertices. Click near first point or press Enter to close. Escape to cancel."));
  canvas->update();
}

void MainWindow::cancelPolygonDrawMode()
{
  MapperGLCanvas* canvas = _polygonDrawOnSource ? sourceCanvas : destinationCanvas;
  _polygonDrawMode = false;
  _polygonPoints.clear();
  canvas->unsetCursor();
  statusBar()->clearMessage();
  canvas->update();
}

void MainWindow::polygonCursorMoved(const QPointF& scenePos)
{
  _polygonCursorPos = scenePos;
  (_polygonDrawOnSource ? sourceCanvas : destinationCanvas)->update();
}

void MainWindow::polygonCanvasClick(const QPointF& scenePos)
{
  MapperGLCanvas* canvas = _polygonDrawOnSource ? sourceCanvas : destinationCanvas;
  // Snap to first point if close enough and we have 3+ points.
  if (_polygonPoints.size() >= 3) {
    QPointF first = _polygonPoints.first();
    QPointF delta = canvas->mapFromScene(scenePos) - canvas->mapFromScene(first);
    if (delta.x()*delta.x() + delta.y()*delta.y() <= sq(MM::VERTEX_SELECT_RADIUS * 2)) {
      finishPolygon();
      return;
    }
  }
  _polygonPoints << scenePos;
  canvas->update();
}

void MainWindow::finishPolygon()
{
  if (_polygonPoints.size() < 3) {
    cancelPolygonDrawMode();
    return;
  }

  uid sourceId = getCurrentSourceId();
  if (sourceId == NULL_UID) { cancelPolygonDrawMode(); return; }
  Source::ptr source = getMappingManager().getSourceById(sourceId);
  if (!source) { cancelPolygonDrawMode(); return; }

  Layer* layerPtr;
  if (source->getSourceType() == SourceType::Color) {
    MShape::ptr outPoly(Util::createFreePolygonForColor(_polygonPoints));
    layerPtr = new ColorLayer(source, outPoly);
  } else {
    QSharedPointer<Texture> texture = qSharedPointerCast<Texture>(source);
    Q_CHECK_PTR(texture);
    if (_polygonDrawOnSource) {
      // Points were placed on the source/input canvas → become the input polygon.
      // Output starts at the same coordinates (user can reposition independently).
      MShape::ptr inPoly(Util::createFreePolygonForColor(_polygonPoints));
      MShape::ptr outPoly(Util::createFreePolygonForColor(_polygonPoints));
      layerPtr = new TextureLayer(source, outPoly, inPoly);
    } else {
      MShape::ptr outPoly(Util::createFreePolygonForColor(_polygonPoints));
      MShape::ptr inPoly(Util::createFreePolygonInputForTexture(_polygonPoints, texture.data()));
      layerPtr = new TextureLayer(source, outPoly, inPoly);
    }
  }

  Layer::ptr layer(layerPtr);
  uid layerId = mappingManager->addLayer(layer);
  undoStack->push(new AddLayerCommand(this, layerId));

  cancelPolygonDrawMode();
}

void MainWindow::addPolygonVertex()
{
  if (_polyEditType != PolyEditAdd) return;
  Layer::ptr layer = getCurrentLayer();
  if (!layer) return;

  if (_polyEditOnSource && qSharedPointerCast<TextureLayer>(layer).isNull())
    return;

  // Choose which shape the user clicked on and which is the "other" shape.
  MShape::ptr editedShape = _polyEditOnSource
    ? qSharedPointerCast<TextureLayer>(layer)->getInputShape()
    : layer->getShape();
  MShape::ptr otherShape;
  TextureLayer::ptr texLayer = qSharedPointerCast<TextureLayer>(layer);
  if (texLayer)
    otherShape = _polyEditOnSource ? layer->getShape()
                                   : texLayer->getInputShape();

  int  edgeIdx = _polyEditIndex;
  qreal t      = _polyEditT;
  int  n       = editedShape->nVertices();

  // Build new vertex list for edited shape: insert interpolated point.
  QVector<QPointF> newEditVerts = editedShape->getVertices();
  QPointF newPt = newEditVerts[edgeIdx] * (1.0 - t)
                + newEditVerts[(edgeIdx + 1) % n] * t;
  newEditVerts.insert(edgeIdx + 1, newPt);

  // Mirror the insertion into the other shape at the same parametric position.
  QVector<QPointF> newOtherVerts;
  if (otherShape) {
    newOtherVerts = otherShape->getVertices();
    QPointF otherPt = newOtherVerts[edgeIdx] * (1.0 - t)
                    + newOtherVerts[(edgeIdx + 1) % n] * t;
    newOtherVerts.insert(edgeIdx + 1, otherPt);
  }

  QVector<QPointF> newOutVerts = _polyEditOnSource ? newOtherVerts : newEditVerts;
  QVector<QPointF> newInVerts  = _polyEditOnSource ? newEditVerts  : newOtherVerts;

  undoStack->push(new SetPolygonVerticesCommand(this, layer->getId(),
                                                 newOutVerts, newInVerts, tr("Add Vertex")));
}

void MainWindow::deletePolygonVertex()
{
  if (_polyEditType != PolyEditDelete) return;
  Layer::ptr layer = getCurrentLayer();
  if (!layer) return;

  MShape::ptr outputShape = layer->getShape();
  if (outputShape->nVertices() <= 3) return; // keep minimum triangle

  int vertIdx = _polyEditIndex;

  QVector<QPointF> newOutVerts = outputShape->getVertices();
  newOutVerts.remove(vertIdx);

  QVector<QPointF> newInVerts;
  TextureLayer::ptr texLayer = qSharedPointerCast<TextureLayer>(layer);
  if (texLayer) {
    newInVerts = texLayer->getInputShape()->getVertices();
    newInVerts.remove(vertIdx);
  }

  undoStack->push(new SetPolygonVerticesCommand(this, layer->getId(),
                                                 newOutVerts, newInVerts, tr("Delete Vertex")));
}

void MainWindow::checkForUpdates(bool autoCheck)
{
  if (!autoCheck) {
    checkForUpdatesAction->setEnabled(false);
    checkForUpdatesAction->setText(tr("Checking..."));
  }

  QNetworkAccessManager *nam = new QNetworkAccessManager(this);
  QNetworkRequest req(QUrl("https://api.github.com/repos/derwin12/MyMapMap/releases/latest"));
  req.setRawHeader("Accept", "application/vnd.github+json");
  req.setRawHeader("User-Agent", "MyMapMap");

  QNetworkReply *reply = nam->get(req);
  connect(reply, &QNetworkReply::finished, this, [this, reply, nam, autoCheck]() {
    checkForUpdatesAction->setEnabled(true);
    checkForUpdatesAction->setText(tr("Check for &Updates..."));
    reply->deleteLater();
    nam->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
      if (!autoCheck)
        QMessageBox::warning(this, tr("Update Check Failed"),
          tr("Could not reach the update server.\n\n%1").arg(reply->errorString()));
      return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    QString latestTag = doc.object().value("tag_name").toString().remove(0, 1); // strip leading 'v'
    QString current   = MM::VERSION;

    if (latestTag.isEmpty()) {
      if (!autoCheck)
        QMessageBox::warning(this, tr("Update Check"), tr("Could not parse version information."));
      return;
    }

    // On auto-check, skip versions the user has chosen to ignore.
    QSettings settings;
    if (autoCheck && settings.value("skipVersion").toString() == latestTag)
      return;

    if (latestTag == current) {
      if (!autoCheck)
        QMessageBox::information(this, tr("Up to Date"),
          tr("You are running the latest version of MyMapMap (%1).").arg(current));
      return;
    }

    // Update available — show dialog with three choices.
    QDialog dlg(this);
    dlg.setWindowTitle(tr("Update Available"));
    dlg.setMinimumWidth(420);

    QLabel *icon = new QLabel;
    icon->setPixmap(style()->standardPixmap(QStyle::SP_MessageBoxInformation).scaled(48, 48, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    icon->setAlignment(Qt::AlignTop);

    QLabel *title = new QLabel(tr("<b style='font-size:13pt'>MyMapMap %1 is available</b>").arg(latestTag));
    QLabel *body  = new QLabel(tr("You are currently running version <b>%1</b>.<br>"
                                  "Would you like to download the new release?").arg(current));
    body->setWordWrap(true);

    QPushButton *btnDownload = new QPushButton(tr("Open Download Page"));
    QPushButton *btnIgnore   = new QPushButton(tr("Ignore This Version"));
    QPushButton *btnLater    = new QPushButton(tr("Remind Me Later"));
    btnDownload->setDefault(true);

    QHBoxLayout *btnRow = new QHBoxLayout;
    btnRow->addStretch();
    btnRow->addWidget(btnLater);
    btnRow->addWidget(btnIgnore);
    btnRow->addWidget(btnDownload);

    QVBoxLayout *text = new QVBoxLayout;
    text->addWidget(title);
    text->addSpacing(6);
    text->addWidget(body);
    text->addSpacing(16);
    text->addLayout(btnRow);

    QHBoxLayout *root = new QHBoxLayout(&dlg);
    root->addWidget(icon);
    root->addSpacing(12);
    root->addLayout(text);

    connect(btnDownload, &QPushButton::clicked, &dlg, [&]() {
      settings.remove("skipVersion");
      QDesktopServices::openUrl(QUrl("https://github.com/derwin12/MyMapMap/releases/latest"));
      dlg.accept();
    });
    connect(btnIgnore, &QPushButton::clicked, &dlg, [&]() {
      settings.setValue("skipVersion", latestTag);
      dlg.reject();
    });
    connect(btnLater, &QPushButton::clicked, &dlg, [&]() {
      settings.remove("skipVersion");
      dlg.reject();
    });

    dlg.exec();
  });
}

void MainWindow::about()
{
  // Stop video playback to avoid lags. XXX Hack
  pause(false);
  _aboutDialog = new AboutDialog(this);
  _aboutDialog->setAttribute(Qt::WA_DeleteOnClose); // Important for ressource management
  _aboutDialog->show();

  // Restart video playback. XXX Hack
  play(false);
}

void MainWindow::updateStatusBar()
{
  QPointF mousePos = destinationCanvas->mapToScene(destinationCanvas->mapFromGlobal(destinationCanvas->cursor().pos()));
  if (currentSelectedItem) // Show mouse coordinate only if layerList is not empty
    mousePosLabel->setText("Mouse coordinate:   X " + QString::number(mousePos.x()) + "   Y " + QString::number(mousePos.y()));
  else
    mousePosLabel->setText(""); // Otherwise set empty text.
  currentMessageLabel->setText(statusBar()->currentMessage());
  sourceZoomLabel->setText("Input Editor: " + QString::number(int(sourceCanvas->getZoomFactor() * 100)).append(QChar('%')));
  destinationZoomLabel->setText("Output Editor: " + QString::number(int(destinationCanvas->getZoomFactor() * 100)).append(QChar('%')));
  lastActionLabel->setText(undoStack->text(undoStack->count() - 1));
}

void MainWindow::showMenuBar(bool shown)
{
  _showMenuBar = shown;

#ifdef Q_OS_MACOS // On Mac OS X
  // Do nothing
#endif
#ifdef Q_OS_LINUX // On Linux
  QString currentDesktop = QString(getenv("XDG_CURRENT_DESKTOP")).toLower();
  if (currentDesktop != "unity")
    menuBar()->setVisible(shown);
#endif
#ifdef Q_OS_WIN32 // On Windows
    menuBar()->setVisible(shown);
#endif
}

/**
 * Called when the user wants to delete an item.
 *
 * Deletes either a Source or a Mapping.
 */
void MainWindow::deleteItem()
{
  bool isLayerTabSelected = (layerSplitter == contentTab->currentWidget());
  bool isSourceTabSelected = (sourceSplitter == contentTab->currentWidget());

  if (currentSelectedItem)
  {
    if (isLayerTabSelected) //currentSelectedItem->listWidget() == layerList)
    {
      // Delete mapping.
      undoStack->push(new DeleteLayerCommand(this, getCurrentLayerId()));
      //currentSelectedItem = NULL;
    }
    else if (isSourceTabSelected) //currentSelectedItem->listWidget() == sourceList)
    {
      // Delete source.
      undoStack->push(new RemoveSourceCommand(this, getItemId(*sourceList->currentItem())));
      //currentSelectedItem = NULL;
    }
    else
    {
      qCritical() << "Selected item neither a mapping nor a source." << Qt::endl;
    }
  }
}

void MainWindow::duplicateLayerItem()
{
  if (currentSelectedIndex.isValid())
  {
    duplicateLayer(currentLayerItemId());
  }
  else
  {
    qCritical() << "No selected mapping" << Qt::endl;
  }
}

void MainWindow::deleteLayerItem()
{
  if (hasCurrentLayer())
  {
    undoStack->push(new DeleteLayerCommand(this, getCurrentLayerId()));
  }
  else
  {
    qCritical() << "No selected mapping" << Qt::endl;
  }
}

void MainWindow::renameLayerItem()
{
  // Set current item editable and rename it
  QModelIndex index = layerList->currentIndex();
  // Used by context menu
  layerList->edit(index);
  // Switch to mapping tab.
  contentTab->setCurrentWidget(layerSplitter);
}

void MainWindow::setLayerItemLocked(bool locked)
{
  setLayerLocked(currentLayerItemId(), locked);
}

void MainWindow::setLayerItemHide(bool hide)
{
  setLayerVisible(currentLayerItemId(), !hide);
}

void MainWindow::setLayerItemSolo(bool solo)
{
  setLayerSolo(currentLayerItemId(), solo);
}

void MainWindow::loadLayerMedia()
{
  QAction *action = qobject_cast<QAction *>(sender());
  Source::ptr media;
  uid currentLayerId = getCurrentLayer()->getId();

  if (action) {
    if (action->data().toString() == "import-new-media") {
      // Due to the fact that we can't assign a media/source without adding a mesh
      importMedia();
      addMesh(); // Creating a temporary mesh
      media = mappingManager->getSourceById(currentSourceId); // The last imported video is current ID
      deleteLayer(getCurrentLayer()->getId()); // Delete the temporary mesh
      setCurrentLayer(currentLayerId); // Set the previous selected layer as the current
    } else {
      media = mappingManager->getSourceById(action->data().toInt());
    }

    if (media && media != getCurrentLayer()->getSource() &&
        getCurrentLayer()->sourceIsCompatible(media)) {
      // Change layer source
      getCurrentLayer()->setSource(media);
    }
  }
}

void MainWindow::transformActionLayerItem()
{
  QAction *actionSender = qobject_cast<QAction *>(sender());

  if (actionSender == layerRotate90CWAction) {
    undoStack->push(new RotateShapeCommand(destinationCanvas, TransformShapeCommand::FREE, destinationCanvas->getCurrentShape(), MShape::Rotate90CW));
  }
  else if (actionSender == layerRotate90CCWAction) {
    undoStack->push(new RotateShapeCommand(destinationCanvas, TransformShapeCommand::FREE, destinationCanvas->getCurrentShape(), MShape::Rotate90CCW));
  }
  else if (actionSender == layerRotate180Action) {
    undoStack->push(new RotateShapeCommand(destinationCanvas, TransformShapeCommand::FREE, destinationCanvas->getCurrentShape(), MShape::Rotate180));
  }

  else if (actionSender == layerHorizontalFlipAction) {
    undoStack->push(new FlipShapeCommand(destinationCanvas, TransformShapeCommand::FREE, destinationCanvas->getCurrentShape(), MShape::Horizontal));
  }
  else if (actionSender == layerVerticalFlipAction) {
    undoStack->push(new FlipShapeCommand(destinationCanvas, TransformShapeCommand::FREE, destinationCanvas->getCurrentShape(), MShape::Vertical));
  }

}

void MainWindow::reorderLayerItem()
{
  QAction *actionSender = qobject_cast<QAction *>(sender());

  if (actionSender == layerRaiseAction) {
    undoStack->push(new MoveLayerCommand(this, getCurrentLayerId(), MM::Raise));
  }
  else if (actionSender == layerLowerAction) {
    undoStack->push(new MoveLayerCommand(this, getCurrentLayerId(), MM::Lower));
  }
  else if (actionSender == layerRaiseToTopAction) {
    undoStack->push(new MoveLayerCommand(this, getCurrentLayerId(), MM::Top));
  }
  else if (actionSender == layerLowerToBottomAction) {
    undoStack->push(new MoveLayerCommand(this, getCurrentLayerId(), MM::Bottom));
  }
}

void MainWindow::renameLayer(uid layerId, const QString &name)
{
  Layer::ptr layer = mappingManager->getLayerById(layerId);
  Q_CHECK_PTR(layer);

  if (!layer.isNull()) {
    QModelIndex index = layerListModel->getIndexFromId(layerId);
    layerListModel->setData(index, name, Qt::EditRole);
    layer->setName(name);
  }
}

//void MainWindow::layerListEditEnd(QWidget *editor)
//{
//  QString name = reinterpret_cast<QLineEdit*>(editor)->text();
//  renameMapping(getItemId(*layerList->currentItem()), name);
//}

void MainWindow::deleteSourceItem()
{
  if(hasCurrentSource())
  {
    undoStack->push(new RemoveSourceCommand(this, getCurrentSourceId()));
  }
  else
  {
    qCritical() << "No selected source" << Qt::endl;
  }
}

void MainWindow::renameSourceItem()
{
  // Set current item editable and rename it
  QListWidgetItem* item = sourceList->currentItem();
  item->setFlags(item->flags() | Qt::ItemIsEditable);
  // Used by context menu
  sourceList->editItem(item);
  // Switch to source tab
  contentTab->setCurrentWidget(sourceSplitter);
}

void MainWindow::renameSource(uid sourceId, const QString &name)
{
  Source::ptr source = mappingManager->getSourceById(sourceId);
  Q_CHECK_PTR(source);
  if (!source.isNull()) {
    source->setName(name);
  }
}

void MainWindow::sourceListEditEnd(QWidget *editor)
{
  QString name = reinterpret_cast<QLineEdit*>(editor)->text();
  renameSource(getItemId(*sourceList->currentItem()), name);
}

void MainWindow::setupOutputScreen()
{
  QAction *actionSender = qobject_cast<QAction *>(sender());

  if (actionSender)
    outputWindow->setPreferredScreen(actionSender->data().toInt());
  // If want that the changes take effect immediatelly
  // when the output is in fullscreen mode
  if (outputFullScreenAction->isChecked()) {
    // XXX: Close and reopen // It's not the best way to do
    outputFullScreenAction->toggle();
    outputFullScreenAction->trigger();
  }
}

void MainWindow::updateScreenCount()
{
  // Clear action list before
  screenActions.clear();
  // Refresh screen action
  updateScreenActions();
  // Update Output menu
  outputScreenMenu->clear();
  outputScreenMenu->addActions(screenActions);
}

void MainWindow::openRecentFile()
{
  QAction *action = qobject_cast<QAction *>(sender());
  if (action)
    loadFile(action->data().toString());
}

void MainWindow::openRecentVideo()
{
  QAction *action = qobject_cast<QAction *>(sender());
  if (action)
    importMediaFile(action->data().toString(), false);
}

bool MainWindow::clearProject()
{
  // Disconnect signals to avoid problems when clearning layerList and sourceList.
  disconnectProjectWidgets();

  // Clear current source / mapping.
  removeCurrentSource();
  removeCurrentLayer();

  // Empty list widgets.
  layerListModel->clear();
  sourceList->clear();
  // clear() deletes all QListWidgetItems including the section headers.
  // Re-create them so addSourceItem() can find them again.
  _sourceSectionImages  = nullptr;
  _sourceSectionVideos  = nullptr;
  _sourceSectionFolders = nullptr;
  initSourceListSections();

  // Clear property panel.
  for (int i=layerPropertyPanel->count()-1; i>=0; i--)
    layerPropertyPanel->removeWidget(layerPropertyPanel->widget(i));

  // Disable property panel.
  layerPropertyPanel->setDisabled(true);

  // Clear list of layerGuis.
  layerGuis.clear();

  // Clear list of source guis.
  sourceGuis.clear();

  // Clear model.
  mappingManager->clearAll();

  // Clear background reference photo.
  clearBackgroundPhotoState();

  // Refresh GL canvases to clear them out.
  sourceCanvas->repaint();
  destinationCanvas->repaint();

  // Reconnect everything.
  connectProjectWidgets();

  // Window was modified.
  windowModified();

  return true;
}

uid MainWindow::createMediaSource(uid sourceId, QString uri, float x, float y,
                                 bool isImage, VideoType type, double rate)
{
  // Cannot create image with already existing id.
  if (Source::getUidAllocator().exists(sourceId))
    return NULL_UID;

  else
  {
    Texture* tex = nullptr;
    if (isImage)
      tex = new Image(uri, sourceId);
    else {
      tex = new Video(uri, type, rate, sourceId);
    }

    // Create new image with corresponding ID.
    tex->setPosition(x, y);

    // Add it to the manager.
    Source::ptr source(tex);

    if (type == VIDEO_WEBCAM) {
      source->setName(tex->getCameraNameFromUri(uri));
    } else {
      source->setName(strippedName(uri));
    }

    // Add source to model and return its uid.
    uid id = mappingManager->addSource(source);

    // Kick off async thumbnail generation for non-webcam videos.
    if (!isImage && type != VIDEO_WEBCAM && _thumbnailCache)
      _thumbnailCache->request(uri, thumbnailCacheDir());

    // Add source widget item.
    undoStack->push(new AddSourceCommand(this, id, source->getIcon(), source->getName()));
    return id;
  }
}

uid MainWindow::createColorSource(uid sourceId, QColor color)
{
  // Cannot create image with already existing id.
  if (Source::getUidAllocator().exists(sourceId))
    return NULL_UID;

  else
  {
    Color* img = new Color(color, sourceId);

    // Add it to the manager.
    Source::ptr source(img);
    source->setName(strippedName(color.name()));

    // Add source to model and return its uid.
    uid id = mappingManager->addSource(source);

    // Add source widget item.
    undoStack->push(new AddSourceCommand(this, id, source->getIcon(), source->getName()));

    return id;
  }
}

bool MainWindow::addTextSource(const QString& text)
{
  uid id = createTextSource(NULL_UID, text);
  return id != NULL_UID;
}

uid MainWindow::createTextSource(uid sourceId, const QString& text)
{
  if (Source::getUidAllocator().exists(sourceId))
    return NULL_UID;

  Text* ts = new Text(text, sourceId);
  Source::ptr source(ts);
  source->setName(text.left(20));

  uid id = mappingManager->addSource(source);
  undoStack->push(new AddSourceCommand(this, id, source->getIcon(), source->getName()));
  return id;
}

uid MainWindow::addFreePolygonLayer(int sourceId, const QVector<QPointF>& vertices)
{
  Source::ptr source = getMappingManager().getSourceById(sourceId);
  if (!source) return NULL_UID;

  Layer* layerPtr;
  if (source->getSourceType() == SourceType::Color)
  {
    MShape::ptr outPoly(Util::createFreePolygonForColor(vertices));
    layerPtr = new ColorLayer(source, outPoly);
  }
  else
  {
    QSharedPointer<Texture> texture = qSharedPointerCast<Texture>(source);
    if (!texture) return NULL_UID;
    MShape::ptr outPoly(Util::createFreePolygonForColor(vertices));
    MShape::ptr inPoly(Util::createFreePolygonInputForTexture(vertices, texture.data()));
    layerPtr = new TextureLayer(source, outPoly, inPoly);
  }

  Layer::ptr layer(layerPtr);
  uid layerId = mappingManager->addLayer(layer);
  undoStack->push(new AddLayerCommand(this, layerId));
  return layerId;
}

uid MainWindow::createFolderSource(uid sourceId, const QString& dirPath)
{
  if (Source::getUidAllocator().exists(sourceId))
    return NULL_UID;

  FolderSource* fs = new FolderSource(dirPath, sourceId);
  if (fs->imageCount() == 0) {
    delete fs;
    return NULL_UID;
  }

  Source::ptr source(fs);
  source->setName(QDir(dirPath).dirName());
  source->play();

  uid id = mappingManager->addSource(source);
  undoStack->push(new AddSourceCommand(this, id, source->getIcon(), source->getName()));
  return id;
}

uid MainWindow::createSyphonSource(uid sourceId, const QString& serverUUID,
                                   const QString& serverName, const QString& appName)
{
#ifdef Q_OS_MAC
  // Cannot create a source with an already existing id.
  if (Source::getUidAllocator().exists(sourceId))
    return NULL_UID;

  Syphon* syph = new Syphon(sourceId);
  syph->connectToServer(serverUUID, serverName, appName);

  Source::ptr source(syph);

  // Default name: the server's description, falling back to a numbered
  // "Syphon N" when created unbound or when the name is already taken.
  SyphonServerDescription desc;
  desc.uuid = serverUUID;
  desc.name = serverName;
  desc.appName = appName;
  const QString base = desc.isEmpty() ? QString("Syphon") : desc.displayName();
  source->setName(mappingManager->generateUniqueSourceName(base));

  // Add source to model and return its uid.
  uid id = mappingManager->addSource(source);

  // Add source widget item.
  undoStack->push(new AddSourceCommand(this, id, source->getIcon(), source->getName()));

  return id;
#else
  Q_UNUSED(sourceId); Q_UNUSED(serverUUID); Q_UNUSED(serverName); Q_UNUSED(appName);
  return NULL_UID;
#endif
}

void MainWindow::setLayerVisible(uid layerId, bool visible)
{
  // Set mapping visibility
  Layer::ptr layer = mappingManager->getLayerById(layerId);

  if (layer.isNull())
  {
    qDebug() << "No such mapping id" << Qt::endl;
  }
  else
  {
    layer->setVisible(visible);
    // Change list item check state
    QModelIndex index = layerListModel->getIndexFromId(layerId);
    layerListModel->setData(index, visible, Qt::CheckStateRole);
    // Update canvases.
    updateCanvases();
  }
}

void MainWindow::setLayerSolo(uid layerId, bool solo)
{
  Layer::ptr layer = mappingManager->getLayerById(layerId);
  if (!layer.isNull()) {
    // Turn this mapping into solo mode
    layer->setSolo(solo);
    // Change list item check state
    QModelIndex index = layerListModel->getIndexFromId(layerId);
    layerListModel->setData(index, solo, Qt::CheckStateRole + 1);
    // Update canvases
    updateCanvases();
  }
}

void MainWindow::setLayerLocked(uid layerId, bool locked)
{
  Layer::ptr layer = mappingManager->getLayerById(layerId);

  if (!layer.isNull()) {
    // Lock position of mapping
    layer->setLocked(locked);
    // Lock shape too.
    layer->getShape()->setLocked(locked);
    // Change list item check state
    QModelIndex index = layerListModel->getIndexFromId(layerId);
    layerListModel->setData(index, locked, Qt::CheckStateRole + 2);
    // Update canvases
    updateCanvases();
  }
}

void MainWindow::deleteLayer(uid layerId)
{
  // Cannot delete unexisting mapping.
  if (Layer::getUidAllocator().exists(layerId))
  {
    removeLayerItem(layerId);
  }
}

void MainWindow::moveLayer(uid layerId, int idx)
{
  // Cannot delete unexisting mapping.
  if (Layer::getUidAllocator().exists(layerId))
  {
    moveLayerItem(layerId, idx);
  }
}

void MainWindow::duplicateLayer(uid layerId)
{
  // Clone current Mapping.
  Layer::ptr clonedMappingPtr(mappingManager->getLayerById(layerId)->clone());

  // Get duplicated mapping id
  uid cloneId = mappingManager->addLayer(clonedMappingPtr);

  // Lets the undo-stack handle Undo/Redo the duplication of mapping item.
  undoStack->push(new DuplicateLayerCommand(this, cloneId));
}

/// Deletes/removes a source and all associated mappigns.
void MainWindow::deleteSource(uid sourceId, bool replace)
{
  // Cannot delete unexisting source.
  if (Source::getUidAllocator().exists(sourceId))
  {
    if (replace == false) {
      int r = QMessageBox::warning(this, tr("MyMapMap"),
                                   tr("Remove this source and all its associated layers?"),
                                   QMessageBox::Ok | QMessageBox::Cancel);
      if (r == QMessageBox::Ok)
      {
        removeSourceItem(sourceId);
      }
    }
    else
      removeSourceItem(sourceId);
  }
}

void MainWindow::windowModified()
{
  setWindowModified(true);
  updateStatusBar();
  updateLayerActions();
}

void MainWindow::createLayout()
{
  // Create source list.
  sourceList = new QListWidget;
  sourceList->setSelectionMode(QAbstractItemView::SingleSelection);
  sourceList->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
  sourceList->setDragDropMode(QAbstractItemView::NoDragDrop); // sections make reorder ambiguous
  sourceList->setMinimumWidth(PAINT_LIST_MINIMUM_HEIGHT);
  sourceList->setIconSize(QSize(MainWindow::PAINT_LIST_ICON_SIZE, MainWindow::PAINT_LIST_ICON_SIZE));

  initSourceListSections();

  // Create source panel.
  sourcePropertyPanel = new QStackedWidget;
  sourcePropertyPanel->setDisabled(true);
  sourcePropertyPanel->setMinimumHeight(PAINT_PROPERTY_PANEL_MINIMUM_HEIGHT);

  // Source preview widget — header pinned above the splitter, image inside the splitter.
  {
    _previewToggleBtn = new QCheckBox(tr("Preview"));
    _previewToggleBtn->setChecked(true);

    // Header row: always visible, lives OUTSIDE the splitter.
    _sourcePreviewContainer = new QWidget;
    auto* headerLayout = new QHBoxLayout(_sourcePreviewContainer);
    headerLayout->setContentsMargins(4, 2, 4, 2);
    headerLayout->setSpacing(2);

    // List / Thumbnail view-mode toggle buttons.
    {
      // List-mode button: 4 horizontal lines.
      auto* listBtn = new QToolButton;
      listBtn->setCheckable(true);
      listBtn->setAutoExclusive(true);
      listBtn->setAutoRaise(true);
      listBtn->setFixedSize(26, 26);
      listBtn->setToolTip(tr("List view"));
      {
        QPixmap pm(20, 20);
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        p.setPen(QPen(palette().color(QPalette::ButtonText), 1.5));
        for (int row = 0; row < 4; ++row) {
          int y = 3 + row * 4;
          p.drawLine(2, y, 18, y);
        }
      listBtn->setIcon(QIcon(pm));
      }
      _viewModeBtns[0] = listBtn;
      headerLayout->addWidget(listBtn);

      // Thumbnail-mode button: 2×2 grid of squares.
      auto* thumbBtn = new QToolButton;
      thumbBtn->setCheckable(true);
      thumbBtn->setAutoExclusive(true);
      thumbBtn->setAutoRaise(true);
      thumbBtn->setFixedSize(26, 26);
      thumbBtn->setToolTip(tr("Thumbnail view"));
      {
        QPixmap pm(20, 20);
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        p.fillRect(2,  2,  7, 7, palette().color(QPalette::ButtonText));
        p.fillRect(11, 2,  7, 7, palette().color(QPalette::ButtonText));
        p.fillRect(2,  11, 7, 7, palette().color(QPalette::ButtonText));
        p.fillRect(11, 11, 7, 7, palette().color(QPalette::ButtonText));
        thumbBtn->setIcon(QIcon(pm));
      }
      _viewModeBtns[1] = thumbBtn;
      headerLayout->addWidget(thumbBtn);

      connect(listBtn,  &QToolButton::clicked, this, [this]() { setSourceListThumbnailMode(false); });
      connect(thumbBtn, &QToolButton::clicked, this, [this]() { setSourceListThumbnailMode(true);  });
    }

    headerLayout->addWidget(_previewToggleBtn);
    headerLayout->addStretch();

    // Thumbnail size picker: 3 buttons with small squares indicating small/medium/large icon size.
    static const int kThumbSizes[3] = { 48, 64, 96 };
    static const int kSquareSizes[3] = { 9, 13, 18 };
    auto applyThumbSize = [this](int size) {
      _sourceListIconSize = size;
      if (_sourceListThumbnailMode) {
        sourceList->setIconSize(QSize(size, size));
        int rowH = size + 8;
        for (int j = 0; j < sourceList->count(); ++j) {
          QListWidgetItem* it = sourceList->item(j);
          if (it) it->setSizeHint(QSize(it->sizeHint().width(), rowH));
        }
      }
      for (int i = 0; i < 3; ++i) {
        static const int kSizes[3] = { 48, 64, 96 };
        _thumbSizeBtns[i]->setChecked(kSizes[i] == size);
      }
    };
    for (int i = 0; i < 3; ++i) {
      auto* btn = new QToolButton;
      btn->setCheckable(true);
      btn->setAutoExclusive(true);
      btn->setAutoRaise(true);
      btn->setFixedSize(26, 26);
      // Draw a filled square that grows with i.
      int sq = kSquareSizes[i];
      QPixmap pm(20, 20);
      pm.fill(Qt::transparent);
      QPainter p(&pm);
      p.fillRect((20 - sq) / 2, (20 - sq) / 2, sq, sq, palette().color(QPalette::ButtonText));
      btn->setIcon(QIcon(pm));
      int size = kThumbSizes[i];
      connect(btn, &QToolButton::clicked, this, [this, size, applyThumbSize]() {
        applyThumbSize(size);
        QSettings().setValue("sourceListIconSize", size);
      });
      headerLayout->addWidget(btn);
      _thumbSizeBtns[i] = btn;
    }
    // Select the button matching the restored icon size; fall back to medium.
    bool anyChecked = false;
    for (int i = 0; i < 3; ++i) {
      bool match = (kThumbSizes[i] == _sourceListIconSize);
      _thumbSizeBtns[i]->setChecked(match);
      if (match) anyChecked = true;
    }
    if (!anyChecked) {
      _sourceListIconSize = kThumbSizes[1]; // stale saved value — reset to medium
      QSettings().setValue("sourceListIconSize", _sourceListIconSize);
      _thumbSizeBtns[1]->setChecked(true);
    }
    _viewModeBtns[1]->setChecked(true);  // default: thumbnail mode

    // Image area: lives INSIDE the splitter so it can be resized.
    _sourcePreviewLabel = new SourcePreviewLabel;
    _sourcePreviewLabel->setMinimumHeight(20);

    // Toggling the checkbox shows/hides the image area and adjusts splitter space.
    // Toggling shows/hides the thumbnail at the bottom (index 2 in splitter).
    connect(_previewToggleBtn, &QCheckBox::toggled, this, [this](bool on) {
      QList<int> sizes = sourceSplitter->sizes();
      if (sizes.size() < 3) return;
      if (on) {
        // Restore: carve out space for thumbnail from the property panel.
        int want = qMax((sizes[1] + sizes[2]) / 2, 80);
        sourceSplitter->setSizes({sizes[0], sizes[1] + sizes[2] - want, want});
        _sourcePreviewLabel->setVisible(true);
      } else {
        // Collapse: give the thumbnail's space to the property panel.
        sourceSplitter->setSizes({sizes[0], sizes[1] + sizes[2], 0});
        _sourcePreviewLabel->setVisible(false);
      }
    });

  }

  // Create mapping list.
  layerList = new QTableView;
  layerList->setSelectionMode(QAbstractItemView::SingleSelection);
  layerList->setSelectionBehavior(QAbstractItemView::SelectRows);
  layerList->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
  layerList->setDragEnabled(true);
  layerList->setAcceptDrops(true);
  layerList->setDropIndicatorShown(true);
  layerList->setEditTriggers(QAbstractItemView::DoubleClicked);
  layerList->setMinimumHeight(MAPPING_LIST_MINIMUM_HEIGHT);
  layerList->setContentsMargins(0, 0, 0, 0);
  // Set view delegate
  layerListModel = new LayerListModel;
  layerItemDelegate = new LayerItemDelegate;
  layerList->setModel(layerListModel);
  layerList->setItemDelegate(layerItemDelegate);
  // Pimp Mapping table widget
  layerList->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
  layerList->setShowGrid(false);
  layerList->horizontalHeader()->hide();
  layerList->verticalHeader()->hide();
  layerList->setMouseTracking(true);// Important
  layerList->setColumnWidth(0, MM::MAPPING_LIST_HIDE_COLUMN);
  layerList->setColumnWidth(1, MM::MAPPING_LIST_NAME_COLUMN);
  layerList->setColumnWidth(2, MM::MAPPING_LIST_BUTTONS_COLUMN);

  // Create property panel.
  layerPropertyPanel = new QStackedWidget;
  layerPropertyPanel->setDisabled(true);
  layerPropertyPanel->setMinimumHeight(MAPPING_PROPERTY_PANEL_MINIMUM_HEIGHT);

  // Create canvases.
  sourceCanvas = new MapperGLCanvas(this, false);
  sourceCanvas->setFocusPolicy(Qt::ClickFocus);
  sourceCanvas->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  sourceCanvas->setMinimumSize(CANVAS_MINIMUM_WIDTH, CANVAS_MINIMUM_HEIGHT);

  sourceCanvasToolbar = new MapperGLCanvasToolbar(sourceCanvas);
  sourceCanvasToolbar->setToolbarTitle(tr("Input Editor"));
  QVBoxLayout* sourceLayout = new QVBoxLayout;
  sourceLayout->setContentsMargins(0, 0, 0, 0);
  sourceLayout->setSpacing(0);
  sourcePanel = new QWidget(this);

  sourceLayout->addWidget(sourceCanvas);
  sourceLayout->addWidget(sourceCanvasToolbar, 0, Qt::AlignRight);
  sourcePanel->setLayout(sourceLayout);

  destinationCanvas = new MapperGLCanvas(this, true, nullptr, qobject_cast<QOpenGLWidget*>(sourceCanvas->viewport()));
  destinationCanvas->setFocusPolicy(Qt::ClickFocus);
  destinationCanvas->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  destinationCanvas->setMinimumSize(CANVAS_MINIMUM_WIDTH, CANVAS_MINIMUM_HEIGHT);

  destinationCanvasToolbar = new MapperGLCanvasToolbar(destinationCanvas);
  destinationCanvasToolbar->setToolbarTitle(tr("Output Editor"));
  destinationCanvasToolbar->setupBackgroundPhotoControls();
  connect(destinationCanvasToolbar, &MapperGLCanvasToolbar::backgroundPhotoToggled,
          this, &MainWindow::onBackgroundPhotoToggled);
  connect(destinationCanvasToolbar, &MapperGLCanvasToolbar::backgroundOpacityChanged,
          this, &MainWindow::onBackgroundPhotoOpacityChanged);
  QVBoxLayout* destinationLayout = new QVBoxLayout;
  destinationLayout->setContentsMargins(0, 0, 0, 0);
  destinationLayout->setSpacing(0);
  destinationPanel = new QWidget(this);

  destinationLayout->addWidget(destinationCanvas);
  destinationLayout->addWidget(destinationCanvasToolbar, 0, Qt::AlignRight);
  destinationPanel->setLayout(destinationLayout);

  // Preferences dialog
  _preferenceDialog = new PreferenceDialog(this);

  // Video exporter — created lazily after the window is shown so that
  // Qt Multimedia / WMF initialization doesn't block the main thread during startup.
  _videoExporter = nullptr;

  outputWindow = new OutputGLWindow(this, destinationCanvas);
  outputWindow->installEventFilter(destinationCanvas);


  // Source scene changed -> change destination.
  connect(sourceCanvas->scene(), SIGNAL(changed(const QList<QRectF>&)),
          destinationCanvas,     SLOT(update()));

  // Destination scene changed -> change output window.
  connect(destinationCanvas->scene(), SIGNAL(changed(const QList<QRectF>&)),
          outputWindow->getCanvas(),  SLOT(update()));

  // Output changed -> change destinatioin
  // XXX si je decommente cette ligne alors quand je clique sur ajouter media ca gele...
  //  connect(outputWindow->getCanvas()->scene(), SIGNAL(changed(const QList<QRectF>&)),
  //          destinationCanvas,                  SLOT(updateCanvas()));

  // Create console logging output
  consoleWindow = ConsoleWindow::console();
  consoleWindow->setVisible(false);
  // Create shortcut window
  _shortcutWindow = new ShortcutWindow;
  _shortcutWindow->setVisible(false);

  // The preview image area goes inside the splitter; the header row is pinned above.
  sourceSplitter = new QSplitter(Qt::Vertical);
  sourceSplitter->setChildrenCollapsible(true);
  sourceSplitter->addWidget(sourceList);
  sourceSplitter->addWidget(sourcePropertyPanel);
  sourceSplitter->addWidget(_sourcePreviewLabel);   // thumbnail at bottom

  // Source column: pinned header above the splitter.
  auto* sourceColumn = new QWidget;
  auto* sourceColumnLayout = new QVBoxLayout(sourceColumn);
  sourceColumnLayout->setContentsMargins(0, 0, 0, 0);
  sourceColumnLayout->setSpacing(0);
  sourceColumnLayout->addWidget(_sourcePreviewContainer); // header, always visible
  sourceColumnLayout->addWidget(sourceSplitter, 1);       // splitter fills rest

  layerSplitter = new QSplitter(Qt::Vertical);
  layerSplitter->setChildrenCollapsible(false);
  layerSplitter->addWidget(layerList);
  layerSplitter->addWidget(layerPropertyPanel);

  // Content tab.
  contentTab = new QTabWidget;
  contentTab->addTab(sourceColumn, themedIcon(":/add-video"), tr("Library"));
  contentTab->addTab(layerSplitter, themedIcon(":/add-mesh"), tr("Layers"));

  canvasSplitter = new QSplitter(Qt::Vertical);
  canvasSplitter->addWidget(sourcePanel);
  canvasSplitter->addWidget(destinationPanel);

  mainSplitter = new QSplitter(Qt::Horizontal);
  mainSplitter->addWidget(canvasSplitter);
  mainSplitter->addWidget(contentTab);
  connect(mainSplitter, SIGNAL(splitterMoved(int, int)), this, SLOT(updateLayerListColumnWidth()));

  // Initialize size to 9:1 proportions.
  QSize sz = mainSplitter->size();
  QList<int> sizes;
  sizes.append(sz.width() * 0.9);
  sizes.append(sz.width() - sizes.at(0));
  mainSplitter->setSizes(sizes);

  // Upon resizing window, give some extra stretch expansion to canvasSplitter.
  mainSplitter->setStretchFactor(0, 1);

  // Final setups.
  setWindowTitle(MM::APPLICATION_NAME);
  resize(DEFAULT_WIDTH, DEFAULT_HEIGHT);
  setCentralWidget(mainSplitter);

  // Connect mapping and source lists signals and slots.
  connectProjectWidgets();

  // Reset focus on main window.
  setFocus();
}

void MainWindow::createActions()
{
  // New.
  newAction = new QAction(tr("&New"), this);
  newAction->setIcon(QIcon(":/new"));
  newAction->setShortcut(QKeySequence::New);
  newAction->setToolTip(tr("Create a new project"));
  newAction->setIconVisibleInMenu(false);
  newAction->setShortcutContext(Qt::ApplicationShortcut);
  addAction(newAction);
  connect(newAction, SIGNAL(triggered()), this, SLOT(newFile()));

  // Open.
  openAction = new QAction(tr("&Open..."), this);
  openAction->setIcon(QIcon(":/open"));
  openAction->setShortcut(QKeySequence::Open);
  openAction->setToolTip(tr("Open an existing project"));
  openAction->setIconVisibleInMenu(false);
  openAction->setShortcutContext(Qt::ApplicationShortcut);
  addAction(openAction);
  connect(openAction, SIGNAL(triggered()), this, SLOT(open()));

  // Save.
  saveAction = new QAction(tr("&Save"), this);
  saveAction->setIcon(QIcon(":/save"));
  saveAction->setShortcut(QKeySequence::Save);
  saveAction->setToolTip(tr("Save the project"));
  saveAction->setIconVisibleInMenu(false);
  saveAction->setShortcutContext(Qt::ApplicationShortcut);
  addAction(saveAction);
  connect(saveAction, SIGNAL(triggered()), this, SLOT(save()));

  // Save as.
  saveAsAction = new QAction(tr("Save &As..."), this);
  saveAsAction->setIcon(QIcon(":/save-as"));
  saveAsAction->setShortcut(QKeySequence::SaveAs);
  saveAsAction->setToolTip(tr("Save the project as..."));
  saveAsAction->setIconVisibleInMenu(false);
  saveAsAction->setShortcutContext(Qt::ApplicationShortcut);
  addAction(saveAsAction);
  connect(saveAsAction, SIGNAL(triggered()), this, SLOT(saveAs()));

  // Recents file
  for (int i = 0; i < MaxRecentFiles; i++)
  {
    recentFileActions[i] = new QAction(this);
    recentFileActions[i]->setVisible(false);
    connect(recentFileActions[i], SIGNAL(triggered()),
            this, SLOT(openRecentFile()));
  }

  // Recent video
  for (int i = 0; i < MaxRecentVideo; i++)
  {
    recentVideoActions[i] = new QAction(this);
    recentVideoActions[i]->setVisible(false);
    connect(recentVideoActions[i], SIGNAL(triggered()), this, SLOT(openRecentVideo()));
  }

  // Clear recent video list action
  clearRecentFileActions = new QAction(this);
  clearRecentFileActions->setVisible(true);
  connect(clearRecentFileActions, SIGNAL(triggered()), this, SLOT(clearRecentFileList()));

  // Empty list of recent video action
  emptyRecentVideos = new QAction(tr("No Recents Videos"), this);
  emptyRecentVideos->setEnabled(false);


  // Import Media.
  importMediaAction = new QAction(tr("&Import Media File..."), this);
  importMediaAction->setShortcut(Qt::CTRL | Qt::Key_I);
  importMediaAction->setIcon(QIcon(":/add-video"));
  importMediaAction->setToolTip(tr("Import a video or image file..."));
  importMediaAction->setIconVisibleInMenu(false);
  importMediaAction->setShortcutContext(Qt::ApplicationShortcut);
  addAction(importMediaAction);
  connect(importMediaAction, SIGNAL(triggered()), this, SLOT(importMedia()));

  // Import Folder.
  importFolderAction = new QAction(tr("Import Files From &Folder..."), this);
  importFolderAction->setShortcut(Qt::CTRL | Qt::SHIFT | Qt::Key_I);
  importFolderAction->setIcon(QIcon(":/add-video"));
  importFolderAction->setToolTip(tr("Import all images and videos from a folder as individual sources..."));
  importFolderAction->setIconVisibleInMenu(false);
  importFolderAction->setShortcutContext(Qt::ApplicationShortcut);
  addAction(importFolderAction);
  connect(importFolderAction, &QAction::triggered, this, &MainWindow::importFolder);

  // Open camera.
  AddCameraAction = new QAction(tr("Open &Camera Device..."), this);
  AddCameraAction->setShortcut(Qt::CTRL | Qt::SHIFT | Qt::Key_C);
  AddCameraAction->setIcon(QIcon(":/add-camera"));
  AddCameraAction->setIconVisibleInMenu(false);
  AddCameraAction->setToolTip(tr("Choose your camera device..."));
  AddCameraAction->setShortcutContext(Qt::ApplicationShortcut);
  addAction(AddCameraAction);
  connect(AddCameraAction, SIGNAL(triggered()), this, SLOT(openCameraDevice()));

  // Add color.
  addColorAction = new QAction(tr("Add &Color Source..."), this);
  addColorAction->setShortcut(Qt::CTRL | Qt::SHIFT | Qt::Key_A);
  addColorAction->setIcon(QIcon(":/add-color"));
  addColorAction->setToolTip(tr("Add a color source..."));
  addColorAction->setIconVisibleInMenu(false);
  addColorAction->setShortcutContext(Qt::ApplicationShortcut);
  addAction(addColorAction);
  connect(addColorAction, SIGNAL(triggered()), this, SLOT(addColor()));

  addTextAction = new QAction(tr("Add &Text Source..."), this);
  addTextAction->setShortcut(Qt::CTRL | Qt::SHIFT | Qt::Key_T);
  addTextAction->setIcon(themedIcon(":/add-text"));
  addTextAction->setToolTip(tr("Add a text source..."));
  addTextAction->setIconVisibleInMenu(false);
  addTextAction->setShortcutContext(Qt::ApplicationShortcut);
  addAction(addTextAction);
  connect(addTextAction, &QAction::triggered, this, &MainWindow::addText);

#ifdef Q_OS_MAC
  // Add Syphon source (macOS only).
  addSyphonAction = new QAction(tr("Add &Syphon Source..."), this);
  addSyphonAction->setShortcut(Qt::CTRL | Qt::SHIFT | Qt::Key_Y);
  addSyphonAction->setIcon(QIcon(":/add-syphon"));
  addSyphonAction->setToolTip(tr("Receive live video from another application via Syphon..."));
  addSyphonAction->setIconVisibleInMenu(false);
  addSyphonAction->setShortcutContext(Qt::ApplicationShortcut);
  addAction(addSyphonAction);
  connect(addSyphonAction, SIGNAL(triggered()), this, SLOT(addSyphon()));
#endif

  // Exit/quit.
  exitAction = new QAction(tr("E&xit"), this);
  exitAction->setShortcut(QKeySequence::Quit);
  exitAction->setToolTip(tr("Exit the application"));
  exitAction->setIconVisibleInMenu(false);
  exitAction->setShortcutContext(Qt::ApplicationShortcut);
  addAction(exitAction);
  connect(exitAction, SIGNAL(triggered()), this, SLOT(close()));

  // Undo action
  undoAction = undoStack->createUndoAction(this, tr("&Undo"));
  undoAction->setShortcut(QKeySequence::Undo);
  undoAction->setIconVisibleInMenu(false);
  undoAction->setShortcutContext(Qt::ApplicationShortcut);
  addAction(undoAction);

  //Redo action
  redoAction = undoStack->createRedoAction(this, tr("&Redo"));
  redoAction->setShortcut(QKeySequence::Redo);
  redoAction->setIconVisibleInMenu(false);
  redoAction->setShortcutContext(Qt::ApplicationShortcut);
  addAction(redoAction);

  // Check for updates.
  checkForUpdatesAction = new QAction(tr("Check for &Updates..."), this);
  checkForUpdatesAction->setToolTip(tr("Check if a newer version of MyMapMap is available"));
  checkForUpdatesAction->setIconVisibleInMenu(false);
  addAction(checkForUpdatesAction);
  connect(checkForUpdatesAction, &QAction::triggered, this, &MainWindow::checkForUpdates);

  // About.
  aboutAction = new QAction(tr("&About %1").arg(MM::APPLICATION_NAME), this);
  aboutAction->setToolTip(tr("Show the application's About box"));
  aboutAction->setIconVisibleInMenu(false);
  aboutAction->setShortcutContext(Qt::ApplicationShortcut);
  addAction(aboutAction);
  connect(aboutAction, SIGNAL(triggered()), this, SLOT(about()));

  // Duplicate.
  duplicateLayerAction = new QAction(tr("Duplicate Layer"), this);
  duplicateLayerAction->setShortcut(Qt::CTRL | Qt::Key_D);
  duplicateLayerAction->setToolTip(tr("Duplicate layer item"));
  duplicateLayerAction->setIconVisibleInMenu(false);
  duplicateLayerAction->setEnabled(false);
  duplicateLayerAction->setShortcutContext(Qt::ApplicationShortcut);
  addAction(duplicateLayerAction);
  connect(duplicateLayerAction, SIGNAL(triggered()), this, SLOT(duplicateLayerItem()));

  // Delete mapping.
  deleteLayerAction = new QAction(tr("Delete Layer"), this);
  deleteLayerAction->setShortcut(QKeySequence::Delete);
  deleteLayerAction->setToolTip(tr("Delete layer item"));
  deleteLayerAction->setIconVisibleInMenu(false);
  deleteLayerAction->setEnabled(false);
  deleteLayerAction->setShortcutContext(Qt::ApplicationShortcut);
  addAction(deleteLayerAction);
  connect(deleteLayerAction, SIGNAL(triggered()), this, SLOT(deleteLayerItem()));

  // Rename mapping.
  renameLayerAction = new QAction(tr("Rename Layer"), this);
  renameLayerAction->setShortcut(Qt::Key_F2);
  renameLayerAction->setToolTip(tr("Rename layer item"));
  renameLayerAction->setIconVisibleInMenu(false);
  renameLayerAction->setEnabled(false);
  renameLayerAction->setShortcutContext(Qt::ApplicationShortcut);
  addAction(renameLayerAction);
  connect(renameLayerAction, SIGNAL(triggered()), this, SLOT(renameLayerItem()));

  // Lock mapping.
  layerLockedAction = new QAction(tr("Lock Layer"), this);
  layerLockedAction->setToolTip(tr("Lock layer item"));
  layerLockedAction->setIconVisibleInMenu(false);
  layerLockedAction->setCheckable(true);
  layerLockedAction->setChecked(false);
  layerLockedAction->setEnabled(false);
  layerLockedAction->setShortcutContext(Qt::ApplicationShortcut);
  addAction(layerLockedAction);
  connect(layerLockedAction, SIGNAL(triggered(bool)), this, SLOT(setLayerItemLocked(bool)));

  // Hide mapping.
  layerHideAction = new QAction(tr("Hide Layer"), this);
  layerHideAction->setToolTip(tr("Hide layer item"));
  layerHideAction->setIconVisibleInMenu(false);
  layerHideAction->setCheckable(true);
  layerHideAction->setChecked(false);
  layerHideAction->setEnabled(false);
  layerHideAction->setShortcutContext(Qt::ApplicationShortcut);
  addAction(layerHideAction);
  connect(layerHideAction, SIGNAL(triggered(bool)), this, SLOT(setLayerItemHide(bool)));

  // Solo mapping.
  layerSoloAction = new QAction(tr("Solo Layer"), this);
  layerSoloAction->setToolTip(tr("Solo layer item"));
  layerSoloAction->setIconVisibleInMenu(false);
  layerSoloAction->setCheckable(true);
  layerSoloAction->setChecked(false);
  layerSoloAction->setEnabled(false);
  layerSoloAction->setShortcutContext(Qt::ApplicationShortcut);
  addAction(layerSoloAction);
  connect(layerSoloAction, SIGNAL(triggered(bool)), this, SLOT(setLayerItemSolo(bool)));

  // Rotate 90 degrees CW action.
  layerRotate90CWAction = new QAction(tr("Rotate 90° CW"), this);
  layerRotate90CWAction->setToolTip(tr("Rotate 90° CW"));
  layerRotate90CWAction->setIconVisibleInMenu(true);
  layerRotate90CWAction->setEnabled(false);
  addAction(layerRotate90CWAction);
  connect(layerRotate90CWAction, SIGNAL(triggered()), SLOT(transformActionLayerItem()));

  // Rotate 90 degrees CW action.
  layerRotate90CCWAction = new QAction(tr("Rotate 90° CW"), this);
  layerRotate90CCWAction->setToolTip(tr("Rotate 90° CW"));
  layerRotate90CCWAction->setIconVisibleInMenu(true);
  layerRotate90CCWAction->setEnabled(false);
  addAction(layerRotate90CCWAction);
  connect(layerRotate90CCWAction, SIGNAL(triggered()), SLOT(transformActionLayerItem()));

  // Rotate 180 degrees action.
  layerRotate180Action = new QAction(tr("Rotate 180°"), this);
  layerRotate180Action->setToolTip(tr("Rotate 180°"));
  layerRotate180Action->setIconVisibleInMenu(true);
  layerRotate180Action->setEnabled(false);
  addAction(layerRotate180Action);
  connect(layerRotate180Action, SIGNAL(triggered()), SLOT(transformActionLayerItem()));

  // Horizontal Flip Action
  layerHorizontalFlipAction = new QAction(tr("Flip Horizontally"), this);
  layerHorizontalFlipAction->setShortcut(Qt::Key_H);
  layerHorizontalFlipAction->setToolTip(tr("Flip Horizontally"));
  layerHorizontalFlipAction->setIconVisibleInMenu(true);
  layerHorizontalFlipAction->setEnabled(false);
  addAction(layerHorizontalFlipAction);
  connect(layerHorizontalFlipAction, SIGNAL(triggered()), SLOT(transformActionLayerItem()));

  // Vertical Flip Action
  layerVerticalFlipAction = new QAction(tr("Flip Vertically"), this);
  layerVerticalFlipAction->setShortcut(Qt::Key_V);
  layerVerticalFlipAction->setToolTip(tr("Flip Vertically"));
  layerVerticalFlipAction->setIconVisibleInMenu(true);
  layerVerticalFlipAction->setEnabled(false);
  addAction(layerVerticalFlipAction);
  connect(layerVerticalFlipAction, SIGNAL(triggered()), SLOT(transformActionLayerItem()));

  layerRaiseAction = new QAction(tr("Raise"), this);
  layerRaiseAction->setShortcut(Qt::Key_PageUp);
  layerRaiseAction->setToolTip(tr("Raise"));
  layerRaiseAction->setIconVisibleInMenu(true);
  layerRaiseAction->setEnabled(false);
  addAction(layerRaiseAction);
  connect(layerRaiseAction, SIGNAL(triggered()), SLOT(reorderLayerItem()));

  layerLowerAction = new QAction(tr("Lower"), this);
  layerLowerAction->setShortcut(Qt::Key_PageDown);
  layerLowerAction->setToolTip(tr("Lower"));
  layerLowerAction->setIconVisibleInMenu(true);
  layerLowerAction->setEnabled(false);
  addAction(layerLowerAction);
  connect(layerLowerAction, SIGNAL(triggered()), SLOT(reorderLayerItem()));

  layerRaiseToTopAction = new QAction(tr("Raise to Top"), this);
  layerRaiseToTopAction->setShortcut(Qt::Key_Home); // bottom = end
  layerRaiseToTopAction->setToolTip(tr("Raise to top"));
  layerRaiseToTopAction->setIconVisibleInMenu(true);
  layerRaiseToTopAction->setEnabled(false);
  addAction(layerRaiseToTopAction);
  connect(layerRaiseToTopAction, SIGNAL(triggered()), SLOT(reorderLayerItem()));

  layerLowerToBottomAction = new QAction(tr("Lower to Bottom"), this);
  layerLowerToBottomAction->setShortcut(Qt::Key_End);
  layerLowerToBottomAction->setToolTip(tr("Lower to bottom"));
  layerLowerToBottomAction->setIconVisibleInMenu(true);
  layerLowerToBottomAction->setEnabled(false);
  addAction(layerLowerToBottomAction);
  connect(layerLowerToBottomAction, SIGNAL(triggered()), SLOT(reorderLayerItem()));

  // Delete source.
  deleteSourceAction = new QAction(tr("Delete Source"), this);
  //deleteSourceAction->setShortcut(tr("CTRL+DEL"));
  deleteSourceAction->setToolTip(tr("Delete source item"));
  deleteSourceAction->setIconVisibleInMenu(false);
  deleteSourceAction->setEnabled(false);
  deleteSourceAction->setShortcutContext(Qt::ApplicationShortcut);
  addAction(deleteSourceAction);
  connect(deleteSourceAction, SIGNAL(triggered()), this, SLOT(deleteSourceItem()));

  // Rename source.
  renameSourceAction = new QAction(tr("Rename Source"), this);
  //renameSourceAction->setShortcut(Qt::Key_F2);
  renameSourceAction->setToolTip(tr("Rename source item"));
  renameSourceAction->setIconVisibleInMenu(false);
  renameSourceAction->setEnabled(false);
  renameSourceAction->setShortcutContext(Qt::ApplicationShortcut);
  addAction(renameSourceAction);
  connect(renameSourceAction, SIGNAL(triggered()), this, SLOT(renameSourceItem()));

  // Import a new media for current layer
  _importLayerMediaAction = new QAction(tr("Import New Media"), this);
  _importLayerMediaAction->setToolTip(tr("Import new media file if not exists on the list"));
  _importLayerMediaAction->setIconVisibleInMenu(false);
  _importLayerMediaAction->setData("import-new-media"); // Important
  connect(_importLayerMediaAction, SIGNAL(triggered()), this, SLOT(loadLayerMedia()));

  // Preferences...
  preferencesAction = new QAction(tr("&Preferences..."), this);
  //preferencesAction->setIcon(QIcon(":/preferences"));
  preferencesAction->setShortcut(Qt::CTRL | Qt::Key_Comma);
  preferencesAction->setToolTip(tr("Configure preferences..."));
  //preferencesAction->setIconVisibleInMenu(false);
  preferencesAction->setShortcutContext(Qt::ApplicationShortcut);
  addAction(preferencesAction);
  connect(preferencesAction, SIGNAL(triggered()), _preferenceDialog, SLOT(exec()));

  // Add mesh.
  addMeshAction = new QAction(tr("Add &Mesh Layer"), this);
  addMeshAction->setShortcut(Qt::CTRL | Qt::Key_M);
  addMeshAction->setIcon(QIcon(":/add-mesh"));
  addMeshAction->setToolTip(tr("Add mesh layer"));
  addMeshAction->setIconVisibleInMenu(false);
  addMeshAction->setShortcutContext(Qt::ApplicationShortcut);
  addAction(addMeshAction);
  connect(addMeshAction, SIGNAL(triggered()), this, SLOT(addMesh()));
  addMeshAction->setEnabled(false);

  // Add triangle.
  addTriangleAction = new QAction(tr("Add &Triangle Layer"), this);
  addTriangleAction->setShortcut(Qt::CTRL | Qt::Key_T);
  addTriangleAction->setIcon(QIcon(":/add-triangle"));
  addTriangleAction->setToolTip(tr("Add triangle layer"));
  addTriangleAction->setIconVisibleInMenu(false);
  addTriangleAction->setShortcutContext(Qt::ApplicationShortcut);
  addAction(addTriangleAction);
  connect(addTriangleAction, SIGNAL(triggered()), this, SLOT(addTriangle()));
  addTriangleAction->setEnabled(false);

  // Add ellipse.
  addEllipseAction = new QAction(tr("Add &Ellipse Layer"), this);
  addEllipseAction->setShortcut(Qt::CTRL | Qt::Key_E);
  addEllipseAction->setIcon(QIcon(":/add-ellipse"));
  addEllipseAction->setToolTip(tr("Add ellipse layer"));
  addEllipseAction->setIconVisibleInMenu(false);
  addEllipseAction->setShortcutContext(Qt::ApplicationShortcut);
  addAction(addEllipseAction);
  connect(addEllipseAction, SIGNAL(triggered()), this, SLOT(addEllipse()));
  addEllipseAction->setEnabled(false);

  // Add polygon (free-form, click-to-place vertices).
  addPolygonAction = new QAction(tr("Add &Polygon Layer"), this);
  addPolygonAction->setShortcut(Qt::CTRL | Qt::Key_P);
  addPolygonAction->setIcon(QIcon(":/add-polygon"));
  addPolygonAction->setToolTip(tr("Draw a free-form polygon layer (click to add vertices, click near first point or press Enter to close)"));
  addPolygonAction->setIconVisibleInMenu(false);
  addPolygonAction->setShortcutContext(Qt::ApplicationShortcut);
  addAction(addPolygonAction);
  connect(addPolygonAction, &QAction::triggered, this, &MainWindow::addPolygon);
  addPolygonAction->setEnabled(false);

  addPolygonVertexAction = new QAction(tr("Add Vertex Here"), this);
  connect(addPolygonVertexAction, &QAction::triggered, this, &MainWindow::addPolygonVertex);

  deletePolygonVertexAction = new QAction(tr("Delete Vertex"), this);
  connect(deletePolygonVertexAction, &QAction::triggered, this, &MainWindow::deletePolygonVertex);

  // Play/Pause — single checkable action so no double-trigger on button swap.
  // Unchecked = paused (shows play ▶ icon); checked = playing (shows pause ∥ icon).
  const QKeySequence PLAY_PAUSE_KEY_SEQUENCE = Qt::CTRL | Qt::SHIFT | Qt::Key_P;
  playAction = new QAction(tr("Play"), this);
  playAction->setCheckable(true);
  playAction->setChecked(false);
  {
    QIcon icon;
    icon.addFile(":/play",  QSize(), QIcon::Normal, QIcon::Off);
    icon.addFile(":/pause", QSize(), QIcon::Normal, QIcon::On);
    playAction->setIcon(icon);
  }
  playAction->setToolTip(tr("Play / Pause"));
  playAction->setIconVisibleInMenu(false);
  playAction->setShortcut(PLAY_PAUSE_KEY_SEQUENCE);
  playAction->setShortcutContext(Qt::ApplicationShortcut);
  addAction(playAction);
  connect(playAction, &QAction::triggered, this, [this](bool checked) {
    if (checked) play(false);
    else         pause(false);
  });

  // Rewind.
  rewindAction = new QAction(tr("Restart"), this);
  rewindAction->setShortcut(Qt::CTRL | Qt::Key_R);
  rewindAction->setIcon(QIcon(":/rewind"));
  rewindAction->setToolTip(tr("Restart"));
  rewindAction->setIconVisibleInMenu(false);
  rewindAction->setShortcutContext(Qt::ApplicationShortcut);
  addAction(rewindAction);
  connect(rewindAction, SIGNAL(triggered()), this, SLOT(rewind()));

  // Record output to video file.
  recordAction = new QAction(tr("Record"), this);
  recordAction->setShortcut(Qt::CTRL | Qt::SHIFT | Qt::Key_R);
  recordAction->setIcon(themedIcon(":/record"));
  recordAction->setToolTip(tr("Record output to video file"));
  recordAction->setIconVisibleInMenu(false);
  recordAction->setCheckable(true);
  recordAction->setChecked(false);
  recordAction->setShortcutContext(Qt::ApplicationShortcut);
  addAction(recordAction);
  connect(recordAction, &QAction::toggled, this, &MainWindow::toggleRecording);

  // Mute all source audio.
  muteAllAction = new QAction(tr("&Mute All Audio"), this);
  muteAllAction->setShortcut(Qt::CTRL | Qt::Key_M);
  muteAllAction->setIcon(themedIcon(":/mute"));
  muteAllAction->setToolTip(tr("Mute all audio"));
  muteAllAction->setIconVisibleInMenu(false);
  muteAllAction->setCheckable(true);
  muteAllAction->setChecked(_audioMuted);
  muteAllAction->setShortcutContext(Qt::ApplicationShortcut);
  addAction(muteAllAction);
  connect(muteAllAction, SIGNAL(toggled(bool)), this, SLOT(toggleMuteAll(bool)));

  // Toggle display of output window.
  outputFullScreenAction = new QAction(tr("Toggle &Fullscreen"), this);
  outputFullScreenAction->setShortcut(Qt::CTRL | Qt::Key_F);
  outputFullScreenAction->setIcon(QIcon(":/fullscreen"));
  outputFullScreenAction->setToolTip(tr("Toggle Fullscreen"));
  outputFullScreenAction->setIconVisibleInMenu(false);
  outputFullScreenAction->setCheckable(true);
  // Don't be displayed by default
  outputFullScreenAction->setChecked(false);
  outputFullScreenAction->setShortcutContext(Qt::ApplicationShortcut);
  addAction(outputFullScreenAction);
  // Manage fullscreen/modal show of GL output window.
  connect(outputFullScreenAction, SIGNAL(toggled(bool)), outputWindow, SLOT(setFullScreen(bool)));
  connect(qApp, &QGuiApplication::screenAdded,   this, [this](QScreen*){ updateScreenCount(); });
  connect(qApp, &QGuiApplication::screenRemoved,  this, [this](QScreen*){ updateScreenCount(); });
  // Create hiden action for closing output window
  QAction *closeOutput = new QAction(this);
  closeOutput->setShortcut(Qt::Key_Escape);
  closeOutput->setShortcutContext(Qt::ApplicationShortcut);
  addAction(closeOutput);
  connect(closeOutput, SIGNAL(triggered(bool)), this, SLOT(exitFullScreen()));

  // Toggle display of canvas controls.
  displayControlsAction = new QAction(tr("&Display Controls"), this);
  displayControlsAction->setShortcut(Qt::ALT | Qt::Key_C);
  displayControlsAction->setIcon(QIcon(":/control-points"));
  displayControlsAction->setToolTip(tr("Display canvas controls"));
  displayControlsAction->setIconVisibleInMenu(false);
  displayControlsAction->setCheckable(true);
  displayControlsAction->setChecked(_displayControls);
  displayControlsAction->setShortcutContext(Qt::ApplicationShortcut);
  addAction(displayControlsAction);
  // Manage show/hide of canvas controls.
  connect(displayControlsAction, SIGNAL(toggled(bool)), this, SLOT(enableDisplayControls(bool)));
  connect(displayControlsAction, SIGNAL(toggled(bool)), outputWindow, SLOT(setCanvasDisplayCrosshair(bool)));

  // Toggle display of canvas controls.
  displaySourceControlsAction = new QAction(tr("&Display Controls of Layers of a Source"), this);
  //displaySourceControlsAction->setShortcut(Qt::ALT | Qt::Key_C);
  displaySourceControlsAction->setIcon(QIcon(":/control-points"));
  displaySourceControlsAction->setToolTip(tr("Display all canvas controls related to current source"));
  displaySourceControlsAction->setIconVisibleInMenu(false);
  displaySourceControlsAction->setCheckable(true);
  displaySourceControlsAction->setChecked(_displaySourceControls);
  displaySourceControlsAction->setShortcutContext(Qt::ApplicationShortcut);
  addAction(displaySourceControlsAction);
  // Manage show/hide of canvas controls.
  connect(displaySourceControlsAction, SIGNAL(toggled(bool)), this, SLOT(enableDisplaySourceControls(bool)));
//  connect(displaySourceControlsAction, SIGNAL(toggled(bool)), outputWindow, SLOT(setDisplayCrosshair(bool)));
  connect(displayControlsAction, SIGNAL(toggled(bool)), displaySourceControlsAction, SLOT(setEnabled(bool)));

  // Toggle sticky vertices
  stickyVerticesAction = new QAction(tr("&Sticky Vertices"), this);
  stickyVerticesAction->setShortcut(Qt::ALT | Qt::Key_S);
  stickyVerticesAction->setIcon(QIcon(":/control-points"));
  stickyVerticesAction->setToolTip(tr("Enable sticky vertices"));
  stickyVerticesAction->setIconVisibleInMenu(false);
  stickyVerticesAction->setCheckable(true);
  stickyVerticesAction->setChecked(_stickyVertices);
  stickyVerticesAction->setShortcutContext(Qt::ApplicationShortcut);
  addAction(stickyVerticesAction);
  // Manage sticky vertices
  connect(stickyVerticesAction, SIGNAL(toggled(bool)), this, SLOT(enableStickyVertices(bool)));

  displayTestSignalAction = new QAction(tr("Show &Test Signal"), this);
  displayTestSignalAction->setShortcut(Qt::ALT | Qt::Key_T);
  displayTestSignalAction->setIcon(QIcon(":/toggle-test-signal"));
  displayTestSignalAction->setToolTip(tr("Show Test signal"));
  displayTestSignalAction->setIconVisibleInMenu(false);
  displayTestSignalAction->setCheckable(true);
  displayTestSignalAction->setChecked(false);
  displayTestSignalAction->setShortcutContext(Qt::ApplicationShortcut);
  addAction(displayTestSignalAction);
  // Manage show/hide of test signal
  connect(displayTestSignalAction, SIGNAL(toggled(bool)), outputWindow, SLOT(setDisplayTestSignal(bool)));
//  connect(displayTestSignalAction, SIGNAL(toggled(bool)), this, SLOT(update()));

  // Toggle display of Undo History
  displayUndoHistoryAction = new QAction(tr("Display &Undo History"), this);
  displayUndoHistoryAction->setShortcut(Qt::ALT | Qt::Key_U);
  displayUndoHistoryAction->setCheckable(true);
  displayUndoHistoryAction->setChecked(_displayUndoStack);
  displayUndoHistoryAction->setShortcutContext(Qt::ApplicationShortcut);
  addAction(displayUndoHistoryAction);
  // Manage show/hide of Undo History
  connect(displayUndoHistoryAction, SIGNAL(toggled(bool)), this, SLOT(displayUndoHistory(bool)));

  // Toggle display of Console output
  openConsoleAction = new QAction(tr("Open Conso&le"), this);
  openConsoleAction->setShortcut(Qt::ALT | Qt::Key_L);
  openConsoleAction->setCheckable(true);
  openConsoleAction->setChecked(false);
  openConsoleAction->setShortcutContext(Qt::ApplicationShortcut);
  addAction(openConsoleAction);
  connect(openConsoleAction, SIGNAL(toggled(bool)), consoleWindow, SLOT(setVisible(bool)));
  // uncheck action when window is closed
  connect(consoleWindow, SIGNAL(windowClosed()), openConsoleAction, SLOT(toggle()));

  // Toggle display of zoom tool buttons
  displayZoomToolAction = new QAction(tr("Display &Zoom Toolbar"), this);
  displayZoomToolAction->setShortcut(Qt::ALT | Qt::Key_Z);
  displayZoomToolAction->setCheckable(true);
  displayZoomToolAction->setChecked(true);
  displayZoomToolAction->setShortcutContext(Qt::ApplicationShortcut);
  addAction(displayZoomToolAction);
  connect(displayZoomToolAction, SIGNAL(toggled(bool)), sourceCanvasToolbar, SLOT(showZoomToolBar(bool)));
  connect(displayZoomToolAction, SIGNAL(toggled(bool)), destinationCanvasToolbar, SLOT(showZoomToolBar(bool)));

  // Background reference photo
  _setBackgroundPhotoAction = new QAction(tr("Set Background &Reference Photo..."), this);
  _setBackgroundPhotoAction->setToolTip(tr("Load a photo of your projection surface to use as a reference while mapping"));
  connect(_setBackgroundPhotoAction, &QAction::triggered, this, &MainWindow::setBackgroundPhoto);

  _clearBackgroundPhotoAction = new QAction(tr("Clear Background Photo"), this);
  _clearBackgroundPhotoAction->setEnabled(false);
  connect(_clearBackgroundPhotoAction, &QAction::triggered, this, &MainWindow::clearBackgroundPhoto);

  _resetMeshInputAction = new QAction(tr("Reset Input Mesh to Source Dimensions"), this);
  _resetMeshInputAction->setToolTip(tr("Redistribute the input mesh vertices to cover the full source width and height"));
  connect(_resetMeshInputAction, &QAction::triggered, this, &MainWindow::resetMeshInputToSource);

  // Toggle show/hide menuBar
  showMenuBarAction = new QAction(tr("&Menu Bar"), this);
  showMenuBarAction->setCheckable(true);
  showMenuBarAction->setChecked(_showMenuBar);
  connect(showMenuBarAction, SIGNAL(toggled(bool)), this, SLOT(showMenuBar(bool)));

  // Perspectives
  // Main perspective (Source + destination)
  mainViewAction = new QAction(tr("Main Layout"), this);
  mainViewAction->setCheckable(true);
  mainViewAction->setChecked(true);
  mainViewAction->setShortcut(Qt::CTRL | Qt::Key_1);
  mainViewAction->setToolTip(tr("Switch to the Main layout."));
  connect(mainViewAction, SIGNAL(triggered(bool)), canvasSplitter->widget(0), SLOT(setVisible(bool)));
  connect(mainViewAction, SIGNAL(triggered(bool)), canvasSplitter->widget(1), SLOT(setVisible(bool)));
  // Source Only
  sourceViewAction = new QAction(tr("Input editor Layout"), this);
  sourceViewAction->setCheckable(true);
  sourceViewAction->setShortcut(Qt::CTRL | Qt::Key_2);
  sourceViewAction->setToolTip(tr("Switch to the Input editor Layout."));
  connect(sourceViewAction, SIGNAL(triggered(bool)), canvasSplitter->widget(0), SLOT(setVisible(bool)));
  connect(sourceViewAction, SIGNAL(triggered(bool)), canvasSplitter->widget(1), SLOT(setHidden(bool)));
  // Destination Only
  destViewAction = new QAction(tr("Output Editor Layout"), this);
  destViewAction->setCheckable(true);
  destViewAction->setShortcut(Qt::CTRL | Qt::Key_3);
  destViewAction->setToolTip(tr("Switch to the Output Editors Layout."));
  connect(destViewAction, SIGNAL(triggered(bool)), canvasSplitter->widget(0), SLOT(setHidden(bool)));
  connect(destViewAction, SIGNAL(triggered(bool)), canvasSplitter->widget(1), SLOT(setVisible(bool)));
  // Groups all actions
  perspectiveActionGroup = new QActionGroup(this);
  perspectiveActionGroup->addAction(mainViewAction);
  perspectiveActionGroup->addAction(sourceViewAction);
  perspectiveActionGroup->addAction(destViewAction);

  //Zoom toolbar
  // Zoom In
  zoomInAction = new QAction(tr("Zoom In"), this);
  zoomInAction->setShortcut(QKeySequence::ZoomIn);
  zoomInAction->setIcon(themedIcon(":/zoom-in"));
  zoomInAction->setToolTip(tr("Zoom In"));
  zoomInAction->setIconVisibleInMenu(false);
  zoomInAction->setEnabled(false);
  connect(zoomInAction, SIGNAL(triggered()), sourceCanvas, SLOT(increaseZoomLevel()));
  connect(zoomInAction, SIGNAL(triggered()), destinationCanvas, SLOT(increaseZoomLevel()));
  // Zoom Out
  zoomOutAction = new QAction(tr("Zoom Out"), this);
  zoomOutAction->setShortcut(QKeySequence::ZoomOut);
  zoomOutAction->setIcon(themedIcon(":/zoom-out"));
  zoomOutAction->setToolTip(tr("Zoom Out"));
  zoomOutAction->setIconVisibleInMenu(false);
  zoomOutAction->setEnabled(false);
  connect(zoomOutAction, SIGNAL(triggered()), sourceCanvas, SLOT(decreaseZoomLevel()));
  connect(zoomOutAction, SIGNAL(triggered()), destinationCanvas, SLOT(decreaseZoomLevel()));
  // Reset zoom
  resetZoomAction = new QAction(tr("Original Size"), this);
  resetZoomAction->setShortcut(Qt::CTRL | Qt::Key_0);
  resetZoomAction->setIcon(themedIcon(":/reset-zoom"));
  resetZoomAction->setToolTip(tr("Reset zoom to original size"));
  resetZoomAction->setIconVisibleInMenu(false);
  resetZoomAction->setEnabled(false);
  connect(resetZoomAction, SIGNAL(triggered()), sourceCanvas, SLOT(resetZoomLevel()));
  connect(resetZoomAction, SIGNAL(triggered()), destinationCanvas, SLOT(resetZoomLevel()));
  // Fit to view
  fitToViewAction = new QAction(tr("Fit To View"), this);
  fitToViewAction->setIcon(themedIcon(":/zoom-fit"));
  fitToViewAction->setToolTip(tr("Fit to viewport (bring all shapes into view)"));
  fitToViewAction->setIconVisibleInMenu(false);
  fitToViewAction->setEnabled(false);
  connect(fitToViewAction, SIGNAL(triggered()), sourceCanvas, SLOT(fitShapeToView()));
  connect(fitToViewAction, SIGNAL(triggered()), destinationCanvas, SLOT(fitShapeToView()));

  // Help actions
  bugReportAction = new QAction(tr("Report an issue"), this);
  connect(bugReportAction, SIGNAL(triggered()), this, SLOT(reportBug()));
  docAction = new QAction(tr("Documentation"), this);
  connect(docAction, SIGNAL(triggered()), this, SLOT(documentation()));
  // Keyboard shortcuts
  shortcutAction = new QAction(tr("&Keyboard shortcuts"), this);
  shortcutAction->setShortcut(Qt::CTRL | Qt::Key_K);
  connect(shortcutAction, SIGNAL(triggered()), this, SLOT(openShortcutWindow()));

  // All available screen as action
  updateScreenActions();
//  outputScreenMenu->addActions(screenActions);
}

void MainWindow::startFullScreen()
{
  // Remove canvas controls.
  displayControlsAction->setChecked(false);
  // Display output window.
  outputFullScreenAction->setChecked(true);
}

void MainWindow::createMenus()
{
  QMenuBar *menuBar = nullptr;

#ifdef __MACOSX_CORE__
  menuBar = new QMenuBar(0);
  //this->setMenuBar(menuBar);
#else
  menuBar = this->menuBar();
#endif

  // File
  fileMenu = menuBar->addMenu(tr("&File"));
  fileMenu->addAction(newAction);
  fileMenu->addAction(openAction);
  fileMenu->addAction(saveAction);
  fileMenu->addAction(saveAsAction);
  fileMenu->addSeparator();
  fileMenu->addAction(importMediaAction);
  fileMenu->addAction(importFolderAction);
  fileMenu->addAction(AddCameraAction);
  fileMenu->addAction(addColorAction);
  fileMenu->addAction(addTextAction);
#ifdef Q_OS_MAC
  fileMenu->addAction(addSyphonAction);
#endif

  // Recent file separator
  separatorAction = fileMenu->addSeparator();
  recentFileMenu = fileMenu->addMenu(tr("Open Recents Projects"));
  for (int i = 0; i < MaxRecentFiles; ++i) {
    recentFileMenu->addAction(recentFileActions[i]);
  }
  recentFileMenu->addAction(clearRecentFileActions);

  // Recent import video
  recentVideoMenu = fileMenu->addMenu(tr("Open Recents Videos"));
  recentVideoMenu->addAction(emptyRecentVideos);
  for (int i = 0; i < MaxRecentVideo; ++i) {
    recentVideoMenu->addAction(recentVideoActions[i]);
  }

  // Exit
  fileMenu->addSeparator();
  fileMenu->addAction(exitAction);

  // Edit.
  editMenu = menuBar->addMenu(tr("&Edit"));
  // Undo & Redo menu
  editMenu->addAction(undoAction);
  editMenu->addAction(redoAction);
  editMenu->addSeparator();
  // Source canvas menu
  editMenu->addAction(deleteSourceAction);
  editMenu->addAction(renameSourceAction);
  editMenu->addSeparator();
  // Destination canvas menu
  editMenu->addAction(duplicateLayerAction);
  editMenu->addAction(deleteLayerAction);
  editMenu->addAction(renameLayerAction);
  editMenu->addSeparator();
  editMenu->addAction(layerRaiseAction);
  editMenu->addAction(layerLowerAction);
  editMenu->addAction(layerRaiseToTopAction);
  editMenu->addAction(layerLowerToBottomAction);
  editMenu->addSeparator();
  editMenu->addAction(layerRotate90CWAction);
  editMenu->addAction(layerRotate90CCWAction);
  editMenu->addAction(layerRotate180Action);
  editMenu->addSeparator();
  editMenu->addAction(layerHorizontalFlipAction);
  editMenu->addAction(layerVerticalFlipAction);
  editMenu->addSeparator();

  editMenu->addAction(layerLockedAction);
  editMenu->addAction(layerHideAction);
  editMenu->addAction(layerSoloAction);
  editMenu->addSeparator();

  // Sticky vertices
  editMenu->addAction(stickyVerticesAction);
  editMenu->addSeparator();
  // Preferences
  editMenu->addAction(preferencesAction);

  // View.
  viewMenu = menuBar->addMenu(tr("&View"));

  viewMenu->addAction(zoomInAction);
  viewMenu->addAction(zoomOutAction);
  viewMenu->addAction(resetZoomAction);
  viewMenu->addAction(fitToViewAction);
  viewMenu->addSeparator();
  viewMenu->addAction(outputFullScreenAction);
  viewMenu->addAction(displayTestSignalAction);
  viewMenu->addAction(displayControlsAction);
  viewMenu->addAction(displaySourceControlsAction);
  viewMenu->addSeparator();
  viewMenu->addAction(_setBackgroundPhotoAction);
  viewMenu->addAction(_clearBackgroundPhotoAction);
  outputScreenMenu = viewMenu->addMenu(tr("&Output screen"));
  outputScreenMenu->addActions(screenActions);
  viewMenu->addSeparator();
  // Playback.
  viewMenu->addAction(playAction);
  viewMenu->addAction(rewindAction);
  viewMenu->addAction(muteAllAction);

  // Window
  windowMenu = menuBar->addMenu(tr("&Window"));

  // Perspectives
  windowMenu->addAction(mainViewAction);
  windowMenu->addAction(sourceViewAction);
  windowMenu->addAction(destViewAction);
  windowMenu->addSeparator();

  // Tools.
  windowMenu->addAction(displayUndoHistoryAction);
  windowMenu->addAction(openConsoleAction);
  windowMenu->addSeparator();

  // Menus/toolbars.
  windowMenu->addAction(displayZoomToolAction);
#ifdef Q_OS_LINUX
  if (QString(getenv("XDG_CURRENT_DESKTOP")).toLower() != "unity")
    windowMenu->addAction(showMenuBarAction);
#endif
#ifdef Q_OS_WIN32
  windowMenu->addAction(showMenuBarAction);
#endif

  // Help.
  helpMenu = menuBar->addMenu(tr("&Help"));
  helpMenu->addAction(docAction);
  helpMenu->addAction(shortcutAction);
  helpMenu->addAction(bugReportAction);
  helpMenu->addSeparator();
  helpMenu->addAction(checkForUpdatesAction);
  helpMenu->addSeparator();
  helpMenu->addAction(aboutAction);

  //  helpMenu->addAction(aboutQtAction);
}

void MainWindow::createLayerContextMenu()
{
  // Context menu.
  layerContextMenu = new QMenu(this);
  layerContextMenu->installEventFilter(this);

  // Polygon vertex editing (shown/hidden dynamically in showLayerContextMenu).
  layerContextMenu->addAction(addPolygonVertexAction);
  layerContextMenu->addAction(deletePolygonVertexAction);
  layerContextMenu->addSeparator();

  // Add different Action
  layerContextMenu->addAction(duplicateLayerAction);
  layerContextMenu->addAction(deleteLayerAction);
  layerContextMenu->addAction(renameLayerAction);
  // Add a little separator
  layerContextMenu->addSeparator();

  // Create menu for source list
  _changeLayerMediaMenu = layerContextMenu->addMenu(tr("Change Layer Source"));

  // Mesh input reset (visible only when right-clicking in the source canvas on a Mesh layer).
  layerContextMenu->addAction(_resetMeshInputAction);

  // Add another separator
  layerContextMenu->addSeparator();
  layerContextMenu->addAction(layerRaiseAction);
  layerContextMenu->addAction(layerLowerAction);
  layerContextMenu->addAction(layerRaiseToTopAction);
  layerContextMenu->addAction(layerLowerToBottomAction);
  layerContextMenu->addSeparator();
  layerContextMenu->addAction(layerRotate90CWAction);
  layerContextMenu->addAction(layerRotate90CCWAction);
  layerContextMenu->addAction(layerRotate180Action);
  layerContextMenu->addSeparator();
  layerContextMenu->addAction(layerHorizontalFlipAction);
  layerContextMenu->addAction(layerVerticalFlipAction);

  layerContextMenu->addSeparator();
  layerContextMenu->addAction(layerLockedAction);
  layerContextMenu->addAction(layerHideAction);
  layerContextMenu->addAction(layerSoloAction);

  // Set context menu policy
  layerList->setContextMenuPolicy(Qt::CustomContextMenu);
  destinationCanvas->setContextMenuPolicy(Qt::CustomContextMenu);
  outputWindow->setContextMenuPolicy(Qt::CustomContextMenu);

  // Context Menu Connexions
  connect(layerItemDelegate, SIGNAL(itemContextMenuRequested(const QPoint&)),
          this, SLOT(showLayerContextMenu(const QPoint&)), Qt::QueuedConnection);
  connect(destinationCanvas, SIGNAL(shapeContextMenuRequested(const QPoint&)), this, SLOT(showLayerContextMenu(const QPoint&)));
  connect(outputWindow->getCanvas(), SIGNAL(shapeContextMenuRequested(const QPoint&)), this, SLOT(showLayerContextMenu(const QPoint&)));
}

void MainWindow::createSourceContextMenu()
{
  // Source Context Menu
  sourceContextMenu = new QMenu(this);
  sourceContextMenu->installEventFilter(this);

  // Add Actions
  sourceContextMenu->addAction(deleteSourceAction);
  sourceContextMenu->addAction(renameSourceAction);

  // Define Context policy
  sourceList->setContextMenuPolicy(Qt::CustomContextMenu);
  sourceCanvas->setContextMenuPolicy(Qt::CustomContextMenu);

  // Connexions
  connect(sourceList, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(showSourceContextMenu(const QPoint&)));
  connect(sourceCanvas, SIGNAL(shapeContextMenuRequested(const QPoint&)), this, SLOT(showLayerContextMenu(const QPoint&)));
}

void MainWindow::createToolBars()
{
  mainToolBar = addToolBar(tr("&Toolbar"));
  mainToolBar->setMovable(false);
  mainToolBar->addAction(importMediaAction);
  mainToolBar->addAction(AddCameraAction);
  mainToolBar->addAction(addColorAction);
  mainToolBar->addAction(addTextAction);
#ifdef Q_OS_MAC
  mainToolBar->addAction(addSyphonAction);
#endif

  mainToolBar->addSeparator();

  mainToolBar->addAction(addPolygonAction);
  mainToolBar->addAction(addMeshAction);
  mainToolBar->addAction(addTriangleAction);
  mainToolBar->addAction(addEllipseAction);
  mainToolBar->addSeparator();

  mainToolBar->addAction(outputFullScreenAction);
  mainToolBar->addAction(displayTestSignalAction);

  // XXX: style hack: dummy expanding widget allows the placement of toolbar at the top right
  // From: http://www.qtcentre.org/threads/9102-QToolbar-setContentsMargins
  QWidget* spacer = new QWidget(mainToolBar);
  spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  mainToolBar->addWidget(spacer);
  mainToolBar->addAction(playAction);
  mainToolBar->addAction(rewindAction);
  mainToolBar->addAction(muteAllAction);
  mainToolBar->addSeparator();
  mainToolBar->addAction(recordAction);

  // Disable toolbar context menu
  mainToolBar->setContextMenuPolicy(Qt::PreventContextMenu);

  // Toggle show/hide of toolbar
  showToolBarAction = mainToolBar->toggleViewAction();
  windowMenu->addAction(showToolBarAction);

  // Add toolbars.
  addToolBar(Qt::TopToolBarArea, mainToolBar);

  // XXX: style hack — keep border-bottom separator; also style action buttons
  mainToolBar->setStyleSheet(
    "QToolBar { border-bottom: solid 5px #272a36; }"
    "QToolButton { border: 1px solid palette(mid); margin: 2px; }"
    "QToolButton:hover { border-color: palette(highlight); background: palette(highlight); }"
    "QToolButton:pressed, QToolButton:checked { background: palette(dark); border-color: palette(highlight); }");
}

void MainWindow::createStatusBar()
{
  // Create canvases zoom level statut
  destinationZoomLabel = new QLabel(statusBar());
  destinationZoomLabel->setFrameStyle(QFrame::Panel | QFrame::Sunken);
  destinationZoomLabel->setContentsMargins(2, 0, 0, 0);
  sourceZoomLabel = new QLabel(statusBar());
  sourceZoomLabel->setFrameStyle(QFrame::Panel | QFrame::Sunken);
  sourceZoomLabel->setContentsMargins(2, 0, 0, 0);
  // last action taking statut
  lastActionLabel = new QLabel(statusBar());
  lastActionLabel->setFrameStyle(QFrame::Panel | QFrame::Sunken);
  lastActionLabel->setContentsMargins(2, 0, 0, 0);
  // Standard message
  currentMessageLabel = new QLabel(statusBar());
  currentMessageLabel->setFrameStyle(QFrame::Panel | QFrame::Sunken);
  currentMessageLabel->setContentsMargins(0, 0, 0, 0);
  // Current location of the mouse
  mousePosLabel = new QLabel(statusBar());
  mousePosLabel->setFrameStyle(QFrame::Panel | QFrame::Sunken);
  mousePosLabel->setContentsMargins(2, 0, 0, 0);
  // FPS.
  trueFramesPerSecondsLabel = new QLabel(statusBar());
  trueFramesPerSecondsLabel->setFrameStyle(QFrame::Panel | QFrame::Sunken);
  trueFramesPerSecondsLabel->setContentsMargins(2, 0, 0, 0);

  // Recording timer.
  recordingTimerLabel = new QLabel(statusBar());
  recordingTimerLabel->setFrameStyle(QFrame::Panel | QFrame::Sunken);
  recordingTimerLabel->setContentsMargins(4, 0, 4, 0);
  recordingTimerLabel->setStyleSheet("color: #e03030; font-weight: bold;");
  recordingTimerLabel->hide();

  // Add permanently into the statut bar
  statusBar()->addPermanentWidget(currentMessageLabel, 5);
  statusBar()->addPermanentWidget(lastActionLabel, 4);
  statusBar()->addPermanentWidget(mousePosLabel, 3);
  statusBar()->addPermanentWidget(sourceZoomLabel, 1);
  statusBar()->addPermanentWidget(destinationZoomLabel, 1);
  statusBar()->addPermanentWidget(trueFramesPerSecondsLabel, 1);
  statusBar()->addPermanentWidget(recordingTimerLabel);

  // Update the status bar
  updateStatusBar();
}

void MainWindow::readSettings()
{
  // All UI state restoration is done in the deferred singleShot in the constructor.
  // Any restoreGeometry/restoreState/setChecked call here triggers a resize or repaint
  // which calls MainWindow::window() before the singleton is fully initialized,
  // causing infinite recursive construction. Only read non-UI values here.
  QSettings settings;
  oscListeningPort = settings.value("oscListeningPort", MM::DEFAULT_OSC_PORT).toInt();
#ifdef HAVE_MCP
  mcpListeningPort = settings.value("mcpListeningPort", MM::DEFAULT_MCP_PORT).toInt();
#endif
}

void MainWindow::writeSettings()
{
  QSettings settings;
  settings.setValue("geometry", saveGeometry());
  settings.setValue("windowState", saveState());
  settings.setValue("mainSplitter", mainSplitter->saveState());
  settings.setValue("sourceSplitter", sourceSplitter->saveState());
  settings.setValue("layerSplitter", layerSplitter->saveState());
  settings.setValue("canvasSplitter", canvasSplitter->saveState());
  settings.setValue("outputWindow", outputWindow->saveGeometry());
  settings.setValue("outputScreen", outputWindow->getPreferredScreen());
  settings.setValue("displayOutputWindow", outputFullScreenAction->isChecked());
  settings.setValue("displayTestSignal", displayTestSignalAction->isChecked());
  settings.setValue("displayAllControls", displaySourceControlsAction->isChecked());
  settings.setValue("sourceListIconSize", _sourceListIconSize);
  settings.setValue("sourceListThumbnailMode", _sourceListThumbnailMode);
  settings.setValue("oscListeningPort", oscListeningPort);
#ifdef HAVE_MCP
  settings.setValue("mcpListeningPort", mcpListeningPort);
#endif
  settings.setValue("displayUndoStack", displayUndoHistoryAction->isChecked());
  settings.setValue("zoomToolBar", displayZoomToolAction->isChecked());
  settings.setValue("showMenuBar", showMenuBarAction->isChecked());
  settings.setValue("stickyVertices", stickyVerticesAction->isChecked());
  settings.setValue("audioMuted", muteAllAction->isChecked());
}

bool MainWindow::okToContinue()
{
  if (isWindowModified())
  {
    int r = QMessageBox::warning(this, tr("MyMapMap"),
                                 tr("The document has been modified.\n"
                                    "Do you want to save your changes?"),
                                 QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
    if (r == QMessageBox::Yes)
    {
      return save();
    }
    else if (r == QMessageBox::Cancel)
    {
      return false;
    }
  }
  return true;
}

bool MainWindow::loadFile(const QString &fileName)
{
  QFile file(fileName);
  QDir currentDir;

  if (! file.open(QFile::ReadOnly | QFile::Text))
  {
    QMessageBox::warning(this, tr("Error reading mapping project file"),
                         tr("Cannot read file %1:\n%2.")
                         .arg(fileName)
                         .arg(file.errorString()));
    return false;
  }

  // Clear current project.
  clearProject();

  // Set curFile early so locateMediaFile can resolve relative paths.
  curFile = fileName;

  // Read new project
  ProjectReader reader(this);
  if (! reader.readFile(&file))
  {
    QMessageBox::warning(this, tr("Error reading mapping project file"),
                         tr("Parse error in file %1:\n\n%2")
                         .arg(fileName)
                         .arg(reader.errorString()));
  }
  else
  {
    settings.setValue("defaultProjectDir", currentDir.absoluteFilePath(fileName));
    statusBar()->showMessage(tr("File loaded"), 2000);
    setCurrentFile(fileName);

    // Fit both editors to the newly loaded content instead of keeping
    // whatever zoom/pan was left over from the previous project.
    sourceCanvas->fitShapeToView();
    destinationCanvas->fitShapeToView();
    outputWindow->getCanvas()->fitToContent();
  }

  return true;
}

bool MainWindow::saveFile(const QString &fileName)
{
  // Write to a temporary file first, then rename for atomic save.
  QString tmpFileName = fileName + ".tmp";
  QFile file(tmpFileName);
  if (! file.open(QFile::WriteOnly | QFile::Text))
  {
    QMessageBox::warning(this, tr("Error saving mapping project"),
                         tr("Cannot write file %1:\n%2.")
                         .arg(fileName)
                         .arg(file.errorString()));
    return false;
  }

  ProjectWriter writer(this, fileName);
  if (writer.writeFile(&file))
  {
    file.close();
    // Remove original and rename temp file over it.
    QFile::remove(fileName);
    if (!QFile::rename(tmpFileName, fileName))
    {
      QMessageBox::warning(this, tr("Error saving mapping project"),
                           tr("Cannot rename temporary file to %1.")
                           .arg(fileName));
      return false;
    }
    setCurrentFile(fileName);
    statusBar()->showMessage(tr("File saved"), 2000);
    return true;
  }
  else
  {
    file.close();
    QFile::remove(tmpFileName);
    return false;
  }
}

void MainWindow::setCurrentFile(const QString &fileName)
{
  curFile = fileName;
  setWindowModified(false);

  QString shownName = tr("Untitled");
  if (!curFile.isEmpty())
  {
    shownName = strippedName(curFile);
    QString canonicalFile = QFileInfo(curFile).canonicalFilePath();
    if (canonicalFile.isEmpty()) canonicalFile = curFile;
    recentFiles = settings.value("recentFiles").toStringList();
    // Normalize existing entries and remove any that resolve to the same file.
    recentFiles.erase(std::remove_if(recentFiles.begin(), recentFiles.end(),
      [&](const QString& f) {
        QString c = QFileInfo(f).canonicalFilePath();
        return c.isEmpty() ? f == canonicalFile : c == canonicalFile;
      }), recentFiles.end());
    recentFiles.prepend(canonicalFile);
    while (recentFiles.size() > MaxRecentFiles)
    {
      recentFiles.removeLast();
    }
    settings.setValue("recentFiles", recentFiles);
    updateRecentFileActions();
  }

  setWindowTitle(tr("%1[*] - %2").arg(shownName).arg(tr("MyMapMap Project")));
}

void MainWindow::setCurrentVideo(const QString &fileName)
{
  curVideo = fileName;

  recentVideos = settings.value("recentVideos").toStringList();
  recentVideos.removeAll(curVideo);
  recentVideos.prepend(curVideo);
  while (recentVideos.size() > MaxRecentVideo)
    recentVideos.removeLast();
  settings.setValue("recentVideos", recentVideos);
  updateRecentVideoActions();
}

void MainWindow::updateRecentFileActions()
{
  recentFiles = settings.value("recentFiles").toStringList();
  int numRecentFiles = qMin(recentFiles.size(), int(MaxRecentFiles));

  for (int j = 0; j < numRecentFiles; ++j)
  {
    QString text = tr("&%1 %2")
        .arg(j + 1)
        .arg(strippedName(recentFiles[j]));
    recentFileActions[j]->setText(text);
    recentFileActions[j]->setData(recentFiles[j]);
    recentFileActions[j]->setVisible(true);
    clearRecentFileActions->setVisible(true);
  }

  for (int i = numRecentFiles; i < MaxRecentFiles; ++i)
  {
    recentFileActions[i]->setVisible(false);
  }

  if (numRecentFiles > 0)
  {
    separatorAction->setVisible(true);
    clearRecentFileActions->setText(tr("Clear List"));
    clearRecentFileActions->setEnabled(true);
  } else {
    clearRecentFileActions->setText(tr("No Recents Projects"));
    clearRecentFileActions->setEnabled(false);
  }
}

void MainWindow::updateRecentVideoActions()
{
  recentVideos = settings.value("recentVideos").toStringList();
  int numRecentVideos = qMin(recentVideos.size(), int(MaxRecentVideo));

  for (int i = 0; i < numRecentVideos; ++i)
  {
    QString text = tr("&%1 %2")
        .arg(i + 1)
        .arg(strippedName(recentVideos[i]));
    recentVideoActions[i]->setText(text);
    recentVideoActions[i]->setData(recentVideos[i]);
    recentVideoActions[i]->setVisible(true);
  }

  for (int j = numRecentVideos; j < MaxRecentVideo; ++j)
    recentVideoActions[j]->setVisible(false);

  if (numRecentVideos >  0)
  {
    emptyRecentVideos->setVisible(false);
  }
}

void MainWindow::updateScreenActions()
{
  // Add new action for each screen
  for (QScreen *screen: QApplication::screens()) {
    QString actionLabel = tr("%1 - %2x%3")
        .arg(screen->name())
        .arg(QString::number(screen->size().width()))
        .arg(QString::number(screen->size().height()));
    if (screen == QApplication::primaryScreen()) {
      actionLabel.append(tr(" - Primary"));
    }
    QAction *action = new QAction(actionLabel, this);
    screenActions.append(action);
    action->setData(screenActions.count() - 1);
  }

  // Configure actions
  screenActionGroup = new QActionGroup(this);
  int preferredScreen = outputWindow->getPreferredScreen();
  for (QAction *action: screenActions) {
    action->setCheckable(true);
    if (action == screenActions.at(preferredScreen)) {
      action->setChecked(true);
    }
    connect(action, SIGNAL(triggered()), this, SLOT(setupOutputScreen()));
    screenActionGroup->addAction(action);
  }
}

void MainWindow::updateMediaListActions()
{
  // Clear media list menu
  _changeLayerMediaMenu->clear();

  if (mappingManager->nSources() > 1) { // No need to load the same video
    for (auto i = 0; i < mappingManager->nSources(); i++) {
      QAction *mediaAction = new QAction(this);
      mediaAction->setText(tr("&%1 %2").arg(i + 1).arg(mappingManager->getSource(i)->getName()));
      mediaAction->setData(mappingManager->getSource(i)->getId());
      mediaAction->setVisible(true);
      connect(mediaAction, SIGNAL(triggered()),
              this, SLOT(loadLayerMedia()));
      // Add new media on action list
      _changeLayerMediaMenu->addAction(mediaAction);
    }
  }
  // Add new media source in case no exists on the list
  _changeLayerMediaMenu->addAction(_importLayerMediaAction);
}

void MainWindow::updateLayerActions()
{
  if (layerListModel->rowCount() < 1) {
    duplicateLayerAction->setEnabled(false);
    deleteLayerAction->setEnabled(false);
    renameLayerAction->setEnabled(false);
    layerLockedAction->setEnabled(false);
    layerHideAction->setEnabled(false);
    layerSoloAction->setEnabled(false);
    layerRotate90CWAction->setEnabled(true);
    layerRotate90CCWAction->setEnabled(true);
    layerRotate180Action->setEnabled(true);
    layerHorizontalFlipAction->setEnabled(false);
    layerVerticalFlipAction->setEnabled(false);
    layerRaiseAction->setEnabled(false);
    layerLowerAction->setEnabled(false);
    layerRaiseToTopAction->setEnabled(false);
    layerLowerToBottomAction->setEnabled(false);
    //Disable zoom menus
    zoomInAction->setEnabled(false);
    zoomOutAction->setEnabled(false);
    resetZoomAction->setEnabled(false);
    fitToViewAction->setEnabled(false);
    // Also disable toobars
    destinationCanvasToolbar->enableZoomToolBar(false);
    sourceCanvasToolbar->enableZoomToolBar(false);
  }
}

void MainWindow::clearRecentFileList()
{
  recentFiles = settings.value("recentFiles").toStringList();

  while (recentFiles.size() > 0)
    recentFiles.clear();

  settings.setValue("recentFiles", recentFiles);
  updateRecentFileActions();
}

// TODO
// bool MainWindow::updateMediaFile(const QString &source_name, const QString &fileName)
// {
// }

bool MainWindow::importMediaFile(const QString &fileName, bool isImage, bool isCamera)
{
  QFile file(fileName);
  QDir currentDir;
  VideoType type = VIDEO_URI;

  if (!fileSupported(fileName, isImage))
    return false;

  if (isCamera) {
    type = VIDEO_WEBCAM;
  }

  if (!isCamera && !file.open(QIODevice::ReadOnly)) {
    QMessageBox::warning(this, tr("MyMapMap Project"),
                         tr("Cannot read file %1:\n%2.")
                         .arg(file.fileName())
                         .arg(file.errorString()));
    return false;
  }

  QApplication::setOverrideCursor(Qt::WaitCursor);

  // Add media file to model.
  uint mediaId = createMediaSource(NULL_UID, fileName, 0, 0, isImage, type);

  // Initialize position (center).
  QSharedPointer<Texture> media = qSharedPointerCast<Texture>(mappingManager->getSourceById(mediaId));
  Q_CHECK_PTR(media);

  media->setPosition((sourceCanvas->width()  - media->getWidth() ) / 2.0f,
                     (sourceCanvas->height() - media->getHeight()) / 2.0f );

  QApplication::restoreOverrideCursor();

  if (!isCamera) { // Do not add camera to recents files
    if (!isImage)
    {
      settings.setValue("defaultVideoDir", currentDir.absoluteFilePath(fileName));
      setCurrentVideo(fileName);
    }
    else
    {
      settings.setValue("defaultImageDir", currentDir.absoluteFilePath(fileName));
    }
  }

  statusBar()->showMessage(tr("File imported"), 2000);

  // Update media list
  updateMediaListActions();

  return true;
}

bool MainWindow::addColorSource(const QColor& color)
{
  QApplication::setOverrideCursor(Qt::WaitCursor);

  // Add color to model.
  uint colorId = createColorSource(NULL_UID, color);

  // Initialize position (center).
  QSharedPointer<Color> colorSource = qSharedPointerCast<Color>(mappingManager->getSourceById(colorId));
  Q_CHECK_PTR(colorSource);

  QApplication::restoreOverrideCursor();

  statusBar()->showMessage(tr("Color source added"), 2000);

  return true;
}

void MainWindow::initSourceListSections()
{
  auto makeHeader = [this](const QString& title, const char* addSlot) -> QListWidgetItem* {
    QListWidgetItem* h = new QListWidgetItem();
    h->setFlags(Qt::ItemIsEnabled);
    h->setSizeHint(QSize(0, 20));
    sourceList->addItem(h);

    QWidget* container = new QWidget;
    container->setAutoFillBackground(false);

    QHBoxLayout* layout = new QHBoxLayout(container);
    layout->setContentsMargins(2, 2, 4, 2);
    layout->setSpacing(2);

    QToolButton* arrow = new QToolButton;
    arrow->setText("▼");
    arrow->setFixedSize(16, 16);
    arrow->setAutoRaise(true);
    arrow->setCursor(Qt::ArrowCursor);
    arrow->setStyleSheet(
      "QToolButton { color: rgb(140,140,140); border: none; font-size: 8px; }"
      "QToolButton:hover { color: rgb(200,200,200); }");
    _sectionArrows[h] = arrow;
    connect(arrow, &QToolButton::clicked, this, [this, h]() {
      setSectionCollapsed(h, !_sectionCollapsed.value(h, false));
    });

    QLabel* label = new QLabel(title);
    QFont f = label->font();
    f.setBold(true);
    f.setPointSizeF(f.pointSizeF() * 0.85);
    label->setFont(f);
    label->setStyleSheet("color: rgb(160,160,160);");

    QToolButton* btn = new QToolButton;
    btn->setText("+");
    btn->setFixedSize(18, 18);
    btn->setAutoRaise(true);
    btn->setCursor(Qt::ArrowCursor);
    btn->setStyleSheet(
      "QToolButton { color: rgb(160,160,160); font-weight: bold; "
      "border: 1px solid rgb(100,100,100); border-radius: 3px; }"
      "QToolButton:hover { background: rgb(80,80,80); }");
    connect(btn, SIGNAL(clicked()), this, addSlot);

    layout->addWidget(arrow);
    layout->addWidget(label);
    layout->addStretch();
    layout->addWidget(btn);

    sourceList->setItemWidget(h, container);
    return h;
  };

  _sourceSectionImages    = makeHeader(tr("Images"),        SLOT(importMedia()));
  _sourceSectionVideos    = makeHeader(tr("Movies"),        SLOT(importMedia()));
  _sourceSectionGenerated = makeHeader(tr("Generated"),     SLOT(addColor()));
  _sourceSectionFolders   = makeHeader(tr("Image Folders"), SLOT(importFolderAsSource()));
}

void MainWindow::setSectionCollapsed(QListWidgetItem* header, bool collapsed)
{
  _sectionCollapsed[header] = collapsed;

  if (_sectionArrows.contains(header))
    _sectionArrows[header]->setText(collapsed ? "▶" : "▼");

  QSet<QListWidgetItem*> headers = {
    _sourceSectionImages, _sourceSectionVideos,
    _sourceSectionGenerated, _sourceSectionFolders
  };
  int start = sourceList->row(header) + 1;
  for (int i = start; i < sourceList->count(); ++i) {
    QListWidgetItem* it = sourceList->item(i);
    if (headers.contains(it)) break;
    it->setHidden(collapsed);
  }
}

void MainWindow::setSourceListThumbnailMode(bool thumbnailMode)
{
  _sourceListThumbnailMode = thumbnailMode;

  if (_viewModeBtns[0]) _viewModeBtns[0]->setChecked(!thumbnailMode);
  if (_viewModeBtns[1]) _viewModeBtns[1]->setChecked(thumbnailMode);

  // Size buttons and preview only make sense in thumbnail mode.
  for (auto* b : _thumbSizeBtns) if (b) b->setVisible(thumbnailMode);
  if (_previewToggleBtn) _previewToggleBtn->setVisible(thumbnailMode);

  int iconPx = thumbnailMode ? _sourceListIconSize : 0;
  sourceList->setIconSize(QSize(iconPx, iconPx));

  int rowH = thumbnailMode ? (_sourceListIconSize + 8) : 24;
  QSet<QListWidgetItem*> headers = {
    _sourceSectionImages, _sourceSectionVideos,
    _sourceSectionGenerated, _sourceSectionFolders
  };
  for (int i = 0; i < sourceList->count(); ++i) {
    QListWidgetItem* it = sourceList->item(i);
    if (headers.contains(it)) continue;
    it->setSizeHint(QSize(it->sizeHint().width(), rowH));
  }

  QSettings().setValue("sourceListThumbnailMode", thumbnailMode);
}

void MainWindow::addSourceItem(uid sourceId, const QIcon& icon, const QString& name)
{
  Source::ptr source = mappingManager->getSourceById(sourceId);
  Q_CHECK_PTR(source);

  // Create source gui.
  SourceGui::ptr sourceGui;
  SourceType sourceType = source->getSourceType();
  if (sourceType == SourceType::Video)
  {
    sourceGui = SourceGui::ptr(new VideoGui(source));
    qSharedPointerCast<Video>(source)->setMuted(_audioMuted);
  }
  else if (sourceType == SourceType::Image)
    sourceGui = SourceGui::ptr(new ImageGui(source));
  else if (sourceType == SourceType::Color)
    sourceGui = SourceGui::ptr(new ColorGui(source));
  else if (sourceType == SourceType::Text)
    sourceGui = SourceGui::ptr(new TextGui(source));
  else if (sourceType == SourceType::Folder)
    sourceGui = SourceGui::ptr(new FolderGui(source));
#ifdef Q_OS_MAC
  else if (sourceType == SourceType::Syphon)
    sourceGui = SourceGui::ptr(new SyphonGui(source));
#endif
  else
    sourceGui = SourceGui::ptr(new SourceGui(source));

  // Add to list of source guis..
  sourceGuis[sourceId] = sourceGui;
  QWidget* sourceEditor = sourceGui->getPropertiesEditor();
  sourcePropertyPanel->addWidget(sourceEditor);
  sourcePropertyPanel->setCurrentWidget(sourceEditor);
  sourcePropertyPanel->setEnabled(true);

  // When source value is changed, update canvases.
  //  connect(sourceGui.get(), SIGNAL(valueChanged()),
  //          this,           SLOT(updateCanvases()));

  connect(sourceGui.data(), SIGNAL(valueChanged(Source::ptr)),
          this,            SLOT(handleSourceChanged(Source::ptr)));

  connect(source.data(), SIGNAL(propertyChanged(uid, QString, QVariant)),
          this,           SLOT(sourcePropertyChanged(uid, QString, QVariant)));

  // TODO: attention: if mapping is invisible canvases will be updated for no reason
  connect(source.data(), SIGNAL(propertyChanged(uid, QString, QVariant)),
          this,           SLOT(updateCanvases()));

  // A source property edit is a real project change.
  connect(source.data(), SIGNAL(propertyChanged(uid, QString, QVariant)),
          this,           SLOT(windowModified()));

#ifdef HAVE_SYPHON
  // Fit input shapes once a Syphon source's real resolution becomes known.
  // Queued so the shapes are not mutated mid-paint (the signal fires while
  // rendering the source).
  if (sourceType == SourceType::Syphon)
    connect(qSharedPointerCast<Syphon>(source).data(),
            SIGNAL(frameSizeKnown(int, int, int)),
            this, SLOT(autoFitSyphonInputShapes(int, int, int)),
            Qt::QueuedConnection);
#endif

  // Add source item to sourceList widget.
  QListWidgetItem* item = new QListWidgetItem(icon, name);
  setItemId(*item, sourceId); // TODO: could possibly be replaced by a Source pointer

  // Set size based on current view mode.
  int rowH = _sourceListThumbnailMode ? (_sourceListIconSize + 8) : 24;
  item->setSizeHint(QSize(item->sizeHint().width(), rowH));

  // Set tooltip.
  item->setToolTip(QString("ID: %1").arg(source->getId()));

  // Switch to source tab.
  contentTab->setCurrentWidget(sourceSplitter);

  // Insert under the appropriate section header.
  QListWidgetItem* sectionHeader = nullptr;
  if (source->getSourceType() == SourceType::Image) {
    int videosRow = sourceList->row(_sourceSectionVideos);
    sourceList->insertItem(videosRow, item);
    sectionHeader = _sourceSectionImages;
  } else if (source->getSourceType() == SourceType::Folder) {
    sourceList->addItem(item);
    sectionHeader = _sourceSectionFolders;
  } else if (source->getSourceType() == SourceType::Color ||
             source->getSourceType() == SourceType::Text) {
    int foldersRow = sourceList->row(_sourceSectionFolders);
    sourceList->insertItem(foldersRow, item);
    sectionHeader = _sourceSectionGenerated;
  } else {
    int generatedRow = sourceList->row(_sourceSectionGenerated);
    sourceList->insertItem(generatedRow, item);
    sectionHeader = _sourceSectionVideos;
  }
  // Respect collapsed state of the section.
  if (sectionHeader && _sectionCollapsed.value(sectionHeader, false))
    item->setHidden(true);
  sourceList->setCurrentItem(item);

  // Update mapping guis.
  updateMappers();

  // Window was modified.
  windowModified();

  // Update playing state.
  updatePlayingState();
}

void MainWindow::updateSourceItem(uid sourceId, const QIcon& icon, const QString& name) {
  QListWidgetItem* item = getItemFromId(*sourceList, sourceId);
  if (item == nullptr) {
    // FIXME there was an assert that seemed to make MapMap crash, here.
    return;
  }

  // Only the name reflects an actual project edit (e.g. a rename); the icon
  // also gets refreshed on every play/pause state change, which should not
  // mark the project as modified.
  bool nameChanged = (item->text() != name);

  // Update item info.
  item->setIcon(icon);
  item->setText(name);

  // Update mapping guis.
  updateMappers();

  // Window was modified.
  if (nameChanged)
    windowModified();
}

void MainWindow::addLayerItem(uid layerId)
{
  Layer::ptr layer = mappingManager->getLayerById(layerId);
  Q_CHECK_PTR(layer);

  QString defaultName;
  QIcon icon;

  ShapeType shapeType = layer->getShape()->getType();
  SourceType sourceType = layer->getSource()->getSourceType();

  // Add mapper.
  // XXX hardcoded for textures
  QSharedPointer<TextureLayer> textureLayer;
  if (sourceType == SourceType::Video || sourceType == SourceType::Image
      || sourceType == SourceType::Folder || sourceType == SourceType::Text
#ifdef Q_OS_MAC
      || sourceType == SourceType::Syphon
#endif
     )
  {
    textureLayer = qSharedPointerCast<TextureLayer>(layer);
    Q_CHECK_PTR(textureLayer);
  }

  LayerGui::ptr mapper;

  // XXX Branching on nVertices() is crap

  // Triangle
  if (shapeType == ShapeType::Triangle)
  {
    defaultName = QString("Triangle %1").arg(layerId);
    icon = MM::themedIcon(":/shape-triangle");

    if (sourceType == SourceType::Color)
      mapper = LayerGui::ptr(new PolygonColorLayerGui(layer));
    else
      mapper = LayerGui::ptr(new TriangleTextureLayerGui(textureLayer));
  }
  // Mesh
  else if (shapeType == ShapeType::Mesh)
  {
    defaultName = QString("Mesh %1").arg(layerId);
    icon = MM::themedIcon(":/shape-mesh");
    if (sourceType == SourceType::Color)
      mapper = LayerGui::ptr(new MeshColorLayerGui(layer));
    else
      mapper = LayerGui::ptr(new MeshTextureLayerGui(textureLayer));
  }
  else if (shapeType == ShapeType::Ellipse)
  {
    defaultName = QString("Ellipse %1").arg(layerId);
    icon = MM::themedIcon(":/shape-ellipse");
    if (sourceType == SourceType::Color)
      mapper = LayerGui::ptr(new EllipseColorLayerGui(layer));
    else
      mapper = LayerGui::ptr(new EllipseTextureLayerGui(textureLayer));
  }
  else if (shapeType == ShapeType::Polygon)
  {
    defaultName = QString("Polygon %1").arg(layerId);
    icon = MM::themedIcon(":/shape-polygon");
    if (sourceType == SourceType::Color)
      mapper = LayerGui::ptr(new PolygonColorLayerGui(layer));
    else
      mapper = LayerGui::ptr(new FreePolygonTextureLayerGui(textureLayer));
  }
  else
  {
    defaultName = QString("Polygon %1").arg(layerId);
    icon = QIcon(":/shape-polygon");
  }

  // Label is only going to be applied if no name is present.
  if (layer->getName().isEmpty())
    layer->setName(defaultName);

  // Add to list of layerGuis.
  layerGuis[layerId] = mapper;
  QWidget* mapperEditor = mapper->getPropertiesEditor();
  layerPropertyPanel->addWidget(mapperEditor);
  layerPropertyPanel->setCurrentWidget(mapperEditor);
  layerPropertyPanel->setEnabled(true);

  // When mapper value is changed, update canvases.
  connect(mapper.data(), SIGNAL(valueChanged()),
          this,          SLOT(updateCanvases()));

  // Shape/vertex edits go straight through the mapper, not Layer::propertyChanged.
  connect(mapper.data(), SIGNAL(valueChanged()),
          this,          SLOT(windowModified()));

  // Also update playing state in case source was changed.
  connect(mapper.data(), SIGNAL(sourceChanged()),
          this,          SLOT(updatePlayingState()));

  connect(sourceCanvas,  SIGNAL(shapeChanged(MShape*)),
          mapper.data(), SLOT(updateShape(MShape*)));

  connect(destinationCanvas, SIGNAL(shapeChanged(MShape*)),
          mapper.data(),     SLOT(updateShape(MShape*)));

  connect(layer.data(), SIGNAL(propertyChanged(uid, QString, QVariant)),
          this,           SLOT(layerPropertyChanged(uid, QString, QVariant)));

  // TODO: attention: if mapping is invisible canvases will be updated for no reason
  connect(layer.data(), SIGNAL(propertyChanged(uid, QString, QVariant)),
          this,           SLOT(updateCanvases()));

  // A layer property edit (opacity, blend, source, visibility, etc.) is a
  // real project change.
  connect(layer.data(), SIGNAL(propertyChanged(uid, QString, QVariant)),
          this,           SLOT(windowModified()));

  // Switch to mapping tab.
  contentTab->setCurrentWidget(layerSplitter);

  // Add item to layerList widget.
  layerListModel->addItem(layer, icon, layer->getName());
  layerListModel->updateModel();
  setCurrentLayer(layerId);

  // Add items to scenes.
  if (mapper->getInputGraphicsItem())
    sourceCanvas->scene()->addItem(mapper->getInputGraphicsItem().data());
  if (mapper->getGraphicsItem())
    destinationCanvas->scene()->addItem(mapper->getGraphicsItem().data());

  // Window was modified.
  windowModified();

  // Update playing state.
  updatePlayingState();

  // Refresh usage badges on source thumbnails.
  refreshSourceBadges();
}

void MainWindow::removeLayerItem(uid layerId)
{
  Layer::ptr layer = mappingManager->getLayerById(layerId);
  Q_CHECK_PTR(layer);

  // Remove mapping from model.
  mappingManager->removeLayer(layerId);

  // Remove associated mapper.
  layerPropertyPanel->removeWidget(layerGuis[layerId]->getPropertiesEditor());
  layerGuis.remove(layerId);

  // Remove widget from layerList.
  int row = layerListModel->getItemRowFromId(layerId);
  Q_ASSERT( row >= 0 );
  layerListModel->removeItem(row);

  // Update list.
  layerListModel->updateModel();

  if (layerListModel->rowCount() == 0)
    removeCurrentLayer();
  else
  {
    int nextSelectedRow = row == layerListModel->rowCount() ? row - 1 : row;
    QModelIndex index = layerListModel->getIndexFromRow(nextSelectedRow);
    layerList->selectionModel()->select(index, QItemSelectionModel::Select);
    layerList->setCurrentIndex(index);
  }

  // Update everything.
  updateCanvases();

  // Window was modified.
  windowModified();

  // Update playing state.
  updatePlayingState();

  // Refresh usage badges on source thumbnails.
  refreshSourceBadges();
}

void MainWindow::moveLayerItem(uid layerId, int idx)
{
  Layer::ptr layer = mappingManager->getLayerById(layerId);
  Q_CHECK_PTR(layer);

  // Remove mapping from model.
  mappingManager->moveLayer(layerId, idx);

  // Remove widget from layerList.
  int row = layerListModel->getItemRowFromId(layerId);
  Q_ASSERT( row >= 0 );
  layerListModel->moveItem(row, idx);

  // Update list.
  layerListModel->updateModel();

  QModelIndex index = layerListModel->getIndexFromRow(idx);
  layerList->selectionModel()->select(index, QItemSelectionModel::Select);
  layerList->setCurrentIndex(index);

  // Update everything.
  updateCanvases();

  // Window was modified.
  windowModified();

  // Update playing state.
  updatePlayingState();
}

void MainWindow::removeSourceItem(uid sourceId)
{
  Source::ptr source = mappingManager->getSourceById(sourceId);
  Q_CHECK_PTR(source);

  // Remove all mappings associated with source.
  QMap<uid, Layer::ptr> sourceLayers = mappingManager->getSourceLayers(source);
  for (QMap<uid, Layer::ptr>::const_iterator it = sourceLayers.constBegin();
       it != sourceLayers.constEnd(); ++it) {
    removeLayerItem(it.key());
  }
  // Remove source from model. Call unconditionally (not inside Q_ASSERT, which
  // is compiled out in release builds and would skip the removal + the source's
  // resource release).
  bool sourceRemoved = mappingManager->removeSource(sourceId);
  Q_ASSERT(sourceRemoved);
  Q_UNUSED(sourceRemoved);

  // Remove associated mapper.
  sourcePropertyPanel->removeWidget(sourceGuis[sourceId]->getPropertiesEditor());
  sourceGuis.remove(sourceId);

  updateMappers();

  // Remove widget from sourceList.
  int row = getItemRowFromId(*sourceList, sourceId);
  Q_ASSERT( row >= 0 );
  QListWidgetItem* item = sourceList->takeItem(row);
  if (item == currentSelectedItem)
    currentSelectedItem = nullptr;
  delete item;

  // Update list.
  sourceList->update();

  // Reset current source.
  removeCurrentSource();

  // Update everything.
  updateCanvases();

  // Window was modified.
  windowModified();
  // Build mapping!
  // FIXME: layer->build(); // I removed this 2014-04-25

  // Update playing state.
  updatePlayingState();
}

void MainWindow::clearWindow()
{
  clearProject();
}

void MainWindow::syncLayerManager()
{
  // Reorder mappings.
  QVector<uid> newOrder;
  for (int row=0; row<layerListModel->rowCount(); row++)
//  for (int row=layerListModel->rowCount()-1; row>=0; row--)
  {
    uid layerId = layerListModel->getIndexFromRow(row).data(Qt::UserRole).toInt();
    newOrder.push_back(layerId);
  }
  mappingManager->reorderLayers(newOrder);
}

bool MainWindow::fileExists(const QString &file)
{
  QFileInfo checkFile(file);
  if (checkFile.exists() && checkFile.isFile())
    return true;

  // Also try resolving relative paths against the current project directory.
  if (!checkFile.isAbsolute() && !curFile.isEmpty()) {
    QFileInfo candidate(QFileInfo(curFile).absoluteDir(), file);
    if (candidate.exists() && candidate.isFile())
      return true;
  }

  return false;
}

bool MainWindow::fileSupported(const QString &file, bool isImage)
{
  QFileInfo fileInfo(file);
  QString fileExtension = fileInfo.suffix();

  if (isImage) {
    if (MM::IMAGE_FILES_FILTER.contains(fileExtension, Qt::CaseInsensitive) &&
        QImageReader(file).canRead())
      return true;
  } else {
    if (MM::VIDEO_FILES_FILTER.contains(fileExtension, Qt::CaseInsensitive))
      return true;
  }

  QMessageBox::warning(this, tr("Warning"),
                       tr("The following file is not supported: %1")
                       .arg(fileInfo.fileName()));
  return false;
}

bool MainWindow::fileSupported(const QString &file, const QString &extension)
{
  if (!QFileInfo(file).suffix().isEmpty() &&
      extension.contains(QFileInfo(file).suffix(), Qt::CaseInsensitive)) {
    return true;
  } else {
    return false;
  }
}

QString MainWindow::locateMediaFile(const QString &uri, bool isImage)
{
  // Try resolving relative URIs against the project file's directory first.
  QFileInfo file(uri);
  if (!file.isAbsolute() && !curFile.isEmpty()) {
    QFileInfo candidate(QFileInfo(curFile).absoluteDir(), uri);
    if (candidate.exists())
      return candidate.absoluteFilePath();
  }
  if (file.exists())
    return file.absoluteFilePath();

  // The name of the file
  QString filename = file.fileName();
  // Handle the case where it is video or image
  QString mediaFilter = isImage ? MM::IMAGE_FILES_FILTER : MM::VIDEO_FILES_FILTER;
  QString mediaType = isImage ? "Images" : "Videos";

  // Show a warning and offer to locate the file
  QMessageBox::warning(this,
                       tr("Cannot load movie"),
                       tr("Unable to use file %1.\n"
                          "The original file is not found. Please locate.")
                       .arg(filename));

  // Set the new uri
#ifdef Q_OS_LINUX
 QString newUri = QFileDialog::getOpenFileName(this,
                                               tr("Locate file %1").arg(filename),
                                               file.absolutePath(),
                                               tr("%1 files (%2)")
                                               .arg(mediaType)
                                               .arg(mediaFilter),
                                               nullptr, QFileDialog::DontUseNativeDialog);
#else
  QString newUri = QFileDialog::getOpenFileName(this,
                                                tr("Locate file %1").arg(filename),
                                                file.absolutePath(),
                                                tr("%1 files (%2)")
                                                .arg(mediaType)
                                                .arg(mediaFilter));
#endif

  return newUri;

}

MainWindow* MainWindow::window() {
  static MainWindow* instance = nullptr;
  if (!instance) {
    instance = new MainWindow;
  }
  return instance;
}

void MainWindow::updateCanvases()
{
  // Update scenes.
  sourceCanvas->scene()->update();
  destinationCanvas->scene()->update();

  // Update canvases.
  sourceCanvas->update();
  destinationCanvas->update();
  outputWindow->getCanvas()->update();

  // Update statut bar
  updateStatusBar();
}

void MainWindow::updateMappers() {
  // Update mapping guis.
  for (QMap<uid, LayerGui::ptr>::iterator it = layerGuis.begin();
       it != layerGuis.end(); ++it) {
    it.value()->updateSources();
  }
}

void MainWindow::processFrame()
{
  // Number of frames processed (restarted every second).
  static unsigned int nFrames = 0;

  // Update canvases.
  updateCanvases();

  // Update recording: drive output canvas repaint so framePainted fires each tick.
  if (_videoExporter && _videoExporter->isRecording()) {
    outputWindow->getCanvas()->update();

    qint64 ms = _videoExporter->duration();
    if (_recordingTotalMs > 0 && ms >= _recordingTotalMs) {
      recordAction->setChecked(false);
      _videoExporter->stop();
    } else {
      auto fmtMs = [](qint64 t) {
        return QString("%1:%2")
          .arg(t / 60000, 2, 10, QChar('0'))
          .arg((t % 60000) / 1000, 2, 10, QChar('0'));
      };
      QString label = _recordingTotalMs > 0
        ? tr("● REC  %1 / %2").arg(fmtMs(ms)).arg(fmtMs(_recordingTotalMs))
        : tr("● REC  %1").arg(fmtMs(ms));
      recordingTimerLabel->setText(label);
    }
  }

  // Update true FPS.
  nFrames++;
  if (nFrames > framesPerSecond())
  {
    // This is the real time needed to process one second.
    qreal trueFramesPerSecond = qreal(nFrames) / qreal(systemTimer->restart()) * 1000.0;
    trueFramesPerSecondsLabel->setText(
        "FPS: " + QString::number(trueFramesPerSecond, 'f', 2) + " / " +
        QString::number(framesPerSecond()  , 'f', 2));
    nFrames = 0;
  }
}

void MainWindow::toggleRecording(bool on)
{
  if (!_videoExporter) {
    statusBar()->showMessage(tr("Video recorder still initializing, try again."), 3000);
    recordAction->setChecked(false);
    return;
  }
  if (on) {
    QSettings s;
    auto format  = (VideoExporter::Format)  s.value("videoFormat",  (int)VideoExporter::H264_MP4).toInt();
    auto quality = (VideoExporter::Quality) s.value("videoQuality", (int)VideoExporter::HighQuality).toInt();

    QString filter = VideoExporter::formatFilter(format);
    QString ext    = VideoExporter::formatExtension(format);
    QString lastDir = s.value("lastRecordingDir", QString()).toString();

    // Default filename: project name (without extension), or "recording" if unsaved.
    QString defaultName;
    if (!curFile.isEmpty())
      defaultName = QFileInfo(curFile).completeBaseName();
    else
      defaultName = tr("recording");
    QString defaultPath = lastDir.isEmpty()
        ? defaultName + "." + ext
        : QDir(lastDir).filePath(defaultName + "." + ext);

    QString path = QFileDialog::getSaveFileName(
        this, tr("Save Recording As"), defaultPath, filter);

    if (path.isEmpty()) {
      recordAction->setChecked(false);
      return;
    }

    // Ensure correct extension.
    if (!path.endsWith("." + ext, Qt::CaseInsensitive))
      path += "." + ext;

    s.setValue("lastRecordingDir", QFileInfo(path).absolutePath());

    // Rewind all video sources, force loop, find longest duration.
    _recordingTotalMs = 0;
    _savedLoopStates.clear();
    for (int i = 0; i < mappingManager->nSources(); i++) {
      Source::ptr src = mappingManager->getSource(i);
      if (src->getSourceType() == SourceType::Video) {
        Video* vid = static_cast<Video*>(src.get());
        _savedLoopStates[src->getId()] = vid->getPlayInLoop();
        vid->setPlayInLoop(true);
        vid->rewind();
        qint64 dur = vid->getDuration();
        if (dur > _recordingTotalMs)
          _recordingTotalMs = dur;
      }
    }

    // QScreen::grabWindow only captures pixels that are composited to the display.
    // A windowed output that is hidden or occluded returns black frames. Go fullscreen
    // when recording starts so the compositor always has real content to grab.
    _recordingOpenedOutputWindow = !outputFullScreenAction->isChecked();
    if (_recordingOpenedOutputWindow) {
      _recordingHadControls = displayControlsAction->isChecked();
      startFullScreen();
      // Allow the GL context and compositor to settle before the first frame grab.
      QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    }

    QSize size = outputWindow->getCanvas()->viewport()->size();
    if (!_videoExporter->start(path, format, quality, size, framesPerSecond())) {
      recordAction->setChecked(false);
      statusBar()->showMessage(tr("Failed to start recording."), 4000);
      // Restore loop states since recording failed to start.
      for (int i = 0; i < mappingManager->nSources(); i++) {
        Source::ptr src = mappingManager->getSource(i);
        if (src->getSourceType() == SourceType::Video) {
          Video* vid = static_cast<Video*>(src.get());
          if (_savedLoopStates.contains(src->getId()))
            vid->setPlayInLoop(_savedLoopStates.value(src->getId()));
        }
      }
      _savedLoopStates.clear();
      _recordingTotalMs = 0;
      if (_recordingOpenedOutputWindow) {
        exitFullScreen();
        _recordingOpenedOutputWindow = false;
      }
    } else {
      // Enable frame grab: main update loop drives canvas->update() each tick,
      // paintEvent captures the frame and emits framePainted → sendFrame.
      OutputGLCanvas* canvas = outputWindow->getCanvas();
      canvas->setFrameGrabEnabled(true);
      connect(canvas, &OutputGLCanvas::framePainted,
              _videoExporter, &VideoExporter::sendFrame);

      recordingTimerLabel->setText("● REC  00:00 / --:--");
      recordingTimerLabel->show();

      QString audioMsg = _videoExporter->audioDeviceName().isEmpty()
          ? tr("No loopback audio device — recording video only. "
               "Enable Stereo Mix in Windows Sound settings to capture audio.")
          : tr("Recording audio from: %1").arg(_videoExporter->audioDeviceName());
      statusBar()->showMessage(audioMsg, 8000);
    }
  } else {
    if (_videoExporter)
      _videoExporter->stop();
  }
}

void MainWindow::onRecordingStopped(const QString& filePath)
{
  // Disconnect frame capture signals before hiding the window.
  OutputGLCanvas* canvas = outputWindow->getCanvas();
  canvas->setFrameGrabEnabled(false);
  disconnect(canvas, &OutputGLCanvas::framePainted,
             _videoExporter, &VideoExporter::sendFrame);

  if (_recordingOpenedOutputWindow) {
    outputWindow->hide();
    displayControlsAction->setChecked(_recordingHadControls);
    _recordingOpenedOutputWindow = false;
  }
  recordAction->setChecked(false);
  recordingTimerLabel->hide();
  statusBar()->showMessage(tr("Recording saved: %1").arg(filePath), 6000);

  // Restore each video's original loop setting.
  for (int i = 0; i < mappingManager->nSources(); i++) {
    Source::ptr src = mappingManager->getSource(i);
    if (src->getSourceType() == SourceType::Video) {
      Video* vid = static_cast<Video*>(src.get());
      if (_savedLoopStates.contains(src->getId()))
        vid->setPlayInLoop(_savedLoopStates.value(src->getId()));
    }
  }
  _savedLoopStates.clear();
  _recordingTotalMs = 0;
}

void MainWindow::updatePlayingState()
{
  // Pause all sources that are not visible.
  if (isPlaying())
  {
    QVector<Source::ptr> visibleSources = mappingManager->getVisibleSources();
    for (int i=0; i<mappingManager->nSources(); i++)
    {
      Source::ptr source = mappingManager->getSource(i);
      if (visibleSources.contains(source))
      {
        source->play();
      }
      else
      {
        source->pause();
      }
    }
  }

  // Pause everyone.
  else
  {
    for (int i=0; i<mappingManager->nSources(); i++)
    {
      mappingManager->getSource(i)->pause();
    }
  }

  // Update all source items with correct icon according to playing state.
  for (int i=0; i<mappingManager->nSources(); i++)
  {
    Source::ptr source = mappingManager->getSource(i);
    updateSourceItem(source->getId(), getSourceIcon(source), source->getName());
  }

}

void MainWindow::enableDisplayControls(bool display)
{
  _displayControls = display;
  updateCanvases();
}

void MainWindow::setFramesPerSecond(qreal fps)
{
  _framesPerSecond = qMax(fps, 0.0);
  videoTimer->setInterval( int( 1000 / _framesPerSecond ) );
}

void MainWindow::enableDisplaySourceControls(bool display)
{
  _displaySourceControls = display;
  updateCanvases();
}

void MainWindow::displayUndoHistory(bool display)
{
  _displayUndoStack = display;

  // Create undo view.
  undoView = new QUndoView(getUndoStack(), this);

  if (display) {
    contentTab->addTab(undoView, tr("Undo history"));
  } else {
    contentTab->removeTab(2);
  }
}

void MainWindow::enableStickyVertices(bool value)
{
  _stickyVertices = value;
  settings.setValue("stickyVertices", _stickyVertices);
}

void MainWindow::toggleMuteAll(bool muted)
{
  _audioMuted = muted;
  settings.setValue("audioMuted", _audioMuted);

  for (int i = 0; i < mappingManager->nSources(); i++)
  {
    Source::ptr source = mappingManager->getSource(i);
    if (source->getSourceType() == SourceType::Video)
      qSharedPointerCast<Video>(source)->setMuted(muted);
  }
}

void MainWindow::showLayerContextMenu(const QPoint &point)
{
  QWidget *objectSender = static_cast<QWidget*>(sender());
  uid layerId = currentLayerItemId();
  Layer::ptr layer = mappingManager->getLayerById(layerId);

  if (!layer) {
    clearPendingPolygonEdit();
    return;
  }

  // Switch to right action check state
  layerLockedAction->setChecked(layer->isLocked());
  layerHideAction->setChecked(!layer->isVisible());
  layerSoloAction->setChecked(layer->isSolo());

  // Show vertex-edit actions only when a polygon edge/vertex was right-clicked.
  addPolygonVertexAction->setVisible(_polyEditType == PolyEditAdd);
  deletePolygonVertexAction->setVisible(_polyEditType == PolyEditDelete);

  // Show "Reset Input Mesh to Source Dimensions" only when right-clicking in the
  // source canvas and the current layer has a Mesh input shape.
  bool fromSourceCanvas = (sender() == sourceCanvas);
  bool hasMeshInput = layer->hasInputShape() &&
                      layer->getInputShape()->getType() == MShape::ShapeType::Mesh;
  _resetMeshInputAction->setVisible(fromSourceCanvas && hasMeshInput);

  if (objectSender != nullptr) {
    if (sender() == layerItemDelegate) // XXX: The item delegate is not a widget
      layerContextMenu->exec(layerList->mapToGlobal(point));
    else
      layerContextMenu->exec(objectSender->mapToGlobal(point));
  }

  clearPendingPolygonEdit();
}

void MainWindow::showSourceContextMenu(const QPoint &point)
{
  QWidget *objectSender = dynamic_cast<QWidget*>(sender());

  if (objectSender != nullptr && sourceList->count() > 0)
    sourceContextMenu->exec(objectSender->mapToGlobal(point));
}

void MainWindow::play(bool updatePlayPauseActions)
{
  if (updatePlayPauseActions)
    playAction->setChecked(true);
  _isPlaying = true;
  updatePlayingState();
}

void MainWindow::pause(bool updatePlayPauseActions)
{
  if (updatePlayPauseActions)
    playAction->setChecked(false);
  _isPlaying = false;
  updatePlayingState();
}

void MainWindow::rewind()
{
  // Rewind all paints.
  for (int i=0; i<mappingManager->nSources(); i++)
    mappingManager->getSource(i)->rewind();
}

QString MainWindow::strippedName(const QString &fullFileName)
{
  return QFileInfo(fullFileName).fileName();
}

const QIcon MainWindow::getSourceIcon(Source::ptr source)
{
  if (source->isPlaying())
    return source->getIcon();
  else
  {
    QPixmap pixmap = source->getIcon().pixmap(MM::SOURCE_THUMBNAIL_SIZE, MM::SOURCE_THUMBNAIL_SIZE);
    QPainter painter(&pixmap);
    painter.setPen(QPen(QColor(255, 0, 0, 180), 6));
    painter.drawLine(0, 0, pixmap.width(), pixmap.height());
    return QIcon(pixmap);
  }
}

static QIcon overlayUsageBadge(const QIcon& base, int count, int size)
{
  QPixmap pm = base.pixmap(size, size);
  if (count <= 0)
    return QIcon(pm);

  QPainter p(&pm);
  p.setRenderHint(QPainter::Antialiasing);

  QFont f = p.font();
  f.setBold(true);
  f.setPixelSize(qMax(9, size / 5));
  p.setFont(f);

  QFontMetrics fm(f);
  const QString text = QString::number(count);
  int tw = fm.horizontalAdvance(text);
  int th = fm.height();
  int badgeW = qMax(tw + 6, th + 4);
  int badgeH = th + 4;
  int bx = pm.width() - badgeW - 2;
  int by = 2;

  p.setPen(Qt::NoPen);
  p.setBrush(QColor(30, 30, 30, 210));
  p.drawRoundedRect(bx, by, badgeW, badgeH, badgeH / 2, badgeH / 2);

  p.setPen(Qt::white);
  p.drawText(QRect(bx, by, badgeW, badgeH), Qt::AlignCenter, text);
  p.end();

  return QIcon(pm);
}

void MainWindow::refreshSourceBadges()
{
  for (int i = 0; i < sourceList->count(); ++i) {
    QListWidgetItem* item = sourceList->item(i);
    if (!item || item->data(Qt::UserRole).isNull()) continue;
    uid id = getItemId(*item);
    if (id == 0) continue;
    Source::ptr source = mappingManager->getSourceById(id);
    if (!source) continue;
    int count = mappingManager->getSourceLayersById(id).size();
    item->setIcon(overlayUsageBadge(getSourceIcon(source), count, MM::SOURCE_THUMBNAIL_SIZE));
  }
}

void MainWindow::connectProjectWidgets()
{
  connect(sourceList, SIGNAL(itemSelectionChanged()),
          this,      SLOT(handleSourceItemSelectionChanged()));

  connect(sourceList, SIGNAL(itemPressed(QListWidgetItem*)),
          this,      SLOT(handleSourceItemSelected(QListWidgetItem*)));

  connect(sourceList, SIGNAL(itemActivated(QListWidgetItem*)),
          this,      SLOT(handleSourceItemSelected(QListWidgetItem*)));
  // Rename Source with double click
  connect(sourceList, SIGNAL(itemDoubleClicked(QListWidgetItem*)),
          this,      SLOT(renameSourceItem()));
  // When finish to edit mapping item
  connect(sourceList->itemDelegate(), SIGNAL(commitData(QWidget*)),
          this, SLOT(sourceListEditEnd(QWidget*)));

  connect(layerList->selectionModel(), SIGNAL(currentRowChanged(QModelIndex,QModelIndex)),
          this,        SLOT(handleLayerItemSelectionChanged(QModelIndex)));

  connect(layerListModel, SIGNAL(dataChanged(QModelIndex,QModelIndex)),
          this,        SLOT(handleLayerItemChanged(QModelIndex)));

  connect(layerListModel, SIGNAL(rowsMoved(QModelIndex,int,int,QModelIndex,int)),
          this,                 SLOT(handleLayerIndexesMoved()));

  connect(layerItemDelegate, SIGNAL(itemDuplicated(uid)),
          this, SLOT(duplicateLayer(uid)));

  connect(layerItemDelegate, SIGNAL(itemRemoved(uid)),
          this, SLOT(deleteLayer(uid)));

  connect(_preferenceDialog, SIGNAL(settingSaved()), this, SLOT(updateSettings()));
}

void MainWindow::disconnectProjectWidgets()
{
  disconnect(sourceList, SIGNAL(itemSelectionChanged()),
             this,      SLOT(handleSourceItemSelectionChanged()));

  disconnect(sourceList, SIGNAL(itemPressed(QListWidgetItem*)),
             this,      SLOT(handleSourceItemSelected(QListWidgetItem*)));

  disconnect(sourceList, SIGNAL(itemActivated(QListWidgetItem*)),
             this,      SLOT(handleSourceItemSelected(QListWidgetItem*)));

  disconnect(layerList->selectionModel(), SIGNAL(currentRowChanged(QModelIndex,QModelIndex)),
          this,        SLOT(handleLayerItemSelectionChanged(QModelIndex)));

  disconnect(layerListModel, SIGNAL(dataChanged(QModelIndex,QModelIndex)),
          this,        SLOT(handleLayerItemChanged(QModelIndex)));

  disconnect(layerListModel, SIGNAL(rowsMoved(QModelIndex,int,int,QModelIndex,int)),
          this,                 SLOT(handleLayerIndexesMoved()));

  disconnect(layerItemDelegate, SIGNAL(itemDuplicated(uid)),
          this, SLOT(duplicateLayer(uid)));

  disconnect(layerItemDelegate, SIGNAL(itemRemoved(uid)),
          this, SLOT(deleteLayer(uid)));

  disconnect(_preferenceDialog, SIGNAL(settingSaved()), this, SLOT(updateSettings()));
}

uid MainWindow::getItemId(const QListWidgetItem& item)
{
  return item.data(Qt::UserRole).toInt();
}

void MainWindow::setItemId(QListWidgetItem& item, uid id)
{
  item.setData(Qt::UserRole, id);
}

QListWidgetItem* MainWindow::getItemFromId(const QListWidget& list, uid id) {
  int row = getItemRowFromId(list, id);
  if (row >= 0)
    return list.item( row );
  else
    return nullptr;
}

int MainWindow::getItemRowFromId(const QListWidget& list, uid id)
{
  for (int row=0; row<list.count(); row++)
  {
    QListWidgetItem* item = list.item(row);
    if (getItemId(*item) == id)
      return row;
  }

  return (-1);
}

uid MainWindow::currentLayerItemId() const
{
  return layerListModel->getItemId(currentSelectedIndex);
}

QIcon MainWindow::createColorIcon(const QColor &color) {
  QPixmap pixmap(100,100);
  pixmap.fill(color);
  return QIcon(pixmap);
}

QIcon MainWindow::createFileIcon(const QString& filename) {
  static QFileIconProvider provider;
  return provider.icon(QFileInfo(filename));
}

QIcon MainWindow::createImageIcon(const QString& filename) {
  return QIcon(filename);
}


void MainWindow::setCurrentSource(int uid)
{
  if (uid == NULL_UID)
    removeCurrentSource();
  else {
    if (currentSourceId != uid) {
      currentSourceId = uid;
      sourceList->setCurrentRow( getItemRowFromId(*sourceList, uid) );
      sourcePropertyPanel->setCurrentWidget(sourceGuis[uid]->getPropertiesEditor());
    }
    _hasCurrentSource = true;
    updateSourcePreview(uid);
  }
}

void MainWindow::setCurrentLayer(int uid)
{
  if (uid == NULL_UID)
    removeCurrentLayer();
  else {
    if (currentLayerId != uid) {
      currentLayerId = uid;
      currentSelectedIndex = layerListModel->getIndexFromRow(layerListModel->getItemRowFromId(uid));
      layerList->setCurrentIndex(currentSelectedIndex);
      layerPropertyPanel->setCurrentWidget(layerGuis[uid]->getPropertiesEditor());
    }
    _hasCurrentLayer = true;
  }
}

void MainWindow::removeCurrentSource() {
  _hasCurrentSource = false;
  currentSourceId = NULL_UID;
  sourceList->clearSelection();
  if (_sourcePreviewLabel)
    static_cast<SourcePreviewLabel*>(_sourcePreviewLabel)->clearAll();
}

void MainWindow::removeCurrentLayer() {
  _hasCurrentLayer = false;
  currentLayerId = NULL_UID;
  layerList->clearSelection();
}

void MainWindow::updateSourcePreview(uid sourceId)
{
  if (!_sourcePreviewLabel)
    return;
  auto* lbl = static_cast<SourcePreviewLabel*>(_sourcePreviewLabel);
  Source::ptr source = mappingManager->getSourceById(sourceId);
  if (source.isNull()) {
    lbl->clearAll();
    return;
  }

  // For video sources try the animated frame cache first.
  if (source->getSourceType() == Source::SourceType::Video && _thumbnailCache) {
    QSharedPointer<Video> vid = qSharedPointerCast<Video>(source);
    if (!vid->getUri().isEmpty()) {
      QStringList frames = ThumbnailCache::cachedFrames(vid->getUri(), thumbnailCacheDir());
      if (!frames.isEmpty()) {
        QVector<QPixmap> pixmaps;
        pixmaps.reserve(frames.size());
        for (const QString& f : frames) {
          QPixmap px(f);
          if (!px.isNull()) pixmaps << px;
        }
        int fps = qMax(1, qRound(4.0 * vid->getRate()));
        lbl->setAnimationFrames(pixmaps, fps);
        return;
      }
      // Not yet cached — request generation and show static fallback.
      _thumbnailCache->request(vid->getUri(), thumbnailCacheDir());
    }
  }

  // Static fallback (images, folders, colour, or uncached video).
  int w = _sourcePreviewLabel->width();
  int h = _sourcePreviewLabel->height();
  if (w < 10) w = 320;
  if (h < 10) h = 200;
  lbl->setSourcePixmap(source->getPreviewPixmap(w, h));
}

void MainWindow::onThumbnailReady(const QString& videoPath, const QStringList& frames)
{
  if (!_sourcePreviewLabel || currentSourceId == NULL_UID)
    return;
  Source::ptr source = mappingManager->getSourceById(currentSourceId);
  if (source.isNull() || source->getSourceType() != Source::SourceType::Video)
    return;
  auto vid = qSharedPointerCast<Video>(source);
  if (vid->getUri() != videoPath)
    return;

  QVector<QPixmap> pixmaps;
  pixmaps.reserve(frames.size());
  for (const QString& f : frames) {
    QPixmap px(f);
    if (!px.isNull()) pixmaps << px;
  }
  int fps = qMax(1, qRound(4.0 * vid->getRate()));
  static_cast<SourcePreviewLabel*>(_sourcePreviewLabel)->setAnimationFrames(pixmaps, fps);
}

void MainWindow::updatePreviewAnimSpeed()
{
  if (!_sourcePreviewLabel || currentSourceId == NULL_UID)
    return;
  Source::ptr source = mappingManager->getSourceById(currentSourceId);
  if (source.isNull() || source->getSourceType() != Source::SourceType::Video)
    return;
  auto vid = qSharedPointerCast<Video>(source);
  int fps = qMax(1, qRound(4.0 * vid->getRate()));
  static_cast<SourcePreviewLabel*>(_sourcePreviewLabel)->setAnimationFps(fps);
}

QString MainWindow::thumbnailCacheDir() const
{
  if (!curFile.isEmpty())
    return QFileInfo(curFile).absoluteDir().filePath(".thumbnails");
  return QStandardPaths::writableLocation(QStandardPaths::TempLocation)
         + "/MyMapMap/thumbnails";
}

void MainWindow::startOscReceiver()
{
  std::ostringstream os;
  os << oscListeningPort;
#if QT_VERSION >= 0x050500
  QMessageLogger(__FILE__, __LINE__, 0).info() << "OSC port: " << oscListeningPort;
#else
  QMessageLogger(__FILE__, __LINE__, 0).debug() << "OSC port: " << oscListeningPort;
#endif
  osc_interface.reset(new OscInterface(oscListeningPort));
  if (oscListeningPort != 0)
  {
    osc_interface->start();
  }
  osc_timer = new QTimer(this); // FIXME: memleak?
  connect(osc_timer, SIGNAL(timeout()), this, SLOT(pollOscInterface()));
  osc_timer->start();
}

bool MainWindow::setOscPort(int port)
{
  if (port <= 1023 || port > 65535)
  {
    qWarning() << "OSC port is out of range: " << port << Qt::endl;
    return false;
  }
  oscListeningPort = port;
  startOscReceiver();
  return true;
}

int MainWindow::getOscPort() const
{
  return oscListeningPort;
}

void MainWindow::setVerbose(bool verbose)
{
  if (osc_interface)
    osc_interface->setVerbose(verbose);
}

bool MainWindow::setOscPort(QString portNumber)
{
  bool ok;
  int port = portNumber.toInt(&ok);
  if (ok)
  {
    return setOscPort(port);
  }
  else
  {
    qWarning() << "OSC port is not a number: " << portNumber << Qt::endl;
    return false;
  }
  return true;
}

#ifdef HAVE_MCP
void MainWindow::startMcpServer()
{
  if (mcp_server.isNull())
    mcp_server.reset(new McpServer(this));

  if (mcpListeningPort == 0)
  {
    QMessageLogger(__FILE__, __LINE__, 0).info() << "MCP server disabled (port 0).";
    return;
  }

  quint16 boundPort = mcp_server->start(static_cast<quint16>(mcpListeningPort));
  if (boundPort != 0)
    QMessageLogger(__FILE__, __LINE__, 0).info()
      << "MCP server listening on http://localhost:" << boundPort << "/mcp";
  else
    qWarning() << "MCP server could not start on port" << mcpListeningPort;
}

bool MainWindow::setMcpPort(int port)
{
  if (port != 0 && (port <= 1023 || port > 65535))
  {
    qWarning() << "MCP port is out of range: " << port << Qt::endl;
    return false;
  }
  mcpListeningPort = port;
  startMcpServer();
  return true;
}

int MainWindow::getMcpPort() const
{
  return mcpListeningPort;
}

bool MainWindow::setMcpPort(QString portNumber)
{
  bool ok;
  int port = portNumber.toInt(&ok);
  if (ok)
  {
    return setMcpPort(port);
  }
  else
  {
    qWarning() << "MCP port is not a number: " << portNumber << Qt::endl;
    return false;
  }
}
#endif // HAVE_MCP

void MainWindow::pollOscInterface()
{
    // FIXME: we should now use its QObject signals instead of polling it
  osc_interface->consume_commands(*this);
}

void MainWindow::exitFullScreen()
{
  outputFullScreenAction->setChecked(false);
  displayTestSignalAction->setChecked(false);
}

void MainWindow::openShortcutWindow()
{
  _shortcutWindow->reload(); // Important for speed
  _shortcutWindow->setVisible(true);
}

void MainWindow::updateSettings()
{
  stickyVerticesAction->setChecked(settings.value("stickyVertices").toBool());
}

void MainWindow::updateLayerListColumnWidth()
{
  layerList->setColumnWidth(1, layerList->horizontalHeader()->width() - (MM::MAPPING_LIST_HIDE_COLUMN + MM::MAPPING_LIST_BUTTONS_COLUMN));
}

// void MainWindow::applyOscCommand(const QVariantList& command)
// {
//   bool VERBOSE = true;
//   if (VERBOSE)
//   {
//     std::cout << "Receive OSC: ";
//     for (int i = 0; i < command.size(); ++i)
//     {
//       if (command.at(i).type()  == QVariant::Int)
//       {
//         std::cout << command.at(i).toInt() << " ";
//       }
//       else if (command.at(i).type()  == QVariant::Double)
//       {
//         std::cout << command.at(i).toDouble() << " ";
//       }
//       else if (command.at(i).type()  == QVariant::String)
//       {
//         std::cout << command.at(i).toString().toStdString() << " ";
//       }
//       else
//       {
//         std::cout << "??? ";
//       }
//     }
//     std::cout << std::endl;
//     std::cout.flush();
//   }
//
//   if (command.size() < 2)
//       return;
//   if (command.at(0).type() != QVariant::String)
//       return;
//   if (command.at(1).type() != QVariant::String)
//       return;
//   std::string path = command.at(0).toString().toStdString();
//   std::string typetags = command.at(1).toString().toStdString();
//
//   // Handle all OSC messages here
//   if (path == "/image/uri" && typetags == "s")
//   {
//       std::string image_uri = command.at(2).toString().toStdString();
//       std::cout << "TODO load /image/uri " << image_uri << std::endl;
//   }
//   else if (path == "/add/quad")
//       addMesh();
//   else if (path == "/add/triangle")
//       addTriangle();
//   else if (path == "/add/ellipse")
//       addEllipse();
//   else if (path == "/project/save")
//       save();
//   else if (path == "/project/open")
//       open();
// }

void MainWindow::quitMapMap()
{
  close();
}

QIcon MainWindow::themedIcon(const QString& resource)
{
  return MM::themedIcon(resource);
}

void MainWindow::refreshIcons()
{
  struct { QAction* action; const char* resource; } entries[] = {
    { newAction,                  ":/new"               },
    { openAction,                 ":/open"              },
    { saveAction,                 ":/save"              },
    { saveAsAction,               ":/save-as"           },
    { importMediaAction,          ":/add-video"         },
    { AddCameraAction,            ":/add-camera"        },
    { addColorAction,             ":/add-color"         },
    { addTextAction,              ":/add-text"          },
    { addMeshAction,              ":/add-mesh"          },
    { addTriangleAction,          ":/add-triangle"      },
    { addEllipseAction,           ":/add-ellipse"       },
    { addPolygonAction,           ":/add-polygon"       },
    { rewindAction,               ":/rewind"            },
    { outputFullScreenAction,     ":/fullscreen"        },
    { displayControlsAction,      ":/control-points"    },
    { displaySourceControlsAction,":/control-points"    },
    { stickyVerticesAction,       ":/control-points"    },
    { displayTestSignalAction,    ":/toggle-test-signal"},
    { muteAllAction,              ":/mute"              },
    { zoomInAction,               ":/zoom-in"           },
    { zoomOutAction,              ":/zoom-out"          },
    { resetZoomAction,            ":/reset-zoom"        },
    { fitToViewAction,            ":/zoom-fit"          },
  };
  for (auto& e : entries) {
    if (e.action)
      e.action->setIcon(themedIcon(e.resource));
  }
  // playAction is a dual-state checkable action — rebuild its compound icon
  // so theme changes don't flatten it back to a single play-only icon.
  {
    QIcon icon;
    icon.addPixmap(themedIcon(":/play").pixmap(64),  QIcon::Normal, QIcon::Off);
    icon.addPixmap(themedIcon(":/pause").pixmap(64), QIcon::Normal, QIcon::On);
    playAction->setIcon(icon);
  }
  if (contentTab) {
    int sourceIdx = contentTab->indexOf(sourceSplitter);
    if (sourceIdx != -1)
      contentTab->setTabIcon(sourceIdx, themedIcon(":/add-video"));
    int layerIdx = contentTab->indexOf(layerSplitter);
    if (layerIdx != -1)
      contentTab->setTabIcon(layerIdx, themedIcon(":/add-mesh"));
  }
  if (sourceCanvasToolbar)
    sourceCanvasToolbar->refreshIcons();
  if (destinationCanvasToolbar)
    destinationCanvasToolbar->refreshIcons();
#ifdef Q_OS_MAC
  if (addSyphonAction)
    addSyphonAction->setIcon(themedIcon(":/add-syphon"));
#endif
}

void MainWindow::changeEvent(QEvent* event)
{
  QMainWindow::changeEvent(event);
}

void MainWindow::resetMeshInputToSource()
{
  Layer::ptr layer = getCurrentLayer();
  if (!layer || !layer->hasInputShape()) return;
  if (layer->getInputShape()->getType() != MShape::ShapeType::Mesh) return;

  Texture* tex = qobject_cast<Texture*>(layer->getSource().data());
  if (!tex) return;
  int w = tex->getWidth();
  int h = tex->getHeight();
  if (w <= 0 || h <= 0) return;

  undoStack->push(new ResetMeshInputCommand(sourceCanvas, (int)tex->getX(), (int)tex->getY(), w, h));
}

void MainWindow::setBackgroundPhoto()
{
  QString path = QFileDialog::getOpenFileName(
    this, tr("Set Background Reference Photo"), QString(),
    tr("Images (*.png *.jpg *.jpeg *.bmp *.tiff *.tif *.gif *.webp)"));
  if (path.isEmpty()) return;
  applyBackgroundPhoto(path, _backgroundPhotoOpacity);
}

void MainWindow::clearBackgroundPhoto()
{
  clearBackgroundPhotoState();
}

void MainWindow::applyBackgroundPhoto(const QString& path, qreal opacity)
{
  QPixmap pix(path);
  if (pix.isNull()) {
    QMessageBox::warning(this, tr("Background Photo"),
                         tr("Could not load image: %1").arg(path));
    return;
  }
  _backgroundPhotoPath    = path;
  _backgroundPhotoOpacity = opacity;

  destinationCanvas->setBackgroundPhoto(pix, opacity);

  destinationCanvasToolbar->setBackgroundPhotoControlsVisible(true);
  destinationCanvasToolbar->setBackgroundOpacityValue(qRound(opacity * 100));
  _clearBackgroundPhotoAction->setEnabled(true);
}

void MainWindow::clearBackgroundPhotoState()
{
  _backgroundPhotoPath.clear();

  destinationCanvas->clearBackgroundPhoto();

  destinationCanvasToolbar->setBackgroundPhotoControlsVisible(false);
  _clearBackgroundPhotoAction->setEnabled(false);
}

void MainWindow::onBackgroundPhotoOpacityChanged(int value)
{
  _backgroundPhotoOpacity = value / 100.0;
  destinationCanvas->setBackgroundPhotoOpacity(_backgroundPhotoOpacity);
}

void MainWindow::onBackgroundPhotoToggled(bool visible)
{
  destinationCanvas->setBackgroundPhotoVisible(visible);
}

}
