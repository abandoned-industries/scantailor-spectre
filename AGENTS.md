# Repository Guidelines

## Project Structure & Module Organization
- `src/app/` holds the Qt UI and application entry points (MainWindow, dialogs).
- `src/core/` contains the processing pipeline, filters, IO, and PDF import/export.
- `src/core/filters/` implements the 8-stage scan pipeline (fix orientation → export).
- `src/imageproc/`, `src/dewarping/`, `src/math/`, and `src/foundation/` provide the core algorithms and utilities.
- `src/resources/` stores icons, QSS themes, translations, and bundled assets.
- `packaging/macos/` contains macOS packaging scripts and build notes.
- `cmake/` includes shared CMake modules.

## Build, Test, and Development Commands
- Configure and build (macOS/Homebrew):
  ```bash
  mkdir build && cd build
  cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=$(brew --prefix qt6) ..
  make -j$(sysctl -n hw.ncpu)
  ```
- Run the app from the build directory:
  ```bash
  open "ScanTailor Spectre.app"
  ```
- Create a DMG (macOS):
  ```bash
  cd packaging/macos
  ./create-dmg.sh /path/to/build
  ```

## Coding Style & Naming Conventions
- C++/ObjC++ sources follow a compact style: 2-space indentation, braces on the same line.
- Types use `PascalCase`, methods/variables use `camelCase`.
- Keep file names aligned with class names (e.g., `PdfReader.h/.mm`).
- Prefer Qt types (`QString`, `QImage`) where already used.

## Testing Guidelines
- Unit tests live under `src/*/tests/` (e.g., `src/imageproc/tests/`).
- Test files typically use the `Test*.cpp` naming pattern.
- Run from the build directory:
  ```bash
  ctest --output-on-failure
  ```

## Commit & Pull Request Guidelines
- Recent history uses short, imperative summaries (e.g., `WIP: ...`, `Revert "..."`).
- Keep commits focused by subsystem (`core`, `imageproc`, `packaging`).
- In PRs, describe platform tested, build type, and any UI-visible changes (screenshots if UI changes).

## Architecture Notes
- The app processes pages through 8 filter stages defined in `src/core/filters/`.
- PDF import/export live in `src/core/PdfReader.*` and `src/core/PdfExporter.*`.

## Agent-Specific Instructions
- Follow `CLAUDE.md` for session startup, build logging, and timestamped DMG naming.
