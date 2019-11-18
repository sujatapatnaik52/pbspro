#!/bin/bash -xe

if [ $(id -u) -ne 0 ]; then
  echo "This script must be run by root user"
  exit 1
fi

. /etc/os-release

if [ "x${ONLY_REBUILD}" != "x1" ]; then
  SPEC_FILE=$(dirname $(dirname $0))/pbspro.spec
  REQ_FILE=$(dirname $(dirname $0))/test/fw/requirements.txt
  if [ ! -r ${SPEC_FILE} -o ! -r ${REQ_FILE} ]; then
    echo "Couldn't find pbspro.spec or requirement.txt"
    exit 1
  fi
  if [ "x${DEBIAN_FRONTEND}" == "x" ]; then
      export DEBIAN_FRONTEND=noninteractive
  fi
  apt-get -y update
  apt-get install -y build-essential dpkg-dev autoconf libtool rpm alien libssl-dev \
                     libxt-dev libpq-dev libexpat1-dev libedit-dev libncurses5-dev \
                     libical-dev libhwloc-dev pkg-config tcl-dev tk-dev python3-dev \
                     swig expat postgresql python3-pip sudo man-db
  pip3 install -r ${REQ_FILE}
fi

if [ "x${ONLY_INSTALL_DEPS}" == "x1" ]; then
  exit 0
fi

_targetdirname=target-${ID}
if [ "x${ONLY_REBUILD}" != "x1" ]; then
  rm -rf ${_targetdirname}
fi
mkdir -p ${_targetdirname}
if [ "x${ONLY_REBUILD}" != "x1" ]; then
  [[ -f Makefile ]] && make distclean || true
  ./autogen.sh
  _cflags="-g -O2 -Wall -Werror"
  if [ "x${ID}" == "xubuntu" ]; then
    _cflags="${_cflags} -Wno-unused-result"
  fi
  cd ${_targetdirname}
  ../configure CFLAGS="${_cflags}" --prefix /opt/pbs --enable-ptl
  cd -
fi
cd ${_targetdirname}
make -j8
make -j8 install
chmod 4755 /opt/pbs/sbin/pbs_iff /opt/pbs/sbin/pbs_rcp
if [ "x${DONT_START_PBS}" != "x1" ]; then
  if [ "x${ONLY_REBUILD}" != "x1" ]; then
    /opt/pbs/libexec/pbs_postinstall server
    sed -i "s@PBS_START_MOM=0@PBS_START_MOM=0@" /etc/pbs.conf
  fi
  sed -i "s@PBS_START_MOM=0@PBS_START_MOM=0@" /etc/pbs.conf
  /etc/init.d/pbs restart
fi
set +e
. /etc/profile.d/ptl.sh
set -e
pbs_config --make-ug
