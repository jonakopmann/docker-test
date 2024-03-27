FROM ubuntu:22.04
ARG DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get -y upgrade && apt-get install -y \
        meson \
        libfmt-dev \
        libssl3 \
        libssl-dev \
        libgstreamer1.0-0 \
        gstreamer1.0-tools \
        gstreamer1.0-plugins-good \
        gstreamer1.0-plugins-bad \
        gstreamer1.0-plugins-ugly \
        gstreamer1.0-libav \
        libgstreamer-plugins-base1.0-dev \
        libgstrtspserver-1.0-0 \
        libjansson4 \
        libyaml-cpp-dev \
        libjsoncpp-dev \
        protobuf-compiler \
        gcc \
        make \
        git \
        python3 \
        wget \
        apt-utils \
        software-properties-common

RUN apt-key adv --fetch-keys https://developer.download.nvidia.com/compute/cuda/repos/ubuntu2204/x86_64/3bf863cc.pub && \
    add-apt-repository "deb https://developer.download.nvidia.com/compute/cuda/repos/ubuntu2204/x86_64/ /" && \
    apt-get update && apt-get install -y cuda-toolkit-12-2


RUN wget --content-disposition 'https://api.ngc.nvidia.com/v2/resources/org/nvidia/deepstream/6.4/files?redirect=true&path=deepstream-6.4_6.4.0-1_amd64.deb' -O deepstream-6.4_6.4.0-1_amd64.deb

RUN apt-get install -y ./deepstream-6.4_6.4.0-1_amd64.deb

RUN rm -rf /var/lib/apt/lists/*

COPY ./src /deepstream-test

RUN cd /deepstream-test && meson build && cd build && ninja install
RUN rm -rf /deepstream-test