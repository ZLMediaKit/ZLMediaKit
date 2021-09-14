FROM ubuntu:18.04 AS build
ARG MODEL
#shell,rtmp,rtsp,rtsps,http,https,rtp
EXPOSE 9000/tcp
EXPOSE 1935/tcp
EXPOSE 554/tcp
EXPOSE 322/tcp
EXPOSE 80/tcp
EXPOSE 443/tcp
EXPOSE 10000/udp
EXPOSE 10000/tcp
EXPOSE 8000/udp

RUN apt-get update && \
         DEBIAN_FRONTEND="noninteractive" \
         apt-get install -y --no-install-recommends \
         build-essential \
         cmake \
         git \
         curl \
         vim \
         wget \
         ca-certificates \
         tzdata \
         libssl-dev \
         libmysqlclient-dev \
         libx264-dev \
         libfaac-dev \
         gcc \
         g++ \
         gdb \
         libmp4v2-dev && \
         apt-get autoremove -y && \
         apt-get clean -y && \
         wget https://github.com/cisco/libsrtp/archive/v2.2.0.tar.gz -O libsrtp-2.2.0.tar.gz && tar xfv libsrtp-2.2.0.tar.gz && \
         cd libsrtp-2.2.0 && ./configure --enable-openssl && make && make install && \
    rm -rf /var/lib/apt/lists/*

RUN mkdir -p /opt/media
COPY . /opt/media/ZLMediaKit
WORKDIR /opt/media/ZLMediaKit
#RUN git submodule update --init --recursive && \
RUN mkdir -p build release/linux/${MODEL}/

WORKDIR /opt/media/ZLMediaKit/build
RUN cmake -DCMAKE_BUILD_TYPE=${MODEL} -DENABLE_WEBRTC=true -DENABLE_TESTS=false -DENABLE_API=false .. && \
    make -j8

FROM ubuntu:18.04
ARG MODEL

RUN apt-get update && \
         DEBIAN_FRONTEND="noninteractive" \
         apt-get install -y --no-install-recommends \
         vim \
         wget \
         ca-certificates \
         tzdata \
         curl \
         libssl-dev \
         libx264-dev \
         libfaac-dev \
         ffmpeg \
         gcc \
         g++ \
         gdb \
         libmp4v2-dev && \
         apt-get autoremove -y && \
         apt-get clean -y && \
    rm -rf /var/lib/apt/lists/*

ENV TZ=Asia/Shanghai
RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime \
        && echo $TZ > /etc/timezone && \
        mkdir -p /opt/media/bin/www

WORKDIR /opt/media/bin/
COPY --from=build /opt/media/ZLMediaKit/release/linux/${MODEL}/MediaServer /opt/media/ZLMediaKit/tests/default.pem /opt/media/bin/
COPY --from=build /opt/media/ZLMediaKit/release/linux/${MODEL}/config.ini /opt/media/conf/
COPY --from=build /opt/media/ZLMediaKit/www/ /opt/media/bin/www/
ENV PATH /opt/media/bin:$PATH
CMD ["sh","-c","./MediaServer -s default.pem -c ../conf/config.ini"]
