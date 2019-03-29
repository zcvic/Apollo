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

#include "modules/planning/tasks/deciders/path_assessment_decider.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "modules/common/configs/vehicle_config_helper.h"
#include "modules/common/proto/pnc_point.pb.h"
#include "modules/map/hdmap/hdmap_util.h"
#include "modules/planning/common/planning_context.h"
#include "modules/planning/tasks/deciders/path_decider_obstacle_utils.h"

namespace apollo {
namespace planning {

using apollo::common::ErrorCode;
using apollo::common::Status;
using apollo::common::VehicleConfigHelper;
using apollo::common::math::Box2d;
using apollo::common::math::NormalizeAngle;
using apollo::common::math::Polygon2d;
using apollo::common::math::Vec2d;
using apollo::hdmap::HDMapUtil;

// PointDecision contains (s, PathPointType, distance to closest obstacle).
using PathPointDecision = std::tuple<double, PathData::PathPointType, double>;

PathAssessmentDecider::PathAssessmentDecider(const TaskConfig& config)
    : Decider(config) {}

Status PathAssessmentDecider::Process(
    Frame* const frame, ReferenceLineInfo* const reference_line_info) {
  // Sanity checks.
  CHECK_NOTNULL(frame);
  CHECK_NOTNULL(reference_line_info);
  const auto& candidate_path_data = reference_line_info->GetCandidatePathData();

  if (candidate_path_data.empty()) {
    ADEBUG << "Candidate path data is empty.";
  } else {
    ADEBUG << "There are " << candidate_path_data.size() << " candidate paths";
  }

  // 1. Remove invalid path.
  std::vector<PathData> valid_path_data;
  for (const auto& curr_path_data : candidate_path_data) {
    // RecordDebugInfo(curr_path_data, curr_path_data.path_label(),
    //                 reference_line_info);
    if (curr_path_data.path_label().find("fallback") != std::string::npos) {
      if (IsValidFallbackPath(*reference_line_info, curr_path_data)) {
        valid_path_data.push_back(curr_path_data);
      }
    } else {
      if (IsValidRegularPath(*reference_line_info, curr_path_data)) {
        valid_path_data.push_back(curr_path_data);
      }
    }
  }

  // 2. Analyze and add important info for speed decider to use.
  for (auto& curr_path_data : valid_path_data) {
    if (curr_path_data.path_label().find("fallback") != std::string::npos) {
      continue;
    }
    ADEBUG << "Path length = " << curr_path_data.frenet_frame_path().size();
    SetPathInfo(*reference_line_info, &curr_path_data);
    // Trim all the lane-borrowing paths so that it ends with an in-lane
    // position.
    TrimTailingOutLanePoints(&curr_path_data);
    // TODO(jiacheng): remove empty path_data.
  }
  // If there is no valid path_data, exit.
  if (valid_path_data.empty()) {
    const std::string msg = "Neither regular nor fallback path is valid.";
    AERROR << msg;
    return Status(ErrorCode::PLANNING_ERROR, msg);
  }
  ADEBUG << "There are " << valid_path_data.size() << " valid path data.";

  // 3. Pick the optimal path.
  std::sort(valid_path_data.begin(), valid_path_data.end(),
            [](const PathData& lhs, const PathData& rhs) {
              if (lhs.Empty() || rhs.Empty()) {
                return rhs.Empty();
              }
              // Regular path goes before fallback path.
              bool lhs_is_regular =
                  lhs.path_label().find("regular") != std::string::npos;
              bool rhs_is_regular =
                  rhs.path_label().find("regular") != std::string::npos;
              if (lhs_is_regular != rhs_is_regular) {
                return lhs_is_regular;
              }
              // Select longer path.
              constexpr double kPathLengthComparisonTolerance = 5.0;
              double lhs_path_length = lhs.discretized_path().back().s();
              double rhs_path_length = rhs.discretized_path().back().s();
              if (std::fabs(lhs_path_length - rhs_path_length) >
                  kPathLengthComparisonTolerance) {
                return lhs_path_length > rhs_path_length;
              }
              // If roughly same length,
              // then select self-lane path over borrowed-lane paths.
              bool lhs_on_selflane =
                  lhs.path_label().find("self") != std::string::npos;
              bool rhs_on_selflane =
                  rhs.path_label().find("self") != std::string::npos;
              if (lhs_on_selflane || rhs_on_selflane) {
                return lhs_on_selflane;
              }
              // If roughly same length, and must borrow neighbor lane,
              // then prefer to borrow forward lane rather than reverse lane.
              bool lhs_on_reverse =
                  ContainsOutOnReverseLane(lhs.path_point_decision_guide());
              bool rhs_on_reverse =
                  ContainsOutOnReverseLane(rhs.path_point_decision_guide());
              if (lhs_on_reverse != rhs_on_reverse) {
                return !lhs_on_reverse;
              }
              // If same length, both neighbor lane are forward,
              // then select the one that returns back to in-lane earlier.
              int lhs_back_idx =
                  GetBackToInLaneIndex(lhs.path_point_decision_guide());
              int rhs_back_idx =
                  GetBackToInLaneIndex(rhs.path_point_decision_guide());
              if (lhs_back_idx != rhs_back_idx) {
                return lhs_back_idx < rhs_back_idx;
              }
              // If same length, both forward, back to inlane at same time,
              // select the left one to side-pass.
              bool lhs_on_leftlane =
                  lhs.path_label().find("left") != std::string::npos;
              bool rhs_on_leftlane =
                  rhs.path_label().find("left") != std::string::npos;
              if (lhs_on_leftlane != rhs_on_leftlane) {
                return lhs_on_leftlane;
              }
              // Otherwise, they are the same path, lhs is not < rhs.
              return false;
            });
  ADEBUG << "There are " << valid_path_data.size() << " path(s).";
  ADEBUG << "Using " << valid_path_data.front().path_label() << " path.";
  *(reference_line_info->mutable_path_data()) = valid_path_data.front();
  reference_line_info->SetBlockingObstacleId(
      valid_path_data.front().blocking_obstacle_id());

  if (!(reference_line_info->GetBlockingObstacleId()).empty()) {
    if (PlanningContext::front_static_obstacle_cycle_counter() < 0) {
      PlanningContext::ResetFrontStaticObstacleCycleCounter();
    }
    PlanningContext::IncrementFrontStaticObstacleCycleCounter();
  } else {
    PlanningContext::ResetFrontStaticObstacleCycleCounter();
  }

  if (reference_line_info->path_data().path_label().find("self") !=
      std::string::npos && std::get<1>(reference_line_info->path_data().
      path_point_decision_guide().front()) ==
      PathData::PathPointType::IN_LANE) {
    if (PlanningContext::able_to_use_self_lane_counter() < 0) {
      PlanningContext::ResetAbleToUseSelfLaneCounter();
    }
    PlanningContext::IncrementAbleToUseSelfLaneCounter();
  } else {
    PlanningContext::ResetAbleToUseSelfLaneCounter();
  }

  // Plot the path in simulator for debug purpose.
  RecordDebugInfo(reference_line_info->path_data(), "Planning PathData",
                  reference_line_info);
  return Status::OK();
}

bool PathAssessmentDecider::IsValidRegularPath(
    const ReferenceLineInfo& reference_line_info, const PathData& path_data) {
  // Basic sanity checks.
  if (path_data.Empty()) {
    ADEBUG << "Regular Path: path data is empty.";
    return false;
  }
  // Check if the path is greatly off the reference line.
  if (IsGreatlyOffReferenceLine(path_data)) {
    ADEBUG << "Regular Path: ADC is greatly off reference line.";
    return false;
  }
  // Check if the path is greatly off the road.
  if (IsGreatlyOffRoad(reference_line_info, path_data)) {
    ADEBUG << "Regular Path: ADC is greatly off road.";
    return false;
  }
  // Check if there is any collision.
  if (IsCollidingWithStaticObstacles(reference_line_info, path_data)) {
    ADEBUG << "Regular Path: ADC has collision.";
    return false;
  }
  return true;
}

bool PathAssessmentDecider::IsValidFallbackPath(
    const ReferenceLineInfo& reference_line_info, const PathData& path_data) {
  // Basic sanity checks.
  if (path_data.Empty()) {
    ADEBUG << "Fallback Path: path data is empty.";
    return false;
  }
  // Check if the path is greatly off the reference line.
  if (IsGreatlyOffReferenceLine(path_data)) {
    ADEBUG << "Fallback Path: ADC is greatly off reference line.";
    return false;
  }
  // Check if the path is greatly off the road.
  if (IsGreatlyOffRoad(reference_line_info, path_data)) {
    ADEBUG << "Fallback Path: ADC is greatly off road.";
    return false;
  }
  return true;
}

void PathAssessmentDecider::SetPathInfo(
    const ReferenceLineInfo& reference_line_info, PathData* const path_data) {
  // Go through every path_point, and label its:
  //  - in-lane/out-of-lane info
  //  - distance to the closest obstacle.
  std::vector<PathPointDecision> path_decision;
  InitPathPointDecision(*path_data, &path_decision);
  SetPathPointType(reference_line_info, *path_data, &path_decision);
  SetObstacleDistance(reference_line_info, *path_data, &path_decision);

  path_data->SetPathPointDecisionGuide(path_decision);
}

void PathAssessmentDecider::TrimTailingOutLanePoints(
    PathData* const path_data) {
  // Don't trim self-lane path or fallback path.
  if (path_data->path_label().find("fallback") != std::string::npos ||
      path_data->path_label().find("self") != std::string::npos) {
    return;
  }

  // Trim.
  auto frenet_path = path_data->frenet_frame_path();
  auto path_point_decision = path_data->path_point_decision_guide();
  CHECK_EQ(frenet_path.size(), path_point_decision.size());
  while (!path_point_decision.empty() &&
         std::get<1>(path_point_decision.back()) !=
            PathData::PathPointType::IN_LANE) {
    frenet_path.pop_back();
    path_point_decision.pop_back();
  }
  path_data->SetFrenetPath(frenet_path);
  path_data->SetPathPointDecisionGuide(path_point_decision);
}

bool PathAssessmentDecider::IsGreatlyOffReferenceLine(
    const PathData& path_data) {
  constexpr double kOffReferenceLineThreshold = 20.0;
  auto frenet_path = path_data.frenet_frame_path();
  for (const auto& frenet_path_point : frenet_path) {
    if (std::fabs(frenet_path_point.l()) > kOffReferenceLineThreshold) {
      return true;
    }
  }
  return false;
}

bool PathAssessmentDecider::IsGreatlyOffRoad(
    const ReferenceLineInfo& reference_line_info, const PathData& path_data) {
  constexpr double kOffRoadThreshold = 10.0;
  auto frenet_path = path_data.frenet_frame_path();
  for (const auto& frenet_path_point : frenet_path) {
    double road_left_width = 0.0;
    double road_right_width = 0.0;
    if (reference_line_info.reference_line().GetRoadWidth(
            frenet_path_point.s(), &road_left_width, &road_right_width)) {
      if (frenet_path_point.l() > road_left_width + kOffRoadThreshold ||
          frenet_path_point.l() < -road_right_width - kOffRoadThreshold) {
        return true;
      }
    }
  }
  return false;
}

bool PathAssessmentDecider::IsCollidingWithStaticObstacles(
    const ReferenceLineInfo& reference_line_info, const PathData& path_data) {
  // Get all obstacles and convert them into frenet-frame polygons.
  std::vector<Polygon2d> obstacle_polygons;
  auto indexed_obstacles = reference_line_info.path_decision().obstacles();
  for (const auto* obstacle : indexed_obstacles.Items()) {
    // Filter out unrelated obstacles.
    if (!IsWithinPathDeciderScopeObstacle(*obstacle)) {
      continue;
    }
    // Convert into polygon and save it.
    const auto obstacle_sl = obstacle->PerceptionSLBoundary();
    obstacle_polygons.push_back(
        Polygon2d({Vec2d(obstacle_sl.start_s(), obstacle_sl.start_l()),
                   Vec2d(obstacle_sl.start_s(), obstacle_sl.end_l()),
                   Vec2d(obstacle_sl.end_s(), obstacle_sl.end_l()),
                   Vec2d(obstacle_sl.end_s(), obstacle_sl.start_l())}));
  }

  // Go through all the four corner points at every path pt, check collision.
  for (const auto& path_point : path_data.discretized_path()) {
    // Get the four corner points ABCD of ADC at every path point.
    const auto& vehicle_box =
        common::VehicleConfigHelper::Instance()->GetBoundingBox(path_point);
    std::vector<Vec2d> ABCDpoints = vehicle_box.GetAllCorners();
    for (const auto& corner_point : ABCDpoints) {
      // For each corner point, project it onto reference_line
      common::SLPoint curr_point_sl;
      if (!reference_line_info.reference_line().XYToSL(corner_point,
                                                       &curr_point_sl)) {
        AERROR << "Failed to get the projection from point onto "
                  "reference_line";
        return true;
      }
      auto curr_point = Vec2d(curr_point_sl.s(), curr_point_sl.l());
      // Check if it's in any polygon of other static obstacles.
      for (const auto& obstacle_polygon : obstacle_polygons) {
        if (obstacle_polygon.IsPointIn(curr_point)) {
          ADEBUG << "ADC is colliding with obstacle at path s = "
                 << path_point.s();
          return true;
        }
      }
    }
  }

  return false;
}

void PathAssessmentDecider::InitPathPointDecision(
    const PathData& path_data,
    std::vector<PathPointDecision>* const path_point_decision) {
  // Sanity checks.
  CHECK_NOTNULL(path_point_decision);
  path_point_decision->clear();

  // Go through every path point in path data, and initialize a
  // corresponding path point decision.
  for (const auto& frenet_path_point : path_data.frenet_frame_path()) {
    path_point_decision->emplace_back(frenet_path_point.s(),
                                      PathData::PathPointType::UNKNOWN,
                                      std::numeric_limits<double>::max());
  }
}

void PathAssessmentDecider::SetPathPointType(
    const ReferenceLineInfo& reference_line_info, const PathData& path_data,
    std::vector<PathPointDecision>* const path_point_decision) {
  // Sanity checks.
  CHECK_NOTNULL(path_point_decision);

  // Go through every path_point, and add in-lane/out-of-lane info.
  const auto& frenet_path = path_data.frenet_frame_path();
  const auto& discrete_path = path_data.discretized_path();
  const auto& vehicle_config =
      common::VehicleConfigHelper::Instance()->GetConfig();
  const double ego_length = vehicle_config.vehicle_param().length();
  const double ego_width = vehicle_config.vehicle_param().width();
  const double ego_back_to_center =
      vehicle_config.vehicle_param().back_edge_to_center();
  const double ego_half_width = ego_width / 2.0;
  const double ego_center_shift_distance =
      ego_length / 2.0 - ego_back_to_center;

  for (size_t i = 0; i < frenet_path.size(); ++i) {
    const auto& frenet_path_point = frenet_path[i];
    const auto& rear_center_path_point = discrete_path[i];
    const double ego_theta = rear_center_path_point.theta();
    Box2d ego_box({rear_center_path_point.x(), rear_center_path_point.y()},
                  ego_theta, ego_length, ego_width);
    Vec2d shift_vec{ego_center_shift_distance * std::cos(ego_theta),
                    ego_center_shift_distance * std::sin(ego_theta)};
    ego_box.Shift(shift_vec);
    SLBoundary ego_sl_boundary;
    reference_line_info.reference_line().GetSLBoundary(ego_box,
                                                       &ego_sl_boundary);
    const Vec2d front_center_path_point_vec2d =
        shift_vec +
        Vec2d(rear_center_path_point.x(), rear_center_path_point.y());
    const double rear_center_path_point_x = rear_center_path_point.x();
    const double rear_center_path_point_y = rear_center_path_point.y();
    const double rear_center_path_point_theta = ego_theta;
    const double front_center_path_point_x = front_center_path_point_vec2d.x();
    const double front_center_path_point_y = front_center_path_point_vec2d.y();
    const double front_center_path_point_theta = ego_theta;

    double lane_left_width = 0.0;
    double lane_right_width = 0.0;
    if (reference_line_info.reference_line().GetLaneWidth(
            frenet_path_point.s(), &lane_left_width, &lane_right_width)) {
      // Rough sl boundary estimate using single point lane width
      if (ego_sl_boundary.end_l() > lane_left_width ||
          ego_sl_boundary.start_l() < -lane_right_width) {
        // The path point is out of the reference_line's lane.
        // To be conservative, by default treat it as reverse lane.
        std::get<1>((*path_point_decision)[i]) =
            PathData::PathPointType::OUT_ON_REVERSE_LANE;
        // Only when the lanes that contain this path point are all
        // forward lanes and none is reverse lane, then treat this
        // path point as OUT_ON_FORWARD_LANE.
        std::vector<hdmap::LaneInfoConstPtr> rear_forward_lanes;
        std::vector<hdmap::LaneInfoConstPtr> rear_reverse_lanes;
        std::vector<hdmap::LaneInfoConstPtr> front_forward_lanes;
        std::vector<hdmap::LaneInfoConstPtr> front_reverse_lanes;
        const auto& rear_axis_forward_search =
            HDMapUtil::BaseMapPtr()->GetLanesWithHeading(
                common::util::MakePointENU(
                    {rear_center_path_point_x, rear_center_path_point_y}),
                ego_half_width, rear_center_path_point_theta, M_PI / 2.0,
                &rear_forward_lanes);
        const auto& rear_axis_backward_search =
            HDMapUtil::BaseMapPtr()->GetLanesWithHeading(
                common::util::MakePointENU(
                    {rear_center_path_point_x, rear_center_path_point_y}),
                ego_half_width,
                NormalizeAngle(rear_center_path_point_theta - M_PI), M_PI / 2.0,
                &rear_reverse_lanes);
        const auto& front_axis_forward_search =
            HDMapUtil::BaseMapPtr()->GetLanesWithHeading(
                common::util::MakePointENU(
                    {front_center_path_point_x, front_center_path_point_y}),
                ego_half_width, front_center_path_point_theta, M_PI / 2.0,
                &front_forward_lanes);
        const auto& front_axis_backward_search =
            HDMapUtil::BaseMapPtr()->GetLanesWithHeading(
                common::util::MakePointENU(
                    {front_center_path_point_x, front_center_path_point_y}),
                ego_half_width,
                NormalizeAngle(front_center_path_point_theta - M_PI),
                M_PI / 2.0, &front_reverse_lanes);
        // TODO(Jinyun) refine the logic on seperating forward and backward lane
        if (rear_axis_forward_search == 0 || rear_axis_backward_search == 0 ||
            front_axis_forward_search == 0 || front_axis_backward_search == 0) {
          if ((!rear_forward_lanes.empty() || !front_forward_lanes.empty()) &&
              front_reverse_lanes.empty() && rear_reverse_lanes.empty()) {
            std::get<1>((*path_point_decision)[i]) =
                PathData::PathPointType::OUT_ON_FORWARD_LANE;
          }
        }
      } else {
        // The path point is within the reference_line's lane.
        std::get<1>((*path_point_decision)[i]) =
            PathData::PathPointType::IN_LANE;
      }
    } else {
      AERROR << "reference line not ready when setting path point guide";
      return;
    }
  }
}

void PathAssessmentDecider::SetObstacleDistance(
    const ReferenceLineInfo& reference_line_info, const PathData& path_data,
    std::vector<PathPointDecision>* const path_point_decision) {
  // Sanity checks
  CHECK_NOTNULL(path_point_decision);

  // Get all obstacles and convert them into frenet-frame polygons.
  std::vector<Polygon2d> obstacle_polygons;
  auto indexed_obstacles = reference_line_info.path_decision().obstacles();
  for (const auto* obstacle : indexed_obstacles.Items()) {
    // Filter out unrelated obstacles.
    if (!IsWithinPathDeciderScopeObstacle(*obstacle)) {
      continue;
    }
    // Convert into polygon and save it.
    const auto obstacle_box = obstacle->PerceptionBoundingBox();
    obstacle_polygons.push_back(Polygon2d(obstacle_box));
  }

  // Go through every path point, update closest obstacle info.
  const auto& discrete_path = path_data.discretized_path();
  for (size_t i = 0; i < discrete_path.size(); ++i) {
    const auto& path_point = discrete_path[i];
    // Get the bounding box of the vehicle at that point.
    const auto& vehicle_box =
        common::VehicleConfigHelper::Instance()->GetBoundingBox(path_point);
    // Go through all the obstacle polygons, and update the min distance.
    double min_distance_to_obstacles = std::numeric_limits<double>::max();
    for (const auto& obstacle_polygon : obstacle_polygons) {
      double distance_to_vehicle = obstacle_polygon.DistanceTo(vehicle_box);
      min_distance_to_obstacles =
          std::min(min_distance_to_obstacles, distance_to_vehicle);
    }
    std::get<2>((*path_point_decision)[i]) = min_distance_to_obstacles;
  }
}

void PathAssessmentDecider::RecordDebugInfo(
    const PathData& path_data, const std::string& debug_name,
    ReferenceLineInfo *const reference_line_info) {
  const auto &path_points = path_data.discretized_path();
  auto *ptr_optimized_path =
      reference_line_info->mutable_debug()->mutable_planning_data()->add_path();
  ptr_optimized_path->set_name(debug_name);
  ptr_optimized_path->mutable_path_point()->CopyFrom(
      {path_points.begin(), path_points.end()});
}

bool ContainsOutOnReverseLane(
    const std::vector<PathPointDecision>& path_point_decision) {
  for (const auto& curr_decision : path_point_decision) {
    if (std::get<1>(curr_decision) ==
        PathData::PathPointType::OUT_ON_REVERSE_LANE) {
      return true;
    }
  }
  return false;
}

int GetBackToInLaneIndex(
    const std::vector<PathPointDecision>& path_point_decision) {
  // CHECK(!path_point_decision.empty());
  // CHECK(std::get<1>(path_point_decision.back()) ==
  //       PathData::PathPointType::IN_LANE);

  for (int i = static_cast<int>(path_point_decision.size()) - 1; i >= 0; --i) {
    if (std::get<1>(path_point_decision[i]) !=
        PathData::PathPointType::IN_LANE) {
      return i + 1;
    }
  }
  return 0;
}

}  // namespace planning
}  // namespace apollo
