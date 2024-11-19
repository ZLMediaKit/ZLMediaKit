#!/bin/bash
function usage_help(){
    echo "usage:"
    echo "./build.sh debug  [options]"
    echo "./build.sh release [options]"
    echo "./build.sh docker [options]"
    echo "output: release/linux/*"
}

#--show-current requires Git 2.22.0+
#git_branch=`git branch --show-current`  
git_branch=`git rev-parse --abbrev-ref HEAD`
git_branch=`echo ${git_branch} |sed 's#\/#\\\/#g'`
git_commit=`git rev-parse --short HEAD`
echo ${git_branch} ${git_commit}

machine_arch=$(uname -m)
if [[ "$machine_arch" == "x86_64" ]]; then
    echo "x86 architecture detected"
    docker_arch="x86_64"
elif [[ "$machine_arch" == "aarch64" ]]; then
    echo "ARM64 architecture detected"
    docker_arch="arm64"
elif [[ "$machine_arch" == "armv7l" ]]; then
    echo "ARMv7 architecture detected"
else
    echo "Unknown architecture"
fi

#export LD_LIBRARY_PATH=/home/mahao/code_dir/media-server/release/linux/debug:$LD_LIBRARY_PATH

function build_DEBUG(){
    if [ -d "build" ]; then
        rm -rf build
    fi
    mkdir build

    cd build
    cmake -S .. -B . -DENABLE_WEBRTC=true -DENABLE_FFMPEG=true -DENABLE_TESTS=true -DENABLE_API=false
    make -j $(nproc)
    cd ../
    # if [[ "$machine_arch" == "x86_64" ]]; then
    #     echo "x86 architecture detected"
    #     cp -r ./third_party/nacos_server/lib/Linux/x86_64/libnacos-cli.so ./release/linux/debug/
    #     cp -r ./third_party/cppkafka/lib/x86_64/* ./release/linux/debug/
    #     cp -r ./third_party/prometheus-cpp/lib/x86_64/* ./release/linux/debug/
    # elif [[ "$machine_arch" == "aarch64" ]]; then
    #     echo "ARM64 architecture detected"
    #     cp -r ./third_party/nacos_server/lib/Linux/aarch64/libnacos-cli.so ./release/linux/debug/
    #     cp -r ./third_party/cppkafka/lib/aarch64/* ./release/linux/debug/
    #     cp -r ./third_party/prometheus-cpp/lib/aarch64/* ./release/linux/debug/
    # else
    #     echo "Unknown architecture"
    # fi


}

function build_RELEASE(){
    if [ -d "build" ]; then
        rm -rf build
    fi
    mkdir build

    cd build

    echo "$(nproc)"
    cmake  -S .. -B .  -DCMAKE_BUILD_TYPE=release -DENABLE_WEBRTC=true -DENABLE_FFMPEG=true -DENABLE_TESTS=false -DENABLE_API=false
    make -j $(nproc)
    cd ../
    # if [[ "$machine_arch" == "x86_64" ]]; then
    #     cp -r ./third_party/nacos_server/lib/Linux/x86_64/libnacos-cli.so ./release/linux/release/
    #     cp -r ./third_party/cppkafka/lib/x86_64/* ./release/linux/release/
    #     cp -r ./third_party/prometheus-cpp/lib/x86_64/* ./release/linux/release/
    # elif [[ "$machine_arch" == "aarch64" ]]; then
    #     cp -r ./third_party/nacos_server/lib/Linux/aarch64/libnacos-cli.so ./release/linux/release/
    #     cp -r ./third_party/cppkafka/lib/aarch64/* ./release/linux/release/
    #     cp -r ./third_party/prometheus-cpp/lib/aarch64/* ./release/linux/release/    
    # else
    #     echo "Unknown architecture"
    # fi
}

function build_docker() {
    bash build_docker_images.sh -t build -p ${docker_arch} -m debug -v ${git_commit}
}

if [ ! -n "$1" ]; then
    usage_help
    exit
elif [ "$1" == "debug" ]; then
    build_DEBUG
elif [ "$1" == "release" ]; then
    build_RELEASE
elif [ "$1" == "docker" ]; then
    build_docker
else
    usage_help
    exit 
fi
