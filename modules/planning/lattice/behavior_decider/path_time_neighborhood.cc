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

#include "modules/planning/lattice/behavior_decider/path_time_neighborhood.h"

#include <utility>
#include <vector>
#include <cmath>

#include "modules/planning/proto/sl_boundary.pb.h"
#include "modules/planning/common/obstacle.h"
#include "modules/planning/lattice/util/lattice_params.h"
#include "modules/planning/lattice/util/reference_line_matcher.h"
#include "modules/planning/lattice/util/lattice_util.h"
#include "modules/common/math/linear_interpolation.h"

namespace apollo {
namespace planning {

using apollo::common::PathPoint;
using apollo::common::TrajectoryPoint;
using apollo::perception::PerceptionObstacle;

namespace {

int LastIndexBefore(const prediction::Trajectory& trajectory, const double t) {
  int num_traj_point = trajectory.trajectory_point_size();
  if (num_traj_point == 0) {
    return -1;
  }
  if (trajectory.trajectory_point(0).relative_time() > t) {
    return -1;
  }
  int start = 0;
  int end = num_traj_point - 1;
  while (start + 1 < end) {
    int mid = start + (end - start) / 2;
    if (trajectory.trajectory_point(mid).relative_time() <= t) {
      start = mid;
    } else {
      end = mid;
    }
  }
  if (trajectory.trajectory_point(end).relative_time() <= t) {
    return end;
  }
  return start;
}

}  // namespace

PathTimeNeighborhood::PathTimeNeighborhood(
    const Frame* frame, const std::array<double, 3>& init_s,
    const ReferenceLine& reference_line,
    const std::vector<common::PathPoint>& discretized_ref_points) {
  init_s_ = init_s;
  SetupObstacles(frame, reference_line, discretized_ref_points);
}

void PathTimeNeighborhood::SetupObstacles(
    const Frame* frame, const ReferenceLine& reference_line,
    const std::vector<common::PathPoint>& discretized_ref_points) {
  const auto& obstacles = frame->obstacles();

  for (const Obstacle* obstacle : obstacles) {
    if (prediction_traj_map_.find(obstacle->Id()) ==
        prediction_traj_map_.end()) {
      prediction_traj_map_[obstacle->Id()] = obstacle->Trajectory();
    } else {
      AWARN << "Duplicated obstacle found [" << obstacle->Id() << "]";
    }

    double relative_time = 0.0;
    while (relative_time < planned_trajectory_time) {
      common::TrajectoryPoint point = obstacle->GetPointAtTime(relative_time);
      common::math::Box2d box = obstacle->GetBoundingBox(point);

      SLBoundary sl_boundary;
      reference_line.GetSLBoundary(box, &sl_boundary);

      // the obstacle is not shown on the region to be considered.
      if (sl_boundary.end_s() < 0.0 ||
          sl_boundary.start_s() > init_s_[0] + planned_trajectory_horizon ||
          (std::abs(sl_boundary.start_l()) > lateral_enter_lane_thred &&
           std::abs(sl_boundary.end_l()) > lateral_enter_lane_thred)) {
        if (path_time_obstacle_map_.find(obstacle->Id()) !=
            path_time_obstacle_map_.end()) {
          break;
        } else {
          relative_time += trajectory_time_resolution;
          continue;
        }
      }

      double v =
          SpeedOnReferenceLine(discretized_ref_points, obstacle, sl_boundary);

      if (path_time_obstacle_map_.find(obstacle->Id()) ==
          path_time_obstacle_map_.end()) {
        path_time_obstacle_map_[obstacle->Id()].set_obstacle_id(obstacle->Id());

        *path_time_obstacle_map_[obstacle->Id()].mutable_bottom_left() =
            SetPathTimePoint(obstacle->Id(), sl_boundary.start_s(),
                             relative_time);

        *path_time_obstacle_map_[obstacle->Id()].mutable_upper_left() =
            SetPathTimePoint(obstacle->Id(), sl_boundary.end_s(),
                             relative_time);
      }

      *path_time_obstacle_map_[obstacle->Id()].mutable_bottom_right() =
          SetPathTimePoint(obstacle->Id(), sl_boundary.start_s(),
                           relative_time);

      *path_time_obstacle_map_[obstacle->Id()].mutable_upper_right() =
          SetPathTimePoint(obstacle->Id(), sl_boundary.end_s(), relative_time);

      relative_time += trajectory_time_resolution;
    }
  }

  for (auto& path_time_obstacle : path_time_obstacle_map_) {
    double s_upper = std::max(path_time_obstacle.second.bottom_right().s(),
                              path_time_obstacle.second.upper_right().s());

    double s_lower = std::min(path_time_obstacle.second.bottom_left().s(),
                              path_time_obstacle.second.upper_left().s());

    path_time_obstacle.second.set_path_lower(s_lower);

    path_time_obstacle.second.set_path_upper(s_upper);

    double t_upper = std::max(path_time_obstacle.second.bottom_right().t(),
                              path_time_obstacle.second.upper_right().t());

    double t_lower = std::min(path_time_obstacle.second.bottom_left().t(),
                              path_time_obstacle.second.upper_left().t());

    path_time_obstacle.second.set_time_lower(t_lower);

    path_time_obstacle.second.set_time_upper(t_upper);
  }
}

double PathTimeNeighborhood::SpeedAtT(const std::string& obstacle_id,
                                      const double t) const {
  bool found =
      prediction_traj_map_.find(obstacle_id) != prediction_traj_map_.end();
  CHECK(found);
  CHECK_GE(t, 0.0);
  prediction::Trajectory trajectory = prediction_traj_map_.at(obstacle_id);
  int num_traj_point = trajectory.trajectory_point_size();
  if (num_traj_point == 0) {
    return 0.0;
  }

  int index = LastIndexBefore(trajectory, t);
  if (index == -1) {
    return trajectory.trajectory_point(0).v();
  }
  if (index == num_traj_point - 1) {
    return trajectory.trajectory_point(index).v();
  }

  double v_before = trajectory.trajectory_point(index).v();
  double t_before = trajectory.trajectory_point(index).relative_time();
  double v_after = trajectory.trajectory_point(index + 1).v();
  double t_after = trajectory.trajectory_point(index + 1).relative_time();

  return apollo::common::math::lerp(v_before, t_before, v_after, t_after, t);
}

PathTimePoint PathTimeNeighborhood::SetPathTimePoint(
    const std::string& obstacle_id, const double s, const double t) const {
  PathTimePoint path_time_point;
  path_time_point.set_s(s);
  path_time_point.set_t(t);
  path_time_point.set_obstacle_id(obstacle_id);
  return path_time_point;
}

double PathTimeNeighborhood::SpeedOnReferenceLine(
    const std::vector<PathPoint>& discretized_ref_points,
    const Obstacle* obstacle, const SLBoundary& sl_boundary) {
  PathPoint obstacle_point_on_ref_line =
      ReferenceLineMatcher::MatchToReferenceLine(discretized_ref_points,
                                                 sl_boundary.start_s());
  const PerceptionObstacle& perception_obstacle = obstacle->Perception();
  double ref_theta = obstacle_point_on_ref_line.theta();
  auto velocity = perception_obstacle.velocity();
  double v =
      std::cos(ref_theta) * velocity.x() + std::sin(ref_theta) * velocity.y();
  return v;
}

std::vector<PathTimeObstacle> PathTimeNeighborhood::GetPathTimeObstacles()
    const {
  std::vector<PathTimeObstacle> path_time_obstacles;
  for (const auto& path_time_obstacle_element : path_time_obstacle_map_) {
    path_time_obstacles.push_back(path_time_obstacle_element.second);
  }
  return path_time_obstacles;
}

bool PathTimeNeighborhood::GetPathTimeObstacle(
    const std::string& obstacle_id, PathTimeObstacle* path_time_obstacle) {
  if (path_time_obstacle_map_.find(obstacle_id) ==
      path_time_obstacle_map_.end()) {
    return false;
  }
  *path_time_obstacle = path_time_obstacle_map_[obstacle_id];
  return true;
}

}  // namespace planning
}  // namespace apollo
