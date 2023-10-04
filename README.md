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