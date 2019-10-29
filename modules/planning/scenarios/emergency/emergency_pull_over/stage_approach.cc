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
 * @file
 **/

#include "modules/planning/scenarios/emergency/emergency_pull_over/stage_approach.h"

#include <string>
#include <vector>

#include "cyber/common/log.h"

#include "modules/common/vehicle_state/vehicle_state_provider.h"
#include "modules/planning/common/frame.h"
#include "modules/planning/common/planning_context.h"
#include "modules/planning/common/util/common.h"
#include "modules/planning/scenarios/util/util.h"
#include "modules/planning/tasks/deciders/path_bounds_decider/path_bounds_decider.h"

namespace apollo {
namespace planning {
namespace scenario {
namespace emergency_pull_over {

using apollo::common::TrajectoryPoint;

EmergencyPullOverStageApproach::EmergencyPullOverStageApproach(
    const ScenarioConfig::StageConfig& config)
    : Stage(config) {}

Stage::StageStatus EmergencyPullOverStageApproach::Process(
    const TrajectoryPoint& planning_init_point, Frame* frame) {
  ADEBUG << "stage: Approach";
  CHECK_NOTNULL(frame);

  scenario_config_.CopyFrom(GetContext()->scenario_config);

  bool plan_ok = ExecuteTaskOnReferenceLine(planning_init_point, frame);
  if (!plan_ok) {
    AERROR << "EmergencyPullOverStageApproach planning error";
  }

  const double adc_speed =
      common::VehicleStateProvider::Instance()->linear_velocity();
  const double max_adc_stop_speed =
      common::VehicleConfigHelper::Instance()->GetConfig()
          .vehicle_param()
          .max_abs_speed_when_stopped();
  // TODO(all): add pull over position check
  if (adc_speed <= max_adc_stop_speed) {
    return FinishStage();
  }

  return StageStatus::RUNNING;
}

Stage::StageStatus EmergencyPullOverStageApproach::FinishStage() {
  next_stage_ = ScenarioConfig::EMERGENCY_PULL_OVER_STANDBY;
  return Stage::FINISHED;
}

}  // namespace emergency_pull_over
}  // namespace scenario
}  // namespace planning
}  // namespace apollo
