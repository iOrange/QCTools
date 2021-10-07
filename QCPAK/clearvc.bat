::
@echo off
::
del /F /Q *.sdf >nul 2>&1
del /F /Q *.suo >nul 2>&1
del /F /Q *.user >nul 2>&1
del /F /Q *.sln >nul 2>&1
del /F /Q *.vcxproj >nul 2>&1
del /F /Q *.filters >nul 2>&1
del /F /Q *.pdb >nul 2>&1
del /F /Q ui_*.h >nul 2>&1
rd /S /Q ipch >nul 2>&1
rd /S /Q debug >nul 2>&1
rd /S /Q release >nul 2>&1
