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

/**
 * @file
 */

#ifndef MODULES_PREDICTION_SCENARIO_FEATURE_EXTRACTOR_FEATURE_EXTRACTOR_H_
#define MODULES_PREDICTION_SCENARIO_FEATURE_EXTRACTOR_FEATURE_EXTRACTOR_H_

#include "modules/prediction/proto/scenario_feature.pb.h"

namespace apollo {
namespace prediction {

class FeatureExtractor {
 public:
  FeatureExtractor();

  virtual ~FeatureExtractor();

  void UpdateVehicleState();

  void UpdateMap();

  void UpdateObstacleInfo();
};

}  // namespace prediction
}  // namespace apollo

#endif /* MODULES_PREDICTION_SCENARIO_FEATURE_EXTRACTOR_FEATURE_EXTRACTOR_H_ */
