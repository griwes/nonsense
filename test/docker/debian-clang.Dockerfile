ARG ARCH=
ARG RELEASE=unstable
FROM ${ARCH}debian:${RELEASE}

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update \
    && apt-get -yq full-upgrade

ARG HOST=amd64
RUN dpkg --add-architecture ${HOST}

RUN apt-get update \
    && apt-get install -yq build-essential pkg-config cmake libsystemd-dev systemd iproute2 iw nftables:${HOST} bind9 \
        ccache clang lld libc++1 libc++abi1 libc++-dev \
    && rm -rf /etc/systemd/system/*

# le sigh
RUN ln -sf /usr/lib/$(gcc -dumpmachine)/libc++abi.so.1 /usr/lib/$(gcc -dumpmachine)/libc++abi.so
# le sigh #2: make the system not wait until a ttyS0 timeout without CONFIG_FHANDLE
RUN systemctl mask serial-getty@ttyS0.service
# le sigh: this fails to mount when running through binfmt
RUN systemctl mask proc-sys-fs-binfmt_misc.automount

COPY . /nonsense
WORKDIR /nonsense

ENV CCACHE_DIR=/nonsense/cache
ENV CCACHE_COMPILERCHECK=content
RUN rm -rf build \
    && mkdir build \
    && cd build \
    && CXX="ccache clang++ -stdlib=libc++" LD="ccache clang++ -stdlib=libc++" LDFLAGS="-fuse-ld=$(which ld.lld)" cmake .. -DENABLE_TESTS=ON \
    && make install -j$(nproc)

