# Changelog

All notable changes to Searchlight 98 are recorded in this file. Versions follow the form
`0.MINOR.PATCH`.

## Unreleased

No changes yet.

## 0.3.4 - 2026-09-28

### Changed

- Searchlight 98 is published by Backport Labs. The license, the setup program, the About box and
  the links in the documentation name it.

## 0.3.3 - 2026-09-28

### Changed

- The source code is divided into one file for each area of the program, with a shared header.
  The program itself is unchanged.
- The build stops on a call to a function that is not declared.

### Fixed

- The top four rows of the backdrop were drawn from memory that had not been filled, which made
  them darker than the rest.

## 0.3.2 - 2026-09-28

First public release.

### Finding and starting programs

- Search across programs, Control Panel settings and documents, by name, word start or initials.
- Automatic grouping of programs. A program is moved to another group by dragging it there.
- Favorites, pinned by drag and drop, with the hotkeys Ctrl+Alt+1 to Ctrl+Alt+5.
- Recently started programs.
- Results ranked by how often each program is started.
- Context menu with Open, Open Containing Folder, Properties, Pin, Hide and Uninstall.

### Commands in the search box

- Expression evaluator. The result can be copied to the Clipboard.
- Run: program names, paths and web addresses, as in Start, Run.
- Screen modes, with automatic return to the previous mode unless confirmed.
- Countdown to shut down or restart, and the power commands Restart, Log Off, Stand By and
  Restart in MS-DOS Mode.

### Running programs

- Open windows and background processes, with filtering.
- Switch to, close, or end a program. Windows that have stopped responding are marked.
- Gauges for processor, memory and system resources.
- Memory of each program, excluding the Windows libraries shared between programs.
- Details box with the figures behind the gauges and the memory used by Searchlight itself.

### Startup programs

- Entries from the registry, the Startup folders and `WIN.INI`.
- Entries are switched off and on without deleting them, in the locations used by the System
  Configuration Utility.

### General

- Tray icon and configurable hotkeys.
- Options for icons, loading of icons, the backdrop with adjustable opacity, and the favorites
  hotkeys.
- Blurred, darkened backdrop while the panel is open.
- Setup program with uninstaller.
