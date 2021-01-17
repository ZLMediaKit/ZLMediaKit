#!/bin/bash
cd ..
git clone --depth=1 https://github.com/xia-chu/ZLMediaKit.git
cd ZLMediaKit
git submodule init
git submodule update

brew install cmake
brew install openssl
brew install sdl
brew install ffmpeg

mkdir -p mac_build
rm -rf ./build
ln -s ./mac_build build
cd mac_build
cmake .. -DOPENSSL_ROOT_DIR=/usr/local/Cellar/openssl/1.0.2j/
make -j4
