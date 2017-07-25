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
 * @file speed_optimizer.cc
 **/

#include "modules/planning/optimizer/speed_optimizer.h"

#include <string>

namespace apollo {
namespace planning {

SpeedOptimizer::SpeedOptimizer(const std::string& name) : Optimizer(name) {}
common::ErrorCode SpeedOptimizer::optimize(PlanningData* planning_data) {
  return process(planning_data->path_data(),
                 planning_data->init_planning_point(),
                 planning_data->mutable_decision_data(),
                 planning_data->mutable_speed_data());
}

}  // namespace planning
}  // namespace apollo
