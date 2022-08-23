call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Professional\VC\Auxiliary\Build\vcvars64.bat"

set SolutionDir=%WORKSPACE%
msbuild %SolutionDir%\win\project\sockperf.vcxproj /t:Build /p:Configuration=Release;Platform=x64
if %errorlevel% neq 0 exit /b %errorlevel%
