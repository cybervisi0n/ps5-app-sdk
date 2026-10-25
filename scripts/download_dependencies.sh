#!/bin/bash

# Run me from the root of the repo!

PAYLOAD_SDK_URL=https://github.com/ps5-payload-dev/sdk/releases/latest/download/ps5-payload-sdk.zip
PACBREW_RELEASE_URL=https://github.com/ps5-payload-dev/pacbrew-repo/releases/latest/download/ps5-payload-dev.tar.gz

mkdir -p deps/

wget $PAYLOAD_SDK_URL
unzip -o ./ps5-payload-sdk.zip -d deps
rm -f ps5-payload-sdk.zip

mkdir -p deps/include
mkdir -p deps/lib

# Download OpenGL libraries
./scripts/download_library.sh https://github.com/cybervisi0n/ps5-mesa ps5-mesa_release.zip
./scripts/download_library.sh https://github.com/cybervisi0n/ps5-opengnm-psbc ps5-opengnm-psbc_release.zip
./scripts/download_library.sh https://github.com/cybervisi0n/ps5-gl-runtime ps5-gl-runtime_release.zip

# Add your own external libraries here
# SDL2 is provided as an example
./scripts/download_library.sh https://github.com/cybervisi0n/ps5-sdl2 ps5-sdl2_release.zip
