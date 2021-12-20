FROM alpine:3.11 as builder

# Install utilities, libraries, and dev tools.
RUN apk add --no-cache \
        bash curl \
        bsd-compat-headers linux-headers \
        build-base git ninja python2 python3

# Install depot_tools.
WORKDIR /
RUN git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git
ENV PATH $PATH:/depot_tools

# Alpine uses musl which does not have mallinfo defined in malloc.h. Define the
# structure to workaround a Chromium base bug.
RUN sed -i \
    '/malloc_usable_size/a \\nstruct mallinfo {\n  int arena;\n  int hblkhd;\n  int uordblks;\n};' \
    /usr/include/malloc.h
ENV GYP_DEFINES='musl=1'

# Bypass VPYTHON included by depot_tools, which no longer works in Alpine.
ENV VPYTHON_BYPASS="manually managed python not supported by chrome operations"

# Build shaka-packager from the current directory, rather than what has been
# merged.
WORKDIR shaka_packager
RUN gclient config https://github.com/google/shaka-packager.git --name=src --unmanaged
COPY . src
RUN gclient sync --force
RUN ninja -C src/out/Release

# Copy only result binaries to our final image.
FROM alpine:3.11
RUN apk add --no-cache libstdc++ python
COPY --from=builder /shaka_packager/src/out/Release/packager \
                    /shaka_packager/src/out/Release/mpd_generator \
                    /shaka_packager/src/out/Release/pssh-box.py \
                    /usr/bin/
# Copy pyproto directory, which is needed by pssh-box.py script. This line
# cannot be combined with the line above as Docker's copy command skips the
# directory itself. See https://github.com/moby/moby/issues/15858 for details.
COPY --from=builder /shaka_packager/src/out/Release/pyproto /usr/bin/pyproto
