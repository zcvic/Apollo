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

#include "modules/prediction/scenario/prioritization/obstacles_prioritizer.h"

#include <algorithm>
#include <limits>
#include <memory>
#include <queue>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "modules/prediction/common/prediction_gflags.h"
#include "modules/prediction/common/prediction_map.h"
#include "modules/prediction/container/adc_trajectory/adc_trajectory_container.h"
#include "modules/prediction/container/container_manager.h"
#include "modules/prediction/container/obstacles/obstacle_clusters.h"
#include "modules/prediction/container/pose/pose_container.h"

namespace apollo {
namespace prediction {

using apollo::perception::PerceptionObstacle;
using common::adapter::AdapterConfig;
using common::math::Box2d;
using common::math::Vec2d;
using hdmap::LaneInfo;
using hdmap::OverlapInfo;
using ConstLaneInfoPtr = std::shared_ptr<const LaneInfo>;

namespace {

bool IsLaneSequenceInReferenceLine(
    const LaneSequence& lane_sequence,
    const ADCTrajectoryContainer* ego_trajectory_container) {
  for (const auto& lane_segment : lane_sequence.lane_segment()) {
    std::string lane_id = lane_segment.lane_id();
    if (ego_trajectory_container->IsLaneIdInReferenceLine(lane_id)) {
      return true;
    }
  }
  return false;
}

int NearestFrontObstacleIdOnLaneSequence(const LaneSequence& lane_sequence) {
  int nearest_front_obstacle_id = std::numeric_limits<int>::min();
  double smallest_relative_s = std::numeric_limits<double>::max();
  for (const auto& nearby_obs : lane_sequence.nearby_obstacle()) {
    if (nearby_obs.s() < 0.0) {
      continue;
    }
    if (nearby_obs.s() < smallest_relative_s) {
      smallest_relative_s = nearby_obs.s();
      nearest_front_obstacle_id = nearby_obs.id();
    }
  }
  return nearest_front_obstacle_id;
}

int NearestBackwardObstacleIdOnLaneSequence(const LaneSequence& lane_sequence) {
  int nearest_backward_obstacle_id = std::numeric_limits<int>::min();
  double smallest_relative_s = std::numeric_limits<double>::max();
  for (const auto& nearby_obs : lane_sequence.nearby_obstacle()) {
    if (nearby_obs.s() > 0.0) {
      continue;
    }
    if (-nearby_obs.s() < smallest_relative_s) {
      smallest_relative_s = -nearby_obs.s();
      nearest_backward_obstacle_id = nearby_obs.id();
    }
  }
  return nearest_backward_obstacle_id;
}

}  // namespace

void ObstaclesPrioritizer::PrioritizeObstacles(
    const EnvironmentFeatures& environment_features,
    const std::shared_ptr<ScenarioFeatures> scenario_features) {
  AssignIgnoreLevel(environment_features, scenario_features);
  AssignCautionLevel(scenario_features);
}

void ObstaclesPrioritizer::AssignCautionLevel(
    const std::shared_ptr<ScenarioFeatures> scenario_features) {
  switch (scenario_features->scenario().type()) {
    case Scenario::JUNCTION: {
      AssignCautionLevelInJunction(scenario_features);
      break;
    }
    case Scenario::CRUISE: {
      AssignCautionLevelInCruise(scenario_features);
      break;
    }
    default: {
      AssignCautionLevelInCruise(scenario_features);
      break;
    }
  }
}

void ObstaclesPrioritizer::AssignIgnoreLevel(
    const EnvironmentFeatures& environment_features,
    const std::shared_ptr<ScenarioFeatures> ptr_scenario_features) {
  auto obstacles_container =
      ContainerManager::Instance()->GetContainer<ObstaclesContainer>(
          AdapterConfig::PERCEPTION_OBSTACLES);

  if (obstacles_container == nullptr) {
    AERROR << "Obstacles container pointer is a null pointer.";
    return;
  }

  auto pose_container =
      ContainerManager::Instance()->GetContainer<PoseContainer>(
          AdapterConfig::LOCALIZATION);
  if (pose_container == nullptr) {
    AERROR << "Pose container pointer is a null pointer.";
    return;
  }

  const PerceptionObstacle* pose_obstacle_ptr =
      pose_container->ToPerceptionObstacle();
  if (pose_obstacle_ptr == nullptr) {
    AERROR << "Pose obstacle pointer is a null pointer.";
    return;
  }

  double pose_theta = pose_obstacle_ptr->theta();
  double pose_x = pose_obstacle_ptr->position().x();
  double pose_y = pose_obstacle_ptr->position().y();

  ADEBUG << "Get pose (" << pose_x << ", " << pose_y << ", " << pose_theta
         << ")";

  // Build rectangular scan_area
  Box2d scan_box({pose_x + FLAGS_scan_length / 2.0 * std::cos(pose_theta),
                  pose_y + FLAGS_scan_length / 2.0 * std::sin(pose_theta)},
                 pose_theta, FLAGS_scan_length, FLAGS_scan_width);

  const auto& obstacle_ids =
      obstacles_container->curr_frame_predictable_obstacle_ids();

  for (const int& obstacle_id : obstacle_ids) {
    Obstacle* obstacle_ptr = obstacles_container->GetObstacle(obstacle_id);
    if (obstacle_ptr->history_size() == 0) {
      AERROR << "Obstacle [" << obstacle_ptr->id() << "] has no feature.";
      continue;
    }
    Feature* latest_feature_ptr = obstacle_ptr->mutable_latest_feature();
    double obstacle_x = latest_feature_ptr->position().x();
    double obstacle_y = latest_feature_ptr->position().y();
    Vec2d ego_to_obstacle_vec(obstacle_x - pose_x, obstacle_y - pose_y);
    Vec2d ego_vec = Vec2d::CreateUnitVec2d(pose_theta);
    double s = ego_to_obstacle_vec.InnerProd(ego_vec);

    double pedestrian_like_nearby_lane_radius =
        FLAGS_pedestrian_nearby_lane_search_radius;
    bool is_near_lane = PredictionMap::HasNearbyLane(
        obstacle_x, obstacle_y, pedestrian_like_nearby_lane_radius);

    // Decide if we need consider this obstacle
    bool is_in_scan_area = scan_box.IsPointIn({obstacle_x, obstacle_y});
    bool is_on_lane = obstacle_ptr->IsOnLane();
    bool is_pedestrian_like_in_front_near_lanes =
        s > FLAGS_back_dist_ignore_ped &&
        (latest_feature_ptr->type() == PerceptionObstacle::PEDESTRIAN ||
         latest_feature_ptr->type() == PerceptionObstacle::BICYCLE ||
         latest_feature_ptr->type() == PerceptionObstacle::UNKNOWN ||
         latest_feature_ptr->type() == PerceptionObstacle::UNKNOWN_MOVABLE) &&
        is_near_lane;
    bool is_near_junction = obstacle_ptr->IsNearJunction();

    bool need_consider = is_in_scan_area || is_on_lane || is_near_junction ||
                         is_pedestrian_like_in_front_near_lanes;

    if (!need_consider) {
      latest_feature_ptr->mutable_priority()->set_priority(
          ObstaclePriority::IGNORE);
    } else {
      latest_feature_ptr->mutable_priority()->set_priority(
          ObstaclePriority::NORMAL);
    }
  }
}

void ObstaclesPrioritizer::AssignCautionLevelInCruise(
    const std::shared_ptr<ScenarioFeatures> scenario_features) {
  // TODO(kechxu) integrate change lane when ready to check change lane
  AssignCautionLevelCruiseKeepLane();
}

void ObstaclesPrioritizer::AssignCautionLevelCruiseKeepLane() {
  ObstaclesContainer* obstacles_container =
      ContainerManager::Instance()->GetContainer<ObstaclesContainer>(
          AdapterConfig::PERCEPTION_OBSTACLES);
  if (obstacles_container == nullptr) {
    AERROR << "Null obstacles container found";
    return;
  }
  Obstacle* ego_vehicle =
      obstacles_container->GetObstacle(FLAGS_ego_vehicle_id);
  if (ego_vehicle == nullptr) {
    AERROR << "Ego vehicle not found";
    return;
  }
  if (ego_vehicle->history_size() < 2) {
    AERROR << "Ego vehicle has no history";
    return;
  }
  const Feature& ego_latest_feature = ego_vehicle->feature(1);
  for (const LaneSequence& lane_sequence :
       ego_latest_feature.lane().lane_graph().lane_sequence()) {
    int nearest_front_obstacle_id =
        NearestFrontObstacleIdOnLaneSequence(lane_sequence);
    if (nearest_front_obstacle_id < 0) {
      continue;
    }
    Obstacle* obstacle_ptr =
        obstacles_container->GetObstacle(nearest_front_obstacle_id);
    if (obstacle_ptr == nullptr) {
      AERROR << "Obstacle [" << nearest_front_obstacle_id << "] Not found";
      continue;
    }
    obstacle_ptr->SetCaution();
  }
}

void ObstaclesPrioritizer::AssignCautionLevelCruiseChangeLane() {
  ObstaclesContainer* obstacles_container =
      ContainerManager::Instance()->GetContainer<ObstaclesContainer>(
          AdapterConfig::PERCEPTION_OBSTACLES);
  if (obstacles_container == nullptr) {
    AERROR << "Null obstacles container found";
    return;
  }
  ADCTrajectoryContainer* ego_trajectory_container =
      ContainerManager::Instance()->GetContainer<ADCTrajectoryContainer>(
          AdapterConfig::PLANNING_TRAJECTORY);
  Obstacle* ego_vehicle =
      obstacles_container->GetObstacle(FLAGS_ego_vehicle_id);
  if (ego_vehicle == nullptr) {
    AERROR << "Ego vehicle not found";
    return;
  }
  if (ego_vehicle->history_size() == 0) {
    AERROR << "Ego vehicle has no history";
    return;
  }
  const Feature& ego_latest_feature = ego_vehicle->latest_feature();
  for (const LaneSequence& lane_sequence :
       ego_latest_feature.lane().lane_graph().lane_sequence()) {
    if (lane_sequence.vehicle_on_lane()) {
      int nearest_front_obstacle_id =
          NearestFrontObstacleIdOnLaneSequence(lane_sequence);
      if (nearest_front_obstacle_id < 0) {
        continue;
      }
      Obstacle* obstacle_ptr =
          obstacles_container->GetObstacle(nearest_front_obstacle_id);
      if (obstacle_ptr == nullptr) {
        AERROR << "Obstacle [" << nearest_front_obstacle_id << "] Not found";
        continue;
      }
      obstacle_ptr->SetCaution();
    } else if (IsLaneSequenceInReferenceLine(lane_sequence,
                                             ego_trajectory_container)) {
      int nearest_front_obstacle_id =
          NearestFrontObstacleIdOnLaneSequence(lane_sequence);
      int nearest_backward_obstacle_id =
          NearestBackwardObstacleIdOnLaneSequence(lane_sequence);
      if (nearest_front_obstacle_id >= 0) {
        Obstacle* front_obstacle_ptr =
            obstacles_container->GetObstacle(nearest_front_obstacle_id);
        if (front_obstacle_ptr != nullptr) {
          front_obstacle_ptr->SetCaution();
        }
      }
      if (nearest_backward_obstacle_id >= 0) {
        Obstacle* backward_obstacle_ptr =
            obstacles_container->GetObstacle(nearest_backward_obstacle_id);
        if (backward_obstacle_ptr != nullptr) {
          backward_obstacle_ptr->SetCaution();
        }
      }
    }
  }
}

void ObstaclesPrioritizer::AssignCautionLevelInJunction(
    const std::shared_ptr<ScenarioFeatures> scenario_features) {
  if (scenario_features->scenario().type() != Scenario::JUNCTION) {
    ADEBUG << "Not in Junction Scenario";
    return;
  }
  std::string junction_id = scenario_features->scenario().junction_id();
  ObstaclesContainer* obstacles_container =
      ContainerManager::Instance()->GetContainer<ObstaclesContainer>(
          AdapterConfig::PERCEPTION_OBSTACLES);
  if (obstacles_container == nullptr) {
    AERROR << "Null obstacles container found";
    return;
  }
  Obstacle* ego_vehicle =
      obstacles_container->GetObstacle(FLAGS_ego_vehicle_id);
  if (ego_vehicle == nullptr) {
    AERROR << "Ego vehicle not found";
    return;
  }
  for (const int id :
       obstacles_container->curr_frame_predictable_obstacle_ids()) {
    Obstacle* obstacle_ptr = obstacles_container->GetObstacle(id);
    if (obstacle_ptr != nullptr && obstacle_ptr->IsInJunction(junction_id)) {
      obstacle_ptr->SetCaution();
      ADEBUG << "SetCaution for obstacle [" << obstacle_ptr->id() << "]";
    }
  }
  AssignCautionLevelByEgoReferenceLine();
}

void ObstaclesPrioritizer::AssignCautionLevelByEgoReferenceLine() {
  ADCTrajectoryContainer* adc_trajectory_container =
      ContainerManager::Instance()->GetContainer<ADCTrajectoryContainer>(
          AdapterConfig::PLANNING_TRAJECTORY);
  const std::vector<std::string>& lane_ids =
      adc_trajectory_container->GetADCLaneIDSequence();
  if (lane_ids.empty()) {
    return;
  }

  double accumulated_s = 0.0;
  for (const std::string& lane_id : lane_ids) {
    std::shared_ptr<const LaneInfo> lane_info_ptr =
        PredictionMap::LaneById(lane_id);
    if (lane_info_ptr == nullptr) {
      AERROR << "Null lane info pointer found.";
      continue;
    }
    accumulated_s += lane_info_ptr->total_length();
    AssignCautionByMerge(lane_info_ptr);
    AssignCautionByOverlap(lane_info_ptr);
    if (accumulated_s > FLAGS_caution_search_distance_ahead) {
      break;
    }
  }
}

void ObstaclesPrioritizer::AssignCautionByMerge(
    std::shared_ptr<const LaneInfo> lane_info_ptr) {
  SetCautionBackward(lane_info_ptr,
                     FLAGS_caution_search_distance_backward_for_merge);
}

void ObstaclesPrioritizer::AssignCautionByOverlap(
    std::shared_ptr<const LaneInfo> lane_info_ptr) {
  std::string lane_id = lane_info_ptr->id().id();
  const std::vector<std::shared_ptr<const OverlapInfo>> cross_lanes_ =
      lane_info_ptr->cross_lanes();
  for (const auto overlap_ptr : cross_lanes_) {
    for (const auto& object : overlap_ptr->overlap().object()) {
      const auto& object_id = object.id().id();
      if (object_id == lane_info_ptr->id().id()) {
        continue;
      } else {
        std::shared_ptr<const LaneInfo> overlap_lane_ptr =
            PredictionMap::LaneById(object_id);
        SetCautionBackward(overlap_lane_ptr,
                           FLAGS_caution_search_distance_backward_for_overlap);
      }
    }
  }
}

void ObstaclesPrioritizer::SetCautionBackward(
    std::shared_ptr<const LaneInfo> start_lane_info_ptr,
    const double max_distance) {
  ObstaclesContainer* obstacles_container =
      ContainerManager::Instance()->GetContainer<ObstaclesContainer>(
          AdapterConfig::PERCEPTION_OBSTACLES);
  std::unordered_map<std::string, std::vector<LaneObstacle>> lane_obstacles =
      ObstacleClusters::GetLaneObstacles();
  std::queue<std::pair<ConstLaneInfoPtr, double>> lane_info_queue;
  lane_info_queue.emplace(start_lane_info_ptr,
                          start_lane_info_ptr->total_length());
  while (!lane_info_queue.empty()) {
    ConstLaneInfoPtr curr_lane = lane_info_queue.front().first;
    double cumu_distance = lane_info_queue.front().second;
    lane_info_queue.pop();
    const std::string& lane_id = curr_lane->id().id();
    std::unordered_set<std::string> visited_lanes;
    if (visited_lanes.find(lane_id) == visited_lanes.end() &&
        lane_obstacles.find(lane_id) != lane_obstacles.end() &&
        !lane_obstacles[lane_id].empty()) {
      visited_lanes.insert(lane_id);
      // find the obstacle with largest lane_s on the lane
      int obstacle_id = lane_obstacles[lane_id].front().obstacle_id();
      double obstacle_s = lane_obstacles[lane_id].front().lane_s();
      for (const LaneObstacle& lane_obstacle : lane_obstacles[lane_id]) {
        if (lane_obstacle.lane_s() > obstacle_s) {
          obstacle_id = lane_obstacle.obstacle_id();
          obstacle_s = lane_obstacle.lane_s();
        }
      }
      Obstacle* obstacle_ptr = obstacles_container->GetObstacle(obstacle_id);
      if (obstacle_ptr == nullptr) {
        AERROR << "Obstacle [" << obstacle_id << "] Not found";
        continue;
      }
      obstacle_ptr->SetCaution();
      continue;
    }
    if (cumu_distance > max_distance) {
      continue;
    }
    for (const auto& pre_lane_id : curr_lane->lane().predecessor_id()) {
      ConstLaneInfoPtr pre_lane_ptr = PredictionMap::LaneById(pre_lane_id.id());
      lane_info_queue.emplace(pre_lane_ptr,
                              cumu_distance + pre_lane_ptr->total_length());
    }
  }
}

}  // namespace prediction
}  // namespace apollo
