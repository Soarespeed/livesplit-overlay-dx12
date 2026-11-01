# Building from source

This add-on is a single C++ file compiled into a ReShade add-on DLL
(`.addon64`). You do not need Visual Studio with a full IDE — just the MSVC
command-line compiler.

## 1. Install the compiler

Download **Build Tools for Visual Studio** (free) from Microsoft:
https://visualstudio.microsoft.com/downloads/ (look for "Build Tools for Visual
Studio" under "Tools for Visual Studio").

During install, check the workload **"Desktop development with C++"**. That gives
you `cl.exe` (the MSVC compiler) and the Windows SDK.

## 2. Get the headers

The add-on needs two sets of headers that are NOT included in this repository,
because they have their own licenses:

- **ReShade add-on headers** — from the ReShade SDK:
  https://github.com/crosire/reshade (the `include` folder), matching the ReShade
  version you target. This project was built against ReShade API version 20.
- **Dear ImGui headers** — version **1.92.5 (docking branch)**:
  https://github.com/ocornut/imgui/tree/docking

Put all of those `.h` / `.hpp` files together in one folder, for example
`include\`, next to the source file.

Expected layout:

```
your-project\
  src\
    livesplit_overlay.cpp
  include\
    reshade.hpp
    reshade_api_device.hpp
    ... (rest of the ReShade headers)
    imgui.h
    imgui_internal.h
    ... (rest of the ImGui headers)
```

## 3. Compile

Open **"x64 Native Tools Command Prompt for VS"** (installed with the Build
Tools), `cd` into the project folder, and run:

```bat
cl /nologo /std:c++17 /EHsc /O2 /LD /DNDEBUG /Iinclude /Iinclude\imgui src\livesplit_overlay.cpp /Fe:livesplit_overlay.addon64 /link user32.lib gdi32.lib
```

What the flags mean:

- `/std:c++17` — the code uses C++17.
- `/O2` — optimize for speed.
- `/LD` — build a DLL (the add-on is a DLL).
- `/Iinclude` — where to find the ReShade and ImGui headers.
- `/Fe:livesplit_overlay.addon64` — output name. ReShade add-ons use the
  `.addon64` extension for 64-bit games.
- `/link user32.lib gdi32.lib` — the add-on uses the Windows GDI API
  (`PrintWindow`, `CreateDIBSection`, etc.) to capture the LiveSplit window.

If it compiles with no errors, you get `livesplit_overlay.addon64` in the folder.

## 4. Test

Copy `livesplit_overlay.addon64` into a game folder that already has ReShade with
add-on support, next to `ReShade.ini`. Launch the game, open the ReShade menu
(Home), and the `livesplit_overlay` section appears under the **Add-ons** tab.

## Notes

- Only `livesplit_overlay.cpp` is the add-on. The other source files in the
  history (`_cmdlist`, `_text`) were experiments and are not the shipped version.
- If `cl` is not recognized, you are not in the "x64 Native Tools Command Prompt".
  Open that specific prompt from the Start menu — a normal cmd will not have the
  compiler on PATH.

  
