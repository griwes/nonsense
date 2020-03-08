ARG RELEASE=rawehide
FROM fedora:${RELEASE}

RUN dnf upgrade -y

RUN dnf install -y @development-tools procps cmake systemd-devel systemd iproute iw nftables bind \
        iputils \
        ccache clang libcxx-devel

# le sigh: make the system not wait until a ttyS0 timeout without CONFIG_FHANDLE
RUN systemctl mask serial-getty@ttyS0.service
RUN ln -sf /usr/lib/systemd/systemd /usr/bin/systemd

COPY . /nonsense
WORKDIR /nonsense

ENV CCACHE_DIR=/nonsense/cache
ENV CCACHE_COMPILERCHECK=content
RUN rm -rf build \
    && mkdir build \
    && cd build \
    && CXX="ccache clang++ -stdlib=libc++" LD="ccache clang++ -stdlib=libc++" cmake .. \
    && make install -j$(nproc)

