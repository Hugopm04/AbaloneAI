# Toolchain setup (Windows)

**Resolved — this machine now builds the project.** The working toolchain is MSYS2 /
MinGW-w64 (Option A below):

- `g++` 16.1.0 and `ninja` 1.13.2 from `C:\msys64\ucrt64\bin` (on `PATH`).
- `cmake` 4.4.0 from `C:\Program Files\CMake\bin`.

Use the **Ninja** generator. CMake otherwise defaults to the Visual Studio 2019 generator,
which still does not work here — see the note under Option B.

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
```

## Option A — MSYS2 / MinGW-w64 (what is installed)

Self-contained, no Windows SDK needed.

1. Install [MSYS2](https://www.msys2.org/) — `winget install MSYS2.MSYS2`.
2. Install the compiler:

   ```sh
   pacman -S --needed mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-ninja
   ```

   (CMake is already installed system-wide, so the `mingw-w64-ucrt-x86_64-cmake` package
   is not needed.)

3. Add `C:\msys64\ucrt64\bin` to your `PATH`.
4. Verify: `g++ --version` and `cmake --version`.

## Option B — repair the Visual Studio Build Tools

Not needed now that Option A works; keep this only if you would rather stay on MSVC.

The 2019 **Build Tools** at
`C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\` are still incomplete:
there is **no Windows SDK** (`C:\Program Files (x86)\Windows Kits\10\Include` does not
exist) and `vswhere.exe` is missing, so `vcvars64.bat` leaves `INCLUDE` empty and `cl`
cannot find even `assert.h`. This is why the VS generator must be avoided.

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
