@echo off
cd libs
echo Copy DLLs to %1
for /r %%f in (*.dll) do @xcopy "%%f" %1 /D
cd ..
