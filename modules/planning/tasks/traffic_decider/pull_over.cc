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

/**
 * @file
 **/

#include "modules/planning/tasks/traffic_decider/pull_over.h"

#include <iomanip>
#include <vector>

#include "modules/common/configs/vehicle_config_helper.h"
#include "modules/common/proto/pnc_point.pb.h"
#include "modules/common/vehicle_state/vehicle_state_provider.h"
#include "modules/map/proto/map_lane.pb.h"
#include "modules/planning/common/frame.h"
#include "modules/planning/common/planning_util.h"

namespace apollo {
namespace planning {

using apollo::common::PointENU;
using apollo::common::Status;
using apollo::common::VehicleConfigHelper;
using apollo::hdmap::HDMapUtil;
using apollo::hdmap::LaneSegment;
using apollo::hdmap::PathOverlap;
using apollo::planning::util::GetPlanningStatus;

PullOver::PullOver(const TrafficRuleConfig& config) : TrafficRule(config) {}

Status PullOver::ApplyRule(Frame* const frame,
                           ReferenceLineInfo* const reference_line_info) {
  frame_ = frame;
  reference_line_info_ = reference_line_info;

  if (!IsPullOver()) {
    return Status::OK();
  }

  common::PointENU stop_point;
  if (GetPullOverStop(&stop_point) != 0) {
    ADEBUG << "Could not find a safe pull over point";
    return Status::OK();
  }

  BuildPullOverStop(stop_point);

  return Status::OK();
}

/**
 * @brief: check if in pull_over state
 */
bool PullOver::IsPullOver() const {
  auto* planning_state = GetPlanningStatus()->mutable_planning_state();
  return (planning_state->has_pull_over() &&
      planning_state->pull_over().in_pull_over());
}

bool PullOver::PullOverCompleted() {
  double adc_speed = reference_line_info_->AdcPlanningPoint().v();
  if (adc_speed > config_.stop_sign().max_stop_speed()) {
    ADEBUG << "ADC not stopped: speed[" << adc_speed << "]";
    return false;
  }

  // TODO(all): check stop close enough to pull-over stop line
  return true;
}

bool PullOver::IsValidStop() const {
  // TODO(all) implement this function
  return true;
}

/**
 * @brief:get pull_over points(start & stop)
 */
int PullOver::GetPullOverStop(common::PointENU* stop_point) {
  auto&  pull_over_status = GetPlanningStatus()->
      mutable_planning_state()->pull_over();
  // reuse existing stop point
  if (pull_over_status.has_start_point() &&
      pull_over_status.has_stop_point()) {
    if (IsValidStop()) {
      stop_point->set_x(pull_over_status.stop_point().x());
      stop_point->set_y(pull_over_status.stop_point().y());
      return 0;
    }
  }

  // calculate new stop point if don't have a pull over stop
  common::SLPoint stop_point_sl;
  if (FindPullOverStop(&stop_point_sl) == 0) {
    const auto& reference_line = reference_line_info_->reference_line();
    common::math::Vec2d point;
    reference_line.SLToXY(stop_point_sl, &point);
    stop_point->set_x(point.x());
    stop_point->set_y(point.y());
    return 0;
  }

  return -1;
}

/**
 * @brief: check if s is on overlaps
 */
bool PullOver::OnOverlap(const double s) {
  const auto& reference_line = reference_line_info_->reference_line();

  // crosswalk
  const std::vector<PathOverlap>& crosswalk_overlaps =
      reference_line.map_path().crosswalk_overlaps();
  for (const auto& crosswalk_overlap : crosswalk_overlaps) {
    if (s >= crosswalk_overlap.start_s && s <= crosswalk_overlap.end_s) {
      return true;
    }
  }

  // junction
  const std::vector<PathOverlap>& junction_overlaps =
      reference_line.map_path().junction_overlaps();
  for (const auto& junction_overlap : junction_overlaps) {
    if (s >= junction_overlap.start_s && s <= junction_overlap.end_s) {
      return true;
    }
  }

  // clear_area
  const std::vector<PathOverlap>& clear_area_overlaps =
      reference_line.map_path().clear_area_overlaps();
  for (const auto& clear_area_overlap : clear_area_overlaps) {
    if (s >= clear_area_overlap.start_s && s <= clear_area_overlap.end_s) {
      return true;
    }
  }

  // speed_bump
  const std::vector<PathOverlap>& speed_bump_overlaps =
      reference_line.map_path().speed_bump_overlaps();
  for (const auto& speed_bump_overlap : speed_bump_overlaps) {
    if (s >= speed_bump_overlap.start_s && s <= speed_bump_overlap.end_s) {
      return true;
    }
  }

  return false;
}

/**
 * @brief: find pull over location(start & stop
 */
int PullOver::FindPullOverStop(common::SLPoint* stop_point_sl) {
  double stop_point_s;
  if (FindPullOverStop(&stop_point_s) != 0) {
    return -1;
  }

  const auto& reference_line = reference_line_info_->reference_line();
  if (stop_point_s > reference_line.Length()) {
    return -1;
  }

  const auto& vehicle_param = VehicleConfigHelper::GetConfig().vehicle_param();
  const double adc_width = vehicle_param.width();

  // TODO(all): temporarily set stop point by lane_boarder
  double lane_left_width = 0.0;
  double lane_right_width = 0.0;
  reference_line.GetLaneWidth(stop_point_s,
                              &lane_left_width, &lane_right_width);
  stop_point_sl->set_s(stop_point_s);
  stop_point_sl->set_l(-(lane_right_width - adc_width / 2 -
      config_.pull_over().pull_over_l_buffer()));

  ADEBUG << "stop_point(" << stop_point_sl->s()
      << ", " << stop_point_sl->l() << ")";
  return 0;
}

int PullOver::FindPullOverStop(double* stop_point_s) {
  const auto& reference_line = reference_line_info_->reference_line();
  const double adc_front_edge_s = reference_line_info_->AdcSlBoundary().end_s();

  double check_length = 0.0;
  double total_check_length = 0.0;
  double check_s = adc_front_edge_s;

  constexpr double kDistanceUnit = 5.0;
  while (total_check_length < config_.pull_over().max_check_distance()) {
    check_s += kDistanceUnit;
    total_check_length += kDistanceUnit;

    // find next_lane to check
    std::string prev_lane_id;
    std::vector<hdmap::LaneInfoConstPtr> lanes;
    reference_line.GetLaneFromS(check_s, &lanes);
    hdmap::LaneInfoConstPtr lane;
    for (auto temp_lane : lanes) {
      if (temp_lane->lane().id().id() == prev_lane_id) {
        continue;
      }
      lane = temp_lane;
      prev_lane_id = temp_lane->lane().id().id();
      break;
    }

    std::string lane_id = lane->lane().id().id();
    ADEBUG << "check_s[" << check_s << "] lane[" << lane_id << "]";

    // check turn type: NO_TURN/LEFT_TURN/RIGHT_TURN/U_TURN
    const auto& turn = lane->lane().turn();
    if (turn != hdmap::Lane::NO_TURN) {
      ADEBUG << "path lane[" << lane_id
          << "] turn[" << Lane_LaneTurn_Name(turn) << "] can't pull over";
      check_length = 0.0;
      continue;
    }

    // check rightmost driving lane:
    //   NONE/CITY_DRIVING/BIKING/SIDEWALK/PARKING
    bool rightmost_driving_lane = true;
    for (auto& neighbor_lane_id :
        lane->lane().right_neighbor_forward_lane_id()) {
      const auto neighbor_lane = HDMapUtil::BaseMapPtr()->GetLaneById(
          neighbor_lane_id);
      if (!neighbor_lane) {
        ADEBUG << "Failed to find lane[" << neighbor_lane_id.id() << "]";
        continue;
      }
      const auto& lane_type = neighbor_lane->lane().type();
      if (lane_type == hdmap::Lane::CITY_DRIVING) {
        ADEBUG << "lane[" << lane_id << "]'s right neighbor forward lane["
              << neighbor_lane_id.id() << "] type["
              << Lane_LaneType_Name(lane_type) << "] can't pull over";
        rightmost_driving_lane = false;
        break;
      }
    }
    if (!rightmost_driving_lane) {
      check_length = 0.0;
      continue;
    }

    // check if on overlaps
    if (OnOverlap(check_s)) {
      check_length = 0.0;
      continue;
    }

    // all the checks passed
    check_length += kDistanceUnit;
    if (check_length >= config_.pull_over().plan_distance()) {
      *stop_point_s = check_s;
      ADEBUG << "stop point: lane[" << lane->id().id()
          << "] stop_point_s[" << *stop_point_s
          << "] adc_front_edge_s[" << adc_front_edge_s << "]";
      return 0;
    }
  }

  return -1;
}

int PullOver::BuildPullOverStop(const common::PointENU stop_point) {
  const auto& reference_line = reference_line_info_->reference_line();

  // check
  common::SLPoint stop_point_sl;
  reference_line.XYToSL(stop_point, &stop_point_sl);
  if (stop_point_sl.s() < 0 || stop_point_sl.s() > reference_line.Length()) {
    return -1;
  }

  // create virtual stop wall
  auto pull_over_reason = GetPlanningStatus()->
      planning_state().pull_over().reason();
  std::string virtual_obstacle_id = PULL_OVER_VO_ID_PREFIX +
      PullOverStatus_Reason_Name(pull_over_reason);
  auto* obstacle = frame_->CreateStopObstacle(reference_line_info_,
                                              virtual_obstacle_id,
                                              stop_point_sl.s());
  if (!obstacle) {
    AERROR << "Failed to create obstacle[" << virtual_obstacle_id << "]";
    return -1;
  }
  PathObstacle* stop_wall = reference_line_info_->AddObstacle(obstacle);
  if (!stop_wall) {
    AERROR << "Failed to create path_obstacle for " << virtual_obstacle_id;
    return -1;
  }

  // build stop decision
  double stop_point_heading =
      reference_line.GetReferencePoint(stop_point_sl.s()).heading();

  ObjectDecisionType stop;
  auto stop_decision = stop.mutable_stop();
  stop_decision->set_reason_code(StopReasonCode::STOP_REASON_PULL_OVER);
  stop_decision->set_distance_s(-config_.pull_over().stop_distance());
  stop_decision->set_stop_heading(stop_point_heading);
  stop_decision->mutable_stop_point()->set_x(stop_point.x());
  stop_decision->mutable_stop_point()->set_y(stop_point.y());
  stop_decision->mutable_stop_point()->set_z(0.0);

  auto* path_decision = reference_line_info_->path_decision();
  // if (!path_decision->MergeWithMainStop(
  //        stop.stop(), stop_wall->Id(), reference_line,
  //        reference_line_info_->AdcSlBoundary())) {
  //  ADEBUG << "signal " << virtual_obstacle_id << " is not the closest stop.";
  //  return -1;
  // }

  path_decision->AddLongitudinalDecision(
      TrafficRuleConfig::RuleId_Name(config_.rule_id()), stop_wall->Id(), stop);

  // record in PlanningStatus
  auto* pull_over_status = GetPlanningStatus()->
      mutable_planning_state()->mutable_pull_over();

  common::SLPoint start_point_sl;
  start_point_sl.set_s(
      stop_point_sl.s() - config_.pull_over().plan_distance());
  start_point_sl.set_l(0.0);
  common::math::Vec2d start_point;
  reference_line.SLToXY(start_point_sl, &start_point);
  pull_over_status->mutable_start_point()->set_x(start_point.x());
  pull_over_status->mutable_start_point()->set_y(start_point.y());

  pull_over_status->mutable_stop_point()->set_x(stop_point.x());
  pull_over_status->mutable_stop_point()->set_y(stop_point.y());
  pull_over_status->set_stop_point_heading(stop_point_heading);

  return 0;
}

}  // namespace planning
}  // namespace apollo
