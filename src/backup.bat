@echo off
set DIR="K:\Archives\SmartHome"
set FN="AELibraries"

7z a -r "%DIR%\%FN%.New.7z"
if errorlevel 1 goto error

ren "%DIR%\%FN%.7z" "%FN%.Old.7z"
ren "%DIR%\%FN%.New.7z" "%FN%.7z"
del "%DIR%\%FN%.Old.7z"

goto :exit

:error
del "%DIR%\%FN%.New.7z"

:exit
