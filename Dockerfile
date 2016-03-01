FROM ubuntu:latest

ENV DEBIAN_FRONTEND noninteractive

# update, and install basic packages
RUN apt-get update
RUN apt-get install -y \
            build-essential \
            wget \
            git \
            python

# install depot_tools http://www.chromium.org/developers/how-tos/install-depot-tools
RUN git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git
ENV PATH /depot_tools:$PATH

# install edash-packager
RUN mkdir edash_packager
WORKDIR edash_packager
RUN gclient config https://www.github.com/google/edash-packager.git --name=src
RUN gclient sync --no-history
RUN cd src && ninja -C out/Release
ENV PATH /edash_packager/src/out/Release:$PATH
