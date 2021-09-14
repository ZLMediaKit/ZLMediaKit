#!/bin/bash
set -e
while getopts c:t:m:v:  opt
do
        case $opt in
                t)
                        type=$OPTARG
                        ;;
	        v)
			version=$OPTARG
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

if [[ ! -n $type ]];then
        echo ".sh [-t build|push] [-m Debug|Release] [-v [version]]"
        exit
fi

if [[ ! -n $model ]];then
        echo ".sh [-t build|push] [-m Debug|Release] [-v [version]]"
        exit
fi

if [[ ! -n $version ]];then
        echo "use latest no version set"
        version="latest"
fi

case $model in
	'Debug')
		;;
	'Release')
		;;
	*)
        	echo "unkonwn model"
		echo ".sh [-t build|push] [-m Debug|Release] [-v [version]]"
                exit
                ;;
esac

namespace="zlmediakit"
packagename="zlm-mediaserver"

case $type in
	'build')
	rm -rf ./build/CMakeCache.txt
	# 以腾讯云账号为例
	docker build --build-arg MODEL=$model -t ccr.ccs.tencentyun.com/$namespace/$packagename:$model.$version .
		;;
	'push')
		echo "push to dst registry"
		# 以腾讯云账号为例
		docker login --username=default_name ccr.ccs.tencentyun.com
		docker push ccr.ccs.tencentyun.com/$namespace/$packagename:$model.$version
		;;
 	*)
                echo "unkonwn type"
                echo ".sh [-t build|push] [-m Debug|Release] [-v [version]]"
                exit
                ;;
esac
