ARG RELEASE=rolling
FROM ubuntu:${RELEASE}

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update \
    && apt-get -yq full-upgrade

RUN apt-get install -yq build-essential pkg-config wget libsystemd-dev systemd iproute2 iw nftables bind9 \
    clang libc++1 libc++abi1 libc++-dev \
    && rm -rf /etc/systemd/system/*

# le sigh
RUN ln -sf /usr/lib/x86_64-linux-gnu/libc++abi.so.1 /usr/lib/x86_64-linux-gnu/libc++abi.so

RUN wget -q https://github.com/Kitware/CMake/releases/download/v3.12.4/cmake-3.12.4-Linux-x86_64.sh \
    && chmod +x cmake-3.12.4-Linux-x86_64.sh \
    && ./cmake-3.12.4-Linux-x86_64.sh --skip-license

COPY . /nonsense
WORKDIR /nonsense

RUN rm -rf build \
    && mkdir build \
    && cd build \
    && CXX="clang++ -stdlib=libc++" LD="clang++ -stdlib=libc++" cmake .. \
    && make install -j$(nproc)

