:: Copyright 2017 Google Inc. All Rights Reserved.
set ROOTDIR=%cd%
set PACKAGERDIR=%ROOTDIR%\git\src

set GYP_DEFINES="target_arch=%PLATFORM%"
if "%PLATFORM%"=="x64" (
  set OUTPUT_DIRECTORY="out\%CONFIGURATION%_x64"
) else (
  set OUTPUT_DIRECTORY="out\%CONFIGURATION%"
)

:: TODO(rkuroiwa): Put this in a batch script and source it, so that this
:: doesn't need to be copied for all configurations.
git clone "https://chromium.googlesource.com/chromium/tools/depot_tools.git" depot_tools
set DEPOTTOOLSDIR=%ROOTDIR%\depot_tools

python %PACKAGERDIR%\kokoro\deps_replacer.py "https://github.com" "https://github.googlesource.com"

cd %PACKAGERDIR%\..

:: Disable downloading google-internal version of WIN TOOLCHAIN. Use locally
:: installed version instead.
set DEPOT_TOOLS_WIN_TOOLCHAIN=0

:: Note that gclient file is a batch script, so 'call' must be used to wait for
:: the result.
:: Also gclient turns off echo, so echo is re-enabled after the command.
call %DEPOTTOOLSDIR%\gclient config "https://github.com/google/shaka-packager.git" --name=src --unmanaged
echo on
call %DEPOTTOOLSDIR%\gclient sync
echo on

cd src
%DEPOTTOOLSDIR%\ninja -C "%OUTPUT_DIRECTORY%" -k 100 || exit /b 1

copy "%OUTPUT_DIRECTORY%\packager.exe" packager-win.exe
for %%f in ("%OUTPUT_DIRECTORY%\*_*test.exe") do (%%f || exit /b 666)
python "%OUTPUT_DIRECTORY%\packager_test.py" -v
