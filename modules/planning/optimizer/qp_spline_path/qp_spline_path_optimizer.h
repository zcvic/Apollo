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
 * @file qp_path_optimizer.h
 **/

#ifndef MODULES_PLANNING_OPTIMIZER_QP_SPLINE_PATH_OPTIMIZER_H_
#define MODULES_PLANNING_OPTIMIZER_QP_SPLINE_PATH_OPTIMIZER_H_

#include "modules/common/proto/error_code.pb.h"
#include "modules/planning/optimizer/path_optimizer.h"

#include "modules/planning/optimizer/qp_spline_path/qp_spline_path_generator.h"

namespace apollo {
namespace planning {

class QPSplinePathOptimizer : public PathOptimizer {
 public:
  explicit QPSplinePathOptimizer(const std::string& name);

 private:
  virtual common::ErrorCode process(const SpeedData& speed_data,
                                    const ReferenceLine& reference_line,
                                    const common::TrajectoryPoint& init_point,
                                    DecisionData* const decision_data,
                                    PathData* const path_data) override;

 private:
  QPSplinePathGenerator _path_generator;
};

}  // namespace planning
}  // namespace apollo

#endif  // MODULES_PLANNING_OPTIMIZER_SPLINE_QP_PATH_OPTIMIZER_H_
