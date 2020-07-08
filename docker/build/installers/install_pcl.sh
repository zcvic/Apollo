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
. /tmp/installers/installer_base.sh

# Install system-provided pcl
# apt-get -y update && \
#   apt-get -y install \
#   libpcl-dev
# exit 0
if ldconfig -p | grep -q libpcl_common ; then
    info "Found existing PCL installation. Skipp re-installation."
    exit 0
fi

# libpcap-dev
# libboost-all-dev
apt-get -y update && \
    apt-get -y install \
    libeigen3-dev \
    libflann-dev \
    libglew-dev \
    libglfw3-dev \
    freeglut3-dev \
    libusb-1.0-0-dev \
    libopenmpi-dev \
    libpng-dev \
    libjpeg-dev

# NOTE(storypku)
# libglfw3-dev depends on libglfw3,
# and libglew-dev have a dependency over libglew2.0

THREAD_NUM=$(nproc)

# VERSION="1.11.0"
# CHECKSUM="4255c3d3572e9774b5a1dccc235711b7a723197b79430ef539c2044e9ce65954" # 1.11.0

VERSION="1.10.1"
CHECKSUM="61ec734ec7c786c628491844b46f9624958c360012c173bbc993c5ff88b4900e" # 1.10.1
PKG_NAME="pcl-${VERSION}.tar.gz"

DOWNLOAD_LINK="https://github.com/PointCloudLibrary/pcl/archive/${PKG_NAME}"

ARCH=$(uname -m)

if [[ "$ARCH" == "x86_64" ]]; then
    download_if_not_cached "${PKG_NAME}" "${CHECKSUM}" "${DOWNLOAD_LINK}"
    tar xzf ${PKG_NAME}

    # Ref: https://src.fedoraproject.org/rpms/pcl.git
    pushd pcl-pcl-${VERSION}/
        patch -p1 < /tmp/installers/pcl-sse-fix-${VERSION}.patch
        mkdir build && cd build

        # -DCUDA_ARCH_BIN="${SUPPORTED_NVIDIA_SMS}"
        cmake .. \
            -DWITH_CUDA=OFF \
            -DPCL_ENABLE_SSE=ON \
            -DBoost_NO_SYSTEM_PATHS=TRUE \
            -DBOOST_ROOT:PATHNAME="${SYSROOT_DIR}" \
            -DBUILD_SHARED_LIBS=ON \
            -DCMAKE_INSTALL_PREFIX="${SYSROOT_DIR}" \
            -DCMAKE_BUILD_TYPE=Release
        make -j${THREAD_NUM}
        make install
    popd

    ldconfig
    #clean up
    rm -fr ${PKG_NAME} pcl-pcl-${VERSION}
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

# Clean up cache to reduce layer size.
apt-get clean && \
    rm -rf /var/lib/apt/lists/*
