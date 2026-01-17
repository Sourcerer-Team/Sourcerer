# Complete WinPTY Integration Steps

## Prerequisites

1. **Download WinPTY**: https://github.com/rprichard/winpty/releases
   - Get `winpty-0.4.3-msvc2015.zip` or latest
   
2. **Extract Structure**:
   ```
   s:\Work\Sourcerer\lib\winpty\
   ├── bin\
   │   ├── winpty.dll          (must be in PATH or next to .exe)
   │   └── winpty-agent.exe    (must be findable)
   ├── include\
   │   └── winpty.h
   └── lib\
       └── winpty.lib
   ```

## CMake Changes (Already Done)

- ✅ Added FindWinPTY.cmake  
- ✅ Updated GuiManager/CMakeLists.txt

## Implementation Overview

WinPTY provides a real pseudo-terminal that makes programs think they're in a real console:

```cpp
// Key WinPTY API calls:
winpty_t* pty = winpty_open(config);  // Create PTY
HANDLE in = winpty_conin_handle(pty);  // Get input pipe
HANDLE out = winpty_conout_handle(pty); // Get output pipe
winpty_spawn(pty, ...);  // Spawn PowerShell
WriteFile(in, data, ...); // Send input  
ReadFile(out, buffer, ...); // Read output
```

## Benefits Over QProcess

| Feature | QProcess | WinPTY |
|---------|----------|--------|
| Interactive programs (Python) | ❌ No output | ✅ Full output |
| Terminal detection | ❌ Pipe detected | ✅ TTY detected |
| ANSI colors | ❌ Not supported | ✅ Full support |
| Ctrl+C | ⚠️ Requires workaround | ✅ Native support |
| Race conditions | ⚠️ Signal timing issues | ✅ None |
| Prompt detection | ⚠️ Complex filtering | ✅ Not needed |

## Alternative: Quick Fix for Current Code

If WinPTY setup is too complex right now, here's a simpler workaround:

**Add to every command that needs interactive mode:**
- Python: Auto-add `-i -u` flags
- Other programs: Check if they need TTY

This won't be perfect but will help with some cases.

## Recommendation

1. Try WinPTY for a proper solution (30 min setup)
2. Or use the auto-flag workaround for quick fix
3. The Ctrl+C implementation I added will work with both approaches

Let me know which approach you prefer!
