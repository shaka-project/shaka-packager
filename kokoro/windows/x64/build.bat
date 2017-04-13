:: Copyright 2017 Google Inc. All Rights Reserved.
set OUTPUT_DIRECTORY=Release_x64
set GYP_DEFINES=target_arch=x64

set ROOTDIR=%cd%
set PACKAGERDIR=%ROOTDIR%\git\src

:: TODO(rkuroiwa): Put this in a batch script and source it, so that this
:: doesn't need to be copied for all configurations.
git clone "https://chromium.googlesource.com/chromium/tools/depot_tools.git" depot_tools
set DEPOTTOOLSDIR=%ROOTDIR%\depot_tools

python %PACKAGERDIR%\kokoro\deps_replacer.py "https://github.com" "https://github.googlesource.com"

cd %PACKAGERDIR%\..

:: Note that gclient file is a batch script, so 'call' must be used to wait for
:: the result.
:: Also gclient turns off echo, so echo is re-enabled after the command.
call %DEPOTTOOLSDIR%\gclient config "https://github.com/google/shaka-packager.git" --name=src --unmanaged
echo on
call %DEPOTTOOLSDIR%\gclient sync
echo on

cd src
%DEPOTTOOLSDIR%\ninja -C "out\%OUTPUT_DIRECTORY%" -k 100

copy "out\%OUTPUT_DIRECTORY%\packager.exe" packager-win.exe
for %%f in ("out\%OUTPUT_DIRECTORY%\*_*test.exe") do (%%f || exit /b 666)
python "out\%OUTPUT_DIRECTORY%\packager_test.py" -v
