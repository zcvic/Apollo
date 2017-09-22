#!/usr/bin/env bash

###############################################################################
# Copyright 2017 The Apollo Authors. All Rights Reserved.
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


addgroup --gid "$DOCKER_GRP_ID" "$DOCKER_GRP"
adduser --disabled-password --gecos '' "$DOCKER_USER" \
    --uid "$DOCKER_USER_ID" --gid "$DOCKER_GRP_ID" 2>/dev/null
usermod -aG sudo "$DOCKER_USER"
echo '%sudo ALL=(ALL) NOPASSWD:ALL' >> /etc/sudoers
cp -r /etc/skel/. /home/${DOCKER_USER}
echo 'if [ -e "/apollo/scripts/apollo_base.sh" ]; then source /apollo/scripts/apollo_base.sh; fi' >> "/home/${DOCKER_USER}/.bashrc"
echo "ulimit -c unlimited" >> /home/${DOCKER_USER}/.bashrc

chown -R ${DOCKER_USER}:${DOCKER_GRP} "/home/${DOCKER_USER}"

# grant caros user to access GPS device
if [ -e /dev/ttyUSB0 ]; then
    sudo chmod a+rw /dev/ttyUSB0 /dev/ttyUSB1
fi

if [ "$RELEASE_DOCKER" != "1" ];then
  # setup map data
  if [ -e /home/tmp/modules_data ]; then
    cp -r /home/tmp/modules_data/* /apollo/modules/
    chown -R ${DOCKER_USER}:${DOCKER_GRP} "/apollo/modules"
  fi

  # setup HMI config for internal maps and vehicles.
  HMI_INTERNAL_PACTH=modules/hmi/conf/internal_config.patch
  if [ -e /apollo/${HMI_INTERNAL_PACTH} ]; then
    cd /apollo
    git apply ${HMI_INTERNAL_PACTH}
    git update-index --assume-unchanged modules/hmi/conf/config.pb.txt
    rm ${HMI_INTERNAL_PACTH}
  fi

  # setup car specific configuration
  if [ -e /home/tmp/esd_can ]; then
    cp -r /home/tmp/esd_can/include /apollo/third_party/can_card_library/esd_can
    cp -r /home/tmp/esd_can/lib /apollo/third_party/can_card_library/esd_can
    chown -R ${DOCKER_USER}:${DOCKER_GRP} "/apollo/third_party/can_card_library/esd_can"
  fi
  if [ -e /home/tmp/gnss_conf ]; then
    cp -r /home/tmp/gnss_conf/* /apollo/modules/drivers/gnss/conf/
    chown -R ${DOCKER_USER}:${DOCKER_GRP} "/apollo/modules/drivers/gnss/conf/"
  fi
fi
