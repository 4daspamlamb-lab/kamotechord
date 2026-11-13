# SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
# Copy the shared KamoteKeys code (core\) into one device tree before a build.
param([Parameter(Mandatory=$true)][ValidateSet('smk37','fm1')][string]$Device)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$app = Join-Path $root "devices\$Device\app"
$controls = if ($Device -eq 'smk37') { 'smk37_controls.c' } else { 'kk_controls.c' }
Copy-Item -Path (Join-Path $root 'core\kamotkeys\*') -Destination (Join-Path $app 'kamotkeys') -Force
Copy-Item -LiteralPath (Join-Path $root 'core\screen.c') -Destination (Join-Path $app 'screen.c') -Force
Copy-Item -LiteralPath (Join-Path $root 'core\kk_controls.c') -Destination (Join-Path $app $controls) -Force
Write-Host "KamoteKeys core copied into devices\$Device"
