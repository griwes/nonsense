ARG RELEASE=rolling
FROM ubuntu:${RELEASE}

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update \
    && apt-get -yq full-upgrade

RUN apt-get update \
    && apt-get install -yq build-essential pkg-config cmake libsystemd-dev systemd iproute2 iw nftables bind9 \
        iputils-ping \
        ccache clang libc++1 libc++abi1 libc++-dev \
    && rm -rf /etc/systemd/system/*

# le sigh
RUN ln -sf /usr/lib/x86_64-linux-gnu/libc++abi.so.1 /usr/lib/x86_64-linux-gnu/libc++abi.so
# le sigh #2: make the system not wait until a ttyS0 timeout without CONFIG_FHANDLE
RUN systemctl mask serial-getty@ttyS0.service

COPY . /nonsense
WORKDIR /nonsense

ENV CCACHE_DIR=/nonsense/cache
ENV CCACHE_COMPILERCHECK=content
RUN rm -rf build \
    && mkdir build \
    && cd build \
    && CXX="ccache clang++ -stdlib=libc++" LD="ccache clang++ -stdlib=libc++" cmake .. -DENABLE_TESTS=ON \
    && make install -j$(nproc)

