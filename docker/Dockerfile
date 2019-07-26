#################################################################
# 在项目根目录执行 docker build -t cl-zlmedia -f ./docker/Dockerfile .
#################################################################

#基础
FROM ajiva/ubuntu-ffmpeg4 AS base
WORKDIR   /tmp/workdir
RUN sed -i s@/archive.ubuntu.com/@/mirrors.163.com/@g /etc/apt/sources.list
RUN apt-get update && \
    apt-get -qqy install --no-install-recommends \
        g++ \
        make \
        cmake \
        git \
        libmysqlclient-dev \
        libssl-dev \
        libx264-dev \
        libfaac-dev \
        libmp4v2-dev \
        wget \
        curl \
        sqlite3 \
        libsqlite3-dev \
        nginx \
        yasm && \
    apt-get -qqy clean && \
    rm -rf /var/lib/apt/lists/*

RUN rm -f /etc/nginx/sites-enabled/default



#后端
FROM base as build_b
WORKDIR /usr/src/ZLMediaKit
ADD  3rdpart   ./3rdpart
ADD  Android   ./Android
ADD  cmake  ./cmake
ADD  server   ./server
ADD  src   ./src
ADD  tests   ./tests
ADD  .travis.yml _config.yml CMakeLists.txt docker/build_for_docker_linux.sh ./
RUN chmod +x build_for_docker_linux.sh
RUN mkdir -p build/bin/dbdata && \
    mkdir -p build/bin/log
RUN ["/bin/bash", "./build_for_docker_linux.sh"]




#前端
FROM base as build_f
ENV ZLMEDIAKIT_NVR_UI_VERSION=1.0.2
WORKDIR /usr/src/ZLMediaKitUI
RUN mkdir ui && \
    curl -sLO https://github.com/chenxiaolei/ZLMediaKit_NVR_UI/releases/download/1.0.2/zlmediakit_nvr_ui.1.0.2.tar.gz &&\
    tar -xvf zlmediakit_nvr_ui.1.0.2.tar.gz -C ui





#最终发布
FROM base AS release
MAINTAINER  chenxiaolei <4854336@qq.com>
COPY --from=build_b /usr/src/ZLMediaKit/build /usr/src/ZLMediaKit/build
COPY --from=build_f /usr/src/ZLMediaKitUI /usr/src/ZLMediaKitUI

WORKDIR /usr/src/ZLMediaKit
COPY docker/nginx_zlmedia.conf /etc/nginx/conf.d/zlmedia.conf
COPY docker/docker-entrypoint.sh  docker/zlmedia-ui-env.sh  /usr/local/bin/
COPY docker/config_docker.ini /usr/src/ZLMediaKit/build/bin/config.ini
RUN  chmod +x /usr/local/bin/docker-entrypoint.sh /usr/local/bin/zlmedia-ui-env.sh && \
     ln -s  /usr/src/ZLMediaKit/build/bin zldata

ENTRYPOINT ["/usr/local/bin/docker-entrypoint.sh"]

EXPOSE 1935 10800 554 80

