@echo off

set SRC=%~dp0\x64\Release\
set DEST=C:\Program Files\omiPlayer\
set EXE=omiPlayer.exe

echo Installing %SRC% into %DEST%
xcopy /Y /D "%SRC%*.dll" "%DEST%"
xcopy /Y /D "%SRC%*.exe" "%DEST%"

SET st2Path=%DEST%%EXE%
echo Register shell extension
@reg add "HKEY_CLASSES_ROOT\*\shell\Open with omiPlayer"         /t REG_SZ        /v ""     /d "Open with omiPlayer" /f
@reg add "HKEY_CLASSES_ROOT\*\shell\Open with omiPlayer"         /t REG_EXPAND_SZ /v "Icon" /d "%st2Path%,0"              /f
@reg add "HKEY_CLASSES_ROOT\*\shell\Open with omiPlayer\command" /t REG_SZ        /v ""     /d "%st2Path% \"%%1\""        /f

pause
