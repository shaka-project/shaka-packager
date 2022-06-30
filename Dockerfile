FROM alpine:3.12 as builder

# Install utilities, libraries, and dev tools.
RUN apk add --no-cache \
        bash curl \
        bsd-compat-headers c-ares-dev linux-headers \
        build-base cmake git go perl python3

# Build shaka-packager from the current directory, rather than what has been
# merged.
WORKDIR shaka_packager
COPY . /shaka_packager/
RUN rm -rf build
RUN mkdir build
RUN cmake -S . -B build
RUN make -C build

# Copy only result binaries to our final image.
FROM alpine:3.12
RUN apk add --no-cache libstdc++ python3
# TODO(joeyparrish): Copy binaries when build system is complete
#COPY --from=builder /shaka_packager/build/packager \
#                    /shaka_packager/build/mpd_generator \
#                    /shaka_packager/build/pssh-box.py \
#                    /usr/bin/

# Copy pyproto directory, which is needed by pssh-box.py script. This line
# cannot be combined with the line above as Docker's copy command skips the
# directory itself. See https://github.com/moby/moby/issues/15858 for details.
# TODO(joeyparrish): Copy binaries when build system is complete
#COPY --from=builder /shaka_packager/build/pyproto /usr/bin/pyproto
