@echo off

set SRC=%~dp0\x64\Release\
set DEST=C:\Program Files\omiPlayerOmni\
set EXE=omiPlayerOmni.exe

echo Uninstalling %DEST%
rmdir /S "%DEST%"

SET st2Path=%DEST%%EXE%
echo Unregister shell extension
@reg delete "HKEY_CLASSES_ROOT\*\shell\Open with omiPlayerOmni" /f

pause
