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

@echo off
set __OLD_DIR="%CD%"
cd "%~dp0..\.."

if not defined CURL_BIN (
    set CURL_BIN=curl
)
if not defined PERL_BIN (
    set PERL_BIN=perl
)
if not defined CMAKE_BIN (
    set CMAKE_BIN=cmake
)

if not defined __BINARIESDIR (
    set __BINARIESDIR=%CD%\binaries
)

if not defined APPVEYOR (
    set APPVEYOR=False
)

if exist "C:\Program Files (x86)\Microsoft Visual Studio\2017\Professional" (
    set "VS150COMNTOOLS=C:\Program Files (x86)\Microsoft Visual Studio\2017\Professional\Common7\Tools\"
) else if exist "C:\Program Files (x86)\Microsoft Visual Studio\2017\Enterprise" (
    set "VS150COMNTOOLS=C:\Program Files (x86)\Microsoft Visual Studio\2017\Enterprise\Common7\Tools\"
) else (
    set "VS150COMNTOOLS=C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\Common7\Tools\"
)

if not exist "%VS150COMNTOOLS%" (
    echo "Could not fine VS2017 common tools"
    exit 1
)

set __RANDOM_VAL=%RANDOM::=_%
set __RANDOM_VAL-%RANDOM_VAL:.=%
set __BINARIESJUNCTION=%__BINARIESDIR:~0,2%\__withoutspace_binariesdir_%__RANDOM_VAL%
if not exist "%__BINARIESDIR%" (
    mkdir "%__BINARIESDIR%"
)
if not "%__BINARIESDIR: =%"=="%__BINARIESDIR%" (
    mklink /J %__BINARIESJUNCTION% "%__BINARIESDIR%"
    if not %ERRORLEVEL% == 0 (
        echo "Could not create junction to %__BINARIESJUNCTION% to %__BINARIESDIR% which contains space"
        exit 1
    )
    cd %__BINARIESJUNCTION%
) else (
    cd %__BINARIESDIR%
)
set BINARIESDIR=%CD%

if not defined LIBEDIT_VERSION (
    set LIBEDIT_VERSION=2.205
)
if not defined PYTHON_VERSION (
    set PYTHON_VERSION=3.6.8
)
if not defined OPENSSL_VERSION (
    set OPENSSL_VERSION=1_1_0j
)
if not defined ZLIB_VERSION (
    set ZLIB_VERSION=1.2.11
)
if not defined SWIG_VERSION (
    set SWIG_VERSION=4.0.1
)

set DO_DEBUG_BUILD=0
if "%~1"=="debug" (
    set DO_DEBUG_BUILD=1
)
if "%~1"=="Debug" (
    set DO_DEBUG_BUILD=1
)

cd %__OLD_DIR%
