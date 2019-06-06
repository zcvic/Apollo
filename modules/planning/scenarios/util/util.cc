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

#include "modules/planning/scenarios/util/util.h"

#include "modules/common/configs/vehicle_config_helper.h"
#include "modules/common/util/util.h"
#include "modules/common/vehicle_state/vehicle_state_provider.h"
#include "modules/map/pnc_map/path.h"
#include "modules/planning/common/planning_context.h"

namespace apollo {
namespace planning {
namespace scenario {
namespace util {

using common::VehicleConfigHelper;
using common::util::DistanceXY;
using hdmap::PathOverlap;

hdmap::PathOverlap* GetOverlapOnReferenceLine(
    const ReferenceLineInfo& reference_line_info, const std::string& overlap_id,
    const ReferenceLineInfo::OverlapType& overlap_type) {
  if (overlap_type == ReferenceLineInfo::SIGNAL) {
    // traffic_light_overlap
    const auto& traffic_light_overlaps =
        reference_line_info.reference_line().map_path().signal_overlaps();
    for (const auto& traffic_light_overlap : traffic_light_overlaps) {
      if (traffic_light_overlap.object_id == overlap_id) {
        return const_cast<hdmap::PathOverlap*>(&traffic_light_overlap);
      }
    }
  } else if (overlap_type == ReferenceLineInfo::STOP_SIGN) {
    // stop_sign_overlap
    const auto& stop_sign_overlaps =
        reference_line_info.reference_line().map_path().stop_sign_overlaps();
    for (const auto& stop_sign_overlap : stop_sign_overlaps) {
      if (stop_sign_overlap.object_id == overlap_id) {
        return const_cast<hdmap::PathOverlap*>(&stop_sign_overlap);
      }
    }
  } else if (overlap_type == ReferenceLineInfo::PNC_JUNCTION) {
    // pnc_junction_overlap
    const auto& pnc_junction_overlaps =
        reference_line_info.reference_line().map_path().pnc_junction_overlaps();
    for (const auto& pnc_junction_overlap : pnc_junction_overlaps) {
      if (pnc_junction_overlap.object_id == overlap_id) {
        return const_cast<hdmap::PathOverlap*>(&pnc_junction_overlap);
      }
    }
  }

  return nullptr;
}

/**
 * @brief: check adc parked properly
 */
PullOverStatus CheckADCPullOver(const ReferenceLineInfo& reference_line_info,
                                const ScenarioPullOverConfig& scenario_config) {
  const double adc_front_edge_s = reference_line_info.AdcSlBoundary().end_s();
  const auto& pull_over_status =
      PlanningContext::Instance()->planning_status().pull_over();

  if (!pull_over_status.is_feasible() ||
      !pull_over_status.has_position() ||
      !pull_over_status.position().has_x() ||
      !pull_over_status.position().has_y() ||
      !pull_over_status.has_theta()) {
    ADEBUG << "pull_over status not set properly: "
           << pull_over_status.DebugString();
    return UNKNOWN;
  }

  common::SLPoint pull_over_sl;
  const auto& reference_line = reference_line_info.reference_line();
  reference_line.XYToSL(
      {pull_over_status.position().x(), pull_over_status.position().y()},
      &pull_over_sl);
  double distance = adc_front_edge_s - pull_over_sl.s();
  if (distance >= scenario_config.pass_destination_threshold()) {
    ADEBUG << "ADC passed pull-over spot: distance[" << distance << "]";
    return PASS_DESTINATION;
  }

  const double adc_speed =
      common::VehicleStateProvider::Instance()->linear_velocity();
  if (adc_speed > scenario_config.max_adc_stop_speed()) {
    ADEBUG << "ADC not stopped: speed[" << adc_speed << "]";
    return APPOACHING;
  }

  constexpr double kStartParkCheckRange = 3.0;  // meter
  if (distance <= -kStartParkCheckRange) {
    ADEBUG << "ADC still far: distance[" << distance << "]";
    return APPOACHING;
  }

  common::SLPoint adc_position_sl;
  const common::math::Vec2d adc_position = {
      common::VehicleStateProvider::Instance()->x(),
      common::VehicleStateProvider::Instance()->y()};
  reference_line.XYToSL(adc_position, &adc_position_sl);

  const double s_diff = pull_over_sl.s() - adc_position_sl.s();
  const double l_diff = std::fabs(pull_over_sl.l() - adc_position_sl.l());

  const double theta_diff = std::fabs(common::math::NormalizeAngle(
      pull_over_status.theta() -
      common::VehicleStateProvider::Instance()->heading()));
  ADEBUG << "adc_position_s[" << adc_position_sl.s() << "] adc_position_l["
         << adc_position_sl.l() << "] pull_over_s[" << pull_over_sl.s()
         << "] pull_over_l[" << pull_over_sl.l() << "] s_diff[" << s_diff
         << "] l_diff[" << l_diff << "] theta_diff[" << theta_diff << "]";

  if (s_diff >= 0 && s_diff <= scenario_config.max_s_error_to_end_point() &&
      l_diff <= scenario_config.max_l_error_to_end_point() &&
      theta_diff <= scenario_config.max_theta_error_to_end_point()) {
    return PARK_COMPLETE;
  } else {
    return PARK_FAIL;
  }
}

PullOverStatus CheckADCPullOverOpenSpace(
    const ScenarioPullOverConfig& scenario_config) {
  const auto& pull_over_status =
      PlanningContext::Instance()->planning_status().pull_over();

  if (!pull_over_status.is_feasible() ||
      !pull_over_status.has_position() ||
      !pull_over_status.position().has_x() ||
      !pull_over_status.position().has_y() ||
      !pull_over_status.has_theta()) {
    ADEBUG << "pull_over status not set properly: "
           << pull_over_status.DebugString();
    return UNKNOWN;
  }

  const common::math::Vec2d adc_position = {
      common::VehicleStateProvider::Instance()->x(),
      common::VehicleStateProvider::Instance()->y()};

  const common::math::Vec2d end_pose = {pull_over_status.position().x(),
                                        pull_over_status.position().y()};

  const double distance_diff = adc_position.DistanceTo(end_pose);

  const double theta_diff = std::fabs(common::math::NormalizeAngle(
      pull_over_status.theta() -
      common::VehicleStateProvider::Instance()->heading()));

  if (distance_diff <= scenario_config.max_distance_error_to_end_point() &&
      theta_diff <= scenario_config.max_theta_error_to_end_point()) {
    return PARK_COMPLETE;
  } else {
    ADEBUG << "distance_diff[" << distance_diff << "] while distance threshold["
           << scenario_config.max_distance_error_to_end_point() << "]";
    ADEBUG << "theta_diff is[" << theta_diff << "] while theta threshold["
           << scenario_config.max_theta_error_to_end_point() << "]";
    return PARK_FAIL;
  }

  return PARK_FAIL;
}
}  // namespace util

}  // namespace scenario
}  // namespace planning
}  // namespace apollo
