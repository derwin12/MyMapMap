@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
cd /d H:\Github\MyMapMap
C:\Qt\6.11.1\msvc2022_64\bin\windeployqt.exe --release --no-system-d3d-compiler bin\mymapmap.exe
if errorlevel 1 (
  echo DEPLOY_FAILED
  exit /b 1
)
echo DEPLOY_OK
