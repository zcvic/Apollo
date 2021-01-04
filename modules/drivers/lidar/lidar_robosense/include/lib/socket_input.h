/******************************************************************************
 * Copyright 2017 The Apollo Authors. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *****************************************************************************/

#ifndef CYBERTRON_ONBOARD_DRIVERS_SUTENG_INCLUDE_SUTENG_LIB_SOCKET_INPUT_H
#define CYBERTRON_ONBOARD_DRIVERS_SUTENG_INCLUDE_SUTENG_LIB_SOCKET_INPUT_H

#include <pcap.h>
#include <stdio.h>
#include <unistd.h>

#include "modules/drivers/lidar/lidar_robosense/include/lib/data_type.h"
#include "modules/drivers/lidar/lidar_robosense/include/lib/input.h"

namespace autobot {
namespace drivers {
namespace robosense {

static const int POLL_TIMEOUT = 1000;  // one second (in msec)

/** @brief Live suteng input from socket. */
class SocketInput : public Input {
 public:
  SocketInput();
  virtual ~SocketInput();
  void init(uint32_t port);
  int get_firing_data_packet(adu::common::sensor::suteng::SutengPacket* pkt,
                             int time_zone, uint64_t _start_time);
  int get_positioning_data_packtet(const NMEATimePtr& nmea_time);

 private:
  int _sockfd;
  int _port;
  bool input_available(int timeout);
};
}  // namespace robosense
}  // namespace drivers
}  // namespace autobot

#endif  // CYBERTRON_ONBOARD_DRIVERS_SUTENG_INCLUDE_SUTENG_LIB_SOCKET_INPUT_H
