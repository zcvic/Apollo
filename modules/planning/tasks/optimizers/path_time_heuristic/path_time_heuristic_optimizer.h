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
 * @file path_time_heuristic_optimizer.h
 **/

#pragma once

#include <string>

#include "modules/planning/proto/dp_st_speed_config.pb.h"
#include "modules/planning/proto/planning_internal.pb.h"

#include "modules/planning/tasks/optimizers/speed_optimizer.h"

namespace apollo {
namespace planning {

/**
 * @class PathTimeHeuristicOptimizer
 * @brief PathTimeHeuristicOptimizer does ST graph speed planning with dynamic
 * programming algorithm.
 */
class PathTimeHeuristicOptimizer : public SpeedOptimizer {
 public:
  explicit PathTimeHeuristicOptimizer(const TaskConfig& config);

 private:
  common::Status Process(const SLBoundary& adc_sl_boundary,
                         const PathData& path_data,
                         const common::TrajectoryPoint& init_point,
                         const ReferenceLine& reference_line,
                         const SpeedData& reference_speed_data,
                         PathDecision* const path_decision,
                         SpeedData* const speed_data) override;

  bool SearchStGraph(SpeedData* speed_data) const;

 private:
  common::TrajectoryPoint init_point_;
  const ReferenceLine* reference_line_ = nullptr;
  SLBoundary adc_sl_boundary_;
  DpStSpeedConfig dp_st_speed_config_;
};

}  // namespace planning
}  // namespace apollo
