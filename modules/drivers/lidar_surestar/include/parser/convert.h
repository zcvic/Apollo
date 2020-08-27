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

#ifndef ONBOARD_DRIVERS_SURESTAR_INCLUDE_VELODYNE_PARSER_CONVERT_H
#define ONBOARD_DRIVERS_SURESTAR_INCLUDE_VELODYNE_PARSER_CONVERT_H

#include <memory>

#include "modules/drivers/lidar_surestar/include/parser/surestar_parser.h"
#include "modules/drivers/lidar_surestar/proto/sensor_surestar.pb.h"
#include "modules/drivers/proto/pointcloud.pb.h"

namespace apollo {
namespace drivers {
namespace surestar {

// convert surestar data to pointcloud and republish
class Convert {
 public:
  explicit Convert(const apollo::drivers::surestar::SurestarConfig& surestar_config);
  ~Convert();

  void convert_velodyne_to_pointcloud(
      const std::shared_ptr<apollo::drivers::Surestar::SurestarScan const>&
          scan_msg,
      const std::shared_ptr<apollo::drivers::PointCloud>& point_cloud);

  bool Init();
  uint32_t GetPointSize();

 private:
  SurestarParser* _parser;
  apollo::drivers::surestar::SurestarConfig _config;
};

}  // namespace  surestar
}  // namespace drivers
}  // namespace apollo

#endif  // ONBOARD_DRIVERS_SURESTAR_INCLUDE_VELODYNE_PARSER_CONVERT_H
