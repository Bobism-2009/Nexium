# Nexium

A lightweight native Windows IDE for the [Nexa](https://github.com/Bobism-2009/Nexa-Lang) programming language, written in Nexa itself. Its interface is drawn with Nexa's `std/ui` module, and the C++ that NexaC generates from it is kept in [`transpiled/`](transpiled/) so you can read exactly what gets compiled.

## Features

**Editor**
- Sharp code text: Cascadia Mono (or Consolas, or any font you choose) drawn with ClearType
- Syntax highlighting for Nexa, plus C/C++, JSON, Markdown (with highlighted code fences), shell/batch/PowerShell/Makefile and INI/TOML/YAML
- Nexa-aware highlighting: f-string interpolation, escapes, enum variants, module calls, `#include <std/...>` names checked against the real module list, and invalid escapes or literals shown in red
- Rainbow brackets, matching-bracket boxes, highlighting of other uses of the word under the caret, and indent guides
- Problems from the Nexa compiler as you save: squiggles, gutter markers, hover messages and marks in the scrollbar
- Completion for keywords, types, `std/` module functions and words in the file
- Go to definition (F12 or Ctrl+click), across the open folder's `.nxa` files, and an Outline view of the file's symbols
- A minimap beside the scrollbar
- Find and replace with match-case and whole-word options
- Line editing: move, copy, duplicate, delete, comment, and indent or outdent
- Auto-closing brackets and quotes, smart indentation, and undo that groups typing
- Keeps each file's line endings (CRLF or LF)

**Workspace**
- File explorer with new file/folder, rename and delete, workspace search and a Problems panel
- Quick open (Ctrl+P): fuzzy file search; type `>` for commands, `@` for symbols, `:` for a line number
- Menus and a command palette covering every command
- Integrated terminal (cmd.exe), and Build/Run through NexaC with the output in the panel
- Reopens your folder and files, and reloads files changed on disk

**AI agent**
- Chat and Agent modes, with OpenAI, Anthropic, OpenRouter, Groq, Ollama or any OpenAI-compatible endpoint
- The agent can read, edit, create, rename and delete files, search the folder, run commands and build with NexaC. Its edits to open files can be undone.
- Answers render Markdown with highlighted code blocks; each tool call shows as a card you can expand to see what it did

## Requirements

- Windows 10 or 11
- [NexaC](https://github.com/Bobism-2009/Nexa-Lang) 0.1.15 or newer (for `std/ui`) with the C++ compiler it uses
- `windres` (from MinGW-w64 or llvm-mingw) for the icon, and `mingw32-make`

To run and build programs from inside Nexium, the Nexa compiler (`NexaC`, `Nexa` or `nexac`) must be on `PATH`, or set in Settings.

## Building

```sh
mingw32-make
# or, if the NexaC on PATH is older than 0.1.15:
mingw32-make NEXAC=path/to/NexaC.exe
```

This builds `Nexium.exe` from `nexium/main.nxa` and writes the generated C++ to `transpiled/nexium.cpp`. Nexium.exe links the C++ runtime statically, so it runs on its own with no DLLs. `mingw32-make transpiled` regenerates only the C++.

The transpiled file also builds without NexaC, with an llvm-mingw or MinGW-w64 compiler:

```sh
windres -I res -O coff -o res/nexium_res.o res/nexium.rc
clang++ -std=c++17 -O2 transpiled/nexium.cpp res/nexium_res.o -o Nexium.exe -static -mwindows -luser32 -lole32 -lshell32 -lgdi32 -lwindowscodecs -lcomdlg32 -lwinmm
```

### Installer

`NexiumSetup.exe` is a single-file graphical installer, also written in Nexa (`installer/setup.nxa`) and drawn with `std/ui`. Build it with:

```sh
mingw32-make installer NEXAC=path/to/NexaC.exe
```

This compiles the setup with NexaC and uses `installer/pack.nxa` to append `Nexium.exe`, its icon, the README and the license to the setup executable. Only `Nexium.exe` and `NexiumSetup.exe` are left afterwards.

The installer can:
- install for the current user (no admin rights needed) or for all users (asks for administrator permission)
- create Start menu and desktop shortcuts, open `.nxa` files with Nexium, and add Nexium to PATH
- if the Nexa compiler is not installed, offer to download the latest NexaC release and run its installer
- register Nexium in Apps & features, with an uninstaller that reverses all of this and restores the previous `.nxa` handler
- update an existing install in place

For unattended installs:

```sh
NexiumSetup.exe --silent [--scope User|Machine] [--dir <path>] [--desktop 0|1] [--startmenu 0|1] [--assoc 0|1] [--path 0|1] [--nexa 0|1]
"<install dir>\Uninstall.exe" --uninstall --silent
```

You can open a file or a folder from the command line:

```sh
Nexium.exe path\to\main.nxa
Nexium.exe path\to\project
```

## Configuration

Settings are stored in `%APPDATA%\Nexium\settings.json`: the open folder and files, recent folders, panel layout, font, the compiler and run arguments, and the agent's provider, model and API keys. Open them from the gear in the activity bar (Ctrl+,).

The agent reads API keys from Settings. If none is set there, it falls back to these environment variables: `OPENAI_API_KEY`, `ANTHROPIC_API_KEY`, `OPENROUTER_API_KEY` or `GROQ_API_KEY`.

## Keyboard shortcuts

| Action | Shortcut |
| --- | --- |
| Quick open / command palette | Ctrl+P / Ctrl+Shift+P (or F1) |
| Go to symbol / line | Ctrl+Shift+O / Ctrl+G |
| Go to definition | F12, Ctrl+click |
| Find / replace | Ctrl+F / Ctrl+H, F3 / Shift+F3 |
| Find in files | Ctrl+Shift+F |
| Toggle line comment | Ctrl+/ |
| Move / copy line | Alt+Up/Down, Shift+Alt+Up/Down |
| Duplicate / delete line | Ctrl+D / Ctrl+Shift+K |
| Select line | Ctrl+L |
| Trigger suggestions | Ctrl+Space |
| Jump to matching bracket | Ctrl+Shift+\ |
| Save / save all | Ctrl+S / Ctrl+K S |
| Open folder | Ctrl+K Ctrl+O |
| Switch tabs | Ctrl+Tab, Ctrl+PageUp/PageDown |
| Build and run / build / stop | F5 / Ctrl+Shift+B / Shift+F5 |
| Toggle sidebar / panel / agent | Ctrl+B / Ctrl+J / Ctrl+Shift+L |
| Problems / output / terminal | Ctrl+Shift+M / Ctrl+Shift+U / Ctrl+` |
| Font size | Ctrl+= / Ctrl+- / Ctrl+0 |

## Project layout

```
nexium/
  main.nxa        Entry point: main loop, input routing, shortcuts, settings
  state.nxa       Every global, in one place (NexaC emits globals in file order)
  draw.nxa        Drawing helpers over std/gfx and std/ui, and line icons
  text.nxa        UTF-8 helpers, visual columns, one-line text fields
  highlight.nxa   Syntax highlighters and the word tables behind completion
  buffer.nxa      Documents: open, save, tabs, undo, editing primitives
  editor.nxa      The code editor: keys, mouse, find, completion, drawing
  workspace.nxa   Explorer, workspace search, outline, quick open
  commands.nxa    The command table, menus, build and run
  panel.nxa       PROBLEMS, OUTPUT and TERMINAL
  agent.nxa       The AI agent: providers, requests, tools and the chat panel
  chrome.nxa      Layout, title bar, activity bar, tabs, status bar, welcome and settings pages
  native.hpp      The Win32 pieces Nexa's library does not cover yet (key repeat, cursors, dialogs, child processes)
  native_text.hpp ClearType text for code, drawn into the std/gfx framebuffer
  native_http.hpp HTTP requests on a background thread, for the agent
transpiled/       The C++ NexaC generates from nexium/ (regenerated by make)
installer/        Nexa setup wizard (setup.nxa) and payload packer (pack.nxa)
res/              Icon and resource script
```

## License

Nexium is released under the MIT License; see [LICENSE](LICENSE). Programs built with `std/ui`, Nexium included, contain the Inter typeface (SIL Open Font License 1.1) and stb_truetype (public domain); their notices travel in the generated source.
