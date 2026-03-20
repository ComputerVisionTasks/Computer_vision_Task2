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