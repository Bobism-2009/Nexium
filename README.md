# Nexium

A lightweight native Windows IDE for the [Nexa](https://github.com/Bobism-2009/Nexa-Lang) programming language, built with Dear ImGui and Direct3D 11. It includes a code editor, file explorer, integrated terminal, live diagnostics and an AI coding agent.

## Features

**Editor**
- Syntax highlighting for Nexa, plus C/C++, JSON, Markdown, shell/batch/PowerShell/Makefile and INI/TOML/YAML
- Nexa-aware highlighting: f-string interpolation, enum members, struct fields, `inline_cpp!` bodies as C++, and invalid escapes or literals shown in red
- Rainbow brackets, matching-bracket highlight, highlighting of other uses of the word under the caret, and indent guides
- Live diagnostics from the Nexa parser, with error squiggles, hover messages and markers in the scrollbar
- Completion for keywords, types, `std/` modules and their functions, and symbols in the file
- Go to definition (F12 or Ctrl+click), a symbol outline and a breadcrumb bar
- Find and replace with match-case and whole-word options
- Line editing: move, copy, duplicate, delete, comment, and indent or outdent
- Undo that treats multi-line edits as one step, and keeps each file's original line endings (CRLF or LF)

**Workspace**
- File explorer, workspace search and a Problems panel
- Quick open (Ctrl+P): fuzzy file search; type `>` for commands, `@` for symbols, `:` for a line number
- Integrated terminal (ConPTY), and Run/Build through the Nexa compiler

**AI agent**
- Chat and Agent modes, with support for OpenAI, Anthropic, OpenRouter, Groq, Ollama or any OpenAI-compatible endpoint
- The agent can read, edit and search files, and run commands and Nexa builds in the open folder. Its edits to open files can be undone.

## Requirements

- Windows 10 or 11
- A MinGW-w64 toolchain (g++ with C++17, `mingw32-make`, `windres`)
- The [Nexa-Lang](https://github.com/Bobism-2009/Nexa-Lang) sources. Nexium uses Nexa's lexer and parser headers for diagnostics.
- The Nexa compiler (`Nexa`, `NexaC` or `nexac`) on `PATH`, to run and build programs

## Building

```sh
mingw32-make NEXA_LANG=D:/Projects/Nexa-Lang
```

`NEXA_LANG` points at your Nexa-Lang checkout; the build uses its `include/` folder. The result is `Nexium.exe` in the repository root.

### Installer

`NexiumSetup.exe` is a single-file graphical installer written in Nexa (`installer/setup.nxa`). Build it with:

```sh
mingw32-make installer
```

This compiles the setup with NexaC and uses `installer/pack.nxa` to append `Nexium.exe`, the MinGW runtime DLLs it needs (`libstdc++-6.dll`, `libgcc_s_seh-1.dll` and the thread runtime), its icon, the README and the license to the setup executable. Only `Nexium.exe` and `NexiumSetup.exe` are left afterwards. Set `MINGW_BIN` if the runtime DLLs are not in your compiler's `bin` folder.

The installer can:
- install for the current user (no admin rights needed) or for all users (asks for administrator permission)
- create Start menu and desktop shortcuts, open `.nxa` files with Nexium, and add Nexium to PATH
- if the Nexa compiler is not installed, offer to download the latest NexaC release and run its installer
- register Nexium in Apps & features, with an uninstaller that reverses all of this and restores the previous `.nxa` handler

For unattended installs:

```sh
NexiumSetup.exe --silent [--scope User|Machine] [--dir <path>] [--desktop 0|1] [--startmenu 0|1] [--assoc 0|1] [--path 0|1] [--nexa 0|1]
"<install dir>\Uninstall.exe" --uninstall --silent
```

You can open files or a folder from the command line:

```sh
Nexium.exe path\to\main.nxa
```

## Configuration

Settings are stored in `%APPDATA%\Nexium\settings.json`, including the open folder, panel layout, compiler name and agent provider.

The agent reads API keys from the Settings panel. If none is set there, it falls back to these environment variables: `OPENAI_API_KEY`, `ANTHROPIC_API_KEY`, `OPENROUTER_API_KEY` or `GROQ_API_KEY`.

## Keyboard shortcuts

| Action | Shortcut |
| --- | --- |
| Quick open / command palette | Ctrl+P / Ctrl+Shift+P (or F1) |
| Go to symbol / line | Ctrl+Shift+O / Ctrl+G |
| Go to definition | F12, Ctrl+click |
| Find / replace | Ctrl+F / Ctrl+H, F3 / Shift+F3 |
| Toggle line comment | Ctrl+/ |
| Move / copy line | Alt+Up/Down, Shift+Alt+Up/Down |
| Duplicate / delete line | Ctrl+D / Ctrl+Shift+K |
| Select line | Ctrl+L |
| Trigger suggestions | Ctrl+Space |
| Jump to matching bracket | Ctrl+Shift+\ |
| Save / save all | Ctrl+S / Ctrl+K S |
| Switch tabs | Ctrl+Tab, Ctrl+PageUp/PageDown |
| Run / build / stop | F5 / Ctrl+Alt+B / Shift+F5 |
| Toggle sidebar / panel / agent | Ctrl+B / Ctrl+J / Ctrl+Shift+L |
| Problems / terminal | Ctrl+Shift+M / Ctrl+` |

## Project layout

```
src/
  main.cpp        Window, Direct3D 11 setup, fonts, main loop
  ide.cpp         Layout, title bar and menus, tabs, status bar, quick open, shortcuts
  editor.cpp      Text buffer, undo, editing commands, editor rendering, find
  highlight.cpp   Syntax highlighters, Nexa outline, diagnostics, completion
  workspace.cpp   Explorer, search, outline and problems views
  terminal.cpp    ConPTY terminal, VT screen buffer, run/build
  agent.cpp       AI agent: providers, streaming, tools
  util.cpp        Paths, file IO, JSON, settings, HTTP
installer/        Nexa setup wizard (setup.nxa) and payload packer (pack.nxa)
res/              Icon and resource script
third_party/      Dear ImGui
```

## License

Nexium is released under the MIT License; see [LICENSE](LICENSE). Dear ImGui is also MIT-licensed (see `third_party/imgui/LICENSE.txt`).
