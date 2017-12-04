#!/bin/bash

mkdir -p ios_build
rm -rf ./build
ln -s ./ios_build build
cd ios_build
cmake .. -DCMAKE_TOOLCHAIN_FILE=../cmake/iOS.cmake -DIOS_PLATFORM=OS
make -j4
sudo make install
