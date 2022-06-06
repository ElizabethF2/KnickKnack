@ECHO OFF
SETLOCAL

set sockets=1
set files=0
set threads=1

cd %~dp0
set binpath=..\KnickKnack.exe
set opt=
set src=../src
if %sockets%==1 (set opt=%opt% -lwsock32 -DKNICKKNACK_ENABLE_SOCKETS)
if %files%==1 (set opt=%opt% -DKNICKKNACK_ENABLE_FILEIO)
if %threads%==1 (set opt=%opt% -lpthread -DKNICKKNACK_ENABLE_THREADS)

del "%binpath%"
g++ -o "%binpath%" -O3 main.cpp -I"%src%" -DKNICKKNACK_CDECL %src%/*.cpp %opt% -lpsapi
