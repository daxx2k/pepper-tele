[CmdletBinding()] param([string]$PepperAddress='',[string]$QuestSerial='', [switch]$Pepper, [switch]$Quest)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$adb=Join-Path $env:LOCALAPPDATA 'Android\Sdk\platform-tools\adb.exe'
if(-not $Pepper -and -not $Quest){$Pepper=$true;$Quest=$true}
if($Pepper){
    if(-not $PepperAddress){throw 'Provide -PepperAddress TABLET_IP:5555'}
    & $adb connect $PepperAddress
    if((& $adb -s $PepperAddress get-state 2>$null) -ne 'device'){throw 'Pepper is not reachable or ADB is not authorized'}
    & $adb -s $PepperAddress install -r (Join-Path $root 'dist\TelePepper-Pepper.apk')
    if($LASTEXITCODE -ne 0){throw 'Pepper installation failed'}
    & $adb -s $PepperAddress shell am start -n it.telepepper.pepper/.MainActivity
}
if($Quest){
    if(-not $QuestSerial){throw 'Provide -QuestSerial from adb devices to select the headset explicitly'}
    & $adb -s $QuestSerial install -r (Join-Path $root 'dist\TelePepper-Quest.apk')
    if($LASTEXITCODE -ne 0){throw 'Quest installation failed'}
    & $adb -s $QuestSerial shell am start -n it.telepepper.quest/.TeleoperationActivity
}
