#!/bin/bash

# Install required dependencies
sudo add-apt-repository universe
if [ $? -ne 0 ]; then
  sudo apt-get install -y software-properties-common
  sudo add-apt-repository universe
fi
sudo apt-get update
sudo apt-get install -y curl build-essential libcurl4-openssl-dev unzip cmake tar
sudo apt-get install -y libglu1-mesa-dev mesa-common-dev libxmu-dev libxi-dev 
sudo apt-get install -y libfreetype6-dev freeglut3-dev qt5-default 
sudo apt-get install -y linux-tools-common

# Load Submodules (vcpkg, libunwindstack)
git submodule update --init --recursive

# Build vcpkg
cd external/vcpkg

if [ -f "vcpkg" ]
then
    echo "Orbit: found vcpkg"
else
    echo "Orbit: compiling vcpkg"
    ./bootstrap-vcpkg.sh
fi

## Override freetype-gl portfile for linux (.lib->.a)
cd ../..
cp "OrbitUtils/freetype-gl-portfile.cmake" "external/vcpkg/ports/freetype-gl/portfile.cmake"
cd external/vcpkg

## Build dependencies
set VCPKG_DEFAULT_TRIPLET=x64-linux
./vcpkg install freetype-gl breakpad capstone asio cereal imgui glew

#remove compiled libfreetype.a TODO: fix in cmakelists.txt
rm "installed/x64-linux/lib/libfreetype.a"

# CMake
cd ../..
if [ ! -d build/ ]; then
mkdir build
fi
cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
#cmake ..
make
