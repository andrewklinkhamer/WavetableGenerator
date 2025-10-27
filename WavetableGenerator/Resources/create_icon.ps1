# PowerShell script to create a high-quality multi-resolution icon for WavetableGenerator
# Creates smooth, anti-aliased icons at multiple sizes

Add-Type -AssemblyName System.Drawing

function Create-IconBitmap {
    param([int]$size)

    $bitmap = New-Object System.Drawing.Bitmap($size, $size, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)

    # High-quality rendering settings
    $graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality
    $graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $graphics.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
    $graphics.CompositingQuality = [System.Drawing.Drawing2D.CompositingQuality]::HighQuality

    # Create solid black background
    $blackBrush = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(255, 0, 0, 0))

    # Draw rounded rectangle background
    $cornerRadius = [int]($size * 0.15)
    $path = New-Object System.Drawing.Drawing2D.GraphicsPath
    $rect = New-Object System.Drawing.Rectangle(0, 0, $size, $size)

    # Create rounded rectangle path
    $path.AddArc($rect.X, $rect.Y, $cornerRadius * 2, $cornerRadius * 2, 180, 90)
    $path.AddArc($rect.Right - $cornerRadius * 2, $rect.Y, $cornerRadius * 2, $cornerRadius * 2, 270, 90)
    $path.AddArc($rect.Right - $cornerRadius * 2, $rect.Bottom - $cornerRadius * 2, $cornerRadius * 2, $cornerRadius * 2, 0, 90)
    $path.AddArc($rect.X, $rect.Bottom - $cornerRadius * 2, $cornerRadius * 2, $cornerRadius * 2, 90, 90)
    $path.CloseFigure()

    $graphics.FillPath($blackBrush, $path)
    $path.Dispose()

    # Calculate wave parameters based on icon size
    $margin = [int]($size * 0.15)
    $waveWidth = $size - ($margin * 2)
    $amplitude = $size * 0.18
    $centerY = $size / 2
    $frequency = 2.5
    $penWidth = [Math]::Max(2, [int]($size * 0.03))
    $glowWidth = [int]($penWidth * 1.5)

    # Create smooth wave points
    $points = New-Object System.Collections.ArrayList
    $step = [Math]::Max(1, [int]($size / 64))

    for ($x = $margin; $x -le ($size - $margin); $x += $step) {
        $progress = ($x - $margin) / $waveWidth
        $angle = $progress * [Math]::PI * $frequency
        $y = $centerY + ([Math]::Sin($angle) * $amplitude)
        [void]$points.Add([System.Drawing.PointF]::new($x, $y))
    }

    if ($points.Count -gt 1) {
        # Draw glow effect first (multiple layers for smooth glow) - gray glow
        for ($i = 3; $i -ge 1; $i--) {
            $glowAlpha = [int](50 * $i)
            $glowSize = $glowWidth * (1 + ($i * 0.3))
            $glowPen = New-Object System.Drawing.Pen([System.Drawing.Color]::FromArgb($glowAlpha, 128, 128, 128), $glowSize)
            $glowPen.LineJoin = [System.Drawing.Drawing2D.LineJoin]::Round
            $glowPen.StartCap = [System.Drawing.Drawing2D.LineCap]::Round
            $glowPen.EndCap = [System.Drawing.Drawing2D.LineCap]::Round
            $graphics.DrawCurve($glowPen, $points.ToArray(), 0.5)
            $glowPen.Dispose()
        }

        # Draw main wave line - bright white
        $pen = New-Object System.Drawing.Pen([System.Drawing.Color]::FromArgb(255, 255, 255, 255), $penWidth)
        $pen.LineJoin = [System.Drawing.Drawing2D.LineJoin]::Round
        $pen.StartCap = [System.Drawing.Drawing2D.LineCap]::Round
        $pen.EndCap = [System.Drawing.Drawing2D.LineCap]::Round
        $graphics.DrawCurve($pen, $points.ToArray(), 0.5)
        $pen.Dispose()
    }

    $graphics.Dispose()
    $blackBrush.Dispose()

    return $bitmap
}

# Create icons at multiple resolutions for proper scaling
$sizes = @(256, 128, 64, 48, 32, 16)
$bitmaps = @()

Write-Host "Creating multi-resolution icon..." -ForegroundColor Cyan

foreach ($size in $sizes) {
    Write-Host "  Generating ${size}x${size} bitmap..." -ForegroundColor Gray
    $bitmaps += Create-IconBitmap -size $size
}

# Save as PNG files and convert to ICO using magick if available, otherwise use native .NET
$iconPath = Join-Path $PSScriptRoot "icon.ico"

# Try to use ImageMagick if available (produces better quality)
$magickPath = Get-Command "magick" -ErrorAction SilentlyContinue
if (-not $magickPath) {
    # Check common installation location if not in PATH
    $magickExe = Get-ChildItem "C:\Program Files\ImageMagick-*\magick.exe" -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($magickExe) {
        $magickPath = @{ Source = $magickExe.FullName }
    }
}
if ($magickPath) {
    Write-Host "Using ImageMagick for high-quality conversion..." -ForegroundColor Yellow
    $pngFiles = @()

    foreach ($i in 0..($sizes.Count - 1)) {
        $pngPath = Join-Path $PSScriptRoot "icon_temp_$($sizes[$i]).png"
        $bitmaps[$i].Save($pngPath, [System.Drawing.Imaging.ImageFormat]::Png)
        $pngFiles += $pngPath
    }

    # Convert PNGs to ICO
    $pngArgs = ($pngFiles -join ' ') + " $iconPath"
    Start-Process -FilePath $magickPath.Source -ArgumentList $pngArgs -Wait -NoNewWindow

    # Cleanup temp files
    foreach ($png in $pngFiles) {
        Remove-Item $png -ErrorAction SilentlyContinue
    }
} else {
    # Fallback: Use .NET to create icon (single resolution)
    Write-Host "Using .NET for icon conversion (install ImageMagick for better quality)..." -ForegroundColor Yellow
    $largestBitmap = $bitmaps[0]
    $icon = [System.Drawing.Icon]::FromHandle($largestBitmap.GetHicon())
    $fileStream = [System.IO.File]::Create($iconPath)
    $icon.Save($fileStream)
    $fileStream.Close()
}

# Cleanup
foreach ($bmp in $bitmaps) {
    $bmp.Dispose()
}

Write-Host "`nIcon created successfully at: $iconPath" -ForegroundColor Green
Write-Host "The icon features a modern rounded square design with black background and white sine wave." -ForegroundColor Cyan
if (-not $magickPath) {
    Write-Host "`nTip: Install ImageMagick for multi-resolution icon support:" -ForegroundColor Yellow
    Write-Host "  winget install ImageMagick.ImageMagick" -ForegroundColor Gray
}

