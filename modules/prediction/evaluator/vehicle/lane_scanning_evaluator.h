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

#pragma once

#include <memory>
#include <string>
#include <vector>

#include "modules/prediction/evaluator/evaluator.h"

namespace apollo {
namespace prediction {

class LaneScanningEvaluator : public Evaluator {
 public:
  /**
   * @brief Constructor
   */
  LaneScanningEvaluator();

  /**
   * @brief Destructor
   */
  virtual ~LaneScanningEvaluator() = default;

  /**
   * @brief Override Evaluate
   * @param Obstacle pointer
   */
  void Evaluate(Obstacle* obstacle_ptr) override;

  /**
   * @brief Extract features for learning model's input
   * @param Obstacle pointer
   */
  void ExtractFeatures(const Obstacle* obstacle_ptr,
                       std::vector<double>* feature_values);

 private:
  /**
   * @brief Extract the features for obstacles
   * @param Obstacle pointer
   *        A vector of doubles to be filled up with extracted features
   */
  void ExtractObstacleFeatures(const Obstacle* obstacle_ptr,
                               std::vector<double>* feature_values);

  /**
   * @brief Set lane feature vector
   * @param Obstacle pointer
   *        A vector of doubles to be filled up with extracted features
   */
  void ExtractStaticEnvFeatures(const Obstacle* obstacle_ptr,
                                std::vector<double>* feature_values);


 private:
  static const size_t OBSTACLE_FEATURE_SIZE = 23 + 5 * 9;
  static const size_t INTERACTION_FEATURE_SIZE = 8;
  static const size_t SINGLE_LANE_FEATURE_SIZE = 4;
  static const size_t LANE_POINTS_SIZE = 20;
};

}  // namespace prediction
}  // namespace apollo
