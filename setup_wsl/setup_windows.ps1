$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$GREEN = 'Green'
$BLUE  = 'Cyan'
$RED   = 'Red'
$YELLOW = 'Yellow'

function Write-Color {
    param(
        [string]$Message,
        [string]$Color = 'White'
    )
    Write-Host $Message -ForegroundColor $Color
}

function Test-CommandAvailable {
    param([string]$Name)
    return $null -ne (Get-Command $Name -ErrorAction SilentlyContinue)
}

function Install-WingetPackage {
    param(
        [Parameter(Mandatory = $true)][string]$Id,
        [Parameter(Mandatory = $true)][string]$Name
    )

    if (-not (Test-CommandAvailable 'winget')) {
        throw "winget was not found. Install/enable WinGet first, then rerun this script."
    }

    $alreadyInstalled = $false
    try {
        $listOutput = winget list --id $Id --exact --accept-source-agreements 2>$null | Out-String
        if ($listOutput -match [regex]::Escape($Id)) {
            $alreadyInstalled = $true
        }
    } catch {
        $alreadyInstalled = $false
    }

    if ($alreadyInstalled) {
        Write-Host "$Name already installed. Skipping."
        return
    }

    Write-Host "Installing $Name ($Id)..."
    winget install --id $Id --exact --accept-source-agreements --accept-package-agreements --silent
}

function Require-Path {
    param([string]$Path)
    if (-not (Test-Path $Path)) {
        throw "Required path not found: $Path"
    }
}

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $ScriptDir

Write-Color "--- Starting Setup in: $ScriptDir ---" $BLUE
Write-Color "Native Windows PowerShell environment detected." $BLUE

Write-Color "--- Installing System Dependencies & Flash Tools ---" $GREEN
Install-WingetPackage -Id 'Python.Python.3.12' -Name 'Python 3'
Install-WingetPackage -Id 'Git.Git' -Name 'Git'
Install-WingetPackage -Id 'Kitware.CMake' -Name 'CMake'
Install-WingetPackage -Id 'Ninja-build.Ninja' -Name 'Ninja'
Install-WingetPackage -Id '7zip.7zip' -Name '7-Zip'

Write-Color "Windows-native exact replacements are NOT available here for: dos2unix, gperf, ccache, dfu-util, device-tree-compiler, gcc/g++, gcc-multilib/g++-multilib, libsdl2-dev, libmagic1, stlink-tools, and openocd." $YELLOW
Write-Color "Install those separately if your board flow still depends on them." $YELLOW

Write-Color "--- Setting up Python Virtual Environment ---" $GREEN
$VenvDir = Join-Path $ScriptDir 'coraldev'
if (-not (Test-Path $VenvDir)) {
    Write-Host "Creating virtual environment 'coraldev'..."
    python -m venv coraldev
} else {
    Write-Host "Virtual environment 'coraldev' already exists. Skipping."
}

$ActivateScript = Join-Path $VenvDir 'Scripts\Activate.ps1'
Require-Path $ActivateScript
& $ActivateScript
Write-Host 'Virtual environment activated.'
python -m pip install --upgrade pip

Write-Color "--- Updating Submodules ---" $GREEN
git submodule update --init --recursive

Write-Color "--- Fetching ARM Toolchain ---" $GREEN
$MakeDir = Join-Path $ScriptDir 'make'
$ArmDir = Join-Path $MakeDir 'arm-gnu-toolchain-14.2.rel1-mingw-w64-i686-arm-none-eabi'
$ArmZip = Join-Path $MakeDir 'arm-gnu-toolchain-14.2.rel1-mingw-w64-i686-arm-none-eabi.zip'
$ArmUrl = 'https://developer.arm.com/-/media/Files/downloads/gnu/14.2.rel1/binrel/arm-gnu-toolchain-14.2.rel1-mingw-w64-i686-arm-none-eabi.zip'

New-Item -ItemType Directory -Force -Path $MakeDir | Out-Null
Push-Location $MakeDir
if (-not (Test-Path $ArmDir)) {
    Write-Host 'Downloading ARM GNU Toolchain 14.2.Rel1 for Windows...'
    Invoke-WebRequest -Uri $ArmUrl -OutFile $ArmZip
    Expand-Archive -Path $ArmZip -DestinationPath $MakeDir -Force
    Remove-Item $ArmZip -Force
} else {
    Write-Host 'Toolchain already in place, skipping download.'
}
Pop-Location

Write-Color "--- Initializing Zephyr Workspace ---" $GREEN
python -m pip install west

$WestDir = Join-Path $ScriptDir '.west'
$WestManifest = Join-Path $ScriptDir 'west.yml'
if (-not (Test-Path $WestDir)) {
    if (-not (Test-Path $WestManifest)) {
        Write-Color "Error: west.yml not found in $ScriptDir!" $RED
        Write-Host 'Zephyr requires a west.yml manifest file to initialize.'
        exit 1
    }
    west init -l .
} else {
    Write-Host 'West workspace already initialized.'
}

west update
west zephyr-export
python -m pip install @((west packages pip) -split ' ')

Write-Color "--- Installing FULL Zephyr SDK ---" $GREEN
$ParentZephyrDir = Resolve-Path (Join-Path $ScriptDir '..\zephyr') -ErrorAction SilentlyContinue
if ($ParentZephyrDir) {
    $ParentDir = Split-Path -Parent $ParentZephyrDir.Path
    $SdkVersion = '1.0.1'
    $SdkArchive = Join-Path $ParentDir "zephyr-sdk-$SdkVersion`_windows-x86_64_gnu.7z"
    $SdkDir = Join-Path $ParentDir "zephyr-sdk-$SdkVersion"
    $SdkUrl = "https://github.com/zephyrproject-rtos/sdk-ng/releases/download/v$SdkVersion/zephyr-sdk-$SdkVersion`_windows-x86_64_gnu.7z"

    if (-not (Test-Path $SdkDir)) {
        Write-Host "Downloading Zephyr SDK $SdkVersion for Windows..."
        Invoke-WebRequest -Uri $SdkUrl -OutFile $SdkArchive

        if (Test-CommandAvailable '7z') {
            7z x $SdkArchive "-o$ParentDir" -y | Out-Null
        } else {
            throw '7z was not found in PATH after installation. Open a new PowerShell window and rerun the script.'
        }
    } else {
        Write-Host 'Zephyr SDK already present. Skipping download.'
    }

    $SdkSetupCmd = Join-Path $SdkDir 'setup.cmd'
    Require-Path $SdkSetupCmd
    Push-Location $SdkDir
    cmd /c setup.cmd
    Pop-Location
} else {
    Write-Color "Warning: '..\\zephyr' directory not found after update. SDK install skipped." $RED
}

Write-Color "--- Building Coralmicro ---" $GREEN
$CoralMicroDir = Join-Path $ScriptDir 'third-party\coralmicro'
if (Test-Path $CoralMicroDir) {
    Push-Location $CoralMicroDir

    @'
hexformat==0.2
hidapi
progress==1.5
pyserial==3.5
pyusb==1.2.0
'@ | Set-Content -Path 'scripts\requirements.txt' -NoNewline

    if (Test-CommandAvailable 'bash') {
        bash setup.sh
        bash build.sh
    } else {
        Write-Color 'Error: bash was not found in PATH.' $RED
        Write-Color 'The original script calls third-party/coralmicro/setup.sh and build.sh. That part cannot be translated 1:1 to native PowerShell unless those scripts are also ported, or Git Bash/another POSIX shell is available.' $RED
        exit 1
    }

    Pop-Location
} else {
    Write-Color 'Error: third-party/coralmicro not found. Check git submodules.' $RED
    exit 1
}

Write-Color '--- Setup Complete, Welcome to Coral Cubed ---' $BLUE
