:: don't echo each command
@echo off

:: get build dir(current)
set "build=%CD%"

:: goto emsdk dir
cd /d C:\emsdk

:: start emscripten environment
call emsdk_env.bat

:: goto simsAndGames dir
cd /d "%build%\.."

:: prompt for project to compile
set /p project=Enter project name: 

if exist "%project%" (
	:: goto project dir
	cd "%project%"
) else (
	echo invalid project
	goto wait
)

:: make out dir
set "out=%build%\%project%"
if not exist "%out%" mkdir "%out%"

if exist assets (
	echo compiling with assets...
	call em++ -std=c++17 -O2 -s ALLOW_MEMORY_GROWTH=1 -s MAX_WEBGL_VERSION=2 -s MIN_WEBGL_VERSION=2 -s USE_LIBPNG=1 -I ..\common src\main.cpp -o "%out%\pge.html" --preload-file assets
) else (
	echo compiling without assets...
	call em++ -std=c++17 -O2 -s ALLOW_MEMORY_GROWTH=1 -s MAX_WEBGL_VERSION=2 -s MIN_WEBGL_VERSION=2 -s USE_LIBPNG=1 -I ..\common src\main.cpp -o "%out%\pge.html"
)

:: copy run_olc.html
copy "%build%\run_olc.html" "%out%"

echo success

:wait
pause