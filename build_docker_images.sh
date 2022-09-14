#!/bin/bash
set -e
while getopts c:t:p:m:v:  opt
do
	case $opt in
		t)
			type=$OPTARG
			;;
		v)
			version=$OPTARG
			;;
		p)
			platform=$OPTARG
			;;
		m)
			model=$OPTARG
			;;
		?)
			echo "unkonwn"
			exit
			;;
       esac
done

help_string=".sh [-t build|push] [-p (amd64|arm64|...,default is `arch`) ] [-m Debug|Release] [-v [version]]"

if [[ ! -n $type ]];then
        echo $help_string
        exit
fi

if [[ ! -n $model ]];then
        echo $help_string
        exit
fi

if [[ ! -n $version ]];then
        echo "use latest no version set"
        version="latest"
fi

if [[ ! -n $platform ]];then
	platform=`arch`
	echo "auto select arch:${platform}" 
fi

case $platform in
"arm64")
	#eg:osx
	platform="linux/arm64"
	;;
"x86_64"|"amd64")
	platform="linux/amd64"
	;;
*)
	echo "unknown cpu-arch ${platform}"
	echo "Use 'docker buildx ls' to get supported ARCH"
	exit
	;;
esac

case $model in
	'Debug')
		;;
	'Release')
		;;
	*)
        echo "unkonwn model"
	echo $help_string
        exit
        ;;
esac

namespace="zlmediakit"
packagename="zlmediakit"

case $type in
	'build')
	rm -rf ./build/CMakeCache.txt
	# 以腾讯云账号为例
	docker buildx build --platform=$platform --network=host --build-arg MODEL=$model -t $namespace/$packagename:$model.$version .
	#docker build --network=host --build-arg MODEL=$model -t $namespace/$packagename:$model.$version .
		;;
	'push')
		echo "push to dst registry"
		# 以腾讯云账号为例
		docker login --username=zlmediakit
		docker push $namespace/$packagename:$model.$version
		;;
 	*)
		echo "unkonwn type"
		echo $help_string
		exit
		;;
esac
