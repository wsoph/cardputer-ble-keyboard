param([Parameter(Mandatory=$true)][string]$OutputPath)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
$bitmap = [Drawing.Bitmap]::new(1280,720)
$graphics = [Drawing.Graphics]::FromImage($bitmap)
$graphics.SmoothingMode = [Drawing.Drawing2D.SmoothingMode]::AntiAlias
$graphics.TextRenderingHint = [Drawing.Text.TextRenderingHint]::AntiAliasGridFit
$brushes = @{}
foreach ($entry in @{Background='#101820';White='#f5f7fb';Muted='#a9bac9';Orange='#ff9e42';Panel='#1c2a36';Screen='#080e14';Green='#8fdda0';Cyan='#69d1c5';Keys='#334757'}.GetEnumerator()) {
    $brushes[$entry.Key] = [Drawing.SolidBrush]::new([Drawing.ColorTranslator]::FromHtml($entry.Value))
}
$fonts = @{}
foreach ($size in @(16,18,20,22,23,25,26,58)) { $fonts[$size] = [Drawing.Font]::new('Arial',$size,[Drawing.FontStyle]::Regular,[Drawing.GraphicsUnit]::Pixel) }
function Text([string]$Value,[int]$X,[int]$Y,[int]$Size,[string]$Color) {
    $graphics.DrawString($Value,$fonts[$Size],$brushes[$Color],$X,$Y)
}
try {
    $graphics.Clear([Drawing.ColorTranslator]::FromHtml('#101820'))
    $graphics.FillRectangle($brushes.Orange,64,72,188,38)
    Text 'CARDPUTER-ADV' 80 80 20 'Background'
    Text 'Cardputer' 61 147 58 'White'
    Text 'BLE Keyboard' 61 219 58 'White'
    Text "Physical keys. Your phone's screen." 64 310 25 'Muted'
    Text 'Opt+B: Manual screen OFF / ON' 64 393 26 'Orange'
    Text 'Keep typing while the screen is off.' 64 443 23 'White'
    Text 'Secure BLE pairing | Two-key shortcuts' 64 550 22 'Muted'
    Text 'Open source | MIT | wsoph' 64 593 22 'Muted'
    $graphics.FillRectangle($brushes.Orange,790,145,420,390)
    $graphics.FillRectangle($brushes.Panel,795,150,410,380)
    $graphics.FillRectangle($brushes.Screen,813,168,374,178)
    Text 'Cardputer Keyboard' 834 194 20 'Cyan'
    Text 'Connected - ready' 834 232 18 'Green'
    Text 'Opt+B: screen OFF / ON' 834 270 18 'White'
    Text 'Opt+H: key help' 834 307 16 'Muted'
    for ($row=0;$row -lt 3;$row++) {
        for ($column=0;$column -lt 12;$column++) {
            $graphics.FillRectangle($brushes.Keys,813+$column*31,370+$row*41,27,30)
        }
    }
    $graphics.FillRectangle($brushes.Orange,946,491,128,21)
    Text 'Illustration | ADV hardware only' 856 560 18 'Muted'
    $parent = Split-Path -Parent $OutputPath
    if ($parent) { New-Item -ItemType Directory -Force -Path $parent | Out-Null }
    $bitmap.Save($OutputPath,[Drawing.Imaging.ImageFormat]::Png)
} finally {
    $graphics.Dispose(); $bitmap.Dispose()
    foreach ($brush in $brushes.Values) { $brush.Dispose() }
    foreach ($font in $fonts.Values) { $font.Dispose() }
}
Write-Output ('Cover generated: ' + $OutputPath)
