ARG RELEASE=rolling
FROM ubuntu:${RELEASE}

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update \
    && apt-get -yq full-upgrade

RUN apt-get update \
    && apt-get install -yq build-essential pkg-config cmake libsystemd-dev systemd iproute2 iw nftables bind9 \
        iputils-ping \
        ccache g++-10 \
    && rm -rf /etc/systemd/system/*

# le sigh: make the system not wait until a ttyS0 timeout without CONFIG_FHANDLE
RUN systemctl mask serial-getty@ttyS0.service

COPY . /nonsense
WORKDIR /nonsense

ENV CCACHE_DIR=/nonsense/cache
ENV CCACHE_COMPILERCHECK=content
RUN rm -rf build \
    && mkdir build \
    && cd build \
    && CXX="ccache g++-10" LD="ccache g++-10" cmake .. -DENABLE_TESTS=ON \
    && make install -j$(nproc)

