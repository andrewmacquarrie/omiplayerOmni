@echo off

set SRC=%~dp0\x64\Release\
set DEST=C:\Program Files\omiPlayerOmni\
set EXE=omiPlayerOmni.exe

echo Installing %SRC% into %DEST%
xcopy /Y "%SRC%*.dll" "%DEST%"
xcopy /Y "%SRC%*.exe" "%DEST%"

pause
