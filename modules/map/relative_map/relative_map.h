/******************************************************************************
 * Copyright 2018 The Apollo Authors. All Rights Reserved.
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

#ifndef MODULES_MAP_RELATIVE_MAP_RELATIVE_MAP_H_
#define MODULES_MAP_RELATIVE_MAP_RELATIVE_MAP_H_

#include <memory>
#include <string>

#include "modules/map/relative_map/proto/navigation.pb.h"
#include "modules/map/relative_map/proto/relative_map_config.pb.h"
#include "modules/perception/proto/perception_obstacle.pb.h"

#include "modules/common/monitor_log/monitor_log_buffer.h"
#include "modules/common/status/status.h"
#include "modules/map/relative_map/navigation_lane.h"
#include "modules/map/relative_map/relative_map_interface.h"

namespace apollo {
namespace relative_map {

class RelativeMap : public RelativeMapInterface {
 public:
  RelativeMap();

  /**
   * @brief module name
   */
  std::string Name() const override { return "RelativeMap"; };

  /**
   * @brief module initialization function
   * @return initialization status
   */
  apollo::common::Status Init() override;

  /**
   * @brief module start function
   * @return start status
   */
  apollo::common::Status Start() override;

  /**
   * @brief module stop function
   */
  void Stop() override;

  /**
   * @brief destructor
   */
  virtual ~RelativeMap() = default;

  /**
   * @brief Data callback upon receiving a perception obstacle message.
   * This function is the main logic of the class.
   * @param perception_obstacles received message.
   */
  void RunOnce(
      const perception::PerceptionObstacles& perception_obstacles) override;

  /**
   * @brief Data callback upon receiving a navigation message.
   * This function is used to update the navigation path.
   * @param navigation_info received message.
   */
  void RunOnce(const NavigationInfo& navigation_info);

 private:
  void CreateMapFromPerception(
      const apollo::perception::PerceptionObstacles& perception_obstacles,
      MapMsg* map_msg);

  bool CreateMapMsgFromNavigationPath(const NavigationPath& navigation_path,
                                      double left_width, double right_width,
                                      relative_map::MapMsg* map_msg);

  common::adapter::AdapterManagerConfig adapter_conf_;
  RelativeMapConfig config_;
  apollo::common::monitor::MonitorLogger monitor_logger_;

  NavigationLane navigation_lane_;
};

}  // namespace relative_map
}  // namespace apollo

#endif  // MODULES_MAP_RELATIVE_MAP_RELATIVE_MAP_H_
