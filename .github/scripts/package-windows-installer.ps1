param(
  [Parameter(Mandatory = $true)]
  [string]$VersionTag,

  [Parameter(Mandatory = $true)]
  [string]$UpstreamVersionCompact,

  [string]$RepoRoot = (Get-Location).Path,

  [string]$OutputDir = (Join-Path (Get-Location).Path "release-assets")
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
$ProgressPreference = "SilentlyContinue"

function Assert-FileExists {
  param(
    [Parameter(Mandatory = $true)]
    [string]$Path
  )

  if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
    throw "Missing expected file: $Path"
  }
}

function Invoke-Native {
  param(
    [Parameter(Mandatory = $true)]
    [string]$FilePath,

    [Parameter()]
    [string[]]$Arguments = @()
  )

  & $FilePath @Arguments
  if ($LASTEXITCODE -ne 0) {
    $argumentText = [string]::Join(" ", $Arguments)
    throw "Command failed with exit code ${LASTEXITCODE}: $FilePath $argumentText"
  }
}

function Join-BinaryFiles {
  param(
    [Parameter(Mandatory = $true)]
    [string]$FirstPath,

    [Parameter(Mandatory = $true)]
    [string]$SecondPath,

    [Parameter(Mandatory = $true)]
    [string]$OutputPath
  )

  $buffer = New-Object byte[] 1048576
  $outputStream = [System.IO.File]::Open($OutputPath, [System.IO.FileMode]::Create, [System.IO.FileAccess]::Write, [System.IO.FileShare]::None)

  try {
    foreach ($inputPath in @($FirstPath, $SecondPath)) {
      $inputStream = [System.IO.File]::OpenRead($inputPath)

      try {
        while (($bytesRead = $inputStream.Read($buffer, 0, $buffer.Length)) -gt 0) {
          $outputStream.Write($buffer, 0, $bytesRead)
        }
      }
      finally {
        $inputStream.Dispose()
      }
    }
  }
  finally {
    $outputStream.Dispose()
  }
}

function Copy-ProductFiles {
  param(
    [Parameter(Mandatory = $true)]
    [string]$Platform,

    [Parameter(Mandatory = $true)]
    [string]$DestinationRoot
  )

  $copyMap = @(
    @{ Source = (Join-Path $RepoRoot "CPP\7zip\UI\FileManager\$Platform\7zFM.exe"); Destination = "7zFM.exe" },
    @{ Source = (Join-Path $RepoRoot "CPP\7zip\UI\GUI\$Platform\7zG.exe"); Destination = "7zG.exe" },
    @{ Source = (Join-Path $RepoRoot "CPP\7zip\UI\Console\$Platform\7z.exe"); Destination = "7z.exe" },
    @{ Source = (Join-Path $RepoRoot "CPP\7zip\Bundles\Format7zF\$Platform\7z.dll"); Destination = "7z.dll" },
    @{ Source = (Join-Path $RepoRoot "CPP\7zip\UI\PasswordPlugin\$Platform\7zPasswordPlugins.dll"); Destination = "7zPasswordPlugins.dll" },
    @{ Source = (Join-Path $RepoRoot "CPP\7zip\UI\Explorer\$Platform\7-zip.dll"); Destination = "7-zip.dll" },
    @{ Source = (Join-Path $RepoRoot "CPP\7zip\Bundles\SFXWin\$Platform\7z.sfx"); Destination = "7z.sfx" },
    @{ Source = (Join-Path $RepoRoot "CPP\7zip\Bundles\SFXCon\$Platform\7zCon.sfx"); Destination = "7zCon.sfx" },
    @{ Source = (Join-Path $RepoRoot "C\Util\7zipUninstall\$Platform\7zipUninstall.exe"); Destination = "Uninstall.exe" }
  )

  if ($Platform -eq "x64") {
    $copyMap += @{ Source = (Join-Path $RepoRoot "CPP\7zip\UI\Explorer\x86\7-zip.dll"); Destination = "7-zip32.dll" }
  }

  foreach ($item in $copyMap) {
    Assert-FileExists -Path $item.Source

    $destinationPath = Join-Path $DestinationRoot $item.Destination
    $destinationParent = Split-Path -Path $destinationPath -Parent

    if ($destinationParent) {
      New-Item -ItemType Directory -Path $destinationParent -Force | Out-Null
    }

    Copy-Item -LiteralPath $item.Source -Destination $destinationPath -Force
  }
}

function Copy-DistributionResources {
  param(
    [Parameter(Mandatory = $true)]
    [string]$DestinationRoot
  )

  $langSourceRoot = Join-Path $RepoRoot "Lang"
  if (Test-Path -LiteralPath $langSourceRoot -PathType Container) {
    $langDestinationRoot = Join-Path $DestinationRoot "Lang"
    New-Item -ItemType Directory -Path $langDestinationRoot -Force | Out-Null

    Get-ChildItem -LiteralPath $langSourceRoot -File | ForEach-Object {
      Copy-Item -LiteralPath $_.FullName -Destination (Join-Path $langDestinationRoot $_.Name) -Force
    }
  }

  $documentMap = @(
    @{ Source = "DOC\License.txt"; Destination = "License.txt" },
    @{ Source = "DOC\readme.txt"; Destination = "readme.txt" }
  )

  foreach ($item in $documentMap) {
    $sourcePath = Join-Path $RepoRoot $item.Source
    if (-not (Test-Path -LiteralPath $sourcePath -PathType Leaf)) {
      continue
    }

    $destinationPath = Join-Path $DestinationRoot $item.Destination
    $destinationParent = Split-Path -Path $destinationPath -Parent
    if ($destinationParent) {
      New-Item -ItemType Directory -Path $destinationParent -Force | Out-Null
    }

    Copy-Item -LiteralPath $sourcePath -Destination $destinationPath -Force
  }
}

function New-InstallerAsset {
  param(
    [Parameter(Mandatory = $true)]
    [string]$Platform,

    [Parameter(Mandatory = $true)]
    [string]$SkeletonRoot,

    [Parameter(Mandatory = $true)]
    [string]$SevenZipExe,

    [Parameter(Mandatory = $true)]
    [string]$WorkingRoot,

    [Parameter(Mandatory = $true)]
    [string]$AssetTag
  )

  $stageRoot = Join-Path $WorkingRoot "stage-$Platform"
  $archivePath = Join-Path $WorkingRoot "$Platform.7z"
  $installerStub = Join-Path $RepoRoot "C\Util\7zipInstall\$Platform\7zipInstall.exe"
  $installerAssetPath = Join-Path $OutputDir "7zip-$AssetTag-windows-$Platform-installer.exe"

  Assert-FileExists -Path $installerStub

  if (Test-Path -LiteralPath $stageRoot) {
    Remove-Item -LiteralPath $stageRoot -Recurse -Force
  }

  New-Item -ItemType Directory -Path $stageRoot -Force | Out-Null
  Copy-Item -Path (Join-Path $SkeletonRoot "*") -Destination $stageRoot -Recurse -Force

  Get-ChildItem -LiteralPath $stageRoot -File |
    Where-Object { @(".exe", ".dll", ".sfx") -contains $_.Extension.ToLowerInvariant() } |
    Remove-Item -Force

  Copy-ProductFiles -Platform $Platform -DestinationRoot $stageRoot
  Copy-DistributionResources -DestinationRoot $stageRoot

  if (Test-Path -LiteralPath $archivePath) {
    Remove-Item -LiteralPath $archivePath -Force
  }

  Invoke-Native -FilePath $SevenZipExe -Arguments @(
    "a",
    $archivePath,
    (Join-Path $stageRoot "*"),
    "-m0=lzma",
    "-mx=9",
    "-ms=on",
    "-mf=bcj2"
  )

  Join-BinaryFiles -FirstPath $installerStub -SecondPath $archivePath -OutputPath $installerAssetPath
}

if ($VersionTag -notmatch '^v(?<major>\d+)\.(?<minor>\d+)') {
  throw "VersionTag must start with v<major>.<minor>, for example: v25.01"
}

$assetTagChars = $VersionTag.ToCharArray() | ForEach-Object {
  if ([System.IO.Path]::GetInvalidFileNameChars() -contains $_) {
    "-"
  }
  else {
    $_
  }
}
$assetTag = -join $assetTagChars

$officialInstallerUrl = "https://www.7-zip.org/a/7z$UpstreamVersionCompact.exe"
$workingRoot = Join-Path $RepoRoot "release-temp"
$officialInstallerPath = Join-Path $workingRoot "official-7zip-installer.exe"
$skeletonRoot = Join-Path $workingRoot "skeleton"
$sevenZipExe = Join-Path $RepoRoot "CPP\7zip\UI\Console\x64\7z.exe"

Assert-FileExists -Path $sevenZipExe

if (Test-Path -LiteralPath $workingRoot) {
  Remove-Item -LiteralPath $workingRoot -Recurse -Force
}
if (Test-Path -LiteralPath $OutputDir) {
  Remove-Item -LiteralPath $OutputDir -Recurse -Force
}

New-Item -ItemType Directory -Path $workingRoot -Force | Out-Null
New-Item -ItemType Directory -Path $skeletonRoot -Force | Out-Null
New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null

Invoke-WebRequest -Uri $officialInstallerUrl -OutFile $officialInstallerPath
Assert-FileExists -Path $officialInstallerPath

Invoke-Native -FilePath $sevenZipExe -Arguments @(
  "x",
  $officialInstallerPath,
  "-y",
  "-o$skeletonRoot"
)

New-InstallerAsset -Platform "x64" -SkeletonRoot $skeletonRoot -SevenZipExe $sevenZipExe -WorkingRoot $workingRoot -AssetTag $assetTag
New-InstallerAsset -Platform "x86" -SkeletonRoot $skeletonRoot -SevenZipExe $sevenZipExe -WorkingRoot $workingRoot -AssetTag $assetTag
