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
ENV DEBIAN_FRONTEND=noninteractive
    RUN mkdir /src /razorback && apt-get update && apt-get install -y \
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
        nlohmann-json3-dev \
        libgtest-dev





    WORKDIR /src
    COPY . /src/api

    WORKDIR /src/api
    RUN ./autojunk.sh && ./configure --prefix=/razorback --enable-debug --enable-assert  && make && make install

FROM ${BASE_IMAGE}
    ENV DEBIAN_FRONTEND=noninteractive
    RUN groupadd -g 10000 razorback && useradd -d /razorback -s /bin/false -u 10000 -g 10000 razorback

    RUN apt-get update && apt-get install -y \
        libuuid1 \
        libcurl4 \
        libssl3 \
        libconfig9 \
        libssh-4 \
        libjson-c5 \
        libmagic1 \
        librabbitmq4 \
        libgrpc++1.51t64 \
        libprotobuf17 \
        && apt-get autoremove -y && apt-get clean -y && rm -rf /var/lib/apt/lists/*

    COPY --from=builder /razorback /razorback
    RUN cp /razorback/etc/razorback/magic.sample /razorback/etc/razorback/magic
    WORKDIR /razorback
