docker exec -it testdev yum clean all
docker exec -it testdev yum -y update
docker exec -it testdev yum -y install yum-utils epel-release rpmdevtools
docker exec -it testdev rpmdev-setuptree
docker exec -it testdev yum-builddep -y ./pbspro.spec
docker exec -it testdev ./autogen.sh
docker exec -it testdev ./configure
docker exec -it testdev make dist
docker exec -it testdev /bin/bash -c 'cp -fv pbspro-*.tar.gz /root/rpmbuild/SOURCES/'
docker exec -it testdev /bin/bash -c 'CFLAGS="-g -O2 -Wall -Werror -fsanitize=address -fno-omit-frame-pointer" rpmbuild -bb pbspro.spec'
docker exec -it testdev /bin/bash -c 'yum -y install /root/rpmbuild/RPMS/x86_64/pbspro-server-??.*.x86_64.rpm'
docker exec -it testdev /bin/bash -c 'sed -i "s@PBS_START_MOM=0@PBS_START_MOM=1@" /etc/pbs.conf'
docker exec -it testdev /etc/init.d/pbs start
docker exec -it testdev yum -y install python-pip sudo which net-tools man-db time.x86_64


