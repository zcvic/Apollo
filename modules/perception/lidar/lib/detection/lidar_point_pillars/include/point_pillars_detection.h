/******************************************************************************
 * Copyright 2020 The Apollo Authors. All Rights Reserved.
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
#pragma once

#include <memory>
#include <string>
#include <vector>

#include "modules/perception/base/object.h"
#include "modules/perception/base/point_cloud.h"
#include "modules/perception/lib/thread/thread_worker.h"
#include "modules/perception/lidar/common/lidar_frame.h"
#include "modules/perception/lidar/lib/detection/lidar_point_pillars/include/point_pillars.h"

namespace apollo {
namespace perception {
namespace lidar {

struct DetectionInitOptions {};

struct DetectionOptions {};

class PointPillarsDetection {
 public:
  PointPillarsDetection() = default;
  ~PointPillarsDetection() = default;

  bool Init(const DetectionInitOptions& options = DetectionInitOptions());

  bool Detect(const DetectionOptions& options, LidarFrame* frame);

 private:
  void PclToArray(const base::PointFCloudPtr& pc_ptr, float* out_points_array,
                  const float normalizing_factor);

  void GetObjects(std::vector<std::shared_ptr<base::Object>>* objects,
                  std::vector<float>* detections);

  // thread worker
  lib::ThreadWorker worker_;

  // reference pointer of lidar frame
  LidarFrame* lidar_frame_ref_ = nullptr;
  std::shared_ptr<base::AttributePointCloud<base::PointF>> original_cloud_;
  int gpu_id_ = -1;

  // PointPillars
  std::unique_ptr<PointPillars> point_pillars_ptr_;
  bool reproduce_result_mode_ = false;
  float score_threshold_ = 0.5;
  float nms_overlap_threshold_ = 0.5;
  std::string pfe_onnx_file_;
  std::string rpn_onnx_file_;

  // time statistics
  double inference_time_ = 0.0;
  double collect_time_ = 0.0;

  // constants
  const float kNormalizingFactor = 255.0f;
  const int kOutputNumBoxFeature = 7;
};

}  // namespace lidar
}  // namespace perception
}  // namespace apollo
