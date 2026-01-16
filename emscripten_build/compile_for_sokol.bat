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
	call em++ ^
		-std=c++17 ^
		-s USE_WEBGL2=1 ^
		-s ALLOW_MEMORY_GROWTH=1 ^
		-I ..\common ^
		src\main.cpp ^
		-o "%out%\sokol.html" ^
		--shell-file "%build%\sokol_shell.html" ^
		--preload-file assets
) else (
	echo compiling without assets...
	call em++ ^
		-std=c++17 ^
		-s USE_WEBGL2=1 ^
		-s ALLOW_MEMORY_GROWTH=1 ^
		-I ..\common ^
		src\main.cpp ^
		-o "%out%\sokol_shell.html" ^
		--shell-file "%build%\shell.html"
)

echo success

:wait
pause