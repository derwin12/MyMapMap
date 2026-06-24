@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
cd /d H:\Github\MyMapMap
msbuild mymapmap.sln /t:mapmap /p:Configuration=Release /p:Platform=x64 /m
if errorlevel 1 (
  echo MSBUILD_FAILED
  exit /b 1
)
echo MSBUILD_OK
