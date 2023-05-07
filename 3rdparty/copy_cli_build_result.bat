@echo off
setlocal

set "target_dir=..\RealSR-NCNN-Android-GUI\app\src\main\assets\realsr"

for /d %%D in (..\*) do (
    if exist "%%D\app\build\intermediates\cmake\debug\obj\arm64-v8a\*-ncnn" (
        rem echo Copying files from "%%D\app\build\intermediates\cmake\debug\obj\arm64-v8a"
        xcopy "%%D\app\build\intermediates\cmake\debug\obj\arm64-v8a\*-ncnn" "%target_dir%" /Y
    )
    
    if exist "%%D\app\build\intermediates\cmake\release\obj\arm64-v8a\*-ncnn" (
        rem echo Copying files from "%%D\app\build\intermediates\cmake\release\obj\arm64-v8a"
        xcopy "%%D\app\build\intermediates\cmake\release\obj\arm64-v8a\*-ncnn" "%target_dir%" /Y
    )
)

echo Done.
pause
