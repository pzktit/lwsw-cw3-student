#!/bin/bash

# where to install
PREFIX=${PWD}

# required packages
sudo apt install crossbuild-essential-arm64 git autoconf autoconf-archive  automake libtool pkg-config

git clone https://git.kernel.org/pub/scm/libs/libgpiod/libgpiod.git

cd libgpiod

# select the same version as on target
git checkout v1.6.3

export CROSS_COMPILE=aarch64-linux-gnu-
export CC=${CROSS_COMPILE}gcc
export CXX=${CROSS_COMPILE}g++
export AR=${CROSS_COMPILE}ar
export LD=${CROSS_COMPILE}ld
export STRIP=${CROSS_COMPILE}strip
export PKG_CONFIG_PATH=/usr/aarch64-linux-gnu/lib/pkgconfig

./autogen.sh --host=aarch64-linux-gnu --prefix=${PREFIX} --enable-bindings-cxx

make

make install

cd ..
rm -rf libgpiod/

#  The script will clone the libgpiod repository, checkout the version 1.6.3, and compile it for the aarch64 architecture. 
#  The script will install the library in the current directory. 
#  The script can be executed with the following command: 
#  $ ./cross-compile-libgpiod.sh
