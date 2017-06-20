#!/bin/bash
path=`pwd`
wget https://raw.githubusercontent.com/xiongziliang/ZLToolKit/master/build_for_ios.sh -O toolkit_build.sh
sudo chmod +x ./toolkit_build.sh
./toolkit_build.sh
cd $path
cd ..
git clone --depth=50 https://github.com/xiongziliang/ZLMediaKit.git
cd ZLMediaKit
mkdir -p ios_build
cd ios_build
cmake .. -DCMAKE_TOOLCHAIN_FILE=../cmake/iOS.cmake -DIOS_PLATFORM=OS
make -j4
sudo make install
