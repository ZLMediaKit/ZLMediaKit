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
    # 该仓库的索引虽在、软件包却已下线(其 pool 下的 deb 均返回 404)，这些版本已无处可取；
    # 而 libc6-dev、perl 都要求与 libc6、perl-base 精确同版本，故须先把已装包对齐到归档主仓库。
    # dist-upgrade 只做升级、不会主动降级(--allow-downgrades 仅是放行降级动作)，
    # 因此逐个比对已装版本与候选版本，显式降级存在差异者。
    # The image ships packages from bullseye-security whose versions outrank the archived main
    # suite (e.g. libc6 u14 vs u11). That suite still serves indexes but no longer serves the
    # packages themselves (its pool returns 404), so those versions are unobtainable; since
    # libc6-dev and perl demand an exact version match against libc6 and perl-base, the installed
    # set must first be aligned onto the archive. dist-upgrade only upgrades and never downgrades
    # on its own (--allow-downgrades merely permits the action), so compare the installed version
    # against the candidate for each package and downgrade the ones that differ.
    set +x
    # 包名取 ${binary:Package} 以带上架构后缀，并与版本一次查出：
    # 若按包名二次查询，multi-arch 包会返回多条记录且无分隔，版本串会被拼接成非法值
    # Take ${binary:Package} so the architecture suffix is kept, and read the version in the
    # same query: looking the version up by bare name afterwards returns one record per
    # architecture with no separator, concatenating them into an invalid version string
    installed_list="$(dpkg-query -W -f='${db:Status-Abbrev}|${binary:Package}|${Version}\n' \
      | awk -F'|' '$1 ~ /^ii/ {print $2"|"$3}')"
    realign=""
    while IFS='|' read -r pkg installed; do
      [ -n "${pkg}" ] || continue
      # 不能用 apt-cache policy 的 Candidate：它表示"apt 会选用的版本"，而 apt 默认不降级，
      # 对已装版本高于源的包它恒等于已装版本，比对将永远相等。
      # madison 只列出源中实际提供的版本(按版本降序)，取其首行才是源里的可用版本。
      # apt-cache 的输出随 locale 变化，固定为 C 以稳定解析。
      # The Candidate from apt-cache policy cannot be used: it denotes the version apt would
      # select, and since apt never downgrades on its own it equals the installed version
      # whenever that outranks the archive, making every comparison match.
      # madison lists only what the configured sources actually offer (newest first), so its
      # first row is the version available from the archive.
      # apt-cache output is localized, so pin the locale to C for stable parsing.
      available="$(LC_ALL=C apt-cache madison "${pkg}" 2>/dev/null \
        | awk -F'|' 'NR==1{gsub(/ /,"",$2); print $2}' || true)"
      if [ -n "${available}" ] && [ "${available}" != "${installed}" ]; then
        realign="${realign} ${pkg}=${available}"
      fi
    done <<INSTALLED_LIST
${installed_list}
INSTALLED_LIST
    set -x
    if [ -n "${realign}" ]; then
      apt-get install -y --allow-downgrades ${realign}
    fi
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

# 执行门禁用例(见 tests/CMakeLists.txt 的 GATING_TESTS)，失败即中断构建
# Run the gating cases (see GATING_TESTS in tests/CMakeLists.txt); a failure aborts the build
ctest --output-on-failure

echo "Build finished. Artifacts under: ${ROOT_DIR}/release"
ls -la "${ROOT_DIR}/release" || true
find "${ROOT_DIR}/release" -type f | head -50
