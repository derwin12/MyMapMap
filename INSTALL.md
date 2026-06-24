Build instructions
==================

Build on GNU/Linux
------------------

Install the dependencies.

Build it:

```
qmake mapmap.pro
make
```

Alternatively:

```
./scripts/build.sh
```

### Ubuntu

Install basic development tools for Qt 6 projects:

```
sudo apt-get install -y \
      qt6-tools-dev \
      qt6-multimedia-dev \
      qt6-base-dev \
      libqt6opengl6-dev \
      libqt6openglwidgets6
```

Install extra packages if you want to build the documentation:

```
sudo apt-get install -y \
      doxygen \
      graphviz \
      rst2pdf \
      markdown
```

### Arch Linux

Install basic development tools for Qt 6 projects:

```
sudo pacman -S qt6-tools qt6-multimedia qt6-base
```

Build on macOS
--------------

Install tools and dependencies:

```
./scripts/sh_install_deps_macos.sh
```

Build:

```
./scripts/sh_build_macos.sh
```

### Troubleshooting

#### Corrupted OSC port

If the appearance of the window of the OSC port number in the preferences seem corrupted, you might want to reset MapMap's preferences:

```
rm -f ~/Library/Preferences/info.mapmap.MapMap.plist
```

Build on Windows
----------------

## Build dynamic version to debug project:
- Download and install [Qt6 MinGW incl. QtCreator](https://www.qt.io/download-open-source/)
- Build and run MapMap project within QtCreator (Ctrl-R)

## Build static version for release:
- Build a [Qt static environment](https://wiki.qt.io/Building_a_static_Qt_for_Windows_using_MinGW)
- Build MapMap using QtCreator (qmake, build release)
- Run MapMap.exe

#### For packaging
- Open Qt terminal via a Start Menu and run the following command:
`windeployqt --release --no-system-d3d-compiler <path-to-app-binary>`
- Replace all the `*.qm` files that exists in `released-binary`/translations folder by the ones from `source-code`/translations folder
- Download and install [Inno Setup](https://jrsoftware.org/isdl.php) to create an installation wizard setup

### MyMapMap fork: building with Visual Studio 2022 / MSVC

This fork additionally supports building with MSVC instead of MinGW:

1. Install **Qt 6.8.x** with the **`msvc2022_64`** kit and the **Multimedia** module,
   via the Qt Maintenance Tool (`C:\Qt\MaintenanceTool.exe` → "Add or remove
   components" → expand the Qt 6.8.x version → check `MSVC 2022 64-bit` and
   `Additional Libraries > Qt Multimedia`).
2. Build from a Visual Studio 2022 x64 command prompt (or run `vcvars64.bat` first),
   using the Qt 6.8.x msvc2022_64 kit's `qmake`:
   ```
   qmake mapmap.pro CONFIG+=release
   nmake
   ```
   See `build_msvc2022.bat` in the repo root for a script that does this end to end.
3. The built binary lands at `bin/mapmap.exe` inside the project folder (set via
   `DESTDIR` in `mapmap.pro`).
4. To open/build in the Visual Studio 2022 IDE instead of the command line, generate
   a solution file: `qmake -tp vc mapmap.pro CONFIG+=release`, then open the resulting
   `mapmap.sln`.
5. `bin/mapmap.exe` is dynamically linked against Qt6 and won't run standalone (outside
   Qt Creator/VS, which inject the Qt `bin` dir onto `PATH` automatically) until you
   deploy the required DLLs alongside it:
   ```
   C:\Qt\6.8.3\msvc2022_64\bin\windeployqt.exe --release --no-system-d3d-compiler bin\mapmap.exe
   ```
   See `deploy_msvc2022.bat` for a script that does this.

#### MSVC-specific source notes

A couple of fixes were needed for this codebase to compile under MSVC (it was
previously only built with MinGW upstream):

- `src/core/Util.h`, `src/core/Source.h`, `src/gui/SourceGui.h`,
  `src/gui/ShapeGraphicsItem.h`, `src/gui/LayerGui.h`: include `<windows.h>` before
  `<GL/gl.h>` on Windows — the Windows SDK's `gl.h` requires Windows types
  (`WINGDIAPI`, `APIENTRY`) to already be defined.
- `src/gui/ShapeGraphicsItem.cpp`: a GNU-only compound-literal cast
  (`(CacheQuadMapping){ ... }`) was replaced with standard brace-initialization
  (`CacheQuadMapping{ ... }`), since MSVC doesn't support that GCC/Clang extension.

Editing translations
--------------------
You might need to update the files:
  
```
cd src/mapmap
lupdate mapmap.pro 
```

Then, do this:

```  
lrelease mapmap.pro
```
