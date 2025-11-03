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
        librabbitmq-dev



    WORKDIR /src
    COPY . /src/api

    WORKDIR /src/api
    RUN ./configure --prefix=/razorback --enable-debug --enable-assert  && make && make install

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
        && apt-get autoremove -y && apt-get clean -y && rm -rf /var/lib/apt/lists/*

    COPY --from=builder /razorback /razorback
    RUN cp /razorback/etc/razorback/magic.sample /razorback/etc/razorback/magic
    WORKDIR /razorback
