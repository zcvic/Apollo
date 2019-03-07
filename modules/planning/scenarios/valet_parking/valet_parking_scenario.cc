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

#include <vector>

#include "modules/planning/scenarios/valet_parking/valet_parking_scenario.h"
#include "modules/planning/scenarios/valet_parking/stage_approaching_parking_spot.h"
#include "modules/planning/scenarios/valet_parking/stage_parking.h"

namespace apollo {
namespace planning {
namespace scenario {
namespace valet_parking {

using apollo::common::Status;
using apollo::common::VehicleState;
using apollo::common::math::Box2d;
using apollo::common::math::Vec2d;
using apollo::hdmap::HDMapUtil;
using apollo::hdmap::LaneSegment;
using apollo::hdmap::ParkingSpaceInfoConstPtr;
using apollo::hdmap::Path;

apollo::common::util::Factory<
    ScenarioConfig::StageType, Stage,
    Stage* (*)(const ScenarioConfig::StageConfig& stage_config)>
    ValetParkingScenario::s_stage_factory_;

void ValetParkingScenario::Init() {
  if (init_) {
    return;
  }

  Scenario::Init();

  if (!GetScenarioConfig()) {
    AERROR << "fail to get scenario specific config";
    return;
  }

  hdmap_ = hdmap::HDMapUtil::BaseMapPtr();
  CHECK_NOTNULL(hdmap_);
}

void ValetParkingScenario::RegisterStages() {
  if (s_stage_factory_.Empty()) {
    s_stage_factory_.Clear();
  }
  s_stage_factory_.Register(
      ScenarioConfig::VALET_PARKING_APPROACHING_PARKING_SPOT,
      [](const ScenarioConfig::StageConfig& config) -> Stage* {
        return new StageApproachingParkingSpot(config);
      });
  s_stage_factory_.Register(
      ScenarioConfig::VALET_PARKING_PARKING,
      [](const ScenarioConfig::StageConfig& config) -> Stage* {
        return new StageParking(config);
      });
}

std::unique_ptr<Stage> ValetParkingScenario::CreateStage(
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

bool ValetParkingScenario::GetScenarioConfig() {
  if (!config_.has_valet_parking_config()) {
    AERROR << "miss scenario specific config";
    return false;
  }
  context_.scenario_config.CopyFrom(config_.valet_parking_config());
  return true;
}

bool ValetParkingScenario::IsTransferable(const Scenario& current_scenario,
                                          const Frame& frame) {
  // TODO(all) Implement avaliable parking spot detection by preception results
  context_.target_parking_spot_id.clear();
  if (frame.local_view().routing->routing_request().has_parking_space() &&
      frame.local_view().routing->routing_request().parking_space().has_id()) {
    context_.target_parking_spot_id =
        frame.local_view().routing->routing_request().parking_space().id().id();
  } else {
    const std::string msg = "No parking space id from routing";
    AERROR << msg;
    return false;
  }

  if (context_.target_parking_spot_id.empty()) {
    return false;
  }

  ParkingSpaceInfoConstPtr target_parking_spot = nullptr;
  Path nearby_path;
  const auto& vehicle_state = frame.vehicle_state();
  auto point = common::util::MakePointENU(vehicle_state.x(), vehicle_state.y(),
                                          vehicle_state.z());
  hdmap::LaneInfoConstPtr nearest_lane;
  double vehicle_lane_s = 0.0;
  double vehicle_lane_l = 0.0;
  int status = HDMapUtil::BaseMap().GetNearestLaneWithHeading(
      point, 5.0, vehicle_state.heading(), M_PI / 3.0, &nearest_lane,
      &vehicle_lane_s, &vehicle_lane_l);
  if (status != 0) {
    AERROR << "GetNearestLaneWithHeading failed at IsTransferable() of valet "
              "parking scenario";
    return false;
  }
  // TODO(Jinyun) Take path from reference line
  LaneSegment nearest_lanesegment =
      LaneSegment(nearest_lane, nearest_lane->accumulate_s().front(),
                  nearest_lane->accumulate_s().back());
  std::vector<LaneSegment> segments_vector;
  int next_lanes_num = nearest_lane->lane().successor_id_size();
  if (next_lanes_num != 0) {
    for (int i = 0; i < next_lanes_num; ++i) {
      auto next_lane_id = nearest_lane->lane().successor_id(i);
      segments_vector.push_back(nearest_lanesegment);
      auto next_lane = hdmap_->GetLaneById(next_lane_id);
      LaneSegment next_lanesegment =
          LaneSegment(next_lane, next_lane->accumulate_s().front(),
                      next_lane->accumulate_s().back());
      segments_vector.emplace_back(next_lanesegment);
      nearby_path = Path(segments_vector);
      SearchTargetParkingSpotOnPath(nearby_path, &target_parking_spot);
      if (target_parking_spot != nullptr) {
        break;
      }
    }
  } else {
    segments_vector.push_back(nearest_lanesegment);
    nearby_path = Path(segments_vector);
    SearchTargetParkingSpotOnPath(nearby_path, &target_parking_spot);
  }

  if (target_parking_spot == nullptr) {
    std::string msg(
        "No such parking spot found after searching all path forward possible");
    AERROR << msg << context_.target_parking_spot_id;
    return false;
  }

  if (!CheckDistanceToParkingSpot(vehicle_state, nearby_path,
                                  target_parking_spot)) {
    std::string msg(
        "target parking spot found, but too far, distance larger than "
        "pre-defined distance");
    AERROR << msg << context_.target_parking_spot_id;
    return false;
  }

  return true;
}

void ValetParkingScenario::SearchTargetParkingSpotOnPath(
    const Path& nearby_path, ParkingSpaceInfoConstPtr* target_parking_spot) {
  const auto& parking_space_overlaps = nearby_path.parking_space_overlaps();
  if (parking_space_overlaps.size() != 0) {
    for (const auto& parking_overlap : parking_space_overlaps) {
      if (parking_overlap.object_id == context_.target_parking_spot_id) {
        hdmap::Id id;
        id.set_id(parking_overlap.object_id);
        *target_parking_spot = hdmap_->GetParkingSpaceById(id);
      }
    }
  }
}

bool ValetParkingScenario::CheckDistanceToParkingSpot(
    const VehicleState& vehicle_state, const Path& nearby_path,
    const ParkingSpaceInfoConstPtr& target_parking_spot) {
  Vec2d left_bottom_point = target_parking_spot->polygon().points().at(0);
  Vec2d right_bottom_point = target_parking_spot->polygon().points().at(1);
  double left_bottom_point_s = 0.0;
  double left_bottom_point_l = 0.0;
  double right_bottom_point_s = 0.0;
  double right_bottom_point_l = 0.0;
  double vehicle_point_s = 0.0;
  double vehicle_point_l = 0.0;
  nearby_path.GetNearestPoint(left_bottom_point, &left_bottom_point_s,
                              &left_bottom_point_l);
  nearby_path.GetNearestPoint(right_bottom_point, &right_bottom_point_s,
                              &right_bottom_point_l);
  Vec2d vehicle_vec(vehicle_state.x(), vehicle_state.y());
  nearby_path.GetNearestPoint(vehicle_vec, &vehicle_point_s, &vehicle_point_l);
  if (std::abs((left_bottom_point_s + right_bottom_point_s) / 2 -
               vehicle_point_s) <
      context_.scenario_config.parking_spot_range_to_start()) {
    return true;
  } else {
    return false;
  }
}

}  // namespace valet_parking
}  // namespace scenario
}  // namespace planning
}  // namespace apollo
