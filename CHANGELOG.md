# Release notes for MyMapMap

&nbsp;

## 2026-06-23 - MyMapMap 0.6.3-mymapmap.1

- Forked from [MapMap](https://github.com/mapmapteam/mapmap) (upstream `dev`, 0.6.3)
- Added Visual Studio 2022 / MSVC build support (Qt 6.8.3 msvc2022_64), alongside upstream's MinGW path
- Fixed MSVC compile errors: `windows.h`/`GL/gl.h` include order, GNU compound-literal cast replaced with standard brace-init
- Build output now stays inside the project folder (`bin/mapmap.exe`) instead of upstream's `../../MapMap/` packaging path
- Rebranded application name/title/About dialog to MyMapMap, with attribution to upstream MapMap
