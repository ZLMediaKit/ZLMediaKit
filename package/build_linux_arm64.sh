#!/usr/bin/env bash
# Build ZLMediaKit for linux/arm64 inside a container or on a native arm64 host.
set -euxo pipefail

export DEBIAN_FRONTEND=noninteractive

if command -v apt-get >/dev/null 2>&1; then
  # Debian 11(bullseye)已结束支持，deb.debian.org 不再保留其软件包，apt-get 会因 404 整体失败。
  # 归档站仅保留主仓库：bullseye-updates 为空、bullseye-security 尚未归档，故只配置 main 一条。
  # 仅在 debian:11 容器内改写，避免影响其它发行版或原生 arm64 主机。
  # Debian 11 (bullseye) is end-of-life and deb.debian.org no longer serves its packages, so
  # apt-get fails outright with 404. The archive keeps the main suite only: bullseye-updates is
  # empty and bullseye-security is not archived, hence the single entry below.
  # Rewrite it only inside a debian:11 container, never on other releases or a native arm64 host.
  OS_TAG=""
  if [ -r /etc/os-release ]; then
    OS_TAG="$(. /etc/os-release && echo "${ID:-}-${VERSION_ID:-}")"
  fi
  if [ "${OS_TAG}" = "debian-11" ]; then
    echo "deb http://archive.debian.org/debian bullseye main" > /etc/apt/sources.list
    rm -f /etc/apt/sources.list.d/*.list /etc/apt/sources.list.d/*.sources 2>/dev/null || true
    # 归档快照的 Release 早已超出有效期，需要放行该校验，否则 apt-get update 仍会拒绝
    # The archived Release is long past its Valid-Until, so that check must be relaxed
    # or apt-get update still refuses the repository
    echo 'Acquire::Check-Valid-Until "false";' > /etc/apt/apt.conf.d/99archive-no-check-valid-until
    apt-get update
    # 镜像内预装的包来自 bullseye-security，版本高于归档主仓库(如 libc6 u14 对 u11)。
    # 该仓库的索引虽在、软件包却已下线，无法再取到这些版本；而 libc6-dev、perl 均要求
    # 与 libc6、perl-base 精确同版本，故必须先把已装包降级对齐到归档主仓库再安装。
    # The image ships packages from bullseye-security whose versions outrank the archived main
    # suite (e.g. libc6 u14 vs u11). That suite still serves indexes but no longer serves the
    # packages themselves, so those versions are unobtainable; since libc6-dev and perl demand
    # an exact version match against libc6 and perl-base, the installed set must be downgraded
    # onto the archive before anything can be installed.
    apt-get -y --allow-downgrades dist-upgrade
  else
    apt-get update
  fi
  apt-get install -y --no-install-recommends --allow-downgrades \
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
