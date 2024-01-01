@echo off
setlocal

set "target_dir=..\RealSR-NCNN-Android-GUI\app\src\main\assets\realsr"

xcopy "ncnn-android-vulkan-shared\arm64-v8a\lib\libncnn.so" "%target_dir%" /Y

:: old project path
:: for /d %%D in (..\*) do (
::     if exist "%%D\app\build\intermediates\cmake\debug\obj\arm64-v8a\*-ncnn" (
::         rem echo Copying files from "%%D\app\build\intermediates\cmake\debug\obj\arm64-v8a"
::         xcopy "%%D\app\build\intermediates\cmake\debug\obj\arm64-v8a\*-ncnn" "%target_dir%" /Y
::     )
::     
::     if exist "%%D\app\build\intermediates\cmake\release\obj\arm64-v8a\*-ncnn" (
::         rem echo Copying files from "%%D\app\build\intermediates\cmake\release\obj\arm64-v8a"
::         xcopy "%%D\app\build\intermediates\cmake\release\obj\arm64-v8a\*-ncnn" "%target_dir%" /Y
::     )
:: )

:: new project path
for /d %%D in (..\RealSR-NCNN-Android-CLI\*) do (
    if exist "%%D\build\intermediates\cmake\debug\obj\arm64-v8a\*-ncnn" (
        rem echo Copying files from "%%D\app\build\intermediates\cmake\debug\obj\arm64-v8a"
        xcopy "%%D\build\intermediates\cmake\debug\obj\arm64-v8a\*-ncnn" "%target_dir%" /Y
    )
    
    if exist "%%D\build\intermediates\cmake\release\obj\arm64-v8a\*-ncnn" (
        rem echo Copying files from "%%D\app\build\intermediates\cmake\release\obj\arm64-v8a"
        xcopy "%%D\build\intermediates\cmake\release\obj\arm64-v8a\*-ncnn" "%target_dir%" /Y
    )
)


echo Done.
pause
