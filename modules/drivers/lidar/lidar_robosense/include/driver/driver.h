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

#ifndef ONBOARD_DRIVERS_SUTENG_INCLUDE_SUTENG_DRIVER_DRIVER_H
#define ONBOARD_DRIVERS_SUTENG_INCLUDE_SUTENG_DRIVER_DRIVER_H

#include <memory>
#include <string>

#include "modules/drivers/lidar/lidar_robosense/include/lib/data_type.h"
#include "modules/drivers/lidar/lidar_robosense/include/lib/pcap_input.h"
#include "modules/drivers/lidar/lidar_robosense/include/lib/socket_input.h"
#include "modules/drivers/lidar/lidar_robosense/proto/sensor_suteng.pb.h"
#include "modules/drivers/lidar/lidar_robosense/proto/sensor_suteng_conf.pb.h"

namespace autobot {
namespace drivers {
namespace robosense {

class RobosenseDriver {
 public:
  RobosenseDriver();
  virtual ~RobosenseDriver() {}

  virtual bool poll(
      const std::shared_ptr<adu::common::sensor::suteng::SutengScan>& scan) {
    return true;
  }
  virtual void init() = 0;
  uint64_t start_time() { return _start_time; }

 protected:
  cybertron::proto::SutengConfig _config;
  std::shared_ptr<Input> _input;

  bool flags = false;

  uint64_t _basetime;
  uint32_t _last_gps_time;
  uint64_t _start_time;
  int poll_standard(
     const  std::shared_ptr<adu::common::sensor::suteng::SutengScan>& scan);
  int poll_sync_count(
     const  std::shared_ptr<adu::common::sensor::suteng::SutengScan>& scan,
      bool main_frame);
  uint64_t _last_count;
  bool set_base_time();
  void set_base_time_from_nmea_time(const NMEATimePtr& nmea_time,
                                    uint64_t* basetime,
                                    bool use_gps_time = false);
  void update_gps_top_hour(unsigned int current_time);

  bool cute_angle(adu::common::sensor::suteng::SutengPacket* packet);
};

class Robosense16Driver : public RobosenseDriver {
 public:
  Robosense16Driver(
    const cybertron::proto::SutengConfig& robo_config);
  ~Robosense16Driver();

  void init();
  bool poll(
    const std::shared_ptr<adu::common::sensor::suteng::SutengScan>& scan);
  void poll_positioning_packet();

 private:
  std::shared_ptr<Input> _positioning_input;
  std::thread positioning_thread_;
  std::atomic<bool> _running = {true};
};

class RobosenseDriverFactory {
 public:
  static RobosenseDriver* create_driver(
     const cybertron::proto::SutengConfig& robo_config);
};
}  // namespace robosense
}  // namespace drivers
}  // namespace autobot

#endif  // ONBOARD_DRIVERS_SUTENG_INCLUDE_SUTENG_DRIVER_DRIVER_H
