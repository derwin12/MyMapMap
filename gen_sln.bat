@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
cd /d H:\Github\MyMapMap
C:\Qt\6.8.3\msvc2022_64\bin\qmake.exe -tp vc mapmap.pro CONFIG+=release
if errorlevel 1 (
  echo QMAKE_SLN_FAILED
  exit /b 1
)
echo SLN_OK
