# MyMapMap — fork guidelines

MyMapMap is a fork of [MapMap](https://github.com/mapmapteam/mapmap), licensed GPLv3.
The `upstream` git remote points at the original project; `origin` points at this fork
(derwin12/MyMapMap). Build toolchain: Qt 5.15.2 (msvc2019_64 kit) with Visual Studio
2022 (binary-compatible MSVC toolset) — no Qt6 is required despite INSTALL.md.

## GPLv3 obligations (apply on any distribution outside this machine)

- Any binary or source distributed to others must remain GPLv3 — do not relicense,
  do not ship a closed-source build.
- Include source code, or a written offer for it, with any distributed binary.
- Preserve existing copyright headers in source files; don't strip attribution.
- Keep visible credit to the MapMap project/team (see README) — don't imply this is
  an official MapMap release or that the MapMap team endorses changes made here.
- None of the above applies to purely private/local use — only triggers on distribution.

## Pulling upstream changes

`git fetch upstream && git merge upstream/dev` (or rebase) to bring in MapMap fixes.
