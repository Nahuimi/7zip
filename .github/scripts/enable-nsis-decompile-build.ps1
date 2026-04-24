param(
  [string]$RepoRoot = (Get-Location).Path
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Update-FileText {
  param(
    [Parameter(Mandatory = $true)]
    [string]$Path,

    [Parameter(Mandatory = $true)]
    [scriptblock]$Transform
  )

  $fullPath = Join-Path $RepoRoot $Path
  $original = Get-Content -LiteralPath $fullPath -Raw
  $updated = & $Transform $original

  if ($updated -ne $original) {
    $utf8NoBom = New-Object System.Text.UTF8Encoding($false)
    [System.IO.File]::WriteAllText($fullPath, $updated, $utf8NoBom)
  }
}

Update-FileText -Path "CPP\7zip\Archive\Nsis\NsisIn.h" -Transform {
  param([string]$Text)
  $Text -replace '(?m)^// #define NSIS_SCRIPT\s*$', '#define NSIS_SCRIPT'
}

Update-FileText -Path "CPP\Build.mak" -Transform {
  param([string]$Text)
  $updated = $Text -replace ' -WX', ''
  $updated = $updated -replace ' /WX', ''
  $updated
}
