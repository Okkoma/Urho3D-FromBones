@echo off
REM if exist "%~dp0Urho3DPlayer.exe" (set "DEBUG=") else (set "DEBUG=_d")
REM if [%1] == [] (set "OPT1=-w -s") else (set "OPT1=")
REM start "" "%~dp0Urho3DPlayer%DEBUG%" Scripts/Editor.as %OPT1% %*

start ./Urho3DPlayer.exe Scripts/Editor.as