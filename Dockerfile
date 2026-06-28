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

COPY tools/build/debian12 /tmp/tools/build/debian12
RUN mkdir /src /razorback \
    && apt-get update \
    && apt-get install -y --no-install-recommends $(cat /tmp/tools/build/debian12/build-deps.txt) \
    && apt-get autoremove -y \
    && apt-get clean -y \
    && rm -rf /var/lib/apt/lists/* /tmp/tools

COPY tools/build/install-opentelemetry-cpp.sh /tmp/tools/build/install-opentelemetry-cpp.sh
RUN /tmp/tools/build/install-opentelemetry-cpp.sh /razorback

COPY . /src/api

WORKDIR /src/api
RUN ./autojunk.sh && PKG_CONFIG_PATH=/razorback/lib/pkgconfig:/razorback/lib64/pkgconfig ./configure --prefix=/razorback --enable-debug --enable-assert && make && make install

FROM ${BASE_IMAGE}
ENV DEBIAN_FRONTEND=noninteractive

COPY tools/build/debian12 /tmp/tools/build/debian12
RUN groupadd -g 10000 razorback && useradd -d /razorback -s /bin/false -u 10000 -g 10000 razorback

RUN apt-get update \
    && apt-get install -y --no-install-recommends $(cat /tmp/tools/build/debian12/run-deps.txt) \
    && apt-get autoremove -y \
    && apt-get clean -y \
    && rm -rf /var/lib/apt/lists/* /tmp/tools

COPY --from=builder /razorback /razorback
RUN cp /razorback/etc/razorback/magic.sample /razorback/etc/razorback/magic \
    && printf '/razorback/lib\n' > /etc/ld.so.conf.d/razorback.conf \
    && ldconfig
WORKDIR /razorback
USER razorback
