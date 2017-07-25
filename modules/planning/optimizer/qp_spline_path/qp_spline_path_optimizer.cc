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
 * @file qp_spline_path_optimizer.cpp
 **/
#include "modules/planning/optimizer/qp_spline_path/qp_spline_path_optimizer.h"

#include "modules/planning/common/planning_gflags.h"

namespace apollo {
namespace planning {

using ErrorCode = common::ErrorCode;

QPSplinePathOptimizer::QPSplinePathOptimizer(const std::string& name)
    : PathOptimizer(name) {}

common::ErrorCode QPSplinePathOptimizer::process(
    const SpeedData& speed_data, const ReferenceLine& reference_line,
    const common::TrajectoryPoint& init_point,
    DecisionData* const decision_data, PathData* const path_data) {
  _path_generator.SetConfig(FLAGS_qp_spline_path_config_file);
  if (!_path_generator.generate(reference_line, *decision_data, speed_data,
                                init_point, path_data)) {
    AERROR << "failed to generate spline path!";
    return ErrorCode::PLANNING_ERROR_FAILED;
  }
  return ErrorCode::PLANNING_OK;
}

}  // namespace planning
}  // namespace apollo
