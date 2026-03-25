#!/bin/sh

set -eu

sudo mkdir -p /razorback
sudo chown `whoami` /razorback
sudo apt-get update
sudo apt-get install -y $(cat tools/build/debian12/build-deps.txt)
sudo apt-get install -y $(cat tools/build/debian12/run-deps.txt)
./tools/build/install-opentelemetry-cpp.sh /razorback
