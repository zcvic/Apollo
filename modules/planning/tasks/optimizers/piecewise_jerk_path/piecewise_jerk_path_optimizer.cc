/******************************************************************************
 * Copyright 2019 The Apollo Authors. All Rights Reserved.
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
 * @file piecewise_jerk_path_optimizer.cc
 **/

#include "modules/planning/tasks/optimizers/piecewise_jerk_path/piecewise_jerk_path_optimizer.h"

#include <memory>
#include <utility>
#include <vector>

#include "modules/planning/common/planning_gflags.h"
#include "modules/planning/math/finite_element_qp/fem_1d_qp_problem.h"

namespace apollo {
namespace planning {

using apollo::common::ErrorCode;
using apollo::common::Status;

PiecewiseJerkPathOptimizer::PiecewiseJerkPathOptimizer(const TaskConfig& config)
    : PathOptimizer(config) {
  SetName("PiecewiseJerkPathOptimizer");
  CHECK(config_.has_piecewise_jerk_path_config());
}

common::Status PiecewiseJerkPathOptimizer::Process(
    const SpeedData& speed_data, const ReferenceLine& reference_line,
    const common::TrajectoryPoint& init_point, PathData* const path_data) {
  const auto frenet_point =
      reference_line.GetFrenetPoint(init_point.path_point());
  const auto& qp_config = config_.qp_piecewise_jerk_path_config();

  std::vector<std::pair<double, double>> lateral_boundaries;
  double start_s = 0.0;
  double delta_s = 0.0;
  reference_line_info_->GetPathBoundaries(&lateral_boundaries, &start_s,
                                          &delta_s);

  auto num_of_points = lateral_boundaries.size();

  std::array<double, 5> w = {
      qp_config.l_weight(),
      qp_config.dl_weight(),
      qp_config.ddl_weight(),
      qp_config.dddl_weight(),
      0.0
  };

  std::array<double, 3> init_lateral_state{frenet_point.l(), frenet_point.dl(),
                                           frenet_point.ddl()};

  auto fem_1d_qp_ = std::make_unique<Fem1dQpProblem>(
      num_of_points, init_lateral_state, delta_s, w, FLAGS_lateral_jerk_bound);

  auto start_time = std::chrono::system_clock::now();

  fem_1d_qp_->SetZeroOrderBounds(lateral_boundaries);
  fem_1d_qp_->SetFirstOrderBounds(FLAGS_lateral_derivative_bound_default);
  fem_1d_qp_->SetSecondOrderBounds(FLAGS_lateral_derivative_bound_default);

  bool success = fem_1d_qp_->Optimize();

  auto end_time = std::chrono::system_clock::now();
  std::chrono::duration<double> diff = end_time - start_time;
  ADEBUG << "Path Optimizer used time: " << diff.count() * 1000 << " ms.";

  if (!success) {
    AERROR << "lateral qp optimizer failed";
    return Status(ErrorCode::PLANNING_ERROR, "lateral qp optimizer failed");
  }

  // TODO(all): use PiecewiseJerkTrajectory1d
  fem_1d_qp_->SetOutputResolution(FLAGS_trajectory_space_resolution);

  std::vector<common::FrenetFramePoint> frenet_frame_path;
  double accumulated_s = frenet_point.s();
  for (size_t i = 0; i < fem_1d_qp_->x().size(); ++i) {
    common::FrenetFramePoint frenet_frame_point;
    frenet_frame_point.set_s(accumulated_s);
    frenet_frame_point.set_l(fem_1d_qp_->x()[i]);
    frenet_frame_point.set_dl(fem_1d_qp_->x_derivative()[i]);
    frenet_frame_point.set_ddl(fem_1d_qp_->x_second_order_derivative()[i]);
    frenet_frame_path.push_back(std::move(frenet_frame_point));
    accumulated_s += qp_config.path_output_resolution();
  }

  path_data->SetReferenceLine(&reference_line);
  path_data->SetFrenetPath(FrenetFramePath(frenet_frame_path));

  return Status::OK();
}

}  // namespace planning
}  // namespace apollo
