#!/bin/bash
set -e
docker build -t gemfield/zlmediakit:20.04-runtime-ubuntu18.04 -f ubuntu18.04/Dockerfile.runtime .
docker build -t gemfield/zlmediakit:20.04-devel-ubuntu18.04 -f ubuntu18.04/Dockerfile.devel .
docker build -t gemfield/zlmediakit:20.04-runtime-ubuntu16.04 -f ubuntu16.04/Dockerfile.runtime .
docker build -t gemfield/zlmediakit:20.04-devel-ubuntu16.04 -f ubuntu16.04/Dockerfile.devel .
docker build -t gemfield/zlmediakit:centos7-runtime -f centos7/Dockerfile.runtime .
