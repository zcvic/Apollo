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

/**
 * @file reference_line_provider.cc
 *
 * @brief Implementation of the class ReferenceLineProvider.
 */

#include <utility>

#include "modules/planning/common/planning_gflags.h"
#include "modules/planning/reference_line/reference_line_provider.h"

/**
 * @namespace apollo::planning
 * @brief apollo::planning
 */
namespace apollo {
namespace planning {

ReferenceLineProvider::ReferenceLineProvider() {}

ReferenceLineProvider::~ReferenceLineProvider() {
  if (thread_ && thread_->joinable()) {
    thread_->join();
  }
}

void ReferenceLineProvider::Init(
    const hdmap::HDMap *hdmap_,
    const QpSplineReferenceLineSmootherConfig &smoother_config) {
  pnc_map_.reset(new hdmap::PncMap(hdmap_));
  smoother_config_ = smoother_config;
  std::vector<double> init_t_knots;
  spline_solver_.reset(new Spline2dSolver(init_t_knots, 1));
  is_initialized_ = true;
}

void ReferenceLineProvider::UpdateRoutingResponse(
    const routing::RoutingResponse &routing) {
  std::lock_guard<std::mutex> lock(pnc_map_mutex_);
  pnc_map_->UpdateRoutingResponse(routing);
  has_routing_ = true;
}

bool ReferenceLineProvider::UpdateVehicleStatus(
    const common::PointENU &position, double speed) {
  std::lock_guard<std::mutex> lock(pnc_map_mutex_);
  vehicle_speed_ = speed;
  if (!pnc_map_->UpdatePosition(position)) {
    AERROR << "Failed to update pnc_map position: "
           << position.ShortDebugString();
    return false;
  }
  return true;
}

bool ReferenceLineProvider::Start() {
  if (!is_initialized_) {
    AERROR << "ReferenceLineProvider has NOT been initiated.";
    return false;
  }
  thread_.reset(new std::thread(&ReferenceLineProvider::Generate, this));
  return true;
}

void ReferenceLineProvider::Stop() {
  is_stop_ = true;
  if (thread_ && thread_->joinable()) {
    thread_->join();
  }
}

void ReferenceLineProvider::Generate() {
  while (!is_stop_) {
    if (!has_routing_) {
      AERROR << "Routing is not ready.";
      constexpr int32_t kRoutingNotReadySleepTimeMs = 500;  // milliseconds
      std::this_thread::sleep_for(std::chrono::duration<double, std::milli>(
          kRoutingNotReadySleepTimeMs));
      continue;
    }
    if (!CreateReferenceLineFromRouting()) {
      AERROR << "Fail to get reference line";
    }
    constexpr int32_t kReferenceLineProviderSleepTime = 200;
    std::this_thread::sleep_for(std::chrono::duration<int32_t, std::milli>(
        kReferenceLineProviderSleepTime));
  }
}

bool ReferenceLineProvider::HasReferenceLine() {
  std::lock_guard<std::mutex> lock(reference_line_groups_mutex_);
  return !reference_line_groups_.empty();
}

bool ReferenceLineProvider::GetReferenceLines(
    std::list<ReferenceLine> *reference_lines,
    std::list<hdmap::RouteSegments> *segments) {
  // TODO(all): implement this function using the current adc position and the
  // existing reference lines. It is required that the current reference lines
  // can cover thoroughly the current adc position so that planning can be make
  // with a minimum planning distance of 100 meters ahead and 10 meters
  // backward.
  CHECK_NOTNULL(reference_lines);
  CHECK_NOTNULL(segments);
  std::lock_guard<std::mutex> lock(reference_line_groups_mutex_);
  if (reference_line_groups_.empty()) {
    return false;
  }
  reference_lines->assign(reference_line_groups_.back().begin(),
                          reference_line_groups_.back().end());
  segments->assign(route_segment_groups_.back().begin(),
                   route_segment_groups_.back().end());
  return true;
}

bool ReferenceLineProvider::CreateReferenceLineFromRouting() {
  std::vector<hdmap::RouteSegments> route_segments;

  double look_forward_distance = (vehicle_speed_ * FLAGS_look_forward_time_sec >
                                  FLAGS_look_forward_min_distance)
                                     ? FLAGS_look_forward_distance
                                     : FLAGS_look_forward_min_distance;
  {
    std::lock_guard<std::mutex> lock(pnc_map_mutex_);
    if (!pnc_map_->GetRouteSegments(FLAGS_look_backward_distance,
                                    look_forward_distance, &route_segments)) {
      AERROR << "Failed to extract segments from routing";
      return false;
    }
  }

  std::unique_ptr<ReferenceLineSmoother> smoother;
  if (FLAGS_enable_spiral_reference_line) {
    double max_deviation = FLAGS_spiral_smoother_max_deviation;
    smoother.reset(new SpiralReferenceLineSmoother(max_deviation));
  } else {
    smoother.reset(new QpSplineReferenceLineSmoother(smoother_config_,
                                                     spline_solver_.get()));
  }

  std::vector<ReferenceLine> reference_lines;
  std::vector<hdmap::RouteSegments> segments;
  for (const auto &lanes : route_segments) {
    hdmap::Path hdmap_path;
    hdmap::PncMap::CreatePathFromLaneSegments(lanes, &hdmap_path);
    if (FLAGS_enable_smooth_reference_line) {
      ReferenceLine raw_reference_line(hdmap_path);
      ReferenceLine reference_line;
      if (!smoother->Smooth(raw_reference_line, &reference_line)) {
        AERROR << "Failed to smooth reference line";
        continue;
      }

      bool is_valid_reference_line = true;
      const double kReferenceLineDiffCheckResolution = 5.0;
      for (int s = 0.0; s < raw_reference_line.Length();
           s += kReferenceLineDiffCheckResolution) {
        auto xy_old = raw_reference_line.GetReferencePoint(s);
        auto xy_new = reference_line.GetReferencePoint(s);
        const double diff = xy_old.DistanceTo(xy_new);

        if (diff > FLAGS_smoothed_reference_line_max_diff) {
          AERROR << "Fail to provide reference line because too large diff "
                    "between smoothed and raw reference lines. diff: "
                 << diff;
          is_valid_reference_line = false;
          break;
        }
      }
      if (is_valid_reference_line) {
        reference_lines.push_back(std::move(reference_line));
        segments.emplace_back(lanes);
      }
    } else {
      reference_lines.emplace_back(hdmap_path);
      segments.emplace_back(lanes);
    }
  }

  if (reference_lines.empty()) {
    AERROR << "No smooth reference lines available";
    return false;
  }

  if (!reference_lines.empty()) {
    std::lock_guard<std::mutex> lock(reference_line_groups_mutex_);
    reference_line_groups_.emplace_back(reference_lines);
    route_segment_groups_.emplace_back(route_segments);
    const size_t kMaxStoredReferenceLineGroups = 3;
    while (reference_line_groups_.size() > kMaxStoredReferenceLineGroups) {
      reference_line_groups_.pop_front();
      route_segment_groups_.pop_front();
    }
  }

  return true;
}

}  // namespace planning
}  // namespace apollo
