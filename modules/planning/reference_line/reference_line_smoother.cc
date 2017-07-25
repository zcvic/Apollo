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
 * @file reference_line_smooother.cpp
 **/
#include "modules/planning/reference_line/reference_line_smoother.h"

#include "Eigen/Core"

#include "modules/common/log.h"
#include "modules/common/math/vec2d.h"
#include "modules/common/proto/path_point.pb.h"
#include "modules/planning/math/curve_math.h"
#include "modules/planning/math/double.h"

namespace apollo {
namespace planning {

ReferenceLineSmoother::ReferenceLineSmoother(
    const ReferenceLineSmootherConfig& ref_config) noexcept
    : smoother_config_(ref_config) {}

bool ReferenceLineSmoother::smooth(
    const ReferenceLine& raw_reference_line,
    const Eigen::Vector2d& vehicle_position,
    std::vector<ReferencePoint>* const smoothed_ref_line) {
  // calculate sampling range
  common::SLPoint sl_point;
  if (!raw_reference_line.get_point_in_Frenet_frame(vehicle_position,
                                                    &sl_point)) {
    AERROR << "Fail to map init point on raw reference line!";
    return false;
  }

  double start_s = sl_point.s();
  double speed_limit = std::numeric_limits<double>::infinity();
  ReferencePoint rlp = raw_reference_line.get_reference_point(sl_point.s());
  for (const auto& lane_waypoint : rlp.lane_waypoints()) {
    speed_limit =
        std::min(speed_limit, lane_waypoint.lane->lane().speed_limit());
  }
  if (rlp.lane_waypoints().size() == 0) {
    speed_limit = 10.0;
  }
  double end_s = std::min(raw_reference_line.reference_map_line().length(),
                          40 + speed_limit * 3 + start_s);

  if (!sampling(raw_reference_line, start_s, end_s)) {
    AERROR << "Fail to sample reference line smoother points!";
    return false;
  }

  spline_solver_.reset(
      new Spline2dSolver(t_knots_, smoother_config_.spline_order()));

  if (!apply_constraint(raw_reference_line)) {
    AERROR << "Add constraint for spline smoother failed";
    return false;
  }

  if (!apply_kernel()) {
    AERROR << "Add kernel for spline smoother failed.";
    return false;
  }

  if (!solve()) {
    AERROR << "Solve spline smoother problem failed";
  }

  // mapping spline to reference line point
  const double start_t = t_knots_.front();
  const double end_t = t_knots_.back();

  // TODO : here change output to configurable version
  const double resolution = (end_t - start_t) / 499;
  for (std::uint32_t i = 0; i < 500; ++i) {
    const double t = i * resolution;
    std::pair<double, double> xy = spline_solver_->spline()(t);
    const double heading = std::atan2(spline_solver_->spline().derivative_y(t),
                                      spline_solver_->spline().derivative_x(t));
    const double kappa = CurveMath::compute_curvature(
        spline_solver_->spline().derivative_x(t),
        spline_solver_->spline().second_derivative_x(t),
        spline_solver_->spline().derivative_y(t),
        spline_solver_->spline().second_derivative_y(t));
    const double dkappa = CurveMath::compute_curvature_derivative(
        spline_solver_->spline().derivative_x(t),
        spline_solver_->spline().second_derivative_x(t),
        spline_solver_->spline().third_derivative_x(t),
        spline_solver_->spline().derivative_y(t),
        spline_solver_->spline().second_derivative_y(t),
        spline_solver_->spline().third_derivative_y(t));
    double s = 0.0;
    if (!get_s_from_param_t(t, &s)) {
      AERROR << "Get corresponding s failed!";
      return false;
    }
    ReferencePoint rlp = raw_reference_line.get_reference_point(s);
    ReferencePoint new_rlp(common::math::Vec2d(xy.first, xy.second), heading,
                           kappa, dkappa, rlp.lane_waypoints());
    smoothed_ref_line->push_back(std::move(new_rlp));
  }
  return true;
}

bool ReferenceLineSmoother::sampling(const ReferenceLine& raw_reference_line,
                                     const double start_s, const double end_s) {
  double length = end_s - start_s;
  if (end_s < start_s) {
    AERROR << "end_s " << end_s << " is less than start_s " << start_s;
    return false;
  }
  const double resolution = length / smoother_config_.num_spline();
  for (std::uint32_t i = 0; i <= smoother_config_.num_spline(); ++i) {
    ReferencePoint rlp =
        raw_reference_line.get_reference_point(start_s + resolution * i);
    common::PathPoint path_point;
    path_point.set_x(rlp.x());
    path_point.set_y(rlp.y());
    path_point.set_theta(rlp.heading());
    path_point.set_s(start_s + i * resolution);
    ref_points_.push_back(std::move(path_point));
    t_knots_.push_back(i);
  }
  return true;
}

bool ReferenceLineSmoother::apply_constraint(
    const ReferenceLine& raw_reference_line) {
  const double t_length = t_knots_.back() - t_knots_.front();
  // TODO set to configuration
  double dt = t_length / (smoother_config_.num_evaluated_points() - 1);
  std::vector<double> evaluated_t;
  for (std::uint32_t i = 0; i < smoother_config_.num_evaluated_points(); ++i) {
    evaluated_t.push_back(i * dt);
  }
  std::vector<common::PathPoint> path_points;
  if (!extract_evaluated_points(raw_reference_line, evaluated_t,
                                &path_points)) {
    AERROR << "Extract evaluated points failed";
    return false;
  }

  // Add x, y boundary constraint
  std::vector<double> angles;
  std::vector<double> longitidinal_bound;
  std::vector<double> lateral_bound;
  std::vector<common::math::Vec2d> xy_points;
  for (std::uint32_t i = 0; i < path_points.size(); ++i) {
    angles.push_back(path_points[i].theta());
    longitidinal_bound.push_back(smoother_config_.boundary_bound());
    lateral_bound.push_back(smoother_config_.boundary_bound());
    xy_points.emplace_back(path_points[i].x(), path_points[i].y());
  }

  if (!spline_solver_->mutable_constraint()->add_2d_boundary(
          evaluated_t, angles, xy_points, longitidinal_bound, lateral_bound)) {
    AERROR << "Add 2d boundary constraint failed";
    return false;
  }

  if (!spline_solver_->mutable_constraint()
           ->add_third_derivative_smooth_constraint()) {
    AERROR << "Add jointness constraint failed";
    return false;
  }

  return true;
}

bool ReferenceLineSmoother::apply_kernel() {
  Spline2dKernel* kernel = spline_solver_->mutable_kernel();

  // add spline kernel
  if (smoother_config_.derivative_weight() > 0) {
    kernel->add_derivative_kernel_matrix(smoother_config_.derivative_weight());
  }

  if (smoother_config_.second_derivative_weight() > 0) {
    kernel->add_second_order_derivative_matrix(
        smoother_config_.second_derivative_weight());
  }

  if (smoother_config_.third_derivative_weight() > 0) {
    kernel->add_third_order_derivative_matrix(
        smoother_config_.third_derivative_weight());
  }

  // TODO: change to a configurable param
  kernel->add_regularization(0.01);
  return true;
}

bool ReferenceLineSmoother::solve() { return spline_solver_->solve(); }

bool ReferenceLineSmoother::extract_evaluated_points(
    const ReferenceLine& raw_reference_line, const std::vector<double>& vec_t,
    std::vector<common::PathPoint>* const path_points) const {
  for (std::uint32_t i = 0; i < vec_t.size(); ++i) {
    double s = 0.0;
    if (!get_s_from_param_t(vec_t[i], &s)) {
      AERROR << "Extract point " << i << " failed";
      return false;
    }
    ReferencePoint rlp = raw_reference_line.get_reference_point(s);
    common::PathPoint path_point;
    path_point.set_x(rlp.x());
    path_point.set_y(rlp.y());
    path_point.set_theta(rlp.heading());
    path_point.set_s(s);
    path_points->push_back(std::move(path_point));
  }
  return true;
}

bool ReferenceLineSmoother::get_s_from_param_t(const double t,
                                               double* const s) const {
  if (t_knots_.size() < 2 || Double::compare(t, t_knots_.back(), 1e-8) > 0) {
    return false;
  }
  std::uint32_t lower = find_index(t);
  std::uint32_t upper = lower + 1;
  double weight = 0.0;
  if (Double::compare(t_knots_[upper], t_knots_[lower], 1e-8) > 0) {
    weight = (t - t_knots_[lower]) / (t_knots_[upper] - t_knots_[lower]);
  }
  *s =
      ref_points_[lower].s() * (1.0 - weight) + ref_points_[upper].s() * weight;
  return true;
}

std::uint32_t ReferenceLineSmoother::find_index(const double t) const {
  auto upper_bound = std::upper_bound(t_knots_.begin() + 1, t_knots_.end(), t);
  return std::min(t_knots_.size() - 1,
                  static_cast<size_t>(upper_bound - t_knots_.begin())) -
         1;
}

}  // namespace apollo
}  // namespace planning
