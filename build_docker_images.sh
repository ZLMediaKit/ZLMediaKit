#!/bin/bash
docker build -t gemfield/zlmediakit:20.01-runtime-ubuntu18.04 -f docker/ubuntu18.04/Dockerfile.runtime .
#docker build -t gemfield/zlmediakit:20.01-devel-ubuntu18.04 -f docker/ubuntu18.04/Dockerfile.devel .
