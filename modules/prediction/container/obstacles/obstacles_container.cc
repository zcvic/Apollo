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

#include "modules/prediction/container/obstacles/obstacles_container.h"

#include <utility>
#include <unordered_set>

#include "modules/prediction/common/feature_output.h"
#include "modules/prediction/common/junction_analyzer.h"
#include "modules/prediction/common/prediction_gflags.h"
#include "modules/prediction/common/prediction_system_gflags.h"
#include "modules/prediction/container/obstacles/obstacle_clusters.h"

namespace apollo {
namespace prediction {

using apollo::perception::PerceptionObstacle;
using apollo::perception::PerceptionObstacles;


ObstaclesContainer::ObstaclesContainer()
    : obstacles_(FLAGS_max_num_obstacles),
      id_mapping_(FLAGS_max_num_obstacles) {}

// This is called by Perception module at every frame to insert all
// detected obstacles.
void ObstaclesContainer::Insert(const ::google::protobuf::Message& message) {
  // Clean up the history and get the PerceptionObstacles
  curr_frame_predictable_obstacle_ids_.clear();
  curr_frame_id_mapping_.clear();
  PerceptionObstacles perception_obstacles;
  perception_obstacles.CopyFrom(
      dynamic_cast<const PerceptionObstacles&>(message));

  // Get the new timestamp and update it in the class
  // - If it's more than 10sec later than the most recent one, clear the
  //   obstacle history.
  // - If it's not a valid time (earlier than history), continue.
  // - Also consider the offline_mode case.

  double timestamp = 0.0;
  if (perception_obstacles.has_header() &&
      perception_obstacles.header().has_timestamp_sec()) {
    timestamp = perception_obstacles.header().timestamp_sec();
  }
  if (std::fabs(timestamp - timestamp_) > FLAGS_replay_timestamp_gap) {
    obstacles_.Clear();
    ADEBUG << "Replay mode is enabled.";
  } else if (timestamp <= timestamp_) {
    AERROR << "Invalid timestamp curr [" << timestamp << "] v.s. prev ["
           << timestamp_ << "].";
    return;
  }
  if (FLAGS_prediction_offline_mode) {
    if (std::fabs(timestamp - timestamp_) > FLAGS_replay_timestamp_gap ||
        FeatureOutput::Size() > FLAGS_max_num_dump_feature) {
      FeatureOutput::Write();
    }
  }
  timestamp_ = timestamp;
  ADEBUG << "Current timestamp is [" << timestamp_ << "]";

  // Prediction tracking adaptation
  if (FLAGS_enable_tracking_adaptation) {
    BuildCurrentFrameIdMapping(perception_obstacles);
  }

  // Set up the ObstacleClusters:
  // 1. Initialize ObstacleClusters
  ObstacleClusters::Init();
  // 2. Insert the Obstacles one by one
  for (const PerceptionObstacle& perception_obstacle :
       perception_obstacles.perception_obstacle()) {
    ADEBUG << "Perception obstacle [" << perception_obstacle.id() << "] "
           << "was detected";
    InsertPerceptionObstacle(perception_obstacle, timestamp_);
    ADEBUG << "Perception obstacle [" << perception_obstacle.id() << "] "
           << "was inserted";
  }
  // 3. Sort the Obstacles
  ObstacleClusters::SortObstacles();
  // 4. Deduct the NearbyObstacles info. from the sorted Obstacles
  for (const PerceptionObstacle& perception_obstacle :
       perception_obstacles.perception_obstacle()) {
    if (IsPredictable(perception_obstacle)) {
      continue;
    }
    Obstacle* obstacle_ptr = GetObstacle(perception_obstacle.id());
    if (obstacle_ptr == nullptr) {
      continue;
    }
    obstacle_ptr->SetNearbyObstacles();
  }
}


Obstacle* ObstaclesContainer::GetObstacle(const int id) {
  return obstacles_.GetSilently(id);
}


const std::vector<int>&
ObstaclesContainer::GetCurrentFramePredictableObstacleIds() const {
  return curr_frame_predictable_obstacle_ids_;
}


void ObstaclesContainer::Clear() {
  obstacles_.Clear();
  id_mapping_.Clear();
  timestamp_ = -1.0;
}


void ObstaclesContainer::InsertPerceptionObstacle(
    const PerceptionObstacle& perception_obstacle, const double timestamp) {
  // Sanity checks.
  int prediction_id = perception_obstacle.id();
  if (curr_frame_id_mapping_.find(prediction_id) !=
      curr_frame_id_mapping_.end()) {
    prediction_id = curr_frame_id_mapping_[prediction_id];
    if (prediction_id != perception_obstacle.id()) {
      ADEBUG << "Obstacle have got AdaptTracking, with perception_id: "
             << perception_obstacle.id()
             << ", and prediction_id: " << prediction_id;
    }
  }
  if (prediction_id < -1) {
    AERROR << "Invalid ID [" << prediction_id << "]";
    return;
  }
  if (!IsPredictable(perception_obstacle)) {
    ADEBUG << "Perception obstacle [" << prediction_id
           << "] is not predictable.";
    return;
  }

  // Insert the obstacle and also update the LRUCache.
  curr_frame_predictable_obstacle_ids_.push_back(prediction_id);
  Obstacle* obstacle_ptr = obstacles_.Get(prediction_id);
  if (obstacle_ptr != nullptr) {
    obstacle_ptr->Insert(perception_obstacle, timestamp, prediction_id);
    ADEBUG << "Refresh obstacle [" << prediction_id << "]";
  } else {
    Obstacle obstacle;
    obstacle.Insert(perception_obstacle, timestamp, prediction_id);
    obstacles_.Put(prediction_id, std::move(obstacle));
    ADEBUG << "Insert obstacle [" << prediction_id << "]";
  }
}



void ObstaclesContainer::BuildCurrentFrameIdMapping(
    const PerceptionObstacles& perception_obstacles) {
  // Go through every obstacle in the current frame, after some
  // sanity checks, build current_frame_id_mapping for every obstacle

  std::unordered_set<int> seen_perception_ids;
  // Loop all precept_id and find those in obstacles_LRU
  for (const PerceptionObstacle& perception_obstacle :
       perception_obstacles.perception_obstacle()) {
    int perception_id = perception_obstacle.id();
    if (GetObstacle(perception_id) != nullptr) {
      seen_perception_ids.insert(perception_id);
    }
  }

  for (const PerceptionObstacle& perception_obstacle :
       perception_obstacles.perception_obstacle()) {
    int perception_id = perception_obstacle.id();
    curr_frame_id_mapping_[perception_id] = perception_id;
    if (seen_perception_ids.find(perception_id) != seen_perception_ids.end()) {
      // find this perception_id in LRUCache, treat it as a tracked obstacle
      continue;
    }
    std::unordered_set<int> seen_prediction_ids;
    int prediction_id = 0;
    if (id_mapping_.GetCopy(perception_id, &prediction_id)) {
      if (seen_perception_ids.find(prediction_id) ==
          seen_perception_ids.end()) {
        // find this perception_id in LRUMapping, map it to a tracked obstacle
        curr_frame_id_mapping_[perception_id] = prediction_id;
        seen_prediction_ids.insert(prediction_id);
      }
    } else {  // process adaption
      common::util::Node<int, Obstacle>* curr = obstacles_.First();
      while (curr != nullptr) {
        int obs_id = curr->key;
        curr = curr->next;
        if (seen_perception_ids.find(obs_id) != seen_perception_ids.end() ||
            seen_prediction_ids.find(obs_id) != seen_prediction_ids.end()) {
          // this obs_id has already been processed
          continue;
        }
        Obstacle* obstacle_ptr = obstacles_.GetSilently(obs_id);
        if (obstacle_ptr == nullptr) {
          AERROR << "Obstacle id [" << obs_id << "] with empty obstacle_ptr.";
          break;
        }
        if (timestamp_ - obstacle_ptr->timestamp() > 0.5) {
          ADEBUG << "Obstacle already reach time threshold.";
          break;
        }
        if (AdaptTracking(perception_obstacle, obstacle_ptr)) {
          id_mapping_.Put(perception_id, obs_id);
          curr_frame_id_mapping_[perception_id] = obs_id;
          break;
        }
      }
    }
  }
}


void ObstaclesContainer::BuildLaneGraph() {
  // Go through every obstacle in the current frame, after some
  // sanity checks, build lane graph for non-junction cases.
  for (const int id : curr_frame_predictable_obstacle_ids_) {
    Obstacle* obstacle_ptr = obstacles_.GetSilently(id);
    if (obstacle_ptr == nullptr) {
      AERROR << "Null obstacle found.";
      continue;
    }
    if (obstacle_ptr->ToIgnore()) {
      ADEBUG << "Ignore obstacle [" << obstacle_ptr->id() << "]";
      continue;
    }
    obstacle_ptr->BuildLaneGraph();
  }
}


void ObstaclesContainer::BuildJunctionFeature() {
  // Go through every obstacle in the current frame, after some
  // sanit checks, build junction features for those that are in junction.
  for (const int id : curr_frame_predictable_obstacle_ids_) {
    Obstacle* obstacle_ptr = obstacles_.GetSilently(id);
    if (obstacle_ptr == nullptr) {
      AERROR << "Null obstacle found.";
      continue;
    }
    if (obstacle_ptr->ToIgnore()) {
      ADEBUG << "Ignore obstacle [" << obstacle_ptr->id() << "]";
      continue;
    }
    const std::string& junction_id = JunctionAnalyzer::GetJunctionId();
    if (obstacle_ptr->IsInJunction(junction_id)) {
      ADEBUG << "Build junction feature for obstacle [" << obstacle_ptr->id()
            << "] in junction [" << junction_id << "]";
      obstacle_ptr->BuildJunctionFeature();
    }
  }
}


bool ObstaclesContainer::AdaptTracking(
    const PerceptionObstacle& perception_obstacle, Obstacle* obstacle_ptr) {
  if (!perception_obstacle.has_type() ||
      perception_obstacle.type() != obstacle_ptr->type()) {
    // different obstacle type, can't be same obstacle
    return false;
  }
  // test perception_obstacle position with possible obstacle position
  if (perception_obstacle.has_position() &&
      perception_obstacle.position().has_x() &&
      perception_obstacle.position().has_y()) {
    double obs_x = obstacle_ptr->latest_feature().position().x() + (timestamp_ -
                   obstacle_ptr->latest_feature().timestamp()) *
                   obstacle_ptr->latest_feature().raw_velocity().x();
    double obs_y = obstacle_ptr->latest_feature().position().y() + (timestamp_ -
                   obstacle_ptr->latest_feature().timestamp()) *
                   obstacle_ptr->latest_feature().raw_velocity().y();
    double dist = std::hypot(perception_obstacle.position().x() - obs_x,
                             perception_obstacle.position().y() - obs_y);
    if (dist < 3.0) {
      return true;
    }
  }
  return false;
}


bool ObstaclesContainer::IsPredictable(
    const PerceptionObstacle& perception_obstacle) {
  if (!perception_obstacle.has_type() ||
      perception_obstacle.type() == PerceptionObstacle::UNKNOWN_UNMOVABLE) {
    return false;
  }
  return true;
}

}  // namespace prediction
}  // namespace apollo
