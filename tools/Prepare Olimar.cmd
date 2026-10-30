@echo off
setlocal
if "%~1"=="" (
  echo Drag your olimar.obj file onto this Prepare Olimar.cmd file.
  echo Keep olimar.mtl beside the OBJ if you have it.
  echo No Docker or Python installation is needed in the Windows kit.
  pause
  exit /b 1
)
"%~dp0runtime\python.exe" -E -s "%~dp0tools\prepare_model.py" "%~1"
if errorlevel 1 (
  echo.
  echo Preparation failed. Read the error above.
  echo If output already exists, move or delete only the old generated olimar.p2mesh and retry.
  pause
  exit /b 1
)
echo.
echo Copy the contents of sd-card to your SD card root.
echo The CIA installer file is in the install folder.
echo This is an untextured model test, not playable Pikmin 2.
pause
