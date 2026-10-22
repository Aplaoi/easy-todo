@echo off
setlocal
cd /d "%~dp0"

set "VCVARS=D:\VisualStudio\VC\Auxiliary\Build\vcvars64.bat"
if defined VCINSTALLDIR goto :build
if exist "%VCVARS%" (
  call "%VCVARS%" >nul
) else (
  where cl >nul 2>nul || (
    echo [ERROR] MSVC not found. Install "Desktop development with C++" or run this from a Developer Command Prompt.
    exit /b 1
  )
)

:build
if not exist build mkdir build

cl /nologo /std:c++20 /utf-8 /permissive- /EHsc /O2 /MT /W4 ^
  /DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN /DNOMINMAX /D_CRT_SECURE_NO_WARNINGS ^
  /I"src" ^
  src\main.cpp src\app.cpp src\panel.cpp src\paint.cpp src\model.cpp src\store.cpp src\json.cpp ^
  /Fe:"build\HomeworkFloat.exe" /Fo:build\ ^
  /link /SUBSYSTEM:WINDOWS /MANIFEST:EMBED /MANIFESTINPUT:"src\app.manifest" ^
  /MANIFESTUAC:"level='asInvoker' uiAccess='false'"

if errorlevel 1 (
  echo.
  echo [ERROR] Build failed.
  exit /b 1
)

echo.
echo [OK] build\HomeworkFloat.exe
for %%F in ("build\HomeworkFloat.exe") do echo      %%~zF bytes
exit /b 0