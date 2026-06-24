---
description: Build and run MyMapMap (Qt/MSVC on Windows)
---

# Build

Run from the repo root in PowerShell:

```powershell
cd H:\Github\MyMapMap; & .\build_msvc2022.bat 2>&1 | Select-Object -Last 40
```

The script calls `vcvars64.bat` to set up the MSVC environment, runs `qmake mymapmap.pro CONFIG+=release`, then `nmake`. Output binary: `bin\mymapmap.exe`.

Expected last line on success: `BUILD_OK`

# Run

```powershell
Start-Process H:\Github\MyMapMap\bin\mymapmap.exe
```

Or just double-click `bin\mymapmap.exe` in Explorer.

# Prerequisites

- Visual Studio 2022 Community at `C:\Program Files\Microsoft Visual Studio\2022\Community`
- Qt 6.11.1 msvc2022_64 at `C:\Qt\6.11.1\msvc2022_64`
- Qt Multimedia module installed (required; HttpServer is optional — MCP server disabled without it)
