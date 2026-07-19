# Toolchain setup (Windows)

This project has not been compiled yet, because this machine currently has no working
C++ toolchain. Specifically:

- `g++`, `clang++`, `cl`, `make` and `cmake` are all absent from `PATH`.
- Visual Studio 2019 **Build Tools** are installed at
  `C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\`, but the installation is
  incomplete: there is **no Windows SDK** (`C:\Program Files (x86)\Windows Kits\10\Include`
  does not exist) and `vswhere.exe` is missing. Running `vcvars64.bat` leaves `INCLUDE`
  empty, so `cl` cannot find even `assert.h` or `math.h`. It cannot compile hello-world in
  this state, let alone this project.

Pick one of the two fixes below.

## Option A — MSYS2 / MinGW-w64 (recommended)

Simplest path: self-contained, no Windows SDK needed, and gives you `g++` plus `cmake`
in one go.

1. Install [MSYS2](https://www.msys2.org/).
2. In the MSYS2 UCRT64 terminal:

   ```sh
   pacman -S mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja
   ```

3. Add `C:\msys64\ucrt64\bin` to your `PATH`.
4. Verify: `g++ --version` and `cmake --version`.

## Option B — repair the Visual Studio Build Tools

Use this if you would rather stay on MSVC.

1. Open **Visual Studio Installer** → Modify the *Build Tools 2019* installation.
2. Under **Individual components**, tick:
   - *MSVC v142 - VS 2019 C++ x64/x86 build tools*
   - ***Windows 10 SDK*** (or Windows 11 SDK) — this is the missing piece
   - *C++ CMake tools for Windows*
3. Install, then build from a **x64 Native Tools Command Prompt for VS 2019**.

MSVC 2019 (14.29) supports C++17, which is all this project needs.

## Building once a toolchain exists

```sh
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build --output-on-failure
```

Run the tests first. `tests/test_rules.cpp` covers board geometry, both openings, sumito
pushing (including push-offs and illegal equal-strength pushes), broadside legality,
move-generation invariants, and the optional move limit. If those pass, the rules are sound.
