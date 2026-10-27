@echo off
setlocal
cd /d "%~dp0.."
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
if not exist tests mkdir tests
cl /nologo /std:c++17 /EHsc /O2 /MD /utf-8 /DNOMINMAX /DWIN32_LEAN_AND_MEAN /DUNICODE /D_UNICODE ^
   /Fe:tests\test_subtitle_render.exe ^
   tests\test_subtitle_render.cpp src\ui\Widgets.cpp src\core\Theme.cpp src\core\Util.cpp src\core\Config.cpp src\core\Json.cpp ^
   /I src /I third_party\sherpa /I third_party\ctranslate2 /I third_party\sentencepiece\include /Fo:tests\ ^
   winhttp.lib ole32.lib oleaut32.lib uuid.lib shell32.lib comdlg32.lib gdi32.lib user32.lib advapi32.lib comctl32.lib dwmapi.lib shcore.lib version.lib
if errorlevel 1 exit /b 1
tests\test_subtitle_render.exe
exit /b %errorlevel%
