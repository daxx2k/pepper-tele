[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$Serial,[string]$Adb='adb')
$ErrorActionPreference='Stop'
$apk=Join-Path $PSScriptRoot 'TelePepper-Quest25.apk'
if(-not (Test-Path -LiteralPath $apk)){throw "Missing APK: $apk"}
& $Adb -s $Serial get-state
if($LASTEXITCODE -ne 0){throw 'Device unavailable. Connect and authorize Quest USB first.'}
& $Adb -s $Serial install -r $apk
if($LASTEXITCODE -ne 0){throw 'Installation failed. Device data has not been cleared.'}
& $Adb -s $Serial shell am start -n 'it.telepepper.quest25/it.telepepper.quest.LauncherActivity'
if($LASTEXITCODE -ne 0){throw 'Installed, but could not open app. Open TelePepper 2.5 on Quest.'}
