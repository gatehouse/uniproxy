#
# Build debian/ubuntu package using checkinstall.
#
#!/bin/bash

if [ "$1" = "" ]
then
   echo "Missing source dir argument!"
   exit 1
fi

version=`sed -n -e 's/const.*version.*"\(.*\)";/\1/p' $1/release.cpp`

ln -s $1/deb-lite/postinstall-pak
ln -s $1/deb-lite/preinstall-pak
ln -s $1/deb-lite/preremove-pak
ln -s $1/deb-lite/postremove-pak
ln -s $1/deb-lite/description-pak
touch install_manifest.txt

make

#run this with sudo. It will remove some of the annoying side effects afterwards.
sudo checkinstall --install=no --pkgversion=${version} --pkgrelease=${BUILD_NUMBER} --pkglicense=GPL --pkgname=uniproxy-lite --maintainer="Torsten Martinsen \<tma@gatehouse.com\>" --provides=uniproxy --requires="openssl" --nodoc --exclude=/home  --backup=no

rm postinstall-pak  postremove-pak  preinstall-pak  preremove-pak description-pak
