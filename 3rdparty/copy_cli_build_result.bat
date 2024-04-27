@echo off
setlocal

set "target_dir=..\RealSR-NCNN-Android-GUI\app\src\main\assets\realsr"

xcopy "..\RealSR-NCNN-Android-CLI\Anime4k\build\intermediates\merged_native_libs\release\mergeReleaseNativeLibs\out\lib\arm64-v8a\*" "%target_dir%" /Y

xcopy "ncnn-android-vulkan-shared\arm64-v8a\lib\libncnn.so" "%target_dir%" /Y


echo Done.
pause
