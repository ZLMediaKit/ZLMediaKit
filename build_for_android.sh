#!/bin/bash
cd ..
git clone --depth=1 https://github.com/xiongziliang/ZLMediaKit.git
cd ZLMediaKit
git submodule init
git submodule update

mkdir -p android_build
rm -rf ./build
ln -s ./android_build build
cd android_build
cmake .. -DCMAKE_TOOLCHAIN_FILE=../cmake/android.toolchain.cmake -DANDROID_NDK=$ANDROID_NDK_ROOT  -DCMAKE_BUILD_TYPE=Release  -DANDROID_ABI="armeabi" -DANDROID_NATIVE_API_LEVEL=android-9
make -j4
