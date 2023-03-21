#
# Build debian/ubuntu package using checkinstall.
#
#!/bin/bash

if [ $# -lt 1 ] 
then
   echo "Please specify the source dir."
   exit 1
fi

if [ "$(basename $PWD)" = "deb" ]
then
   echo "Run this in the build dir!"
   exit 1
fi

version=`sed -n -e 's/const.*version.*"\(.*\)";/\1/p' $1/release.cpp`

ln -s $1/deb/postinstall-pak
ln -s $1/deb/preinstall-pak
ln -s $1/deb/preremove-pak
ln -s $1/deb/postremove-pak
ln -s $1/deb/description-pak
touch install_manifest.txt

make

#run this with sudo. It will remove some of the annoying side effects afterwards. ${BUILD_NUMBER}
sudo checkinstall --install=no --pkgversion=${version} --pkgrelease=1 --pkglicense=GPL --pkgname=uniproxy --maintainer="GateHouse \<support@gatehouse.com\>" --provides=uniproxy --requires="openssl" --nodoc --exclude=/home  --backup=no -y

rm postinstall-pak  postremove-pak  preinstall-pak  preremove-pak description-pak

