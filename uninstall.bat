@echo off

set SRC=%~dp0\x64\Release\
set DEST=C:\Program Files\omiPlayer\
set EXE=omiPlayer.exe

echo Uninstalling %DEST%
rmdir /S "%DEST%"

SET st2Path=%DEST%%EXE%
echo Unregister shell extension
@reg delete "HKEY_CLASSES_ROOT\*\shell\Open with omiPlayer" /f

pause
