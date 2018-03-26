#!/bin/bash
cd ..
git clone --depth=1 https://github.com/xiongziliang/ZLMediaKit.git
cd ZLMediaKit
git submodule init
git submodule update

mkdir -p ios_build
rm -rf ./build
ln -s ./ios_build build
cd ios_build
cmake .. -DCMAKE_TOOLCHAIN_FILE=../cmake/iOS.cmake -DIOS_PLATFORM=OS
make -j4
