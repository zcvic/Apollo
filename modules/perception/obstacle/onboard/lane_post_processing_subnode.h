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

// @brief: lane_post_processing_subnode header file

#ifndef MODULES_PERCEPTION_OBSTACLE_ONBOARD_LANE_POST_PROCESSING_SUBNODE_H_
#define MODULES_PERCEPTION_OBSTACLE_ONBOARD_LANE_POST_PROCESSING_SUBNODE_H_

#include <string>

#include <Eigen/Core>

#include "modules/common/adapters/adapter_manager.h"
#include "modules/perception/obstacle/base/object.h"
#include "modules/perception/obstacle/camera/interface/base_lane_post_processor.h"
#include "modules/perception/obstacle/onboard/camera_shared_data.h"
#include "modules/perception/obstacle/onboard/fusion_shared_data.h"
#include "modules/perception/obstacle/onboard/lane_shared_data.h"
#include "modules/perception/obstacle/onboard/object_shared_data.h"
#include "modules/perception/onboard/subnode.h"
#include "modules/perception/onboard/subnode_helper.h"

namespace apollo {
namespace perception {

class LanePostProcessingSubnode : public Subnode {
 public:
  LanePostProcessingSubnode() = default;
  virtual ~LanePostProcessingSubnode() {}
  apollo::common::Status ProcEvents() override;

 protected:
  bool InitInternal() override;

 private:
  bool InitSharedData();
  bool InitAlgorithmPlugin();
  bool InitWorkRoot();
  bool GetSharedData(const Event& event,
                     std::shared_ptr<SensorObjects>* objs);
  void PublishDataAndEvent(
      double timestamp,
      const SharedDataPtr<LaneObjects>& lane_objects);

  std::unique_ptr<BaseCameraLanePostProcessor> lane_post_processor_;
  CameraObjectData* camera_object_data_ = nullptr;
  std::string device_id_;
  std::string work_root_dir_;
  LaneSharedData* lane_shared_data_;
  uint64_t seq_num_;

  DISALLOW_COPY_AND_ASSIGN(LanePostProcessingSubnode);
};

}  // namespace perception
}  // namespace apollo

#endif  // MODULES_PERCEPTION_OBSTACLE_ONBOARD_LANE_POST_PROCESSING_SUBNODE_H_
