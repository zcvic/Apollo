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

#include "modules/planning/scenarios/traffic_light/unprotected_right_turn/traffic_light_unprotected_right_turn_scenario.h"

#include "modules/perception/proto/perception_obstacle.pb.h"
#include "modules/perception/proto/traffic_light_detection.pb.h"
#include "modules/planning/proto/planning_config.pb.h"

#include "cyber/common/log.h"
#include "modules/common/time/time.h"
#include "modules/common/vehicle_state/vehicle_state_provider.h"
#include "modules/planning/common/frame.h"
#include "modules/planning/common/planning_context.h"
#include "modules/planning/scenarios/traffic_light/unprotected_right_turn/stage_creep.h"
#include "modules/planning/scenarios/traffic_light/unprotected_right_turn/stage_intersection_cruise.h"
#include "modules/planning/scenarios/traffic_light/unprotected_right_turn/stage_stop.h"

namespace apollo {
namespace planning {
namespace scenario {
namespace traffic_light {

using hdmap::HDMapUtil;
using perception::TrafficLight;

void TrafficLightUnprotectedRightTurnScenario::Init() {
  if (init_) {
    return;
  }

  Scenario::Init();

  if (!GetScenarioConfig()) {
    AERROR << "fail to get scenario specific config";
    return;
  }

  const std::string traffic_light_overlap_id =
      PlanningContext::GetScenarioInfo()->next_traffic_light_overlap.object_id;
  if (traffic_light_overlap_id.empty()) {
    return;
  }

  hdmap::SignalInfoConstPtr traffic_light = HDMapUtil::BaseMap().GetSignalById(
      hdmap::MakeMapId(traffic_light_overlap_id));
  if (!traffic_light) {
    AERROR << "Could not find traffic light: " << traffic_light_overlap_id;
    return;
  }

  init_ = true;
}

apollo::common::util::Factory<
    ScenarioConfig::StageType, Stage,
    Stage* (*)(const ScenarioConfig::StageConfig& stage_config)>
    TrafficLightUnprotectedRightTurnScenario::s_stage_factory_;

void TrafficLightUnprotectedRightTurnScenario::RegisterStages() {
  if (!s_stage_factory_.Empty()) {
    s_stage_factory_.Clear();
  }
  s_stage_factory_.Register(
      ScenarioConfig::TRAFFIC_LIGHT_UNPROTECTED_RIGHT_TURN_STOP,
      [](const ScenarioConfig::StageConfig& config) -> Stage* {
        return new StageStop(config);
      });
  s_stage_factory_.Register(
      ScenarioConfig::TRAFFIC_LIGHT_UNPROTECTED_RIGHT_TURN_CREEP,
      [](const ScenarioConfig::StageConfig& config) -> Stage* {
        return new StageCreep(config);
      });
  s_stage_factory_.Register(
      ScenarioConfig::TRAFFIC_LIGHT_UNPROTECTED_RIGHT_TURN_INTERSECTION_CRUISE,
      [](const ScenarioConfig::StageConfig& config) -> Stage* {
        return new StageIntersectionCruise(config);
      });
}

std::unique_ptr<Stage> TrafficLightUnprotectedRightTurnScenario::CreateStage(
    const ScenarioConfig::StageConfig& stage_config) {
  if (s_stage_factory_.Empty()) {
    RegisterStages();
  }
  auto ptr = s_stage_factory_.CreateObjectOrNull(stage_config.stage_type(),
                                                 stage_config);
  if (ptr) {
    ptr->SetContext(&context_);
  }
  return ptr;
}

bool TrafficLightUnprotectedRightTurnScenario::IsTransferable(
    const Scenario& current_scenario,
    const common::TrajectoryPoint& ego_point,
    const Frame& frame) {
  const std::string traffic_light_overlap_id =
      PlanningContext::GetScenarioInfo()->next_traffic_light_overlap.object_id;
  if (traffic_light_overlap_id.empty()) {
    return false;
  }

  const auto& reference_line_info = frame.reference_line_info().front();
  const double adc_front_edge_s = reference_line_info.AdcSlBoundary().end_s();
  const double traffic_light_overlap_start_s =
      PlanningContext::GetScenarioInfo()->next_traffic_light_overlap.start_s;
  const double adc_distance_to_stop_line =
      traffic_light_overlap_start_s - adc_front_edge_s;
  const double adc_speed =
      common::VehicleStateProvider::Instance()->linear_velocity();

  auto scenario_config = config_.traffic_light_unprotected_right_turn_config();

  bool is_stopped_for_traffic_light = true;
  if (adc_speed > scenario_config.max_adc_stop_speed() ||
      adc_distance_to_stop_line > scenario_config.max_valid_stop_distance()) {
    is_stopped_for_traffic_light = false;
    ADEBUG << "ADC not stopped: speed[" << adc_speed
        << "] adc_distance_to_stop_line[" << adc_distance_to_stop_line << "]";
  }

  bool right_turn = (reference_line_info.GetPathTurnType() ==
      hdmap::Lane::RIGHT_TURN);

  switch (current_scenario.scenario_type()) {
    case ScenarioConfig::LANE_FOLLOW:
    case ScenarioConfig::CHANGE_LANE:
    case ScenarioConfig::SIDE_PASS:
    case ScenarioConfig::APPROACH:
      return (is_stopped_for_traffic_light && right_turn &&
          PlanningContext::GetScenarioInfo()->traffic_light_color ==
              TrafficLight::RED);
    case ScenarioConfig::STOP_SIGN_PROTECTED:
    case ScenarioConfig::STOP_SIGN_UNPROTECTED:
    case ScenarioConfig::TRAFFIC_LIGHT_PROTECTED:
    case ScenarioConfig::TRAFFIC_LIGHT_UNPROTECTED_LEFT_TURN:
      return false;
    case ScenarioConfig::TRAFFIC_LIGHT_UNPROTECTED_RIGHT_TURN:
      return (current_scenario.GetStatus() !=
              Scenario::ScenarioStatus::STATUS_DONE);
    default:
      break;
  }

  return false;
}

/*
 * read scenario specific configs and set in context_ for stages to read
 */
bool TrafficLightUnprotectedRightTurnScenario::GetScenarioConfig() {
  if (!config_.has_traffic_light_unprotected_right_turn_config()) {
    AERROR << "miss scenario specific config";
    return false;
  }
  context_.scenario_config.CopyFrom(
      config_.traffic_light_unprotected_right_turn_config());
  return true;
}

}  // namespace traffic_light
}  // namespace scenario
}  // namespace planning
}  // namespace apollo
