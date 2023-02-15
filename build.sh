#!/bin/bash

# Std ubuntu libraris
sudo apt update
sudo apt install -y checkinstall cmake git g++ python libssl-dev libgcrypt20-dev libicu-dev libpcre++-dev libboost-dev

# CppCMS
VERSION_CPPCMS=1.3.0gh
VERSION_CPPCMS_GIT=61da055ffeb349b4

pushd .
git clone https://github.com/gatehouse/cppcms.git
cd cppcms
git checkout $VERSION_CPPCMS_GIT
mkdir build
cd build
cmake .. 
make
make install
popd

# Build
mkdir build
cd build
cmake ..
make
