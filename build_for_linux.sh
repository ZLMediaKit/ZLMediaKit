#!/bin/bash
path=`pwd`
wget https://github.com/xiongziliang/ZLToolKit/blob/master/build_for_linux.sh -o ./toolkit_build.sh
sudo chmod +x ./toolkit_build.sh
./toolkit_build.sh
sudo apt-get install libx264-dev
sudo apt-get install libfaac-dev
sudo apt-get install libmp4v2-dev
sudo apt-get install libsdl-dev
sudo apt-get install libavcodec-dev
sudo apt-get install libavutil-dev
cd $path
cd ..
git clone --depth=50 https://github.com/xiongziliang/ZLMediaKit.git
cd ZLMediaKit
mkdir -p build
cd build
cmake ..
make -j4
sudo make install
