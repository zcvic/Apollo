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
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
 *implied. See the License for the specific language governing
 *permissions and limitations under the License.
 *****************************************************************************/

#ifndef MODULES_PREDICTION_COMMON_FEATURE_OUTPUT_H_
#define MODULES_PREDICTION_COMMON_FEATURE_OUTPUT_H_

#include "modules/prediction/proto/feature.pb.h"
#include "modules/prediction/proto/offline_features.pb.h"

namespace apollo {
namespace prediction {

class FeatureOutput {
 public:
  /**
   * @brief Destructor
   */
  ~FeatureOutput() = default;

  /**
   * @brief Close the output stream
   */
  static void Close();

  /**
   * @brief Check if output is ready
   * @return True if output is ready
   */
  static bool Ready();

  /**
   * @brief Write a proto to file
   */
  static void Write(const Feature& feature);

 private:
  FeatureOutput() = delete;
};

}  // namespace prediction
}  // namespace apollo

#endif  // MODULES_PREDICTION_COMMON_FEATURE_OUTPUT_H_
