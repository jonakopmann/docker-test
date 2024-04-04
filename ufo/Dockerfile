FROM ubuntu:22.04
ARG DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get -y upgrade && apt-get install -y \
        apt-utils \
        build-essential \
        meson \
        ninja-build \
        git \
        gcc \
        g++ \
        gobject-introspection \
        make \
        ca-certificates \
        cmake \
        liblapack-dev \
        libjpeg-dev \
        libtiff5-dev \
        libglib2.0-dev \
        libjson-glib-dev \
        libopenmpi-dev \
        libhdf5-dev \
        libclfft-dev \
        libgsl-dev \
        libgirepository1.0-dev \
        pkg-config \
        python3 \
        python-is-python3 \
        python3-dev \
        ocl-icd-opencl-dev \
        fftw3-dev \
        clinfo \
        zlib1g-dev && \
        rm -rf /var/lib/apt/lists/*

ENV LD_LIBRARY_PATH /usr/local/lib/:${LD_LIBRARY_PATH}
ENV GI_TYPELIB_PATH=/usr/local/lib/girepository-1.0:$GI_TYPELIB_PATH
ENV PKG_CONFIG_PATH=/usr/local/lib/pkgconfig:$PKG_CONFIG_PATH

RUN mkdir -p /etc/OpenCL/vendors && \
    echo "libnvidia-opencl.so.1" > /etc/OpenCL/vendors/nvidia.icd
ENV NVIDIA_VISIBLE_DEVICES all
ENV NVIDIA_DRIVER_CAPABILITIES compute,utility

RUN ln -s /usr/lib/x86_64-linux-gnu/pkgconfig/python3.pc /usr/lib/x86_64-linux-gnu/pkgconfig/python.pc

#RUN git clone https://github.com/ufo-kit/ufo-core.git
RUN git clone https://github.com/ufo-kit/ufo-filters.git
COPY ./ufo-core ./ufo-core

RUN cd /ufo-core && meson build && cd build && ninja install
#RUN cd /ufo-core/python && python3 setup.py install
RUN cd /ufo-filters && meson build && cd build && ninja install
#RUN rm -rf /ufo-core /ufo-filters

COPY ./src /ufo-test

RUN cd /ufo-test && meson build && cd build && ninja install
RUN rm -rf /ufo-test