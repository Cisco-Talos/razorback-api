#!/usr/bin/env bash

set -euo pipefail

readonly OPENTELEMETRY_CPP_VERSION="1.26.0"
readonly OPENTELEMETRY_CPP_SHA256="8a878777a18a013e0ee6604629d1b5f29b162354c14489ad1dccd370f14ac372"

if [ "$#" -ne 1 ]; then
    echo "usage: $0 <install-prefix>" >&2
    exit 1
fi

install_prefix=$1
version="${OPENTELEMETRY_CPP_VERSION}"
archive_sha256="${OPENTELEMETRY_CPP_SHA256}"
workdir=/tmp/opentelemetry-cpp-build
archive_name="opentelemetry-cpp-${version}.tar.gz"
archive_path="${workdir}/${archive_name}"
source_dir="${workdir}/opentelemetry-cpp-${version}"
build_dir="${workdir}/build"
archive_url="https://github.com/open-telemetry/opentelemetry-cpp/archive/refs/tags/v${version}.tar.gz"

rm -rf "${workdir}"
mkdir -p "${workdir}"

curl -fsSL -o "${archive_path}" "${archive_url}"
echo "${archive_sha256}  ${archive_path}" | sha256sum -c -
tar -xzf "${archive_path}" -C "${workdir}"

cmake -S "${source_dir}" -B "${build_dir}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="${install_prefix}" \
    -DCMAKE_INSTALL_LIBDIR=lib \
    -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
    -DBUILD_SHARED_LIBS=ON \
    -DWITH_OTLP_GRPC=ON \
    -DWITH_OTLP_HTTP=ON \
    -DBUILD_TESTING=OFF \
    -DWITH_BENCHMARK=OFF \
    -DWITH_EXAMPLES=OFF \
    -DOPENTELEMETRY_INSTALL=ON

cmake --build "${build_dir}" --parallel
cmake --install "${build_dir}"

rm -rf "${workdir}"
