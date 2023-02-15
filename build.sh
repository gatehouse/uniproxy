#!/bin/bash

# Std ubuntu libraris
sudo apt update
sudo apt install -y checkinstall cmake git g++ python libssl-dev libgcrypt20-dev libicu-dev libpcre++-dev libboost-dev

# CppCMS
VERSION_CPPCMS=1.2.1
VERSION_CPPCMS_GIT=v1.2.1

pushd .
git clone https://github.com/artyom-beilis/cppcms.git cppcms
cd cppcms
git checkout $VERSION_CPPCMS_GIT
mkdir build
cd build
cmake .. -DCMAKE_INSTALL_PREFIX=/opt/cppcms/$VERSION_CPPCMS
make -j 4
sudo make install
popd

# Build
mkdir build
cd build
cmake ..
make
