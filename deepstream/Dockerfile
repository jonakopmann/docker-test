FROM nvcr.io/nvidia/deepstream:7.0-triton-multiarch

RUN apt-get update && apt-get install -y \
        meson

RUN rm -rf /var/lib/apt/lists/*

COPY ./src /deepstream-test
COPY deepstream.pc /usr/local/lib/x86_64-linux-gnu/pkgconfig/

RUN cd /deepstream-test && meson build && cd build && ninja install
RUN rm -rf /deepstream-test