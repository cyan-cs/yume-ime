#   .\register-ime.ps1 通常
#   .\register-ime.ps1 -SkipBuild ビルド済み
#   .\register-ime.ps1 -SkipBuild -SkipLanguageTip レジストリだけ

[CmdletBinding()]
param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release",

    [switch]$NoClean,
    [switch]$SkipBuild,
    [switch]$SkipLanguageTip,
    [switch]$SkipRegistryBootstrap
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
$imeName = "yume-ime"
$languageProfileBase = "SOFTWARE\Microsoft\CTF\TIP\$textServiceClsid\LanguageProfile\0x00000411\$languageProfileGuid"

if (-not $SkipBuild) {
    if ((-not $NoClean) -and (Test-Path $buildDir)) {
        Invoke-Step -Name "Removing build directory" -Action {
            Remove-Item -Recurse -Force $buildDir
        }
    }

    Invoke-Step -Name "Configuring CMake" -Action {
        cmake -S $repoRoot -B $buildDir
    }

    Invoke-Step -Name "Building $Configuration" -Action {
        cmake --build $buildDir --config $Configuration
    }
}

if (-not (Test-Path $dllPath)) {
    throw "Built DLL was not found: $dllPath"
}

Invoke-Step -Name "Registering DLL with regsvr32" -Action {
    & $regsvr32 $dllPath
}

if (-not $SkipLanguageTip) {
    Invoke-Step -Name "Adding Yume IME to the current user language list" -Action {
        $list = Get-WinUserLanguageList
        if ($list.Count -eq 0) {
            throw "Get-WinUserLanguageList returned no entries."
        }

        if (-not $list[0].InputMethodTips.Contains($tip)) {
            [void]$list[0].InputMethodTips.Add($tip)
            Set-WinUserLanguageList $list -Force
        }

        Get-WinUserLanguageList | Format-List
    }
}

if (-not $SkipRegistryBootstrap) {
    if (-not (Test-IsAdministrator)) {
        Write-Warning "Skipping HKLM TIP bootstrap because this PowerShell session is not elevated."
    } else {
        Invoke-Step -Name "Bootstrapping TSF registry entries" -Action {
            $cats = @(
                "{34745C63-B2F0-4784-8B67-5E12C8701A31}",
                "{49D2F9CF-1F5E-11D7-A6D3-00065B84435C}",
                "{CCF05DD7-4A87-11D7-A6E2-00065B84435C}"
            )

            $modulePath = $dllPath
            $base = "HKLM:\SOFTWARE\Microsoft\CTF\TIP\$textServiceClsid"

            New-Item "$base\LanguageProfile\0x00000411\$languageProfileGuid" -Force | Out-Null
            New-ItemProperty -Path "$base\LanguageProfile\0x00000411\$languageProfileGuid" -Name "Description" -Value $imeName -PropertyType String -Force | Out-Null
            New-ItemProperty -Path "$base\LanguageProfile\0x00000411\$languageProfileGuid" -Name "Enable" -Value 1 -PropertyType DWord -Force | Out-Null
            New-ItemProperty -Path "$base\LanguageProfile\0x00000411\$languageProfileGuid" -Name "IconFile" -Value $modulePath -PropertyType ExpandString -Force | Out-Null
            New-ItemProperty -Path "$base\LanguageProfile\0x00000411\$languageProfileGuid" -Name "IconIndex" -Value 0 -PropertyType DWord -Force | Out-Null
            New-ItemProperty -Path "$base\LanguageProfile\0x00000411\$languageProfileGuid" -Name "ProfileFlags" -Value 0 -PropertyType DWord -Force | Out-Null

            New-Item "$base\Category\Item\$textServiceClsid" -Force | Out-Null
            New-ItemProperty -Path "$base\Category\Item\$textServiceClsid" -Name "Description" -Value $imeName -PropertyType String -Force | Out-Null

            foreach ($cat in $cats) {
                New-Item "$base\Category\Category\$cat\$($textServiceClsid.ToLower())" -Force | Out-Null
                New-Item "$base\Category\Item\$($textServiceClsid.ToLower())\$cat" -Force | Out-Null
            }

            $hkcuProfile = "HKCU:\Software\Microsoft\CTF\TIP\$textServiceClsid\LanguageProfile\0x00000411\$languageProfileGuid"
            New-Item $hkcuProfile -Force | Out-Null
            New-ItemProperty -Path $hkcuProfile -Name "Enable" -Value 1 -PropertyType DWord -Force | Out-Null
        }
    }
}

Write-Host ""
Write-Host "Yume IME registration flow completed."
Write-Host "DLL: $dllPath"
Write-Host "TIP: $tip"
Write-Host "If the IME does not appear immediately, sign out or restart ctfmon.exe."
