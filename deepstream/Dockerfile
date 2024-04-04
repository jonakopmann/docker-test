FROM nvcr.io/nvidia/deepstream:6.4-gc-triton-devel
ARG DEBIAN_FRONTEND=noninteractive

COPY ./src /deepstream-test

RUN cd /deepstream-test && meson build && cd build && ninja install
RUN rm -rf /deepstream-test