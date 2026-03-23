@echo off
echo Building project...

REM Go to backend folder
cd backend

REM Create build folder if it doesn't exist
if not exist build mkdir build
cd build

REM Build project
cmake ..
cmake --build .

REM Run server
echo Running server...
if exist Debug\server.exe (
    cd Debug
    .\server.exe
) else if exist server.exe (
    .\server.exe
) else (
    echo server.exe not found!
)

pause


@REM @echo off
@REM setlocal

@REM set "ROOT=%~dp0"
@REM set "CMAKE_EXE=cmake"

@REM if exist "C:\Program Files\CMake\bin\cmake.exe" (
@REM     set "CMAKE_EXE=C:\Program Files\CMake\bin\cmake.exe"
@REM )

@REM if exist "C:\msys64\mingw64\bin\g++.exe" (
@REM     set "PATH=C:\msys64\mingw64\bin;C:\msys64\usr\bin;%PATH%"
@REM )

@REM where g++.exe >nul 2>nul
@REM if errorlevel 1 (
@REM     echo Error: g++.exe not found. Install MSYS2 MinGW64 or add it to PATH.
@REM     pause
@REM     exit /b 1
@REM )

@REM echo Building project...

@REM REM Go to backend folder
@REM cd /d "%ROOT%backend"

@REM REM Create build folder if it doesn't exist
@REM if not exist build-mingw mkdir build-mingw
@REM cd build-mingw

@REM REM Build project
@REM "%CMAKE_EXE%" -G "MinGW Makefiles" ..
@REM if errorlevel 1 (
@REM     echo Configure failed.
@REM     pause
@REM     exit /b 1
@REM )

@REM "%CMAKE_EXE%" --build .
@REM if errorlevel 1 (
@REM     echo Build failed.
@REM     pause
@REM     exit /b 1
@REM )

@REM REM Run server
@REM echo Running server...
@REM if exist server.exe (
@REM     .\server.exe
@REM ) else (
@REM     echo server.exe not found!
@REM     pause
@REM     exit /b 1
@REM )

@REM pause