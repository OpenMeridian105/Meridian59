@echo off
setlocal

if "%~2"=="" exit /b 1

set "SRC_DIR=%~1"
set "DST_DIR=%~2"

for %%F in (
    libpq.dll
    libcrypto-3-x64.dll
    libintl-9.dll
    libssl-3-x64.dll
    libiconv-2.dll
    libwinpthread-1.dll
) do (
    copy /Y "%SRC_DIR%\%%F" "%DST_DIR%" >nul || exit /b 1
)

exit /b 0