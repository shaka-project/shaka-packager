FROM alpine:3.8 as builder

# Install packages needed for Shaka Packager.
RUN apk add --no-cache bash build-base curl findutils git ninja python \
                       bsd-compat-headers linux-headers libexecinfo-dev

# Install depot_tools.
RUN git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git
ENV PATH $PATH:/depot_tools

# Alpine uses musl which does not have mallinfo defined in malloc.h. Define the
# structure to workaround a Chromium base bug.
RUN sed -i \
    '/malloc_usable_size/a \\nstruct mallinfo {\n  int arena;\n  int hblkhd;\n  int uordblks;\n};' \
    /usr/include/malloc.h

ENV GYP_DEFINES='clang=0 use_experimental_allocator_shim=0 use_allocator=none musl=1'

# Build shaka-packager
WORKDIR shaka_packager
RUN gclient config https://www.github.com/google/shaka-packager.git --name=src --unmanaged
COPY . src
RUN gclient sync
RUN cd src && ninja -C out/Release

# Copy only result binaries to our final image.
FROM alpine:3.8
RUN apk add --no-cache libstdc++ python
COPY --from=builder /shaka_packager/src/out/Release/packager \
                    /shaka_packager/src/out/Release/mpd_generator \
                    /shaka_packager/src/out/Release/pssh-box.py \
                    /usr/bin/
# Copy pyproto directory, which is needed by pssh-box.py script. This line
# cannot be combined with the line above as Docker's copy command skips the
# directory itself. See https://github.com/moby/moby/issues/15858 for details.
COPY --from=builder /shaka_packager/src/out/Release/pyproto /usr/bin/pyproto
