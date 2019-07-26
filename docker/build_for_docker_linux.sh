#!/bin/bash
git submodule init
git submodule update

mkdir -p linux_build
rm -rf ./build
ln -s ./linux_build build
cd linux_build

cmake ..
make -j4
