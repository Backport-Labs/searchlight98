# Contributing to Searchlight 98

Contributions are welcome. This document describes how to report problems, how to build and test
the program, and what a pull request should contain.

## Scope

Searchlight 98 is a program launcher for Windows 95 and Windows 98. Changes are judged against
three goals:

- It runs on an unmodified Windows 95 or Windows 98 installation.
- It stays small, in the executable and in memory.
- It remains a single program without dependencies of its own.

Proposals that need a newer operating system, an additional runtime or a background indexer are
outside the scope of the project.

## Reporting a problem

Open an issue and include:

- The version of Searchlight 98, shown in the About box.
- The version of Windows, and whether it runs on hardware or in a virtual machine.
- The steps that lead to the problem, and what happened instead of what you expected.
- The contents of `SLIGHT98.LOG` from the installation folder.
- For problems with the gauges or memory figures, the text of the Details box
  (F1 in the Running view).

Report security problems privately. See [SECURITY.md](SECURITY.md).

## Building

Requirements:

- [Tiny C Compiler 0.9.27](http://download.savannah.gnu.org/releases/tinycc/), 32-bit Windows build
- [Inno Setup 5.4.3](https://files.jrsoftware.org/is/5/), non-Unicode
- Windows PowerShell 5.1

```powershell
.\build\build.ps1 -Tcc C:\tools\tcc\tcc.exe -Iscc "C:\tools\Inno Setup 5\ISCC.exe"
```

The build runs on current versions of Windows. It compiles `SLIGHT98.EXE`, runs the self-test and
builds `dist\SL98SETUP.EXE`. The same steps run automatically for every pull request.

## Testing

Run the build script. It fails if the output of the self-test differs from `tests\EXPECTED.OUT`.

When a change affects behaviour covered by the self-test, add lines to `tests\TESTS.TXT` and the
matching lines to `tests\EXPECTED.OUT`. The line formats are described by the comments in
`SelfTest` in `src\slight98.c`.

Changes to the user interface should be checked with `/shot`, which renders the panel to a bitmap,
and on Windows 98 itself, on hardware or in a virtual machine. State in the pull request which
versions of Windows the change was tested on.

### Test mode

`/selftest` and `/shot` run on a development computer, so they must never change it. Code that
writes to the registry, moves files, changes the screen mode, ends a process or shuts Windows
down must return early when `g_testMode` is set. Test mode uses fixed sample data for running
programs, startup entries and documents.

## Coding guidelines

- **Language.** C, as accepted by Tiny C Compiler 0.9.27. Declarations come before statements.
- **Platform.** Use functions present in Windows 95. A function that exists only on later
  versions is looked up at run time with `GetProcAddress`, and the program must work without it.
- **Character set.** Use the ANSI variants of Windows functions. Windows 95 and 98 do not
  implement most Unicode variants.
- **Dependencies.** Do not add libraries. Imports from system libraries that the compiler does
  not cover are listed in `src\*.def`.
- **Memory.** Allocate tables when they are needed and free them when they are not. Avoid
  fixed-size buffers in structures that exist once per program entry.
- **Comments.** Explain why, in plain language. A comment should help someone who knows C but
  not the Windows 9x specifics involved.
- **Files.** Text files use Windows line endings. Files that contain accented characters and are
  read on Windows 9x use the Windows-1252 character set. See `.gitattributes`.
- **User interface text.** Short, plain sentences. The user guide in `app\README.TXT` is updated
  together with the feature it describes.

## Pull requests

1. Fork the repository and create a branch for the change.
2. Keep each pull request to one topic.
3. Run the build script and confirm that the self-test passes.
4. Update `app\README.TXT`, `README.md` and `CHANGELOG.md` where the change is visible to users.
5. Describe what changed, why, and how it was tested.

Changes reach `main` through pull requests. Direct pushes, force pushes and deletion of `main`
are restricted.

## Versions

Versions follow the form `0.MINOR.PATCH`. The minor number changes with new features, the patch
number with fixes and smaller changes. The version is set in `APP_VERSION` in `src\slight98.c`
and in `AppVersion` in `installer\SLIGHT98.ISS`.

## License

Searchlight 98 is released under the MIT License. By contributing you agree that your
contribution is released under the same license.
