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

#include "modules/dreamview/backend/simulation_world/simulation_world_service.h"

#include <algorithm>
#include <chrono>
#include <unordered_set>
#include <vector>

#include "google/protobuf/util/json_util.h"
#include "modules/canbus/proto/chassis.pb.h"
#include "modules/common/math/quaternion.h"
#include "modules/common/proto/geometry.pb.h"
#include "modules/common/proto/vehicle_signal.pb.h"
#include "modules/common/time/time.h"
#include "modules/common/util/file.h"
#include "modules/common/util/points_downsampler.h"
#include "modules/common/util/util.h"
#include "modules/dreamview/backend/common/dreamview_gflags.h"
#include "modules/dreamview/backend/util/trajectory_point_collector.h"
#include "modules/dreamview/proto/simulation_world.pb.h"
#include "modules/localization/proto/localization.pb.h"
#include "modules/planning/proto/planning.pb.h"
#include "modules/planning/proto/planning_internal.pb.h"
#include "modules/prediction/proto/prediction_obstacle.pb.h"

namespace apollo {
namespace dreamview {

using apollo::canbus::Chassis;
using apollo::common::Point3D;
using apollo::common::TrajectoryPoint;
using apollo::common::VehicleConfigHelper;
using apollo::common::adapter::AdapterManager;
using apollo::common::adapter::ChassisAdapter;
using apollo::common::adapter::LocalizationAdapter;
using apollo::common::adapter::MonitorAdapter;
using apollo::common::adapter::PerceptionObstaclesAdapter;
using apollo::common::adapter::PlanningAdapter;
using apollo::common::monitor::MonitorMessage;
using apollo::common::monitor::MonitorMessageItem;
using apollo::common::time::Clock;
using apollo::common::time::ToSecond;
using apollo::common::time::millis;
using apollo::common::util::DownsampleByAngle;
using apollo::common::util::GetProtoFromFile;
using apollo::hdmap::Path;
using apollo::localization::LocalizationEstimate;
using apollo::perception::PerceptionObstacle;
using apollo::perception::PerceptionObstacles;
using apollo::planning::ADCTrajectory;
using apollo::planning::DecisionResult;
using apollo::planning::StopReasonCode;
using apollo::planning_internal::PlanningData;
using apollo::prediction::PredictionObstacle;
using apollo::prediction::PredictionObstacles;
using apollo::routing::RoutingResponse;

using Json = nlohmann::json;

namespace {

double CalculateAcceleration(const Point3D &acceleration,
                             const Point3D &velocity) {
  // Calculates the dot product of acceleration and velocity. The sign
  // of this projection indicates whether this is acceleration or
  // deceleration.
  double projection =
      acceleration.x() * velocity.x() + acceleration.y() * velocity.y();

  // Calculates the magnitude of the acceleration. Negate the value if
  // it is indeed a deceleration.
  double magnitude = std::hypot(acceleration.x(), acceleration.y());
  return std::signbit(projection) ? -magnitude : magnitude;
}

Object::DisengageType DeduceDisengageType(const Chassis &chassis) {
  if (chassis.error_code() != Chassis::NO_ERROR) {
    return Object::DISENGAGE_CHASSIS_ERROR;
  }

  switch (chassis.driving_mode()) {
    case Chassis::COMPLETE_AUTO_DRIVE:
      return Object::DISENGAGE_NONE;
    case Chassis::COMPLETE_MANUAL:
      return Object::DISENGAGE_MANUAL;
    case Chassis::AUTO_STEER_ONLY:
      return Object::DISENGAGE_AUTO_STEER_ONLY;
    case Chassis::AUTO_SPEED_ONLY:
      return Object::DISENGAGE_AUTO_SPEED_ONLY;
    case Chassis::EMERGENCY_MODE:
      return Object::DISENGAGE_EMERGENCY;
    default:
      return Object::DISENGAGE_UNKNOWN;
  }
}

void SetObstacleInfo(const PerceptionObstacle &obstacle, Object *world_object) {
  if (world_object == nullptr) {
    return;
  }

  world_object->set_id(std::to_string(obstacle.id()));
  world_object->set_position_x(obstacle.position().x());
  world_object->set_position_y(obstacle.position().y());
  world_object->set_heading(obstacle.theta());
  world_object->set_length(obstacle.length());
  world_object->set_width(obstacle.width());
  world_object->set_height(obstacle.height());
  world_object->set_speed(
      std::hypot(obstacle.velocity().x(), obstacle.velocity().y()));
  world_object->set_speed_heading(
      std::atan2(obstacle.velocity().y(), obstacle.velocity().x()));
  world_object->set_timestamp_sec(obstacle.timestamp());
  world_object->set_confidence(obstacle.confidence());
}

void SetObstaclePolygon(const PerceptionObstacle &obstacle,
                        Object *world_object) {
  if (world_object == nullptr) {
    return;
  }

  using apollo::common::util::PairHash;
  std::unordered_set<std::pair<double, double>, PairHash> seen_points;
  world_object->clear_polygon_point();
  for (const auto &point : obstacle.polygon_point()) {
    // Filter out duplicate xy pairs.
    std::pair<double, double> xy_pair = {point.x(), point.y()};
    if (seen_points.count(xy_pair) == 0) {
      PolygonPoint *poly_pt = world_object->add_polygon_point();
      poly_pt->set_x(point.x());
      poly_pt->set_y(point.y());
      seen_points.insert(xy_pair);
    }
  }
}

void SetObstacleType(const PerceptionObstacle &obstacle, Object *world_object) {
  if (world_object == nullptr) {
    return;
  }

  switch (obstacle.type()) {
    case PerceptionObstacle::UNKNOWN:
      world_object->set_type(Object_Type_UNKNOWN);
      break;
    case PerceptionObstacle::UNKNOWN_MOVABLE:
      world_object->set_type(Object_Type_UNKNOWN_MOVABLE);
      break;
    case PerceptionObstacle::UNKNOWN_UNMOVABLE:
      world_object->set_type(Object_Type_UNKNOWN_UNMOVABLE);
      break;
    case PerceptionObstacle::PEDESTRIAN:
      world_object->set_type(Object_Type_PEDESTRIAN);
      break;
    case PerceptionObstacle::BICYCLE:
      world_object->set_type(Object_Type_BICYCLE);
      break;
    case PerceptionObstacle::VEHICLE:
      world_object->set_type(Object_Type_VEHICLE);
      break;
    default:
      world_object->set_type(Object_Type_VIRTUAL);
  }
}

void SetStopReason(const StopReasonCode &reason_code, Decision *decision) {
  switch (reason_code) {
    case StopReasonCode::STOP_REASON_HEAD_VEHICLE:
      decision->set_stopreason(Decision::STOP_REASON_HEAD_VEHICLE);
      break;
    case StopReasonCode::STOP_REASON_DESTINATION:
      decision->set_stopreason(Decision::STOP_REASON_DESTINATION);
      break;
    case StopReasonCode::STOP_REASON_PEDESTRIAN:
      decision->set_stopreason(Decision::STOP_REASON_PEDESTRIAN);
      break;
    case StopReasonCode::STOP_REASON_OBSTACLE:
      decision->set_stopreason(Decision::STOP_REASON_OBSTACLE);
      break;
    case StopReasonCode::STOP_REASON_SIGNAL:
      decision->set_stopreason(Decision::STOP_REASON_SIGNAL);
      break;
    case StopReasonCode::STOP_REASON_STOP_SIGN:
      decision->set_stopreason(Decision::STOP_REASON_STOP_SIGN);
      break;
    case StopReasonCode::STOP_REASON_YIELD_SIGN:
      decision->set_stopreason(Decision::STOP_REASON_YIELD_SIGN);
      break;
    case StopReasonCode::STOP_REASON_CLEAR_ZONE:
      decision->set_stopreason(Decision::STOP_REASON_CLEAR_ZONE);
      break;
    case StopReasonCode::STOP_REASON_CROSSWALK:
      decision->set_stopreason(Decision::STOP_REASON_CROSSWALK);
      break;
    default:
      AWARN << "Unrecognizable stop reason code:" << reason_code;
  }
}

void UpdateTurnSignal(const apollo::common::VehicleSignal &signal,
                      Object *auto_driving_car) {
  if (signal.turn_signal() == apollo::common::VehicleSignal::TURN_LEFT) {
    auto_driving_car->set_current_signal("LEFT");
  } else if (signal.turn_signal() ==
             apollo::common::VehicleSignal::TURN_RIGHT) {
    auto_driving_car->set_current_signal("RIGHT");
  } else if (signal.emergency_light()) {
    auto_driving_car->set_current_signal("EMERGENCY");
  } else {
    auto_driving_car->set_current_signal("");
  }
}

bool LocateMarker(const apollo::planning::ObjectDecisionType &decision,
                  Decision *world_decision) {
  apollo::common::PointENU fence_point;
  double heading;
  if (decision.has_stop() && decision.stop().has_stop_point()) {
    world_decision->set_type(Decision_Type_STOP);
    fence_point = decision.stop().stop_point();
    heading = decision.stop().stop_heading();
  } else if (decision.has_follow() && decision.follow().has_fence_point()) {
    world_decision->set_type(Decision_Type_FOLLOW);
    fence_point = decision.follow().fence_point();
    heading = decision.follow().fence_heading();
  } else if (decision.has_yield() && decision.yield().has_fence_point()) {
    world_decision->set_type(Decision_Type_YIELD);
    fence_point = decision.yield().fence_point();
    heading = decision.yield().fence_heading();
  } else if (decision.has_overtake() && decision.overtake().has_fence_point()) {
    world_decision->set_type(Decision_Type_OVERTAKE);
    fence_point = decision.overtake().fence_point();
    heading = decision.overtake().fence_heading();
  } else {
    return false;
  }

  world_decision->set_position_x(fence_point.x());
  world_decision->set_position_y(fence_point.y());
  world_decision->set_heading(heading);
  return true;
}

void FindNudgeRegion(const apollo::planning::ObjectDecisionType &decision,
                     const Object &world_obj, Decision *world_decision) {
  std::vector<apollo::common::math::Vec2d> points;
  for (auto &polygon_pt : world_obj.polygon_point()) {
    points.emplace_back(polygon_pt.x(), polygon_pt.y());
  }
  const apollo::common::math::Polygon2d obj_polygon(points);
  const apollo::common::math::Polygon2d &nudge_polygon =
      obj_polygon.ExpandByDistance(std::fabs(decision.nudge().distance_l()));
  const std::vector<apollo::common::math::Vec2d> &nudge_points =
      nudge_polygon.points();
  for (auto &nudge_pt : nudge_points) {
    PolygonPoint *poly_pt = world_decision->add_polygon_point();
    poly_pt->set_x(nudge_pt.x());
    poly_pt->set_y(nudge_pt.y());
  }
  world_decision->set_type(Decision_Type_NUDGE);
}

void CreatePredictionTrajectory(Object *world_object,
                                const PredictionObstacle &obstacle) {
  for (const auto &traj : obstacle.trajectory()) {
    Prediction *prediction = world_object->add_prediction();
    prediction->set_probability(traj.probability());
    for (const auto &point : traj.trajectory_point()) {
      PolygonPoint *world_point = prediction->add_predicted_trajectory();
      world_point->set_x(point.path_point().x());
      world_point->set_y(point.path_point().y());
      world_point->set_z(point.path_point().z());
    }
  }
}

}  // namespace

constexpr int SimulationWorldService::kMaxMonitorItems;

SimulationWorldService::SimulationWorldService(const MapService *map_service,
                                               bool routing_from_file)
    : map_service_(map_service),
      monitor_(apollo::common::monitor::MonitorMessageItem::SIMULATOR) {
  RegisterMessageCallbacks();
  if (routing_from_file) {
    ReadRoutingFromFile(FLAGS_routing_response_file);
  }

  // Populate vehicle parameters.
  Object *auto_driving_car = world_.mutable_auto_driving_car();
  const auto &vehicle_param = VehicleConfigHelper::GetConfig().vehicle_param();
  auto_driving_car->set_height(vehicle_param.height());
  auto_driving_car->set_width(vehicle_param.width());
  auto_driving_car->set_length(vehicle_param.length());
}

void SimulationWorldService::Update() {
  if (to_clear_) {
    // Clears received data.
    AdapterManager::GetChassis()->ClearData();
    AdapterManager::GetLocalization()->ClearData();
    AdapterManager::GetPerceptionObstacles()->ClearData();
    AdapterManager::GetPlanning()->ClearData();
    AdapterManager::GetPrediction()->ClearData();
    AdapterManager::GetRoutingResponse()->ClearData();
    AdapterManager::GetMonitor()->ClearData();

    // Clears simulation world except for the car information.
    auto car = world_.auto_driving_car();
    world_.Clear();
    *world_.mutable_auto_driving_car() = car;
    to_clear_ = false;
  }

  AdapterManager::Observe();
  UpdateWithLatestObserved("Chassis", AdapterManager::GetChassis());
  UpdateWithLatestObserved("Localization", AdapterManager::GetLocalization());

  // Clear objects received from last frame and populate with the new objects.
  // TODO(siyangy, unacao): For now we are assembling the simulation_world with
  // latest received perception, prediction and planning message. However, they
  // may not always be perfectly aligned and belong to the same frame.
  obj_map_.clear();
  world_.clear_object();
  UpdateWithLatestObserved("Perception",
                           AdapterManager::GetPerceptionObstacles());
  UpdateWithLatestObserved("PredictionObstacles",
                           AdapterManager::GetPrediction());
  UpdateWithLatestObserved("Planning", AdapterManager::GetPlanning());
  for (const auto &kv : obj_map_) {
    *world_.add_object() = kv.second;
  }

  UpdateDelays();

  world_.set_sequence_num(world_.sequence_num() + 1);
}

void SimulationWorldService::UpdateDelays() {
  auto *delays = world_.mutable_delay();
  delays->set_chassis(AdapterManager::GetChassis()->GetDelayInMs());
  delays->set_localization(AdapterManager::GetLocalization()->GetDelayInMs());
  delays->set_perception_obstacle(
      AdapterManager::GetPerceptionObstacles()->GetDelayInMs());
  delays->set_planning(AdapterManager::GetPlanning()->GetDelayInMs());
  delays->set_prediction(AdapterManager::GetPrediction()->GetDelayInMs());
  // TODO(siyangy): Add traffic light delay when ready.
}

Json SimulationWorldService::GetUpdateAsJson(double radius) const {
  std::string sim_world_json;
  ::google::protobuf::util::MessageToJsonString(world_, &sim_world_json);

  Json update = GetMapElements(radius);
  update["type"] = "SimWorldUpdate";
  update["timestamp"] = apollo::common::time::AsInt64<millis>(Clock::Now());
  update["world"] = Json::parse(sim_world_json);

  return update;
}

Json SimulationWorldService::GetMapElements(double radius) const {
  // Gather required map element ids based on current location.
  apollo::common::PointENU point;
  point.set_x(world_.auto_driving_car().position_x());
  point.set_y(world_.auto_driving_car().position_y());

  MapElementIds map_element_ids =
      map_service_->CollectMapElementIds(point, radius);

  Json map;
  map["mapElementIds"] = map_element_ids.Json();
  map["mapHash"] = map_element_ids.Hash();
  map["mapRadius"] = radius;

  return map;
}

template <>
void SimulationWorldService::UpdateSimulationWorld(
    const MonitorMessage &monitor_msg) {
  std::vector<MonitorMessageItem> updated;
  updated.reserve(SimulationWorldService::kMaxMonitorItems);
  // Save the latest messages at the top of the history.
  int remove_size =
      monitor_msg.item_size() > SimulationWorldService::kMaxMonitorItems
          ? monitor_msg.item_size() - SimulationWorldService::kMaxMonitorItems
          : 0;
  std::copy(monitor_msg.item().begin(),
            std::prev(monitor_msg.item().end(), remove_size),
            std::back_inserter(updated));

  // Copy over the previous messages until there is no more history or
  // the max number of total messages has been hit.
  auto history = world_.monitor().item();
  remove_size = history.size() + monitor_msg.item_size() -
                SimulationWorldService::kMaxMonitorItems;
  if (remove_size < 0) {
    remove_size = 0;
  }
  std::copy(history.begin(), std::prev(history.end(), remove_size),
            std::back_inserter(updated));

  // Refresh the monitor message list in simulation_world.
  ::google::protobuf::RepeatedPtrField<MonitorMessageItem> items(
      updated.begin(), updated.end());
  world_.mutable_monitor()->mutable_item()->Swap(&items);
  world_.mutable_monitor()->mutable_header()->set_timestamp_sec(
      ToSecond(Clock::Now()));
}

template <>
void SimulationWorldService::UpdateSimulationWorld(
    const LocalizationEstimate &localization) {
  Object *auto_driving_car = world_.mutable_auto_driving_car();
  const auto &pose = localization.pose();

  // Updates position with the input localization message.
  auto_driving_car->set_position_x(pose.position().x());
  auto_driving_car->set_position_y(pose.position().y());
  auto_driving_car->set_heading(pose.heading());

  // Updates acceleration with the input localization message.
  auto_driving_car->set_speed_acceleration(CalculateAcceleration(
      pose.linear_acceleration(), pose.linear_velocity()));

  // Updates the timestamp with the timestamp inside the localization
  // message header. It is done on both the SimulationWorld object
  // itself and its auto_driving_car() field.
  auto_driving_car->set_timestamp_sec(localization.header().timestamp_sec());
  world_.set_timestamp_sec(
      std::max(world_.timestamp_sec(), localization.header().timestamp_sec()));
}

template <>
void SimulationWorldService::UpdateSimulationWorld(const Chassis &chassis) {
  Object *auto_driving_car = world_.mutable_auto_driving_car();

  auto_driving_car->set_speed(chassis.speed_mps());
  auto_driving_car->set_throttle_percentage(chassis.throttle_percentage());
  auto_driving_car->set_brake_percentage(chassis.brake_percentage());

  // In case of out-of-range percentages, reduces it to zero.
  int angle_percentage = chassis.steering_percentage();
  if (angle_percentage > 100 || angle_percentage < -100) {
    angle_percentage = 0;
  }
  auto_driving_car->set_steering_angle(angle_percentage);

  UpdateTurnSignal(chassis.signal(), auto_driving_car);

  auto_driving_car->set_disengage_type(DeduceDisengageType(chassis));

  // Updates the timestamp with the timestamp inside the chassis message header.
  world_.set_timestamp_sec(chassis.header().timestamp_sec());
}

Object &SimulationWorldService::CreateWorldObjectIfAbsent(
    const PerceptionObstacle &obstacle) {
  const std::string id = std::to_string(obstacle.id());
  // Create a new world object and put it into object map if the id does not
  // exist in the map yet.
  if (obj_map_.find(id) == obj_map_.end()) {
    Object &world_obj = obj_map_[id];
    SetObstacleInfo(obstacle, &world_obj);
    SetObstaclePolygon(obstacle, &world_obj);
    SetObstacleType(obstacle, &world_obj);
  }
  return obj_map_[id];
}

template <>
void SimulationWorldService::UpdateSimulationWorld(
    const PerceptionObstacles &obstacles) {
  for (const auto &obstacle : obstacles.perception_obstacle()) {
    CreateWorldObjectIfAbsent(obstacle);
  }
  world_.set_timestamp_sec(
      std::max(world_.timestamp_sec(), obstacles.header().timestamp_sec()));
}

void SimulationWorldService::UpdatePlanningTrajectory(
    const ADCTrajectory &trajectory) {
  const double cutoff_time = world_.auto_driving_car().timestamp_sec();
  const double header_time = trajectory.header().timestamp_sec();

  util::TrajectoryPointCollector collector(&world_);

  size_t i = 0;
  const size_t trajectory_length = trajectory.trajectory_point_size();
  bool collecting_started = false;
  while (i < trajectory_length) {
    const TrajectoryPoint &point = trajectory.trajectory_point(i);
    // Trajectory points with a timestamp older than the cutoff time
    // (which is effectively the timestamp of the most up-to-date
    // localization/chassis message) will be dropped.
    //
    // Note that the last two points are always included.
    if (collecting_started ||
        point.relative_time() + header_time >= cutoff_time) {
      collecting_started = true;
      collector.Collect(point);
      if (i == trajectory_length - 1) {
        // Break if the very last point is collected.
        break;
      } else if (i == trajectory_length - 2) {
        // Move on to the last point if the last but one is collected.
        i = trajectory_length - 1;
      } else if (i < trajectory_length - 2) {
        // When collecting the trajectory points, downsample with a
        // ratio of 10.
        constexpr double downsample_ratio = 10;
        i += downsample_ratio;
        if (i > trajectory_length - 2) {
          i = trajectory_length - 2;
        }
      } else {
        break;
      }
    } else {
      ++i;
    }
  }
}

void SimulationWorldService::UpdateMainDecision(
    const apollo::planning::MainDecision &main_decision,
    double update_timestamp_sec, Object *world_main_stop) {
  apollo::common::math::Vec2d stop_pt;
  double stop_heading = 0.0;
  auto decision = world_main_stop->add_decision();
  decision->set_type(Decision::STOP);
  if (main_decision.has_not_ready()) {
    // The car is not ready!
    // Use the current ADC pose since it is better not to self-drive.
    stop_pt.set_x(world_.auto_driving_car().position_x());
    stop_pt.set_y(world_.auto_driving_car().position_y());
    stop_heading = world_.auto_driving_car().heading();
    decision->set_stopreason(Decision::STOP_REASON_NOT_READY);
  } else if (main_decision.has_estop()) {
    // Emergency stop.
    // Use the current ADC pose since it is better to stop immediately.
    stop_pt.set_x(world_.auto_driving_car().position_x());
    stop_pt.set_y(world_.auto_driving_car().position_y());
    stop_heading = world_.auto_driving_car().heading();
    decision->set_stopreason(Decision::STOP_REASON_EMERGENCY);
    world_.mutable_auto_driving_car()->set_current_signal("EMERGENCY");
  } else {
    // Normal stop.
    const apollo::planning::MainStop &stop = main_decision.stop();
    stop_pt.set_x(stop.stop_point().x());
    stop_pt.set_y(stop.stop_point().y());
    stop_heading = stop.stop_heading();
    if (stop.has_reason_code()) {
      SetStopReason(stop.reason_code(), decision);
    }
  }
  world_main_stop->set_position_x(stop_pt.x());
  world_main_stop->set_position_y(stop_pt.y());
  world_main_stop->set_heading(stop_heading);
  world_main_stop->set_timestamp_sec(update_timestamp_sec);
}

void SimulationWorldService::UpdateDecision(const DecisionResult &decision_res,
                                            double header_time) {
  // Update turn signal.
  UpdateTurnSignal(decision_res.vehicle_signal(),
                   world_.mutable_auto_driving_car());

  apollo::planning::MainDecision main_decision = decision_res.main_decision();

  // Update speed limit.
  if (main_decision.target_lane_size() > 0) {
    world_.set_speed_limit(main_decision.target_lane(0).speed_limit());
  }

  // Update relevant main stop with reason.
  world_.clear_main_stop();
  if (main_decision.has_not_ready() || main_decision.has_estop() ||
      main_decision.has_stop()) {
    Object *world_main_stop = world_.mutable_main_stop();
    UpdateMainDecision(main_decision, header_time, world_main_stop);
  }

  // Update obstacle decision.
  for (const auto &obj_decision : decision_res.object_decision().decision()) {
    if (obj_decision.has_perception_id()) {
      int id = obj_decision.perception_id();
      Object &world_obj = obj_map_[std::to_string(id)];
      if (!world_obj.has_type()) {
        world_obj.set_type(Object_Type_VIRTUAL);
        ADEBUG << id << " is not a current perception object";
      }

      for (const auto &decision : obj_decision.object_decision()) {
        Decision *world_decision = world_obj.add_decision();
        world_decision->set_type(Decision_Type_IGNORE);
        if (decision.has_stop() || decision.has_follow() ||
            decision.has_yield() || decision.has_overtake()) {
          if (!LocateMarker(decision, world_decision)) {
            AWARN << "No decision marker position found for object id=" << id;
            continue;
          }
        } else if (decision.has_nudge()) {
          if (world_obj.polygon_point_size() == 0) {
            if (world_obj.type() == Object_Type_VIRTUAL) {
              AWARN << "No current perception object with id=" << id
                    << " for nudge decision";
            } else {
              AWARN << "No polygon points found for object id=" << id;
            }
            continue;
          }
          FindNudgeRegion(decision, world_obj, world_decision);
        } else if (decision.has_sidepass()) {
          world_decision->set_type(Decision_Type_SIDEPASS);
        }
      }

      world_obj.set_timestamp_sec(
          std::max(world_obj.timestamp_sec(), header_time));
    }
  }
}

void SimulationWorldService::UpdatePlanningData(const PlanningData &data) {
  auto planning_data = world_.mutable_planning_data();
  planning_data->CopyFrom(data);
}

template <>
void SimulationWorldService::UpdateSimulationWorld(
    const ADCTrajectory &trajectory) {
  const double header_time = trajectory.header().timestamp_sec();

  UpdatePlanningTrajectory(trajectory);

  UpdateDecision(trajectory.decision(), header_time);

  UpdatePlanningData(trajectory.debug().planning_data());

  world_.set_timestamp_sec(std::max(world_.timestamp_sec(), header_time));
}

template <>
void SimulationWorldService::UpdateSimulationWorld(
    const PredictionObstacles &obstacles) {
  for (const auto &obstacle : obstacles.prediction_obstacle()) {
    // Note: There's a perfect one-to-one mapping between the perception
    // obstacles and prediction obstacles within the same frame. Creating a new
    // world object here is only possible when we happen to be processing a
    // percpetion and prediction message from two frames.
    auto &world_obj = CreateWorldObjectIfAbsent(obstacle.perception_obstacle());

    // Add prediction trajectory to the object.
    CreatePredictionTrajectory(&world_obj, obstacle);

    world_obj.set_timestamp_sec(
        std::max(obstacle.timestamp(), world_obj.timestamp_sec()));
  }
  world_.set_timestamp_sec(
      std::max(world_.timestamp_sec(), obstacles.header().timestamp_sec()));
}

template <>
void SimulationWorldService::UpdateSimulationWorld(
    const RoutingResponse &routing_response) {
  auto header_time = routing_response.header().timestamp_sec();

  std::vector<Path> paths;
  if (!map_service_->GetPathsFromRouting(routing_response, &paths)) {
    return;
  }

  world_.clear_route_path();
  world_.set_routing_time(header_time);

  for (const Path &path : paths) {
    // Downsample the path points for frontend display.
    // Angle threshold is about 5.72 degree.
    constexpr double angle_threshold = 0.1;
    std::vector<int> sampled_indices =
        DownsampleByAngle(path.path_points(), angle_threshold);

    RoutePath *route_path = world_.add_route_path();
    for (int index : sampled_indices) {
      const auto &path_point = path.path_points()[index];
      PolygonPoint *route_point = route_path->add_point();
      route_point->set_x(path_point.x());
      route_point->set_y(path_point.y());
    }
  }

  world_.set_timestamp_sec(std::max(world_.timestamp_sec(), header_time));
}

void SimulationWorldService::ReadRoutingFromFile(
    const std::string &routing_response_file) {
  RoutingResponse routing_response;
  if (!GetProtoFromFile(routing_response_file, &routing_response)) {
    AWARN << "Unable to read routing response from file: "
          << routing_response_file;
    return;
  }
  AINFO << "Loaded routing from " << routing_response_file;

  sleep(1);  // Wait to make sure the connection has been established before
             // publishing.
  AdapterManager::PublishRoutingResponse(routing_response);
  AINFO << "Published RoutingResponse read from file.";
}

void SimulationWorldService::RegisterMessageCallbacks() {
  AdapterManager::AddMonitorCallback(
      &SimulationWorldService::UpdateSimulationWorld, this);
  AdapterManager::AddRoutingResponseCallback(
      &SimulationWorldService::UpdateSimulationWorld, this);
}

}  // namespace dreamview
}  // namespace apollo
