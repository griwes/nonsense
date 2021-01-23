ARG RELEASE=rawhide
FROM fedora:${RELEASE}

RUN dnf upgrade -y

RUN dnf install -y @development-tools procps cmake systemd-devel systemd iproute iw nftables bind \
        iputils \
        ccache clang lld libcxx-devel

# le sigh: make the system not wait until a ttyS0 timeout without CONFIG_FHANDLE
RUN systemctl mask serial-getty@ttyS0.service
# le sigh: this fails to mount when running through binfmt
RUN systemctl mask proc-sys-fs-binfmt_misc.automount
RUN systemctl mask systemd-resolved.service
RUN ln -sf /usr/lib/systemd/systemd /usr/bin/systemd

COPY . /nonsense
WORKDIR /nonsense

ENV CCACHE_DIR=/nonsense/cache
ENV CCACHE_COMPILERCHECK=content
RUN rm -rf build \
    && mkdir build \
    && cd build \
    && CXX="ccache clang++ -stdlib=libc++" LD="ccache clang++ -stdlib=libc++" LDFLAGS="-fuse-ld=$(which ld.lld)" cmake .. -DENABLE_TESTS=ON \
    && make install -j$(nproc)

