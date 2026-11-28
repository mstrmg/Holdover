# Contact sheet of the range check pictures (for a quick look; judge details from the full-size pictures).
# Usage: powershell -File contact_sheet.ps1 <folder with png> <out.jpg> [columns] [width]
param([string]$Dir, [string]$Out, [int]$Cols = 4, [int]$W = 640, [string]$Ref = "")
Add-Type -AssemblyName System.Drawing
$files = Get-ChildItem $Dir -Filter *.png | Sort-Object Name
if ($Ref -ne "") { $files = @(Get-Item $Ref) + $files }
$H = [int]($W * 9 / 16)
$rows = [math]::Ceiling($files.Count / $Cols)
$bmp = New-Object System.Drawing.Bitmap ($Cols * $W), ($rows * ($H + 18))
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
$g.Clear([System.Drawing.Color]::Black)
$font = New-Object System.Drawing.Font 'Arial', 10
for ($i = 0; $i -lt $files.Count; $i++) {
    $im = [System.Drawing.Image]::FromFile($files[$i].FullName)
    $x = ($i % $Cols) * $W; $y = [math]::Floor($i / $Cols) * ($H + 18)
    $g.DrawImage($im, $x, $y + 18, $W, $H)
    $g.DrawString($files[$i].BaseName, $font, [System.Drawing.Brushes]::White, $x + 2, $y + 2)
    $im.Dispose()
}
$codec = [System.Drawing.Imaging.ImageCodecInfo]::GetImageEncoders() | Where-Object { $_.MimeType -eq 'image/jpeg' }
$ep = New-Object System.Drawing.Imaging.EncoderParameters 1
$ep.Param[0] = New-Object System.Drawing.Imaging.EncoderParameter ([System.Drawing.Imaging.Encoder]::Quality), 88L
$bmp.Save($Out, $codec, $ep)
$g.Dispose(); $bmp.Dispose()
