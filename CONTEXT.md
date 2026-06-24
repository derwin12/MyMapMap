# MyMapMap Session Context

## Current Task
Polishing UI and project hygiene; all items this session committed to dev.

## Key Decisions
- Light/dark theme via `MainApplication::applyTheme()` + `refreshIcons()` in MainWindow; SVG colorization replaces `stroke="white"` for light mode
- Check-for-updates uses 3-button dialog (download / ignore version / remind later); ignored version stored in QSettings `skipVersion`
- App icon (`mymapmap.ico`) generated from code (Pillow); RC metadata updated to MyMapMap 1.0.0

## Next Steps
- Decide whether to commit `examples/MyDemo.mmp` and `.claude/settings.local.json`
- Next feature work: Task #6 Spout input/output, Task #7 NDI, or Task #10 Basic cue system
- CI (Task #14) still blocked on aqt supporting Qt 6.11.1
