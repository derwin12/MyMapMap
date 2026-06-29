Build instructions
==================

Build on GNU/Linux
------------------

Install the dependencies.

Build it:

```
qmake mymapmap.pro
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

#### One-shot build script

`build_ubuntu.sh` is the Linux counterpart of `build_msvc2022.bat`: it installs
the dependencies above (via `sudo apt-get`) and then configures and builds with
CMake + Ninja. The binary is written to `bin/mymapmap`.

```
./build_ubuntu.sh            # install deps, then build
./build_ubuntu.sh --no-deps  # skip apt, just configure + build
```

To build manually with CMake instead:

```
cmake -B build-linux -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-linux
```

> The `qt6-base-private-dev` package is required — MyMapMap uses Qt private
> headers (`<private/qzipwriter_p.h>`) for project package export.

#### Packaging an AppImage

To produce a self-contained, distributable `MyMapMap-<version>-x86_64.AppImage`
(bundles the Qt runtime, including the Multimedia ffmpeg plugin used by video
playback, video export and FPP Connect):

```
./build_ubuntu.sh
./scripts/sh_make_appimage.sh
```

The script downloads `linuxdeploy` and its Qt plugin on first run. CI builds the
same AppImage on every push — see `.github/workflows/ubuntu-build.yml`.

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

If the OSC port number in preferences appears corrupted, reset preferences:

```
rm -f ~/Library/Preferences/info.mapmap.MapMap.plist
```

Build on Windows
----------------

### MyMapMap fork: building with Visual Studio 2022 / MSVC (recommended)

This is the primary tested Windows build path for this fork.

1. Install **Qt 6.11.x** with the **`msvc2022_64`** kit and the **Multimedia** module,
   via the Qt Maintenance Tool (`C:\Qt\MaintenanceTool.exe` → "Add or remove
   components" → expand the Qt 6.11.x version → check `MSVC 2022 64-bit` and
   `Additional Libraries > Qt Multimedia`).

2. Build from a Visual Studio 2022 x64 command prompt (or run `vcvars64.bat` first):
   ```
   build_msvc2022.bat
   ```
   This runs `qmake mymapmap.pro CONFIG+=release` then `nmake` automatically.
   Alternatively run qmake and nmake manually:
   ```
   qmake mymapmap.pro CONFIG+=release
   nmake
   ```

3. The built binary lands at `bin\mymapmap.exe` inside the project folder.

4. To open in the Visual Studio 2022 IDE, generate a solution file first:
   ```
   gen_sln.bat
   ```
   Then open `mymapmap.sln`.

5. `bin\mymapmap.exe` is dynamically linked against Qt6 and requires Qt DLLs alongside
   it to run outside the IDE. Deploy them with:
   ```
   deploy_msvc2022.bat
   ```
   which runs:
   ```
   C:\Qt\6.11.1\msvc2022_64\bin\windeployqt.exe --release --no-system-d3d-compiler bin\mymapmap.exe
   ```

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

### Build with MinGW (upstream / legacy)

The upstream project used MinGW. This path is not regularly tested in this fork.

**Dynamic build (for development):**
- Download and install [Qt6 MinGW incl. QtCreator](https://www.qt.io/download-open-source/)
- Build and run within QtCreator (Ctrl-R)

**Static build (for release):**
- Build a [Qt static environment](https://wiki.qt.io/Building_a_static_Qt_for_Windows_using_MinGW)
- Build using QtCreator (qmake, build release)

**Packaging:**
- Open Qt terminal from the Start Menu and run:
  `windeployqt --release --no-system-d3d-compiler <path-to-app-binary>`
- Replace all `*.qm` files in the release `translations` folder with those from the source tree
- Use [Inno Setup](https://jrsoftware.org/isdl.php) to create an installer

Editing translations
--------------------
You might need to update the files:
  
```
cd src/mapmap
lupdate mymapmap.pro 
```

Then, do this:

```  
lrelease mymapmap.pro
```
