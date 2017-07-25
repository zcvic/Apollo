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
 * @file dp_poly_path_optimizer.cpp
 **/
#include <string>

#include "modules/common/util/file.h"
#include "modules/planning/common/planning_gflags.h"
#include "modules/planning/optimizer/dp_poly_path/dp_poly_path_optimizer.h"
#include "modules/planning/optimizer/dp_poly_path/dp_road_graph.h"

namespace apollo {
namespace planning {

DpPolyPathOptimizer::DpPolyPathOptimizer(const std::string &name)
    : PathOptimizer(name) {}

common::ErrorCode DpPolyPathOptimizer::process(
    const SpeedData &speed_data, const ReferenceLine &reference_line,
    const ::apollo::planning::TrajectoryPoint &init_point,
    DecisionData *const decision_data, PathData *const path_data) {
  CHECK_NOTNULL(decision_data);
  CHECK_NOTNULL(path_data);
  if (!common::util::GetProtoFromFile(FLAGS_dp_poly_path_config_file,
                                      &config_)) {
    AERROR << "failed to load config file " << FLAGS_dp_poly_path_config_file;
    return common::ErrorCode::PLANNING_ERROR_FAILED;
  }
  DpRoadGraph dp_road_graph(config_, init_point, speed_data);
  dp_road_graph.find_tunnel(reference_line, decision_data, path_data);
  return common::ErrorCode::PLANNING_OK;
}

}  // namespace planning
}  // namespace apollo
