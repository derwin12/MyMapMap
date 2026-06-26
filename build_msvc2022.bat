@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
cd /d H:\Github\MyMapMap
C:\Qt\6.11.1\msvc2022_64\bin\qmake.exe mymapmap.pro CONFIG+=release
if errorlevel 1 (
  echo QMAKE_FAILED
  exit /b 1
)
nmake
if errorlevel 1 (
  echo NMAKE_FAILED
  exit /b 1
)
:: Copy Qt modules that windeployqt misses (HttpServer and its WebSockets dep)
set QT_BIN=C:\Qt\6.11.1\msvc2022_64\bin
if exist "%QT_BIN%\Qt6HttpServer.dll"  copy /y "%QT_BIN%\Qt6HttpServer.dll"  bin\ >nul
if exist "%QT_BIN%\Qt6WebSockets.dll"  copy /y "%QT_BIN%\Qt6WebSockets.dll"  bin\ >nul
echo BUILD_OK
