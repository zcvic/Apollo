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

// The base class of camera 2D object detection

#ifndef MODULES_PERCEPTION_OBSTACLE_CAMERA_INTERFACE_BASE_CAMERA_DETECTOR_H_
#define MODULES_PERCEPTION_OBSTACLE_CAMERA_INTERFACE_BASE_CAMERA_DETECTOR_H_

#include <string>
#include <vector>

#include "Eigen/Core"
#include "opencv2/opencv.hpp"

#include "modules/common/macro.h"
#include "modules/perception/lib/base/registerer.h"
#include "modules/perception/obstacle/camera/common/visual_object.h"

namespace apollo {
namespace perception {

class BaseCameraDetector {
 public:
  BaseCameraDetector() {}
  virtual ~BaseCameraDetector() {}

  virtual bool init() = 0;

  // @brief: Object detection on image from camera
  // @param [in]: image frame from camera
  // @param [in/out]: detected objects
  virtual bool detect(const cv::Mat& frame,
                      std::vector<VisualObjectPtr>* objects) = 0;

  virtual bool multitask(const cv::Mat& frame,
                         std::vector<VisualObjectPtr>* objects, cv::Mat* mask) {
    return true;
  };

  // @brief: Extract deep learning ROI features for each object
  // @param [in/out]: detected objects, with 2D bbox and its features
  virtual bool extract(std::vector<VisualObjectPtr>* objects) = 0;

  virtual std::string name() const = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(BaseCameraDetector);
};

REGISTER_REGISTERER(BaseCameraDetector);
#define REGISTER_CAMERA_DETECTOR(name) REGISTER_CLASS(BaseCameraDetector, name)

}  // namespace perception
}  // namespace apollo

#endif  // MODULES_PERCEPTION_OBSTACLE_CAMERA_INTERFACE_BASE_CAMERA_DETECTOR_H_
