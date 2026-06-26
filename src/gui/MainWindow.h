/*
 * MainWindow.h
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

#ifndef MAIN_WINDOW_H_
#define MAIN_WINDOW_H_

#include <QtGui>
#include <QtWidgets>
#include <QMediaDevices>
#include <QCameraDevice>
#include <QTimer>
#include <QElapsedTimer>
#include <QVariant>
#include <QMap>
#include <QMessageLogger>
#include <QNetworkAccessManager>
#include <QNetworkReply>

#include "MM.h"

#include "MapperGLCanvas.h"
#include "MapperGLCanvasToolbar.h"
#include "OscInterface.h"
#ifdef HAVE_MCP
#include "McpServer.h"
#endif

#include "OutputGLWindow.h"
#include "ConsoleWindow.h"
#include "VideoExporter.h"

#include "MappingManager.h"
#include "ThumbnailCache.h"
#include "LayerItemDelegate.h"
#include "LayerListModel.h"

#include "qtpropertymanager.h"
#include "qtvariantproperty.h"
#include "qttreepropertybrowser.h"
#include "qtgroupboxpropertybrowser.h"

#include "SourceGui.h"

namespace mmp {

class PreferenceDialog;
class AboutDialog;
class ShortcutWindow;

/**
 * This is the main window of MapMap. It acts as both a view and a controller interface.
 */
class MainWindow: public QMainWindow
{
Q_OBJECT

public:
  // Constructor.
  MainWindow();

  // Destructor.
  ~MainWindow();

  // XXX Unused.
  //void applyOscCommand(const QVariantList& command);

protected:
  // Events ///////////////////////////////////////////////////////////////////////////////////////////////////
  void closeEvent(QCloseEvent *event);
  void keyPressEvent(QKeyEvent *event);
  bool eventFilter(QObject *object, QEvent *event);
  void changeEvent(QEvent *event) override;

  void dragEnterEvent(QDragEnterEvent *event);
  void dragMoveEvent(QDragMoveEvent *event);
  void dragLeaveEvent(QDragLeaveEvent *event);
  void dropEvent(QDropEvent *event);

  // Slots ////////////////////////////////////////////////////////////////////////////////////////////////////
private slots:

  // Recording.
  void toggleRecording(bool on);
  void onRecordingStopped(const QString& filePath);

  // Source preview.
  void updateSourcePreview(uid sourceId);
  void onThumbnailReady(const QString& videoPath, const QStringList& frames);
  void updatePreviewAnimSpeed();

  // Menus slots.
  // File menu.
  void newFile();
  void open();
  bool save();
  bool saveAs();
  void importMedia();
  void importFolder();
  void importFolderAsSource();
  void openCameraDevice();
  void addColor();
  void addText();
  void addSyphon();
  void about();
  void checkForUpdates(bool autoCheck = false);
  void updateStatusBar();
  void showMenuBar(bool shown);
  void openRecentFile();
  void clearRecentFileList();
  void openRecentVideo();
  void quitMapMap();
  // Edit menu.
  void deleteItem();
  // Context menu for mappings.
  void duplicateLayerItem();
  void deleteLayerItem();
  void renameLayerItem();
  void setLayerItemLocked(bool locked);
  void setLayerItemHide(bool hide);
  void setLayerItemSolo(bool solo);
  void loadLayerMedia();
  void transformActionLayerItem();
  void reorderLayerItem();
  // Context menu for sources
  void deleteSourceItem();
  void renameSourceItem();
  void sourceListEditEnd(QWidget* editor);
  void setSectionCollapsed(QListWidgetItem* header, bool collapsed);
  void setSourceListThumbnailMode(bool thumbnailMode);
  // Output menu
  void setupOutputScreen();
  void updateScreenCount();

  // Widget callbacks.
  void handleSourceItemSelectionChanged();
//  void handleItemDoubleClicked(QListWidgetItem* item);
  void handleLayerItemSelectionChanged(const QModelIndex &index);
  void handleLayerItemChanged(const QModelIndex &index);
  void handleLayerIndexesMoved();
  void handleSourceItemSelected(QListWidgetItem* item);
  void handleSourceChanged(Source::ptr source);

  // Fits a Syphon source's input shapes to its real resolution once known.
  void autoFitSyphonInputShapes(int sourceId, int width, int height);

  void layerPropertyChanged(uid id, QString propertyName, QVariant value);
  void sourcePropertyChanged(uid id, QString propertyName, QVariant value);


  // Other.
  void windowModified();
  void pollOscInterface();
  void exitFullScreen();

  // Some help links
  void documentation() {
      QDesktopServices::openUrl(QUrl(MM::WEBSITE_URL));
  }
  // Report an issue
  void reportBug() {
      QDesktopServices::openUrl(QUrl("https://github.com/derwin12/MyMapMap/issues/new"));
  }

  void openShortcutWindow();

  void updateSettings();

  void updateLayerListColumnWidth();

public slots:

  void refreshIcons();

  // Layer creation.
  void addMesh();
  void addTriangle();
  void addPolygonVertex();
  void deletePolygonVertex();
  void addEllipse();
  void addPolygon();

  // Polygon draw-mode API (called by MapperGLCanvas).
  bool isPolygonDrawMode() const { return _polygonDrawMode; }
  bool isPolygonDrawOnSource() const { return _polygonDrawOnSource; }
  const QVector<QPointF>& polygonPoints() const { return _polygonPoints; }
  const QPointF& polygonCursorPos() const { return _polygonCursorPos; }
  void polygonCanvasClick(const QPointF& scenePos);
  void polygonCursorMoved(const QPointF& scenePos);
  void finishPolygon();
  void cancelPolygonDrawMode();

  // CRUD.

  /// Clears all mappings and sources.
  bool clearProject();

  /// Create or replace a media source (or image).
  uid createMediaSource(uid sourceId, QString uri, float x, float y, bool isImage, VideoType type, double rate=1.0);

  /// Create or replace a color source.
  uid createColorSource(uid sourceId, QColor color);
  uid createTextSource(uid sourceId, const QString& text);

  /// Create a folder source from a directory path.
  uid createFolderSource(uid sourceId, const QString& dirPath);

  /// Create a free-polygon layer for the given source with the given vertices.
  uid addFreePolygonLayer(int sourceId, const QVector<QPointF>& vertices);

  /// Create a Syphon source pointing at the given server (macOS only).
  uid createSyphonSource(uid sourceId, const QString& serverUUID,
                         const QString& serverName, const QString& appName);

  /// Sets visibility of mapping.
  void setLayerVisible(uid mappingId, bool visible);

  /// Sets solo status of mapping.
  void setLayerSolo(uid mappingId, bool solo);

  /// Sets locked attribute of mapping.
  void setLayerLocked(uid mappingId, bool locked);

  /// Deletes/removes a mapping.
  void deleteLayer(uid mappingId);

  /// Moves a mapping to given index.
  void moveLayer(uid mappingId, int idx);

  /// Clone/duplicate a mapping
  void duplicateLayer(uid mappingId);

  /// Deletes/removes a source and all associated mappigns.
  void deleteSource(uid sourceId, bool replace = false);

  /// Updates all canvases.
  void updateCanvases();

	/// Update all mapping guis.
	void updateMappers();

  /**
   * This function is triggered framesPerSeconds() times per second. It makes sure
   * the image is refreshed (updateCanvases()) and performs other necessary operations.
   */
  void processFrame();

  /**
   * Performs operations related to the playing state, such as making sure to play only sources
   * that are visible.
   */
  void updatePlayingState();

  // Editing toggles.
  void setFramesPerSecond(qreal fps);
  void enableDisplayControls(bool display);
  void enableDisplaySourceControls(bool display);
  void enableStickyVertices(bool display);
  void toggleMuteAll(bool muted);
  void displayUndoHistory(bool display);

  // Show Mapping Context Menu
  void showLayerContextMenu(const QPoint &point);
  // Show Source Context Menu
  void showSourceContextMenu(const QPoint &point);

  /// Start playback.
  void play(bool updatePlayPauseActions=true);

  /// Pause playback.
  void pause(bool updatePlayPauseActions=true);

  /// Reset playback.
  void rewind();

private:
  // Internal methods. //////////////////////////////////////////////////////////////////////////////////////

  // Creation of view elements.
  void createLayout();
  void createActions();
  void createMenus();
  void createLayerContextMenu();
  void createSourceContextMenu();

  // Theme helpers.
  static QIcon themedIcon(const QString& resource);
  void createToolBars();
  void createStatusBar();
  void updateRecentFileActions();
  void updateRecentVideoActions();
  void updateScreenActions();
  void updateMediaListActions();
  void updateLayerActions();

  // Settings.
  void readSettings();
  void writeSettings();

  // OSC.
  void startOscReceiver();

  // Polygon draw mode internals.
  void startPolygonDrawMode();

#ifdef HAVE_MCP
  // MCP server.
  void startMcpServer();
#endif

  // Actions-related.
  bool okToContinue();

public:
  bool loadFile(const QString &fileName);
  bool saveFile(const QString &fileName);
  void setCurrentFile(const QString &fileName);
  void setCurrentVideo(const QString &filename);
  bool importMediaFile(const QString &fileName, bool isImage = false, bool isCamera = false);
  bool addColorSource(const QColor& color);
  bool addTextSource(const QString& text);
  void addLayerItem(uid mappingId);
  void removeLayerItem(uid mappingId);
  void moveLayerItem(uid mappingId, int steps);
  void initSourceListSections(); // (re)create Images / Videos header items
  void addSourceItem(uid sourceId, const QIcon& icon, const QString& name);
  void updateSourceItem(uid sourceId, const QIcon& icon, const QString& name);
  void removeSourceItem(uid sourceId);
  void renameLayer(uid mappingId, const QString& name);
  void renameSource(uid sourceId, const QString& name);
  void clearWindow();
  // Resync mapping manager order the same as the GUI.
  void syncLayerManager();
  // Check if the file exists
  bool fileExists(const QString& file);
  QString thumbnailCacheDir() const;
  // Check if the file is supported
  bool fileSupported(const QString& file, bool isImage);
  bool fileSupported(const QString &file, const QString &extension);
  // Locate the file not found
  QString locateMediaFile(const QString& uri, bool isImage);

  static MainWindow* window();

  // Returns a short version of filename.
  static QString strippedName(const QString &fullFileName);

  // Returns the source icon depending on play/pause state.
  static const QIcon getSourceIcon(Source::ptr source);

  // Refreshes the usage-count badge on every source thumbnail.
  void refreshSourceBadges();

private:
  // Connects/disconnects project-specific widgets (sources and mappings).
  void connectProjectWidgets();
  void disconnectProjectWidgets();

  // Get/set id from list item.
  static uid getItemId(const QListWidgetItem& item);
  static void setItemId(QListWidgetItem& item, uid id);
  static QListWidgetItem* getItemFromId(const QListWidget& list, uid id);
  static int getItemRowFromId(const QListWidget& list, uid id);
  uid currentLayerItemId() const;

  static QIcon createColorIcon(const QColor& color);
  static QIcon createFileIcon(const QString& filename);
  static QIcon createImageIcon(const QString& filename);

  // GUI elements. ////////////////////////////////////////////////////////////////////////////////////////

  // Menu actions.
  QMenu *fileMenu;
  QMenu *editMenu;
  QMenu *toolsMenu;
  QMenu *viewMenu;
  QMenu *windowMenu;
  QMenu *helpMenu;

  // Sub-menus.
  QMenu *outputScreenMenu;
  QMenu *recentFileMenu;
  QMenu *recentVideoMenu;
  QMenu *layerContextMenu;
  QMenu *sourceContextMenu;

  // Some menus when need to be separated
  QMenu *sourceMenu;
  QMenu *destinationMenu;

  QMenu *_changeLayerMediaMenu;
  QAction *_importLayerMediaAction;

  // Toolbar.
  QToolBar *mainToolBar;

  // Actions.
  QAction *separatorAction;
  QAction *newAction;
  QAction *openAction;
  QAction *importMediaAction;
  QAction *importFolderAction;
  QAction *AddCameraAction;
  QAction *addColorAction;
  QAction *addTextAction;
  QAction *addSyphonAction;
  QAction *saveAction;
  QAction *saveAsAction;
  QAction *exitAction;
  QAction *undoAction;
  QAction *redoAction;
  // Mappings context menu actions
  QAction *duplicateLayerAction;
  QAction *deleteLayerAction;
  QAction *renameLayerAction;
  QAction *layerSoloAction;
  QAction *layerLockedAction;
  QAction *layerHideAction;
  // Transform.
  QAction *layerRotate90CWAction;
  QAction *layerRotate90CCWAction;
  QAction *layerRotate180Action;
  QAction *layerHorizontalFlipAction;
  QAction *layerVerticalFlipAction;
  // Layer reordering.
  QAction *layerRaiseAction;
  QAction *layerLowerAction;
  QAction *layerRaiseToTopAction;
  QAction *layerLowerToBottomAction;

  // Sources context menu action
  QAction *deleteSourceAction;
  QAction *renameSourceAction;
  QAction *preferencesAction;
  QAction *aboutAction;
  QAction *checkForUpdatesAction;
  QAction *clearRecentFileActions;
  QAction *emptyRecentVideos;

  QAction *addMeshAction;
  QAction *addTriangleAction;
  QAction *addEllipseAction;
  QAction *addPolygonAction = nullptr;
  QAction *addPolygonVertexAction = nullptr;
  QAction *deletePolygonVertexAction = nullptr;

  // Polygon draw-mode state.
  bool _polygonDrawMode = false;
  bool _polygonDrawOnSource = false; // true when drawing on source/input canvas
  QVector<QPointF> _polygonPoints;
  QPointF _polygonCursorPos;

  // Polygon vertex-edit state (set by MapperGLCanvas before context menu).
  int  _polyEditType     = 0;  // 0=none, 1=add, 2=delete (PolyEditType, kept as int to avoid private enum access)
  int  _polyEditIndex    = -1;
  qreal _polyEditT       = 0.0;
  bool  _polyEditOnSource = false;

  QAction *playAction; // checkable: unchecked=paused (play icon), checked=playing (pause icon)
  QAction *rewindAction;
  QAction *muteAllAction;
  QAction *recordAction;

  QAction *outputFullScreenAction;
  QAction *displayControlsAction;
  QAction *displaySourceControlsAction;
  QAction *displayTestSignalAction;
  QAction *stickyVerticesAction;
  QAction *displayUndoHistoryAction;
  QAction *displayZoomToolAction;
  QAction *openConsoleAction;
  QAction *showMenuBarAction;
  QAction *showToolBarAction;

  QActionGroup *perspectiveActionGroup;
  QAction *mainViewAction;
  QAction *sourceViewAction;
  QAction *destViewAction;

  enum { MaxRecentFiles = 10 };
  enum { MaxRecentVideo = 5 };
  QAction *recentFileActions[MaxRecentFiles];
  QAction *recentVideoActions[MaxRecentVideo];

  // help actions
  QAction *bugReportAction;
  QAction *docAction;
  QAction *shortcutAction;

  // Screen output action
  QList<QAction *> screenActions;
  QActionGroup *screenActionGroup;

  // Canvas zoom actions
  QAction *zoomInAction;
  QAction *zoomOutAction;
  QAction *resetZoomAction;
  QAction *fitToViewAction;

  // Widgets and layout.
  QTabWidget* contentTab;

  QSplitter* sourceSplitter;
  QListWidget* sourceList;
  QListWidgetItem* _sourceSectionImages    = nullptr; // non-selectable section header
  QListWidgetItem* _sourceSectionVideos    = nullptr; // non-selectable section header
  QListWidgetItem* _sourceSectionGenerated = nullptr; // non-selectable section header (Color, Text)
  QListWidgetItem* _sourceSectionFolders   = nullptr; // non-selectable section header
  QMap<QListWidgetItem*, QToolButton*> _sectionArrows;
  QMap<QListWidgetItem*, bool>         _sectionCollapsed;
  bool _sourceListThumbnailMode = true; // false = list, true = thumbnail
  int  _sourceListIconSize = PAINT_LIST_ICON_SIZE;
  QStackedWidget* sourcePropertyPanel;

  QWidget*      _sourcePreviewContainer = nullptr;
  QLabel*       _sourcePreviewLabel     = nullptr;
  QCheckBox*    _previewToggleBtn       = nullptr;
  QToolButton*  _thumbSizeBtns[3]      = {};
  QToolButton*  _viewModeBtns[2]       = {}; // [0]=list, [1]=thumbnail
  ThumbnailCache* _thumbnailCache       = nullptr;

  QSplitter* layerSplitter;
  QTableView* layerList;
  QStackedWidget* layerPropertyPanel;

  QUndoView* undoView;

  MapperGLCanvas* sourceCanvas;
  MapperGLCanvasToolbar* sourceCanvasToolbar;
  QWidget* sourcePanel;
  MapperGLCanvas* destinationCanvas;
  MapperGLCanvasToolbar* destinationCanvasToolbar;
  QWidget* destinationPanel;

  OutputGLWindow* outputWindow;
  ConsoleWindow* consoleWindow;

  QSplitter* mainSplitter;
  QSplitter* canvasSplitter;

  // Internal variables. ///////////////////////////////////////////////////////////////////////////////////

  // Recent files
  QStringList recentFiles;
  QStringList recentVideos;

  // Current filename.
  QString curFile;

  // Current video name
  QString curVideo;

  // Settings
  QSettings settings;

  // Model.
  MappingManager* mappingManager;
  LayerListModel *layerListModel;
  LayerItemDelegate *layerItemDelegate;

  // OSC.
  OscInterface::ptr osc_interface;
  int oscListeningPort;
  QTimer *osc_timer;

#ifdef HAVE_MCP
  // MCP server.
  QScopedPointer<McpServer> mcp_server;
  int mcpListeningPort = MM::DEFAULT_MCP_PORT;
#endif

  // View.

  // The view counterpart of Mappings.
  QMap<uid, LayerGui::ptr> layerGuis;
  QMap<uid, SourceGui::ptr> sourceGuis;

  // Current selected source/mapping.
  uid currentSourceId;
  uid currentLayerId;
  bool _hasCurrentLayer;
  bool _hasCurrentSource;

  // Number of frames per second.
  qreal _framesPerSecond;

  // True iff the play button is currently pressed.
  bool _isPlaying;

  // True iff we are displaying the controls.
  bool _displayControls;

  // True iff we are displaying the borders of all controls of all shapes related to a source.
  bool _displaySourceControls;

  // True iff we want vertices to stick to each other.
  bool _stickyVertices;

  // True iff all source audio is currently muted.
  bool _audioMuted;

  bool _displayUndoStack;

  // Menu bar hidden state
  bool _showMenuBar;

  // Keeps track of the current selected item, wether it's a source or mapping.
  QListWidgetItem* currentSelectedItem;
  QModelIndex currentSelectedIndex;
  QTimer *videoTimer;
  QElapsedTimer *systemTimer;
  // Video recorder
  VideoExporter* _videoExporter;
  qint64 _recordingTotalMs = 0;
  QMap<uid, bool> _savedLoopStates;
  bool _recordingOpenedOutputWindow = false;
  bool _recordingHadControls        = false; // controls state before auto-fullscreen

  // Preference dialog
  PreferenceDialog* _preferenceDialog;
  // About dialog
  AboutDialog *_aboutDialog;
  // Shortcut Windows
  ShortcutWindow *_shortcutWindow;

  // UndoStack
  QUndoStack *undoStack;

  // Labels for status bar
  QLabel *destinationZoomLabel;
  QLabel *sourceZoomLabel;
  QLabel *lastActionLabel;
  QLabel *currentMessageLabel;
  QLabel *mousePosLabel;
  QLabel *trueFramesPerSecondsLabel;
  QLabel *recordingTimerLabel;

  typedef Source::SourceType SourceType;
  typedef MShape::ShapeType ShapeType ;

public:
  // Accessor/mutators for the view. ///////////////////////////////////////////////////////////////////
  MappingManager& getMappingManager() const { return *mappingManager; }

  LayerGui::ptr getLayerGuiByLayerId(uint id) const { return layerGuis[id]; }
  SourceGui::ptr getSourceGuiBySourceId(uint id) const { return sourceGuis[id]; }

  uid getCurrentSourceId() const { return currentSourceId; }
  uid getCurrentLayerId() const { return currentLayerId; }

  Layer::ptr getCurrentLayer() const { return mappingManager->getLayerById(currentLayerId); }
  Source::ptr getCurrentSource() const { return mappingManager->getSourceById(currentSourceId); }

  bool hasCurrentSource() const { return _hasCurrentSource; }
  bool hasCurrentLayer() const { return _hasCurrentLayer; }
  void setCurrentSource(int uid);
  void setCurrentLayer(int uid);
  void removeCurrentSource();
  void removeCurrentLayer();

  OutputGLWindow* getOutputWindow() const { return outputWindow; }
  MapperGLCanvas* getSourceCanvas() const { return sourceCanvas; }
  MapperGLCanvas* getDestinationCanvas() const { return destinationCanvas; }
  int getPreferredScreen() const { return outputWindow->getPreferredScreen(); }

  /// Returns the number of frames per second.
  qreal framesPerSecond() const { return _framesPerSecond; }

  /// Returns true iff MapMap is currently playing (ie. not in pause).
  bool isPlaying() const { return _isPlaying; }

  /// Returns true iff we should display the controls.
  bool displayControls() const { return _displayControls; }

  /// Returns true iff we should display all of the shapes related to a source.
  bool displaySourceControls() const { return _displaySourceControls; }

  /// Returns true iff we want vertices to stick to each other.
  bool isStickyVertices() const { return _stickyVertices; }

  // Use the same undoStack for whole program
  QUndoStack* getUndoStack() const { return undoStack; }

  void startFullScreen();
  bool setOscPort(QString portNumber);
  bool setOscPort(int portNumber);
  int getOscPort() const;
  void setVerbose(bool verbose);
#ifdef HAVE_MCP
  bool setMcpPort(QString portNumber);
  bool setMcpPort(int portNumber);
  int getMcpPort() const;
#endif
  void setOutputWindowFullScreen(bool enable);

public:
  // Constants. ///////////////////////////////////////////////////////////////////////////////////////
  static const int DEFAULT_WIDTH = 1360;
  static const int DEFAULT_HEIGHT = 768;
  static const int PAINT_LIST_ITEM_HEIGHT = 72;
  static const int PAINT_LIST_ICON_SIZE = 64;
  static const int SHAPE_LIST_ITEM_HEIGHT = 40;
  static const int PAINT_LIST_MINIMUM_HEIGHT = 290;
  static const int MAPPING_LIST_MINIMUM_HEIGHT = 290;
  static const int PAINT_PROPERTY_PANEL_MINIMUM_HEIGHT = 290;
  static const int MAPPING_PROPERTY_PANEL_MINIMUM_HEIGHT = 290;
  static const int CANVAS_MINIMUM_WIDTH  = 480;
  static const int CANVAS_MINIMUM_HEIGHT = 270;
  static const int OUTPUT_WINDOW_MINIMUM_WIDTH = 480;
  static const int OUTPUT_WINDOW_MINIMUM_HEIGHT = 270;

  // Polygon vertex-edit API (called by MapperGLCanvas before showing context menu).
  static const int PolyEditNone   = 0;
  static const int PolyEditAdd    = 1;
  static const int PolyEditDelete = 2;
  void setPendingPolygonEdit(int type, int index, qreal t, bool onSource) {
    _polyEditType = type; _polyEditIndex = index; _polyEditT = t; _polyEditOnSource = onSource;
  }
  void clearPendingPolygonEdit() { _polyEditType = PolyEditNone; }
  bool hasPendingPolygonEdit() const { return _polyEditType != PolyEditNone; }
};

}

#endif /* MAIN_WINDOW_H_ */
