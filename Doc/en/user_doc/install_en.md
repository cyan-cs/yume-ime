# Yume IME Installation Guide

## System Requirements

- **OS**: Windows 10/11 (x64)
- **Architecture**: x86-64 (x64)
- **Disk Space**: ~50 MB
- **RAM**: 256 MB minimum

## Installation Methods

### Method 1: Simple Installation (Recommended for Users)

**Prerequisites**: 
- PowerShell 5.0+
- Administrator privileges

**Steps**:

1. Download the repository or latest release
2. Open PowerShell as Administrator
3. Navigate to the repository root:
   ```powershell
   cd C:\path\to\yume-ime
   ```
4. Run the registration script:
   ```powershell
   .\scripts\Register-YumeIME.ps1
   ```
5. The IME should be registered in Windows

**.NET Requirements**:
- If you encounter errors, ensure .NET Framework 4.5+ is installed

### Method 2: Build from Source

**Prerequisites**:
- Visual Studio 2022 Community Edition or later (C++ workload)
- CMake 3.20+
- PowerShell 5.0+

**Build Steps**:

1. Clone the repository:
   ```powershell
   git clone https://github.com/cyan-cs/yume-ime.git
   cd yume-ime
   ```

2. Build only (without registration):
   ```powershell
   .\scripts\build.ps1
   ```
   - Output: `./build/Release/YumeIME.dll`

3. Build and Register:
   ```powershell
   # As Administrator
   .\scripts\Register-YumeIME.ps1
   ```

### Method 3: Manual Registration

If scripts don't work, manually register the DLL:

```powershell
# Run as Administrator
$imePath = "C:\path\to\yume-ime\build\Release\YumeIME.dll"

# Register in Windows registry
regsvr32 $imePath
```

## Enabling the IME in Windows

After installation:

1. Open **Settings** → **Time & language** → **Language & region**
2. Under "Input methods for Chinese, Japanese, or Korean", click **Add a language** (if needed)
3. Select **日本語 (Japanese)**
4. The Yume IME should appear in the input method list
5. Set it as the default IME (optional)

Alternatively, use Alt+` (grave) or Ctrl+Shift to switch input methods.

## Uninstallation

To remove Yume IME:

```powershell
# Run as Administrator
.\scripts\unregister-YumeIME.ps1
```

Or manually:
```powershell
$imePath = "C:\path\to\yume-ime\build\Release\YumeIME.dll"
regsvr32 /u $imePath
```

## Troubleshooting

### "Access Denied" Error
- Ensure PowerShell is running as Administrator
- Try manual registration using `regsvr32`

### IME not appearing in Windows settings
- Rebuild from source and ensure the DLL is created
- Check that the DLL is 64-bit (x64)
- Verify .NET Framework is installed

### "Cannot find path" Error
- Ensure you're in the repository root directory
- Verify `scripts/Register-YumeIME.ps1` exists

See [TROUBLESHOOTING.md](./TROUBLESHOOTING.md) for more help.

## Verifying Installation

After installation, test the IME:

1. Open Notepad or any text editor
2. Switch to Yume IME
3. Type in Japanese (ひらがな):
   - Input: "わたし"
   - Suggest: [私, 渡し, ...]
4. Select a suggestion with ↑/↓ and press Space/Enter

## File Locations

- **Installation**: `%APPDATA%\Yume IME\` - User data directory
- **Logs**: `%APPDATA%\Yume IME\logs\` - Debug logs
- **Dictionaries**: `%APPDATA%\Yume IME\settings\` - Custom dictionary storage
