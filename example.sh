#!/bin/sh
set -x

# Builds tinybuild into build/tinybuild_build using build/tinybuild
P=$PATH  # only env variable we want to keep
env -i FROM=ubuntu \
IMGCOPY0="/etc/resolv.conf:/etc/resolv.conf" \
INSTALL0="apt-get update -y && apt-get upgrade -y" \
INSTALL1="apt-get install -y gcc make" \
COPY0="src:/build/src" \
COPY1="Makefile:/build/Makefile" \
EXEC="mkdir /build/build/ && cd /build/ && make" \
POSTCOPY0="/build/build/tinybuild:./build/tinybuild_build" \
PATH=$P build/tinybuild
