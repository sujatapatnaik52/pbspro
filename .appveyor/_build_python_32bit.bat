@echo off
REM Copyright (C) 1994-2019 Altair Engineering, Inc.
REM For more information, contact Altair at www.altair.com.
REM
REM This file is part of the PBS Professional ("PBS Pro") software.
REM
REM Open Source License Information:
REM
REM PBS Pro is free software. You can redistribute it and/or modify it under the
REM terms of the GNU Affero General Public License as published by the Free
REM Software Foundation, either version 3 of the License, or (at your option) any
REM later version.
REM
REM PBS Pro is distributed in the hope that it will be useful, but WITHOUT ANY
REM WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
REM PARTICULAR PURPOSE.  See the GNU Affero General Public License for more details.
REM
REM You should have received a copy of the GNU Affero General Public License along
REM with this program.  If not, see <http://www.gnu.org/licenses/>.
REM
REM Commercial License Information:
REM
REM The PBS Pro software is licensed under the terms of the GNU Affero General
REM Public License agreement ("AGPL"), except where a separate commercial license
REM agreement for PBS Pro version 14 or later has been executed in writing with Altair.
REM
REM Altair’s dual-license business model allows companies, individuals, and
REM organizations to create proprietary derivative works of PBS Pro and distribute
REM them - whether embedded or bundled with other software - under a commercial
REM license agreement.
REM
REM Use of Altair’s trademarks, including but not limited to "PBS™",
REM "PBS Professional®", and "PBS Pro™" and Altair’s logos is subject to Altair's
REM trademark licensing policies.

@echo on
setlocal

call "%~dp0set_paths.bat" %~1

cd "%BINARIESDIR%"

if not defined PYTHON_VERSION (
    echo "Please set PYTHON_VERSION to Python version!"
    exit /b 1
)

if exist "%BINARIESDIR%\python" (
    echo "%BINARIESDIR%\python exist already!"
    exit /b 0
)

if not exist "%BINARIESDIR%\cpython-%PYTHON_VERSION%.zip" (
    "%CURL_BIN%" -qkL -o "%BINARIESDIR%\cpython-%PYTHON_VERSION%.zip" https://github.com/python/cpython/archive/v%PYTHON_VERSION%.zip
    if not exist "%BINARIESDIR%\cpython-%PYTHON_VERSION%.zip" (
        echo "Failed to download python"
        exit /b 1
    )
)
2>nul rd /S /Q "%BINARIESDIR%\cpython-%PYTHON_VERSION%"
"%UNZIP_BIN%" -q "%BINARIESDIR%\cpython-%PYTHON_VERSION%.zip"
cd "%BINARIESDIR%\cpython-%PYTHON_VERSION%"

REM Restore externals directory if python_externals.tar.gz exists
if exist "%BINARIESDIR%\python_externals.tar.gz" (
    if not exist "%BINARIESDIR%\cpython-%PYTHON_VERSION%\externals" (
        "%MSYSDIR%\bin\bash" --login -i -c "cd \"$BINARIESDIR_M/cpython-$PYTHON_VERSION\" && tar -xf \"$BINARIESDIR_M/python_externals.tar.gz\""
    )
)
REM "Set MSBUILD to VS2017 before calling env.bat"
call "%BINARIESDIR%\cpython-%PYTHON_VERSION%\PCbuild\find_msbuild.bat"
call "%BINARIESDIR%\cpython-%PYTHON_VERSION%\PCbuild\env.bat" x86

REM call "%BINARIESDIR%\cpython-%PYTHON_VERSION%\PCbuild\build.bat" -e 
if not %ERRORLEVEL% == 0 (
    echo "Failed to compile Python 32bit"
    exit /b 1
)

set PATH=%BINARIESDIR%;%PATH%

REM Workaround for python368.chm
mkdir "%BINARIESDIR%\cpython-%PYTHON_VERSION%\Doc\build\htmlhelp"
echo "dummy chm file to make msi.py happy" > "%BINARIESDIR%\cpython-%PYTHON_VERSION%\Doc\build\htmlhelp\python368.chm"

cd "%BINARIESDIR%\cpython-%PYTHON_VERSION%\Tools\msi"
set PCBUILD=PCbuild\win32
set SNAPSHOT=0

REM set PYTHON="%BINARIESDIR%\cpython-%PYTHON_VERSION%\%PCBUILD%\python.exe"
cd %BINARIESDIR%\cpython-%PYTHON_VERSION%\%PCBUILD%"

call "%BINARIESDIR%\cpython-%PYTHON_VERSION%\Tools\msi\build.bat" -x86
REM Find the installer generated.
for /f "usebackq" %%a in (`dir /b %BINARIESDIR%\cpython-%PYTHON_VERSION%\%PCBUILD%\en-us\python-3.6*.exe`) do ( set PY_EXE_NAME=%%a )

if not exist "%BINARIESDIR%\cpython-%PYTHON_VERSION%\%PCBUILD%\en-us\%PY_EXE_NAME%" (
    echo "Failed to generate Python 32bit no binary"
    exit /b 1
)

start /wait /d "%BINARIESDIR%\cpython-%PYTHON_VERSION%\%PCBUILD%\en-us\" %PY_EXE_NAME% /quiet InstallAllUsers=0 TargetDir="%BINARIESDIR%\python"
if not %ERRORLEVEL% == 0 (
    echo "Failed to extract  Python 32bit to %BINARIESDIR%\python"
    exit /b 1
)

"%BINARIESDIR%\cpython-%PYTHON_VERSION%\%PCBUILD%\python.exe" -m ensurepip -U --default-pip
if not %ERRORLEVEL% == 0 (
    echo "Failed to run ensurepip for Python 32bit"
    exit /b 1
)
set PIP_EXTRA_ARGS=
if exist "%BINARIESDIR%\pypiwin32-223-py3-none-any.whl" (
    set PIP_EXTRA_ARGS=--no-index --find-links="%BINARIESDIR%" --no-cache-dir
)
"%BINARIESDIR%\cpython-%PYTHON_VERSION%\%PCBUILD%\python.exe" -m pip install %PIP_EXTRA_ARGS% pypiwin32==223
if not %ERRORLEVEL% == 0 (
    echo "Failed to install pypiwin32 for Python 32bit"
    exit /b 1
)

REM "%BINARIESDIR%\cpython-%PYTHON_VERSION%\%PCBUILD%\python.exe" -m pip install %PIP_EXTRA_ARGS% sphinx
if not %ERRORLEVEL% == 0 (
    echo "Failed to install sphinx for Python 32bit"
    exit /b 1
)

REM "%BINARIESDIR%\python\python.exe" -Wi "%BINARIESDIR%\python\Lib\compileall.py" -q -f -x "bad_coding|badsyntax|site-packages|py3_" "%BINARIESDIR%\python\Lib"
REM "%BINARIESDIR%\python\python.exe" -O -Wi "%BINARIESDIR%\python\Lib\compileall.py" -q -f -x "bad_coding|badsyntax|site-packages|py3_" "%BINARIESDIR%\python\Lib"
REM "%BINARIESDIR%\python\python.exe" -c "import lib2to3.pygram, lib2to3.patcomp;lib2to3.patcomp.PatternCompiler()"
"%BINARIESDIR%\python\python.exe" -m ensurepip -U --default-pip
if not %ERRORLEVEL% == 0 (
    echo "Failed to run ensurepip in %BINARIESDIR%\python for Python 32bit"
    exit /b 1
)
"%BINARIESDIR%\python\python.exe" -m pip install %PIP_EXTRA_ARGS% pypiwin32==223
if not %ERRORLEVEL% == 0 (
    echo "Failed to install pypiwin32 in %BINARIESDIR%\python for Python 32bit"
    exit /b 1
)
copy /B /Y "%VS140COMNTOOLS%..\..\VC\redist\x64\Microsoft.VC140.CRT\msvcp140.dll" "%BINARIESDIR%\python\"
if not %ERRORLEVEL% == 0 (
    echo "Failed to copy msvcr90.dll in %BINARIESDIR%\python for Python 32bit"
    exit /b 1
)
REM copy /B /Y "%VS140COMNTOOLS%..\..\VC\redist\x64\Microsoft.VC140.CRT\Microsoft.VC90.CRT.manifest" "%BINARIESDIR%\python\"
if not %ERRORLEVEL% == 0 (
    echo "Failed to copy Microsoft.VC90.CRT.manifest in %BINARIESDIR%\python for Python 32bit"
    exit /b 1
)


cd "%BINARIESDIR%"
2>nul rd /S /Q "%BINARIESDIR%\cpython-%PYTHON_VERSION%"
2>nul del /Q /F "%BINARIESDIR%\cabarc.exe"

exit /b 0

