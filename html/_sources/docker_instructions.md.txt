# Using Docker

[Docker](https://www.docker.com/whatisdocker) is a tool that can package an
application and its dependencies in a virtual container to run on different
host operating systems.

## Get Shaka Packager from Dockerhub

To pull latest Shaka Packager:

```shell
$ docker pull google/shaka-packager
```

You can pull a specific version, e.g. v1.6.2:

```shell
$ docker pull google/shaka-packager:release-v1.6.2
```

The full list of tags is available
[here](https://hub.docker.com/r/google/shaka-packager/tags/).

## Run the container

Assume you have your media files stored in `host_media_path` in the host
machine.

This runs the container and maps `host_media_path` to `media` in the container:

```shell
$ docker run -v /host_media_path/:/media -it --rm google/shaka-packager
```

Note that the networking in the container is containerized by default, so if
you want to access UDP multicast in the host network, you will need to configure
the network explicitly. You may do this with `--net=host` option, i.e.

```shell
$ docker run -v /host_media_path/:/media -it --net=host --rm google/shaka-packager
```

Then in the container, run the packager command, e.g.:

```shell
$ packager input=/media/example.mp4,stream=audio,output=/media/audio.mp4 \
           input=/media/example.mp4,stream=video,output=/media/video.mp4 \
           --mpd_output /media/example.mpd
```

Outputs are available in the host's media folder `host_media_path`.
