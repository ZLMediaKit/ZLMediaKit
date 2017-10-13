#!/bin/bash
path=`pwd`
wget https://raw.githubusercontent.com/xiongziliang/ZLToolKit/master/build_for_android.sh -O toolkit_build.sh
sudo chmod +x ./toolkit_build.sh
./toolkit_build.sh
cd $path
cd ..
git clone --depth=50 https://github.com/xiongziliang/ZLMediaKit.git
cd ZLMediaKit
mkdir -p android_build
rm -rf ./build
ln -s ./android_build build
cd android_build
cmake .. -DCMAKE_TOOLCHAIN_FILE=../cmake/android.toolchain.cmake -DANDROID_NDK=$ANDROID_NDK_ROOT  -DCMAKE_BUILD_TYPE=Release  -DANDROID_ABI="armeabi" -DANDROID_NATIVE_API_LEVEL=android-9
make -j4
sudo make install
