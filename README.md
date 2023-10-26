# OpenTok Encoder

## Configuration

Create `.env` file with the following parameters filled out:

```shell
API_KEY=<OPENTOK_PROJECT_API_KEY>
SESSION_ID=<OPENTOK_SESSION_ID>
TOKEN=<OPENTOK_SESSION_TOKEN>
```

## Development Dockerfile

Building image

```shell
docker build -t opentok_encoder_builder:latest -f Dockerfile.cpp-env .
```

Run image with mount to local source code.

```shell
docker run -it --rm --name=opentok_encoder_builder \
 --mount type=bind,source=${PWD},target=/src \
 opentok_encoder_builder:latest \
 bash
```

Building and running source code in build container.

```shell
cd src

# Create and cd into a build directory
mkdir build && cd build

# Set Build Time environment variables here eg. "HTTP_SERVER_HOST=0.0.0.0 cmake .."
cmake ..

# Build and create binary
make

# Run binary, can set run time environment variables here
./streamer
```

# Proxy Server

## SRS

### Repo

https://github.com/ossrs/srs

### Documentation

https://ossrs.io/lts/en-us/docs/v5/doc/getting-started#webrtc

### Setup

Set the `candidate` variable (see: https://ossrs.io/lts/en-us/docs/v5/doc/webrtc#config-candidate)

```shell
# For macOS
CANDIDATE=$(ifconfig en0 inet | grep 'inet ' | awk '{print $2}')

# For CentOS
CANDIDATE=$(ifconfig eth0 | grep 'inet '| awk '{print $2}')

# Directly set ip.
CANDIDATE="192.168.3.10"
```

Run docker container:

```shell
export CANDIDATE=$(ifconfig en0 inet | grep 'inet ' | awk '{print $2}')
docker run --rm --env CANDIDATE=$CANDIDATE \
  -p 1935:1935 -p 8080:8080 -p 1985:1985 -p 8000:8000/udp \
  ossrs/srs:5 \
  objs/srs -c conf/rtmp.conf
```

Use ffmpeg to stream video:

```shell
ffmpeg -stream_loop -1 -re -i ~/Downloads/timer.mp4 -c copy -f flv rtmp://localhost/live/livestream
```

Use VLC or some other media player to play back RTMP stream from URL: `rtmp://localhost/live/livestream`
