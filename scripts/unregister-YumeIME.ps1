#   .\unregister-ime.ps1
#   .\unregister-ime.ps1 -SkipLanguageTip
#   .\unregister-ime.ps1 -SkipRegistryCleanup

[CmdletBinding()]
param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release",

    [switch]$SkipLanguageTip,
    [switch]$SkipRegistryCleanup
)

$ErrorActionPreference = "Stop"

function Test-IsAdministrator {
    $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = New-Object Security.Principal.WindowsPrincipal($identity)
    return $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

function Invoke-Step {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Name,

        [Parameter(Mandatory = $true)]
        [scriptblock]$Action
    )

    Write-Host "==> $Name"
    & $Action
}

$repoRoot = Split-Path -Parent $PSScriptRoot
$buildDir = Join-Path $repoRoot "build"
$dllPath = Join-Path $buildDir "$Configuration\YumeIME.dll"
$regsvr32 = Join-Path $env:WINDIR "System32\regsvr32.exe"

$textServiceClsid = "{4E5F3910-7A67-45D4-889E-619C44D01591}"
$languageProfileGuid = "{7E7A20F1-1AB4-4F09-8410-5A0A3C692291}"
$tip = "0411:$textServiceClsid$languageProfileGuid"

# ------------------------------------------------------------
# DLL Unregister
# ------------------------------------------------------------

if (Test-Path $dllPath) {
    Invoke-Step -Name "Unregistering DLL with regsvr32" -Action {
        & $regsvr32 /u /s $dllPath
    }
} else {
    Write-Warning "DLL not found: $dllPath"
}

# ------------------------------------------------------------
# Remove Language TIP
# ------------------------------------------------------------

if (-not $SkipLanguageTip) {
    Invoke-Step -Name "Removing Yume IME from user language list" -Action {

        $list = Get-WinUserLanguageList
        $changed = $false

        foreach ($lang in $list) {
            if ($lang.InputMethodTips.Contains($tip)) {
                $lang.InputMethodTips.Remove($tip)
                $changed = $true
            }
        }

        if ($changed) {
            Set-WinUserLanguageList $list -Force
        }

        Get-WinUserLanguageList | Format-List
    }
}

# ------------------------------------------------------------
# Registry cleanup
# ------------------------------------------------------------

if (-not $SkipRegistryCleanup) {

    if (-not (Test-IsAdministrator)) {
        Write-Warning "Skipping HKLM cleanup because this PowerShell session is not elevated."
    }
    else {
        Invoke-Step -Name "Removing TSF registry entries" -Action {

            $baseHKLM = "HKLM:\SOFTWARE\Microsoft\CTF\TIP\$textServiceClsid"
            $baseHKCU = "HKCU:\Software\Microsoft\CTF\TIP\$textServiceClsid"

            if (Test-Path $baseHKLM) {
                Remove-Item $baseHKLM -Recurse -Force
            }

            if (Test-Path $baseHKCU) {
                Remove-Item $baseHKCU -Recurse -Force
            }
        }
    }
}

Write-Host ""
Write-Host "Yume IME removal flow completed."
Write-Host "TIP: $tip"
Write-Host "If the IME still appears, sign out or restart ctfmon.exe."