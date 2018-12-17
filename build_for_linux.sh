#!/bin/bash
cd ..
git clone --depth=1 https://github.com/xiongziliang/ZLMediaKit.git
cd ZLMediaKit
git submodule init
git submodule update

sudo apt-get install cmake
sudo apt-get install libmysqlclient-dev
sudo apt-get install libssl-dev
sudo apt-get install libx264-dev
sudo apt-get install libfaac-dev
sudo apt-get install libmp4v2-dev
#sudo apt-get install libsdl-dev
#sudo apt-get install libavcodec-dev
#sudo apt-get install libavutil-dev

mkdir -p linux_build
rm -rf ./build
ln -s ./linux_build build
cd linux_build

cmake ..
make -j4
