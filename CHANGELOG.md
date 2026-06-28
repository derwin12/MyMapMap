# Release notes for MyMapMap

&nbsp;

## 2026-06-28 - MyMapMap 1.0.3

- Fix: pan and zoom-to-cursor work correctly at all zoom levels
- Fix: resolve input canvas zoom-jump and cross-canvas pan contamination
- Fix: scrollbar jumping, middle-click pan, and pan direction
- Fix: preserve user zoom in output canvas through panel resizes
- Fix: lock output editor coordinate space to projector screen resolution
- Feat: Export Package — bundles .mmp and all media into a zip
- Feat: Open and Save buttons added to main toolbar
- Feat: output resolution preference (720p / 1080p / 2K / 4K)
- Feat: max default recording length preference
- Feat: Text generator source type
- Feat: FreePolygon layer with click-to-draw canvas mode and right-click add/delete vertex
- Feat: FPP MultiSync follower to chase a Falcon Player show clock
- Feat: source preview panel, thumbnail cache, and quick-access sliders
- Feat: thumbnail size picker in Library panel header
- Feat: background reference photo + reset mesh input to source
- UI: source badges, larger thumbnails, button styling, recent-files dedup
- MCP: additional tools — freepolygon, folder source, source vertices, nudge, preview

&nbsp;

## 2026-06-14 - MyMapMap 1.0.2

- Feat: Folder source slideshow with FolderGui panel
- Feat: video recording — output canvas to MP4/AVI
- Feat: audio capture in recordings
- Feat: persistent recording timer in status bar
- Feat: save/restore output screen; reset output to 100% on load
- Feat: sectioned media library (Images / Videos) and Import Folder
- Fix: shape handles hidden on startup
- Fix: FolderSource crash in addLayerItem
- Fix: restore section headers after project load clears source list
- Fix: play/pause toggle and audio device detection

&nbsp;

## 2026-06-07 - MyMapMap 1.0.1

- Feat: per-layer mute with themed icons
- Feat: pre-fill default filename in Save As dialog
- Feat: soft-edge blending per layer
- Fix: false "project has been changed" dialog on quit
- Fix: triangle/mesh/ellipse layers created with all vertices stacked
- Fix: dark theme dropdown readability
- Fix: detect old XML .mmp files and show a clear error
- Fix: image loading from relative URIs
- Fix: video frame pixel conversion moved off GUI thread
- Polish: layer list UI improvements

&nbsp;

## 2026-06-01 - MyMapMap 1.0.0

Initial numbered release of the MyMapMap fork.

- Light/dark theme switching with themed toolbar icons
- Check-for-updates with ignore-version support
- MyMapMap app icon and Windows version metadata
- Portable paths; build output in `bin/mymapmap.exe`
- Renamed OSC namespace and build artifacts from mapmap to mymapmap
- Sample project (MyDemo.mmp)

&nbsp;

## 2026-05-15 - MyMapMap 0.6.3-mymapmap.1

- Forked from [MapMap](https://github.com/mapmapteam/mapmap) (upstream `dev`, 0.6.3)
- Added Visual Studio 2022 / MSVC build support (Qt 6.8.3 msvc2022_64)
- Fixed MSVC compile errors: `windows.h`/`GL/gl.h` include order, brace-init fixes
- Build output stays inside the project folder
- Rebranded application name/title/About dialog to MyMapMap, with attribution to upstream MapMap
