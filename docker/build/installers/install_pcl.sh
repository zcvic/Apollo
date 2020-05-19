#!/usr/bin/env bash

###############################################################################
# Copyright 2018 The Apollo Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
###############################################################################

# Fail on first error.
set -e

cd "$(dirname "${BASH_SOURCE[0]}")"

apt-get -y update && \
    apt-get -y install \
    libeigen3-dev \
    libflann-dev \
    libvtk6-dev

. /tmp/installers/installer_base.sh

THREAD_NUM=$(nproc)

# Install from source
VERSION="1.11.0"
PKG_NAME="pcl-${VERSION}.tar.gz"
CHECKSUM="4255c3d3572e9774b5a1dccc235711b7a723197b79430ef539c2044e9ce65954"
DOWNLOAD_LINK="https://github.com/PointCloudLibrary/pcl/archive/${PKG_NAME}"

ARCH=$(uname -m)
if [ "$ARCH" == "aarch64" ]; then
  BUILD=$1
  shift
fi

if [ "$BUILD" == "build" ] || [ "$ARCH" == "x86_64" ]; then
  download_if_not_cached "${PKG_NAME}" "${CHECKSUM}" "${DOWNLOAD_LINK}"

  tar xzvf ${PKG_NAME}

  pushd pcl-pcl-${VERSION}/
  echo "add_definitions(-D_GLIBCXX_USE_CXX11_ABI=0)" > temp
  cat CMakeLists.txt >> temp
  mv temp CMakeLists.txt
  mkdir build
  cd build
  cmake ..
  make -j${THREAD_NUM}
  make install
  popd

  #clean up
  rm -fr pcl-${VERSION}.tar.gz pcl-pcl-${VERSION}
else
  # aarch64 prebuilt package
  wget https://apollocache.blob.core.windows.net/apollo-cache/pcl.zip
  unzip pcl.zip

  pushd pcl/
  mkdir -p /usr/local/include/pcl-1.7/
  cd include
  cp -r pcl /usr/local/include/pcl-1.7/
  cd ../
  cp -r lib /usr/local/
  popd

  #clean up
  rm -fr pcl.zip pcl
fi
