#!/bin/bash -xe
yum clean all
yum -y update
yum -y install yum-utils epel-release rpmdevtools openssh-clients openssh-server
rpmdev-setuptree
yum-builddep -y ./pbspro.spec
./autogen.sh
./configure
make dist
/bin/bash -c 'cp -fv pbspro-*.tar.gz /root/rpmbuild/SOURCES/'
/bin/bash -c 'CFLAGS="-g -O2 -Wall -Werror" rpmbuild -bb pbspro.spec'
/bin/bash -c 'yum -y install /root/rpmbuild/RPMS/x86_64/pbspro-server-??.*.x86_64.rpm'
/etc/init.d/pbs start
yum -y install python36-pip sudo which net-tools man-db time.x86_64
rm -f /etc/ssh/ssh_host_ecdsa_key /etc/ssh/ssh_host_rsa_key
ssh-keygen -q -N "" -t dsa -f /etc/ssh/ssh_host_ecdsa_key
ssh-keygen -q -N "" -t rsa -f /etc/ssh/ssh_host_rsa_key
sed -i "s/UsePAM.*/UsePAM yes/g" /etc/ssh/sshd_config
sed -i "s/#UsePrivilegeSeparation.*/UsePrivilegeSeparation no/g" /etc/ssh/sshd_config
nohup /usr/sbin/sshd -E /tmp/sshd.log >/dev/null 2>&1
