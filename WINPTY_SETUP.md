# WinPTY Integration Guide (MSYS2/MinGW)

## Step 1: Install WinPTY via MSYS2

### Option A: Install from MSYS2 packages (Recommended)
```bash
# Open MSYS2 MINGW64 or UCRT64 terminal
pacman -S mingw-w64-x86_64-winpty
# or for UCRT64:
pacman -S mingw-w64-ucrt-x86_64-winpty
```

### Option B: Copy from MSYS2 installation
Copy files from your MSYS2 installation to the project:

**From:** `C:\msys64\mingw64\` (or `C:\msys64\ucrt64\`)

**To:** `s:\Work\Sourcerer\lib\winpty\`

Files needed:
```
s:\Work\Sourcerer\lib\winpty\
├── bin\
│   ├── msys-winpty.dll    (or libwinpty.dll)
│   └── winpty-agent.exe
├── include\
│   └── winpty.h
└── lib\
    └── libwinpty.dll.a    (import library)
```

## Step 2: Ensure DLL is accessible

The `msys-winpty.dll` (or `libwinpty.dll`) and `winpty-agent.exe` must be:
- In your PATH, OR
- In the same directory as your executable, OR
- Copied to `s:\Work\Sourcerer\src\build\Desktop_Qt_6_10_1_MinGW_64_bit-Debug\` (and Release)

## Step 3: Rebuild

CMake will automatically find the WinPTY library from either:
- Your MSYS2 installation (if `MSYSTEM_PREFIX` is set)
- The `lib/winpty/` directory in your project

## Why WinPTY?

- ✅ Real pseudo-terminal - programs detect interactive mode
- ✅ Python, gcc, and all console apps work perfectly  
- ✅ No race conditions or signal timing issues
- ✅ Proper Ctrl+C handling
- ✅ Full ANSI escape sequence support
- ✅ Used by VS Code, ConEmu, Windows Terminal

## MSYS2 vs MSVC Build

Since you're using MinGW (Qt 6.10.1 MinGW 64-bit), the MSYS2 version is the correct choice:
- ✅ Binary compatible with MinGW toolchain
- ✅ Easy to install via pacman
- ✅ Maintained and up-to-date
