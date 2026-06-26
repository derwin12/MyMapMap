# CMakeLists.txt for MyMapMap (a fork of MapMap, GPLv3)
#
# Equivalent to the qmake project (mymapmap.pro + src/*.pri). Builds the
# bin/mymapmap.exe GUI application with Qt 6.
#
# Configure & build (from repo root), e.g. with the Qt 6.8 MSVC kit:
#
#   cmake -B build -G "Ninja" -DCMAKE_BUILD_TYPE=Release \
#         -DCMAKE_PREFIX_PATH=D:/Qt/6.8.3/msvc2022_64
#   cmake --build build
#
# The optional Qt HttpServer module enables the in-app MCP server (HAVE_MCP).

cmake_minimum_required(VERSION 3.21)

project(mymapmap VERSION 1.0.2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Mirror DESTDIR = $$PWD/bin from mymapmap.pro
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")

# ---------------------------------------------------------------------------
# Qt
# ---------------------------------------------------------------------------
find_package(Qt6 REQUIRED COMPONENTS
    Core Gui Widgets OpenGL OpenGLWidgets
    Network Multimedia MultimediaWidgets
    LinguistTools
)
# Optional: enables the MCP server when the HttpServer module is installed.
find_package(Qt6 QUIET COMPONENTS HttpServer)

qt_standard_project_setup()
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# ---------------------------------------------------------------------------
# Sources (from src/*.pri)
# ---------------------------------------------------------------------------
set(CORE_DIR    src/core)
set(SHAPE_DIR   src/shape)
set(GUI_DIR     src/gui)
set(CONTROL_DIR src/control)
set(APP_DIR     src/app)
set(QPB_DIR     src/gui/contrib/qtpropertybrowser/src)
set(QPBE_DIR    src/gui/contrib/qtpropertybrowser-extension)
set(QOSC_DIR    src/control/qosc)

set(SOURCES
    # core
    ${CORE_DIR}/ThumbnailCache.cpp
    ${CORE_DIR}/Commands.cpp
    ${CORE_DIR}/CameraImpl.cpp
    ${CORE_DIR}/CameraSurface.cpp
    ${CORE_DIR}/Element.cpp
    ${CORE_DIR}/Layer.cpp
    ${CORE_DIR}/MappingManager.cpp
    ${CORE_DIR}/MetaObjectRegistry.cpp
    ${CORE_DIR}/MM.cpp
    ${CORE_DIR}/Source.cpp
    ${CORE_DIR}/ProjectLabels.cpp
    ${CORE_DIR}/ProjectReader.cpp
    ${CORE_DIR}/ProjectWriter.cpp
    ${CORE_DIR}/Serializable.cpp
    ${CORE_DIR}/UidAllocator.cpp
    ${CORE_DIR}/VideoFrameConverter.cpp
    ${CORE_DIR}/VideoImpl.cpp
    ${CORE_DIR}/VideoPlayerImpl.cpp
    ${CORE_DIR}/Util.cpp

    # shape
    ${SHAPE_DIR}/Ellipse.cpp
    ${SHAPE_DIR}/Mesh.cpp
    ${SHAPE_DIR}/Polygon.cpp
    ${SHAPE_DIR}/Shape.cpp

    # gui
    ${GUI_DIR}/AboutDialog.cpp
    ${GUI_DIR}/ConsoleWindow.cpp
    ${GUI_DIR}/MainWindow.cpp
    ${GUI_DIR}/MapperGLCanvas.cpp
    ${GUI_DIR}/MapperGLCanvasToolbar.cpp
    ${GUI_DIR}/LayerGui.cpp
    ${GUI_DIR}/LayerItemDelegate.cpp
    ${GUI_DIR}/LayerListModel.cpp
    ${GUI_DIR}/OutputGLCanvas.cpp
    ${GUI_DIR}/OutputGLWindow.cpp
    ${GUI_DIR}/SourceGui.cpp
    ${GUI_DIR}/PreferenceDialog.cpp
    ${GUI_DIR}/ShapeControlPainter.cpp
    ${GUI_DIR}/ShapeGraphicsItem.cpp
    ${GUI_DIR}/ShortcutWindow.cpp
    ${GUI_DIR}/VideoExporter.cpp

    # gui/contrib qtpropertybrowser
    ${QPB_DIR}/qtpropertybrowser.cpp
    ${QPB_DIR}/qtpropertymanager.cpp
    ${QPB_DIR}/qteditorfactory.cpp
    ${QPB_DIR}/qtvariantproperty.cpp
    ${QPB_DIR}/qttreepropertybrowser.cpp
    ${QPB_DIR}/qtbuttonpropertybrowser.cpp
    ${QPB_DIR}/qtgroupboxpropertybrowser.cpp
    ${QPB_DIR}/qtpropertybrowserutils.cpp

    # gui/contrib qtpropertybrowser-extension
    ${QPBE_DIR}/fileedit.cpp
    ${QPBE_DIR}/filepathmanager.cpp
    ${QPBE_DIR}/fileeditfactory.cpp
    ${QPBE_DIR}/variantmanager.cpp
    ${QPBE_DIR}/variantfactory.cpp

    # control
    ${CONTROL_DIR}/OscInterface.cpp
    ${CONTROL_DIR}/FppMultiSyncListener.cpp

    # control/qosc (OSC support)
    ${QOSC_DIR}/contrib/oscpack/OscOutboundPacketStream.cpp
    ${QOSC_DIR}/contrib/oscpack/OscPrintReceivedElements.cpp
    ${QOSC_DIR}/contrib/oscpack/OscReceivedElements.cpp
    ${QOSC_DIR}/contrib/oscpack/OscTypes.cpp
    ${QOSC_DIR}/oscreceiver.cpp
    ${QOSC_DIR}/oscsender.cpp

    # app
    ${APP_DIR}/MainApplication.cpp
    ${APP_DIR}/main.cpp
)

# Headers are listed so AUTOMOC processes the Q_OBJECT ones (and IDEs see them).
set(HEADERS
    ${CORE_DIR}/ThumbnailCache.h ${CORE_DIR}/Commands.h ${CORE_DIR}/CameraImpl.h
    ${CORE_DIR}/CameraSurface.h ${CORE_DIR}/Element.h ${CORE_DIR}/Layer.h
    ${CORE_DIR}/MappingManager.h ${CORE_DIR}/Maths.h ${CORE_DIR}/MetaObjectRegistry.h
    ${CORE_DIR}/MM.h ${CORE_DIR}/Source.h ${CORE_DIR}/ProjectLabels.h
    ${CORE_DIR}/ProjectReader.h ${CORE_DIR}/ProjectWriter.h ${CORE_DIR}/Serializable.h
    ${CORE_DIR}/UidAllocator.h ${CORE_DIR}/VideoFrameConverter.h ${CORE_DIR}/VideoImpl.h
    ${CORE_DIR}/VideoPlayerImpl.h ${CORE_DIR}/Util.h

    ${SHAPE_DIR}/Ellipse.h ${SHAPE_DIR}/FreePolygon.h ${SHAPE_DIR}/Mesh.h
    ${SHAPE_DIR}/Polygon.h ${SHAPE_DIR}/Quad.h ${SHAPE_DIR}/Shape.h
    ${SHAPE_DIR}/Shapes.h ${SHAPE_DIR}/Triangle.h

    ${GUI_DIR}/AboutDialog.h ${GUI_DIR}/ConsoleWindow.h ${GUI_DIR}/GuiForward.h
    ${GUI_DIR}/MainWindow.h ${GUI_DIR}/MapperGLCanvas.h ${GUI_DIR}/MapperGLCanvasToolbar.h
    ${GUI_DIR}/LayerGui.h ${GUI_DIR}/LayerItemDelegate.h ${GUI_DIR}/LayerListModel.h
    ${GUI_DIR}/OutputGLCanvas.h ${GUI_DIR}/OutputGLWindow.h ${GUI_DIR}/SourceGui.h
    ${GUI_DIR}/PreferenceDialog.h ${GUI_DIR}/ShapeControlPainter.h
    ${GUI_DIR}/ShapeGraphicsItem.h ${GUI_DIR}/ShortcutWindow.h ${GUI_DIR}/VideoExporter.h

    ${QPB_DIR}/qtpropertybrowser.h ${QPB_DIR}/qtpropertymanager.h
    ${QPB_DIR}/qteditorfactory.h ${QPB_DIR}/qtvariantproperty.h
    ${QPB_DIR}/qttreepropertybrowser.h ${QPB_DIR}/qtbuttonpropertybrowser.h
    ${QPB_DIR}/qtgroupboxpropertybrowser.h ${QPB_DIR}/qtpropertybrowserutils_p.h

    ${QPBE_DIR}/fileedit.h ${QPBE_DIR}/filepathmanager.h ${QPBE_DIR}/fileeditfactory.h
    ${QPBE_DIR}/variantmanager.h ${QPBE_DIR}/variantfactory.h

    ${CONTROL_DIR}/ConcurrentQueue.h ${CONTROL_DIR}/OscInterface.h
    ${CONTROL_DIR}/FppMultiSyncListener.h
    ${QOSC_DIR}/oscreceiver.h ${QOSC_DIR}/oscsender.h

    ${APP_DIR}/MainApplication.h
)

# MCP server: only when the optional HttpServer module is present.
if(Qt6HttpServer_FOUND)
    list(APPEND SOURCES ${CONTROL_DIR}/McpServer.cpp)
    list(APPEND HEADERS ${CONTROL_DIR}/McpServer.h)
    message(STATUS "Qt HttpServer module found — MCP server enabled")
else()
    message(STATUS "Qt HttpServer module not found — MCP server disabled")
endif()

# ---------------------------------------------------------------------------
# Translations (.ts -> .qm) consumed by translations/translation.qrc
# ---------------------------------------------------------------------------
set(TS_FILES
    translations/mapmap_en.ts
    translations/mapmap_es.ts
    translations/mapmap_fr.ts
    translations/mapmap_zh_CN.ts
    translations/mapmap_zh_TW.ts
)
# The .qrc embeds .qm files that sit next to the .ts files, so generate them
# in-source before AUTORCC runs (mirrors qmake's lrelease step).
get_target_property(LRELEASE_EXECUTABLE Qt6::lrelease IMPORTED_LOCATION)
foreach(ts ${TS_FILES})
    get_filename_component(ts_name ${ts} NAME_WE)
    set(qm "${CMAKE_CURRENT_SOURCE_DIR}/translations/${ts_name}.qm")
    add_custom_command(
        OUTPUT ${qm}
        COMMAND ${LRELEASE_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/${ts}" -qm ${qm}
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${ts}"
        COMMENT "lrelease ${ts}"
        VERBATIM
    )
    list(APPEND QM_FILES ${qm})
endforeach()
add_custom_target(translations_qm DEPENDS ${QM_FILES})

# ---------------------------------------------------------------------------
# Resources (.qrc) handled by AUTORCC
# ---------------------------------------------------------------------------
set(RESOURCES
    main.qrc
    translations/translation.qrc
    docs/documentation.qrc
    resources/interface.qrc
)
# qtpropertybrowser ships cursor PNGs that are not committed to this repo. Only
# embed its .qrc when the images are present, otherwise rcc fails on the missing
# files. Without them the cursor-type editor icons are simply blank (cosmetic).
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${QPB_DIR}/images/cursor-arrow.png")
    list(APPEND RESOURCES ${QPB_DIR}/qtpropertybrowser.qrc)
else()
    message(WARNING
        "qtpropertybrowser cursor images missing — skipping qtpropertybrowser.qrc "
        "(cursor-type editor icons will be blank)")
endif()

# ---------------------------------------------------------------------------
# Executable
# ---------------------------------------------------------------------------
qt_add_executable(mymapmap WIN32 ${SOURCES} ${HEADERS} ${RESOURCES})
add_dependencies(mymapmap translations_qm)

if(WIN32)
    target_sources(mymapmap PRIVATE resources/windows_resource.rc)
endif()

target_include_directories(mymapmap PRIVATE
    ${CORE_DIR} ${SHAPE_DIR} ${GUI_DIR} ${CONTROL_DIR} ${APP_DIR}
    ${QPB_DIR} ${QPBE_DIR}
    ${QOSC_DIR} ${QOSC_DIR}/contrib/packosc ${QOSC_DIR}/contrib/oscpack
)

target_compile_definitions(mymapmap PRIVATE
    UNICODE QT_THREAD_SUPPORT QT_CORE_LIB QT_GUI_LIB QT_MESSAGELOGCONTEXT
)

target_link_libraries(mymapmap PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::OpenGL Qt6::OpenGLWidgets
    Qt6::Network Qt6::Multimedia Qt6::MultimediaWidgets
)

if(Qt6HttpServer_FOUND)
    target_compile_definitions(mymapmap PRIVATE HAVE_MCP)
    target_link_libraries(mymapmap PRIVATE Qt6::HttpServer)
endif()

# Windows-specific (from src.pri win32 block)
if(WIN32)
    target_compile_definitions(mymapmap PRIVATE WIN32 _USE_MATH_DEFINES)
    target_link_libraries(mymapmap PRIVATE opengl32)
endif()

# Linux-specific
if(UNIX AND NOT APPLE)
    target_compile_definitions(mymapmap PRIVATE UNIX)
endif()
