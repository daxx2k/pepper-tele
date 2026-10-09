[CmdletBinding()] param([ValidateSet('All','Quest','Pepper')][string]$Target='All')
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
if(Test-Path -LiteralPath (Join-Path $root '.local\build.env.ps1')){. (Join-Path $root '.local\build.env.ps1')}
if(-not $env:ANDROID_HOME){$env:ANDROID_HOME=$env:ANDROID_SDK_ROOT}
if(-not $env:ANDROID_HOME){$env:ANDROID_HOME=Join-Path $env:LOCALAPPDATA 'Android\Sdk'}
if(-not (Test-Path -LiteralPath (Join-Path $env:ANDROID_HOME 'platform-tools\adb.exe'))){throw 'Set ANDROID_HOME to your Android SDK directory.'}
if(-not $env:JAVA_HOME){$env:JAVA_HOME=Join-Path $env:ProgramFiles 'Android\Android Studio\jbr'}
if(-not (Test-Path -LiteralPath (Join-Path $env:JAVA_HOME 'bin\java.exe'))){throw 'Set JAVA_HOME to JDK 17 or a compatible Android Studio runtime.'}
$env:PATH=(Join-Path $env:JAVA_HOME 'bin')+';'+$env:PATH
$sdk=$env:ANDROID_HOME.Replace('\','/')
Set-Content -LiteralPath (Join-Path $root 'local.properties') -Value "sdk.dir=$sdk"
$gradle=Join-Path $root 'gradlew.bat'
if(-not (Test-Path -LiteralPath $gradle)){throw 'Gradle wrapper missing; restore gradlew.bat and gradle/wrapper from the source package.'}
$tasks=@(switch($Target){'Quest'{':quest:assembleDebug'} 'Pepper'{':pepper:tablet:assembleDebug'} default {':quest:assembleDebug',':pepper:tablet:assembleDebug'}})
Push-Location $root
try { & $gradle @tasks --console=plain --no-daemon; if($LASTEXITCODE -ne 0){throw 'Build failed'} } finally { Pop-Location }
$dist=Join-Path $root 'dist';New-Item -ItemType Directory -Force -Path $dist | Out-Null
if($Target -ne 'Pepper'){Copy-Item -LiteralPath (Join-Path $root 'quest\build\outputs\apk\debug\quest-debug.apk') -Destination (Join-Path $dist 'TelePepper-Quest.apk')}
if($Target -ne 'Quest'){Copy-Item -LiteralPath (Join-Path $root 'pepper\tablet\build\outputs\apk\debug\tablet-debug.apk') -Destination (Join-Path $dist 'TelePepper-Pepper.apk')}
