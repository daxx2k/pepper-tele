[CmdletBinding()] param()
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
if(-not $env:ROBOTTELE_META_KEYSTORE -or -not $env:ROBOTTELE_META_KEY_PASSWORD){throw 'Set the private signing key path and password environment variables first.'}
if(-not (Test-Path -LiteralPath $env:ROBOTTELE_META_KEYSTORE)){throw 'Private signing key not found.'}
if(-not $env:ANDROID_HOME){$env:ANDROID_HOME=Join-Path $env:LOCALAPPDATA 'Android/Sdk'}
if(-not $env:JAVA_HOME){$env:JAVA_HOME=Join-Path $env:ProgramFiles 'Android/Android Studio/jbr'}
$env:PATH=(Join-Path $env:JAVA_HOME 'bin')+';'+$env:PATH
$env:ROBOTTELE_META_BUILD_ROOT=Join-Path $env:LOCALAPPDATA ('RobotTele/meta-build/'+(Split-Path $root -Leaf))
New-Item -ItemType Directory -Force $env:ROBOTTELE_META_BUILD_ROOT | Out-Null
$init=Join-Path $env:ROBOTTELE_META_BUILD_ROOT 'output-paths.gradle'
@'
allprojects {
  buildDir = new File(System.getenv('ROBOTTELE_META_BUILD_ROOT'), project.path.replace(':','_'))
  plugins.withId('com.android.application') {
    android.externalNativeBuild.cmake.buildStagingDirectory = new File(buildDir,'cxx')
  }
}
'@ | Set-Content -LiteralPath $init -Encoding utf8
Set-Content -LiteralPath (Join-Path $root 'local.properties') -Value ('sdk.dir='+$env:ANDROID_HOME.Replace('\','/'))
$previousNaoBuildRoot=$env:TELENAO_BUILD_ROOT
$env:TELENAO_BUILD_ROOT=$null
Push-Location $root
try {
 & .\gradlew.bat -I $init -PmetaBeta=true :quest:assembleRelease --console=plain --no-daemon
 if($LASTEXITCODE -ne 0){throw 'Meta Beta build failed'}
 $apk=Join-Path $env:ROBOTTELE_META_BUILD_ROOT '_quest/outputs/apk/release/quest-release.apk'
 $dist=Join-Path $root 'dist/meta-beta';New-Item -ItemType Directory -Force $dist | Out-Null
 $name='TelePepper25-MetaBeta.apk'
 Copy-Item -LiteralPath $apk -Destination (Join-Path $dist $name)
 Get-FileHash -LiteralPath (Join-Path $dist $name) -Algorithm SHA256
} finally {Pop-Location;$env:TELENAO_BUILD_ROOT=$previousNaoBuildRoot}
