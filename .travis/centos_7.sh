#!/bin/bash -xe
yum clean all
yum -y update
yum -y install yum-utils epel-release rpmdevtools
rpmdev-setuptree
yum-builddep -y ./pbspro.spec
./autogen.sh
./configure
make dist
/bin/bash -c 'cp -fv pbspro-*.tar.gz /root/rpmbuild/SOURCES/'
/bin/bash -c 'CFLAGS="-g -O2 -Wall -Werror" rpmbuild -bb pbspro.spec'
/bin/bash -c 'yum -y install /root/rpmbuild/RPMS/x86_64/pbspro-server-??.*.x86_64.rpm'
/bin/bash -c 'sed -i "s@PBS_START_MOM=0@PBS_START_MOM=1@" /etc/pbs.conf'
/etc/init.d/pbs start
/opt/pbs/bin/qstat -Bf
/opt/pbs/bin/pbsnodes -av
yum -y install python-pip sudo which net-tools man-db time.x86_64
