#!/bin/bash

# Std ubuntu libraris
sudo apt update
sudo apt install -y checkinstall cmake git g++ python3 python-is-python3 libssl-dev libgcrypt20-dev libicu-dev libpcre++-dev libboost-dev libsystemd-dev libboost-filesystem-dev libboost-system-dev libboost-chrono-dev libboost-regex-dev libboost-date-time-dev libboost-iostreams-dev

# CppCMS
VERSION_CPPCMS=2.0.0
VERSION_CPPCMS_GIT=v2.0.0.beta2

pushd .
git clone https://github.com/artyom-beilis/cppcms.git cppcms
cd cppcms
git checkout $VERSION_CPPCMS_GIT
mkdir build
cd build
cmake .. -DCMAKE_INSTALL_PREFIX=/opt/cppcms/$VERSION_CPPCMS
make -j 4
#sudo make install
popd

# Build
mkdir build
cd build
cmake ..
make -j 4

#../deb/build-package.sh ..

