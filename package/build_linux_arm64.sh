#!/usr/bin/env bash
# Build ZLMediaKit for linux/arm64 inside a container or on a native arm64 host.
set -euxo pipefail

export DEBIAN_FRONTEND=noninteractive

if command -v apt-get >/dev/null 2>&1; then
  apt-get update
  apt-get install -y --no-install-recommends \
    git wget ca-certificates gcc g++ make perl python3 \
    tar gzip xz-utils pkg-config zlib1g-dev
elif command -v yum >/dev/null 2>&1; then
  yum install -y git wget gcc gcc-c++ make perl python3 tar gzip which zlib-devel
else
  echo "Unsupported package manager" >&2
  exit 1
fi

ARCH="$(uname -m)"
case "${ARCH}" in
  aarch64|arm64) CMAKE_ARCH="aarch64" ;;
  x86_64|amd64)  CMAKE_ARCH="x86_64" ;;
  *) echo "Unsupported arch: ${ARCH}" >&2; exit 1 ;;
esac

CMAKE_VER="3.29.5"
CMAKE_DIR="/opt/cmake-${CMAKE_VER}-linux-${CMAKE_ARCH}"
if [ ! -x "${CMAKE_DIR}/bin/cmake" ]; then
  wget -q "https://github.com/Kitware/CMake/releases/download/v${CMAKE_VER}/cmake-${CMAKE_VER}-linux-${CMAKE_ARCH}.tar.gz" \
    -O "/tmp/cmake.tar.gz"
  tar -xzf /tmp/cmake.tar.gz -C /opt
fi
export PATH="${CMAKE_DIR}/bin:${PATH}"
cmake --version
gcc --version
uname -a

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
cd "${ROOT_DIR}"

INSTALL_DIR="${ROOT_DIR}/thirdparty_install"
mkdir -p "${INSTALL_DIR}"

# OpenSSL (static, no-asm: its armv8 assembly is non-PIC and breaks .so linking;
# no-dso: avoids requiring -ldl for the dlfcn engine)
cd "${ROOT_DIR}/3rdpart/openssl"
make distclean >/dev/null 2>&1 || true
./config no-shared no-asm no-dso -fPIC --prefix="${INSTALL_DIR}"
make -j"$(nproc)"
make install_sw
ls -la "${INSTALL_DIR}/lib" "${INSTALL_DIR}/include/openssl" || ls -la "${INSTALL_DIR}/lib64" || true

export PKG_CONFIG_PATH="${INSTALL_DIR}/lib/pkgconfig:${INSTALL_DIR}/lib64/pkgconfig:${PKG_CONFIG_PATH:-}"
export CPPFLAGS="-I${INSTALL_DIR}/include ${CPPFLAGS:-}"
export LDFLAGS="-L${INSTALL_DIR}/lib -L${INSTALL_DIR}/lib64 ${LDFLAGS:-}"
export LIBS="-ldl -lpthread ${LIBS:-}"

# usrsctp
cd "${ROOT_DIR}/3rdpart/usrsctp"
rm -rf build
mkdir -p build && cd build
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_POSITION_INDEPENDENT_CODE=ON ..
make -j"$(nproc)"
make install

# libsrtp
# GCC 10+ defaults to -fno-common and breaks libsrtp 2.3.0 tests
# (multiple definition of `bit_string`). Same fix as project dockerfile:
# always pass CFLAGS=-fcommon for configure/make/install.
cd "${ROOT_DIR}/3rdpart/libsrtp"
make distclean >/dev/null 2>&1 || true
export CFLAGS="-fcommon ${CPPFLAGS:-}"
export LDFLAGS="${LDFLAGS:-}"
export LIBS="${LIBS:-}"
./configure --enable-openssl --with-openssl-dir="${INSTALL_DIR}"
make -j"$(nproc)"
make install
# Ensure headers/libs are visible to CMake
ls -la /usr/local/lib/libsrtp* /usr/local/include/srtp* 2>/dev/null || true
ls -la ./*.a ./include 2>/dev/null || true

# ZLMediaKit
cd "${ROOT_DIR}"
rm -rf linux_build
mkdir -p linux_build
cd linux_build
cmake .. \
  -DOPENSSL_ROOT_DIR="${INSTALL_DIR}" \
  -DCMAKE_BUILD_TYPE=Release
make -j"$(nproc)"

echo "Build finished. Artifacts under: ${ROOT_DIR}/release"
ls -la "${ROOT_DIR}/release" || true
find "${ROOT_DIR}/release" -type f | head -50
