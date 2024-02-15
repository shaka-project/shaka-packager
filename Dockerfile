FROM alpine:3.19 as builder

# Install utilities, libraries, and dev tools.
RUN apk add --no-cache \
        bash curl \
        bsd-compat-headers linux-headers \
        build-base cmake git ninja python3

# Build shaka-packager from the current directory, rather than what has been
# merged.
WORKDIR shaka-packager
COPY . /shaka-packager/
RUN rm -rf build
RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -G Ninja
RUN cmake --build build/ --config Debug --parallel

# Copy only result binaries to our final image.
FROM alpine:3.19
RUN apk add --no-cache libstdc++ python3
COPY --from=builder /shaka-packager/build/packager/packager \
                    /shaka-packager/build/packager/mpd_generator \
                    /shaka-packager/build/packager/pssh-box.py \
                    /usr/bin/

# Copy pyproto directory, which is needed by pssh-box.py script. This line
# cannot be combined with the line above as Docker's copy command skips the
# directory itself. See https://github.com/moby/moby/issues/15858 for details.
COPY --from=builder /shaka-packager/build/packager/pssh-box-protos \
                    /usr/bin/pssh-box-protos
