# Copyright (c) 2011-2026 Cisco Systems, Inc.
#
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License version 2 as
#  published by the Free Software Foundation.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
#  MA 02110-1301, USA.

ARG BASE_IMAGE="debian:12-slim"
FROM ${BASE_IMAGE} AS builder
ARG OPENTELEMETRY_CPP_VERSION
ARG OPENTELEMETRY_CPP_SHA256
ENV DEBIAN_FRONTEND=noninteractive

RUN mkdir /src /razorback && apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates \
    curl \
    git \
    build-essential \
    cmake \
    automake \
    autoconf \
    libtool \
    pkg-config \
    uuid-dev \
    libcurl4-openssl-dev \
    libssl-dev \
    libconfig-dev \
    libssh-dev \
    libjson-c-dev \
    libmagic-dev \
    librabbitmq-dev \
    libprotobuf-dev \
    protobuf-compiler \
    protobuf-compiler-grpc \
    libgrpc++-dev \
    libabsl-dev \
    nlohmann-json3-dev \
    libgtest-dev \
    zlib1g-dev \
    && apt-get autoremove -y \
    && apt-get clean -y \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
RUN curl -fsSL -o opentelemetry-cpp.tar.gz "https://github.com/open-telemetry/opentelemetry-cpp/archive/refs/tags/v${OPENTELEMETRY_CPP_VERSION}.tar.gz" \
    && echo "${OPENTELEMETRY_CPP_SHA256}  opentelemetry-cpp.tar.gz" | sha256sum -c - \
    && tar -xzf opentelemetry-cpp.tar.gz \
    && cmake -S "/src/opentelemetry-cpp-${OPENTELEMETRY_CPP_VERSION}" -B /src/opentelemetry-cpp-build \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/razorback \
        -DCMAKE_INSTALL_LIBDIR=lib \
        -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
        -DBUILD_SHARED_LIBS=ON \
        -DWITH_OTLP_GRPC=ON \
        -DWITH_OTLP_HTTP=ON \
        -DBUILD_TESTING=OFF \
        -DWITH_BENCHMARK=OFF \
        -DWITH_EXAMPLES=OFF \
        -DOPENTELEMETRY_INSTALL=ON \
    && cmake --build /src/opentelemetry-cpp-build --parallel \
    && cmake --install /src/opentelemetry-cpp-build \
    && rm -rf /src/opentelemetry-cpp.tar.gz \
        /src/opentelemetry-cpp-build \
        "/src/opentelemetry-cpp-${OPENTELEMETRY_CPP_VERSION}"

COPY . /src/api

WORKDIR /src/api
RUN ./autojunk.sh && PKG_CONFIG_PATH=/razorback/lib/pkgconfig:/razorback/lib64/pkgconfig ./configure --prefix=/razorback --enable-debug --enable-assert && make && make install

FROM ${BASE_IMAGE}
ENV DEBIAN_FRONTEND=noninteractive

RUN groupadd -g 10000 razorback && useradd -d /razorback -s /bin/false -u 10000 -g 10000 razorback

RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates \
    libuuid1 \
    libcurl4 \
    libssl3 \
    libconfig9 \
    libssh-4 \
    libjson-c5 \
    libmagic1 \
    librabbitmq4 \
    libgrpc++1.51 \
    libprotobuf32 \
    zlib1g \
    && apt-get autoremove -y \
    && apt-get clean -y \
    && rm -rf /var/lib/apt/lists/*

COPY --from=builder /razorback /razorback
RUN cp /razorback/etc/razorback/magic.sample /razorback/etc/razorback/magic \
    && printf '/razorback/lib\n' > /etc/ld.so.conf.d/razorback.conf \
    && ldconfig
WORKDIR /razorback
