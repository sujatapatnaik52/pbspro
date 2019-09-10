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

1>nul 2>nul call "%~dp0set_paths.bat" %~1

if defined WIX (
	set "PATH=%WIX%bin;%PATH%"
)

call "%~dp0..\..\pbspro\win_configure\msi\wix\prep.bat" "%BINARIESDIR%" %~1
if not %ERRORLEVEL% == 0 (
    echo Failed to prepare PBS_EXEC!
    exit /b 1
)

if not exist "%BINARIESDIR%\vcredist_x86.exe" (
    "%CURL_BIN%" -qkL -o "%BINARIESDIR%\vcredist_x86.exe" https://download.microsoft.com/download/1/6/5/165255E7-1014-4D0A-B094-B6A430A6BFFC/vcredist_x86.exe
    if not exist "%BINARIESDIR%\vcredist_x86.exe" (
        echo "Failed to download Microsoft Visual C++ 2010 Redistributable Package (x86)"
        exit /b 1
    )
)

if exist "%VS150COMNTOOLS%\..\..\VC\Redist\MSVC\14.16.27012\vcredist_x86.exe" (
    1>nul copy /B /Y "%VS150COMNTOOLS%\..\..\VC\Redist\MSVC\14.16.27012\vcredist_x86.exe" "%~dp0..\..\PBS\exec\etc\vcredist_x86.exe"
    if not exist "%~dp0..\..\PBS\exec\etc\vcredist_x86.exe" (
        echo "Failed to copy Microsoft Visual C++ 2017 Redistributable Package (x86)"
    )
)

if not exist "%VS150COMNTOOLS%\..\..\VC\Redist\MSVC\14.16.27012\vcredist_x86.exe" (
    1>nul copy /B /Y "%BINARIESDIR%\vcredist_x86.exe" "%~dp0..\..\PBS\exec\etc\vcredist_x86.exe"
    echo "Copying 2010 distributable"
)


call "%~dp0..\..\pbspro\win_configure\msi\wix\create_msi.bat" %~1
if not %ERRORLEVEL% == 0 (
    echo Failed to generate PBS Pro MSI!
    exit /b 1
)

dir "%~dp0..\..\win_build\msi"

exit /b 0

