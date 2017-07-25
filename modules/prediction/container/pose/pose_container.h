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

/**
 * @file
 * @brief Obstacles container
 */

#ifndef MODULES_PREDICTION_CONTAINER_POSE_OBSTACLES_H_
#define MODULES_PREDICTION_CONTAINER_POSE_OBSTACLES_H_

#include <memory>
#include <mutex>

#include "modules/prediction/container/container.h"
#include "modules/perception/proto/perception_obstacle.pb.h"
#include "modules/localization/proto/localization.pb.h"

namespace apollo {
namespace prediction {

class PoseContainer : public Container {
 public:
  /**
   * @brief Constructor
   */
  PoseContainer() = default;

  /**
   * @brief Destructor
   */
  virtual ~PoseContainer() = default;

  /**
   * @brief Insert a data message into the container
   * @param Data message to be inserted in protobuf
   */
  void Insert(const ::google::protobuf::Message& message) override;

  /**
   * @brief Transform pose to a perception obstacle.
   * @return A pointer to a perception obstacle.
   */
  apollo::perception::PerceptionObstacle* ToPerceptionObstacle();

  /**
   * @brief Get timestamp
   */
  double GetTimestamp();

 private:
  void Update(const localization::LocalizationEstimate &localization);

 private:
  std::unique_ptr<apollo::perception::PerceptionObstacle> obstacle_ptr_;

  static std::mutex g_mutex_;
  static int id_;
  static apollo::perception::PerceptionObstacle::Type type_;
};

}  // namespace prediction
}  // namespace apollo

#endif  // MODULES_PREDICTION_CONTAINER_POSE_OBSTACLES_H_
