param(
  [Parameter(Mandatory = $true)]
  [string]$VersionTag,

  [string]$AssetFlavor = "",

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

function Reset-Directory {
  param(
    [Parameter(Mandatory = $true)]
    [string]$Path
  )

  if (Test-Path -LiteralPath $Path) {
    Remove-Item -LiteralPath $Path -Recurse -Force
  }

  New-Item -ItemType Directory -Path $Path -Force | Out-Null
}

function New-AssetName {
  param(
    [Parameter(Mandatory = $true)]
    [string]$AssetTag,

    [Parameter(Mandatory = $true)]
    [string]$Suffix
  )

  $assetNameParts = @("7zip", $AssetTag)
  if (-not [string]::IsNullOrWhiteSpace($AssetFlavor)) {
    $assetNameParts += $AssetFlavor.Trim()
  }
  $assetNameParts += @("windows", $Suffix)
  $assetNameParts -join "-"
}

function Compress-DirectoryContents {
  param(
    [Parameter(Mandatory = $true)]
    [string]$SourceRoot,

    [Parameter(Mandatory = $true)]
    [string]$DestinationPath
  )

  if (Test-Path -LiteralPath $DestinationPath) {
    Remove-Item -LiteralPath $DestinationPath -Force
  }

  Compress-Archive -Path (Join-Path $SourceRoot "*") -DestinationPath $DestinationPath -CompressionLevel Optimal
}

function New-AllProductsPackage {
  param(
    [Parameter(Mandatory = $true)]
    [string]$PackageRoot
  )

  Reset-Directory -Path $PackageRoot

  $sourceRoot = (Resolve-Path -LiteralPath (Join-Path $RepoRoot "CPP\7zip")).Path
  $productFiles = Get-ChildItem -LiteralPath $sourceRoot -Recurse -File | Where-Object {
    $_.DirectoryName -match '[\\/](x64|x86)$' -and
    @(".exe", ".dll", ".sfx") -contains $_.Extension.ToLowerInvariant()
  } | Sort-Object FullName -Unique

  if (-not $productFiles) {
    throw "No built products were found under $sourceRoot"
  }

  $sourcePrefix = $sourceRoot.TrimEnd("\") + "\"

  foreach ($file in $productFiles) {
    $relativePath = $file.FullName.Substring($sourcePrefix.Length)
    $target = Join-Path $PackageRoot $relativePath
    New-Item -ItemType Directory -Path (Split-Path -Path $target -Parent) -Force | Out-Null
    Copy-Item -LiteralPath $file.FullName -Destination $target -Force
  }

  $langTargetRoot = Join-Path $PackageRoot "Lang"
  New-Item -ItemType Directory -Path $langTargetRoot -Force | Out-Null
  Copy-Item -Path (Join-Path $RepoRoot "Lang\*.txt") -Destination $langTargetRoot -Force
  $enLanguagePath = Join-Path $RepoRoot "Lang\en.ttt"
  Assert-FileExists -Path $enLanguagePath
  Copy-Item -LiteralPath $enLanguagePath -Destination (Join-Path $langTargetRoot "en.ttt") -Force

  $licenseTargetRoot = Join-Path $PackageRoot "Licenses"
  New-Item -ItemType Directory -Path $licenseTargetRoot -Force | Out-Null

  $licenseMap = @(
    @{ Source = "DOC\License.txt"; Destination = "7zip-License.txt" },
    @{ Source = "CPP\Common\CompactEncDet\LICENSE"; Destination = "compact_enc_det-Apache-2.0.txt" },
    @{ Source = "THIRD_PARTY_NOTICES.txt"; Destination = "THIRD_PARTY_NOTICES.txt" }
  )

  foreach ($item in $licenseMap) {
    $sourcePath = Join-Path $RepoRoot $item.Source
    Assert-FileExists -Path $sourcePath
    Copy-Item -LiteralPath $sourcePath -Destination (Join-Path $licenseTargetRoot $item.Destination) -Force
  }
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

$workingRoot = Join-Path $RepoRoot "release-products-temp"
$allProductsRoot = Join-Path $workingRoot "all-products"

if (Test-Path -LiteralPath $workingRoot) {
  Remove-Item -LiteralPath $workingRoot -Recurse -Force
}
New-Item -ItemType Directory -Path $workingRoot -Force | Out-Null
New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null

New-AllProductsPackage -PackageRoot $allProductsRoot

Compress-DirectoryContents -SourceRoot $allProductsRoot -DestinationPath (Join-Path $OutputDir (New-AssetName -AssetTag $assetTag -Suffix "all-products.zip"))
