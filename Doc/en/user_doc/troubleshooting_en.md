# Troubleshooting - Yume IME

## Installation Issues

### Error: "Access Denied" or "Permission Denied"

**Cause**: Script requires administrator privileges

**Solution**:
1. Right-click PowerShell and select "Run as administrator"
2. Try the registration script again:
   ```powershell
   .\scripts\Register-YumeIME.ps1
   ```

### Error: "The system cannot find the file specified"

**Cause**: Build output not found (not built yet)

**Solution**:
1. Build the project first:
   ```powershell
   .\scripts\build.ps1 -Configuration Release
   ```
2. Verify that `.\build\Release\YumeIME.dll` exists
3. Then run registration

### Error: ".NET Framework" not installed

**Cause**: Missing .NET Framework dependency

**Solution**:
1. Download [.NET Framework 4.5+](https://dotnet.microsoft.com/download/dotnet-framework)
2. Install it
3. Restart your computer
4. Try registration again

### IME not appearing in Windows input method settings

**Cause**: DLL not properly registered or wrong architecture

**Solution**:
1. Verify the DLL is 64-bit:
   ```powershell
   # Check file properties or use:
   dumpbin /headers .\build\Release\YumeIME.dll | findstr /C:"machine"
   ```
   Should show: `FILE HEADER VALUES ... machine (x64)`

2. Manual registration as admin:
   ```powershell
   regsvr32 "C:\full\path\to\build\Release\YumeIME.dll"
   ```

3. If still not appearing, check Event Viewer:
   - Windows Logs → System
   - Look for errors related to "YumeIME"

## Runtime Issues

### IME not responding or crashes when typing

**Cause**: Potential memory issue or initialization problem

**Solution**:
1. Check logs for errors:
   ```powershell
   cat "$env:APPDATA\Yume IME\logs\debug.log" | Select-Object -Last 50
   ```

2. Unregister and re-register:
   ```powershell
   # As admin
   .\scripts\unregister-YumeIME.ps1
   .\scripts\Register-YumeIME.ps1
   ```

3. If still crashing, report a bug with the log file

### Candidate window not appearing

**Cause**: UI renderer issue or theme incompatibility

**Solution**:
1. Verify Windows display scaling is set to 100%
2. Try a different text editor (Notepad, Word, etc.)
3. Check system logs: `%APPDATA%\Yume IME\logs\`
4. Report issue with OS version and display setup

### Wrong input mode (Latin/Hiragana switching not working)

**Cause**: State machine bug or input event missed

**Solution**:
1. Restart the IME:
   - Alt+` to switch input methods and back to Yume IME
   - Or Ctrl+Shift

2. Restart the application (notepad, etc.)

3. If persistent, check logs and report

## Input/Typing Issues

### Pressing space doesn't generate candidates

**Cause**: Dictionary not loaded or candidate generation failed

**Solution**:
1. Verify the dictionary exists:
   ```powershell
   ls "$env:APPDATA\Yume IME\settings\*"
   ```

2. Check logs for dictionary loading errors:
   ```powershell
   cat "$env:APPDATA\Yume IME\logs\debug.log" | Select-String "Dictionary"
   ```

3. Verify you typed in hiragana:
   - Input "ひらがな" not "Hiragana"

### Pressing hyphen (-) causes issues (bug from todo)

**Cause**: Known issue - hyphen incorrectly locks candidate selection

**Workaround**:
- Avoid using hyphen temporarily
- Use alternative separators if available
- This is in the todo list for v0.2

### Space incorrectly locks candidate selection (bug from todo)

**Cause**: Known issue in current version

**Workaround**:
- Press Escape to cancel and retry
- Or confirm the current selection if acceptable
- Planned fix for v0.2 release

## Performance Issues

### IME is slow or laggy

**Cause**: Dictionary size too large or system under load

**Solution**:
1. Check if system is under high load:
   ```powershell
   Get-Process | Sort-Object CPU -Descending | Select-Object -First 5
   ```

2. Archive old log files:
   ```powershell
   Remove-Item "$env:APPDATA\Yume IME\logs\*.log" -OlderThan (Get-Date).AddDays(-30)
   ```

3. Reduce custom dictionary size if present

## Uninstall Issues

### Error unregistering IME

**Cause**: Insufficient permissions or DLL in use

**Solution**:
1. Close all applications using the IME
2. Run PowerShell as Administrator
3. Try unregister script:
   ```powershell
   .\scripts\unregister-YumeIME.ps1
   ```

4. If still failing, manual unregister:
   ```powershell
   regsvr32 /u "C:\full\path\to\build\Release\YumeIME.dll"
   ```

## Reporting a Bug

If you encounter an issue not listed above:

1. **Gather information**:
   - OS version: `winver`
   - Log file: `%APPDATA%\Yume IME\logs\debug.log`
   - Steps to reproduce
   - Error messages

2. **Create a GitHub Issue** with:
   - Clear title
   - Reproduction steps
   - Expected vs. actual behavior
   - System info
   - Relevant log excerpts

3. **Include configuration** (if applicable):
   - `%APPDATA%\Yume IME\settings\` contents

## Getting Help

- Check the [Installation Guide](./install_en.md)
- Review [Architecture guide](./architecture_en.md) for system design
- Search existing [GitHub Issues](https://github.com/cyan-cs/yume-ime/issues)
- Open a [Discussion](https://github.com/cyan-cs/yume-ime/discussions)

---

Still stuck? Please report the issue or reach out via GitHub!
