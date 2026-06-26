# MyMapMap Fork Rules

MyMapMap is a fork of [MapMap](https://github.com/mapmapteam/mapmap), licensed GPLv3.
The `upstream` git remote points at the original project; `origin` points at this fork
(derwin12/MyMapMap). Build verified working: Qt 6.11.1 (`msvc2022_64` kit + Multimedia
module) with Visual Studio 2022, via `build_msvc2022.bat` (qmake + nmake). Output binary
is `bin/mymapmap.exe`, kept inside the project folder (`DESTDIR` in `mymapmap.pro`; the
upstream `win32` block in `src/src.pri` used to redirect `TARGET` outside the repo —
that override was removed). Full build steps and the MSVC-specific source fixes are
documented in `INSTALL.md` under "MyMapMap fork: building with Visual Studio 2022 / MSVC".

## NEVER push to upstream

`upstream` (mapmapteam/mapmap) is read-only for this project — fetch/merge from it only.
**Never run `git push` with `upstream` as the remote, and never create issues, PRs, or
any other writes against `mapmapteam/mapmap`.** All pushes, issues, and PRs go to
`origin` (derwin12/MyMapMap) only. This rule applies even if a command would otherwise
default to the wrong remote (e.g. `gh issue create` without `--repo` has previously
picked `upstream` by mistake) — always pass `--repo derwin12/MyMapMap` explicitly to
`gh` commands, and double-check the remote before any `git push`.

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

## Keep the wiki up to date

The wiki lives at https://github.com/derwin12/MyMapMap/wiki (git remote:
`https://github.com/derwin12/MyMapMap.wiki.git`, branch `master`).

**After any session that adds, removes, or changes a user-facing feature**, update the
relevant wiki page(s) before wrapping up:

- Clone the wiki repo into a temp directory, edit the affected `.md` file(s), commit, and
  push to `origin master`.
- Pages to keep current: `MCP-Server.md` (tools, endpoint, config), `Getting-Started.md`
  (install/first-run), `Keyboard-Shortcuts.md`, `OSC-Commands.md`.
- If a new feature has no existing page, create one and add a link from `Home.md`.
- The wiki is public — write for an end-user audience, not a developer audience.
