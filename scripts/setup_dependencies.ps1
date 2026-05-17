$wv2Version = "1.0.2592.51"
$url = "https://www.nuget.org/api/v2/package/Microsoft.Web.WebView2/$wv2Version"
$outputDir = "third_party/webview2"
$tempFile = "wv2.zip"

if (!(Test-Path "third_party")) { New-Item -ItemType Directory -Path "third_party" }
if (!(Test-Path $outputDir)) { New-Item -ItemType Directory -Path $outputDir }

echo "Downloading WebView2 SDK v$wv2Version..."
Invoke-WebRequest -Uri $url -OutFile $tempFile

echo "Extracting SDK..."
Expand-Archive -Path $tempFile -DestinationPath $outputDir -Force
Remove-Item $tempFile

echo "WebView2 SDK setup complete."
