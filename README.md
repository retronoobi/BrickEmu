# BrickEmu Libretro 1.0.1

BrickEmu is a port of BrickEmuPy created by azya52.
The port was created entirely by AI.

Native libretro core for LCD handheld games, developed for 64-bit Windows and
compatible with the base API provided by RetroArch 1.7.5, as used by EmuVR.

## Supported games

- E23 Plus Mark II 96-in-1
- E88 8-in-1
- E33 2-in-1
- Apollo 18-in-1 B0302
- Apollo 126-in-1 B0202
- Keychain 55-in-1
- GA878
- GA888
- Micon KC32

Video, audio, controls, and deterministic save states are supported for all
nine games. The LCD segment masks and sound ROM data required by the hardware
are embedded in generated core headers. SVG files, Python, ImageMagick, and the
complete BrickEmuPy project are not required to compile or run this package.

Game ROMs are not included.

## Automatic power-off

The core option `Prevent automatic power-off` is enabled by default. It
automatically wakes the handheld when its firmware attempts to enter deep sleep
after approximately five to six minutes without input. This keeps consoles
active in EmuVR without changing video, audio, controls, or save states.

Set this option to `disabled` to restore the original automatic power-off
behavior. While prevention is enabled, the power button will not keep the
handheld permanently turned off.

## Build dependencies

- 64-bit Windows 10 or Windows 11.
- Visual Studio 2022 Community or Visual Studio Build Tools 2022.
- The `Desktop development with C++` workload.
- The MSVC v143 compiler and a Windows 10/11 SDK, installed by that workload.

There are no external libraries, submodules, or additional downloads.

## Building

1. Install Visual Studio 2022 or Build Tools with the C++ desktop workload.
2. Open `x64 Native Tools Command Prompt for VS 2022` from the Start menu.
3. Enter the extracted source package directory:

```bat
cd C:\path\to\brickemu-libretro-1.0.1-source
```

4. Run:

```bat
build_msvc.bat
```

The resulting core will be created at:

```text
build\brickemu_libretro.dll
```

All object and auxiliary build files also remain inside the `build` directory.

## Installation

Copy `brickemu_libretro.dll` to the RetroArch/EmuVR core directory. The
`brickemu_libretro.info` file may be copied to the RetroArch `info` directory
when core metadata is used.

Load any supported `.bin` ROM directly. The core identifies each game by ROM
content rather than relying only on its filename.

[Download: brickemu_libretro.dll +info](https://github.com/user-attachments/files/30235393/brickemu_libretro.zip)
[Assts](https://github.com/retronoobi/BrickEmu/tree/main/assets)

## Source package contents

- Native HT943, EM73000, SPL0X, and KS56CX2X processor implementations.
- A libretro interface compatible with RetroArch 1.7.5.
- Pre-generated and embedded LCD masks and sound data.
- MSVC build script, exports file, and core metadata.

License: CC0 1.0 Universal. See `LICENSE`.
