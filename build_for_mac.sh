#!/bin/bash
path=`pwd`
wget https://raw.githubusercontent.com/xiongziliang/ZLToolKit/develop/build_for_mac.sh -O toolkit_build.sh
sudo chmod +x ./toolkit_build.sh
./toolkit_build.sh
brew install x264
brew install faac
brew install mp4v2
brew install sdl
brew install ffmpeg
cd $path
cd ..
git clone --depth=50 https://github.com/xiongziliang/ZLMediaKit.git
cd ZLMediaKit
mkdir -p mac_build
rm -rf ./build
ln -s ./mac_build build
cd mac_build
cmake .. -DOPENSSL_ROOT_DIR=/usr/local/Cellar/openssl/1.0.2j/
make -j4
sudo make install
