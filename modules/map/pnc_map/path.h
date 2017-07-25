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

#ifndef MODULES_MAP_PNC_MAP_TRAJECTORY_H_
#define MODULES_MAP_PNC_MAP_TRAJECTORY_H_

#pragma once

#include <cmath>
#include <functional>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "modules/map/proto/map_lane.pb.h"

#include "modules/common/log.h"
#include "modules/common/math/box2d.h"
#include "modules/common/math/line_segment2d.h"
#include "modules/common/math/vec2d.h"
#include "modules/map/hdmap/hdmap.h"
#include "modules/map/hdmap/hdmap_common.h"

namespace apollo {
namespace hdmap {

// class LaneInfo;
// class OverlapInfo;

struct LaneWaypoint {
  LaneWaypoint() = default;
  LaneWaypoint(const apollo::hdmap::LaneInfo* lane, const double s)
      : lane(CHECK_NOTNULL(lane)), s(s) {}
  const apollo::hdmap::LaneInfo* lane = nullptr;
  double s = 0.0;

  std::string debug_string() const;
};

struct LaneSegment {
  LaneSegment() = default;
  LaneSegment(const apollo::hdmap::LaneInfo* lane, const double start_s,
              const double end_s)
      : lane(CHECK_NOTNULL(lane)), start_s(start_s), end_s(end_s) {}
  const apollo::hdmap::LaneInfo* lane = nullptr;
  double start_s = 0.0;
  double end_s = 0.0;

  std::string debug_string() const;
};

struct PathOverlap {
  PathOverlap() = default;
  PathOverlap(std::string object_id, const double start_s, const double end_s)
      : object_id(std::move(object_id)), start_s(start_s), end_s(end_s) {}

  std::string object_id;
  double start_s = 0.0;
  double end_s = 0.0;

  std::string debug_string() const;
};

class PathPoint : public apollo::common::math::Vec2d {
 public:
  PathPoint() = default;
  PathPoint(const apollo::common::math::Vec2d& point, double heading)
      : Vec2d(point.x(), point.y()), _heading(heading) {}
  PathPoint(const apollo::common::math::Vec2d& point, double heading,
            LaneWaypoint lane_waypoint)
      : Vec2d(point.x(), point.y()), _heading(heading) {
    _lane_waypoints.emplace_back(std::move(lane_waypoint));
  }
  PathPoint(const apollo::common::math::Vec2d& point, double heading,
            std::vector<LaneWaypoint> lane_waypoints)
      : Vec2d(point.x(), point.y()),
        _heading(heading),
        _lane_waypoints(std::move(lane_waypoints)) {}

  double heading() const { return _heading; }
  void set_heading(const double heading) { _heading = heading; }

  const std::vector<LaneWaypoint>& lane_waypoints() const {
    return _lane_waypoints;
  }

  void add_lane_waypoint(LaneWaypoint lane_waypoint) {
    _lane_waypoints.emplace_back(std::move(lane_waypoint));
  }
  void add_lane_waypoints(const std::vector<LaneWaypoint>& lane_waypoints) {
    _lane_waypoints.insert(_lane_waypoints.end(), lane_waypoints.begin(),
                           lane_waypoints.end());
  }

  void clear_lane_waypoints() { _lane_waypoints.clear(); }

  std::string debug_string() const;

 protected:
  double _heading = 0.0;
  double _curvature = 0.0;
  double _curvature_derivative = 0.0;
  std::vector<LaneWaypoint> _lane_waypoints;
};

class Path;

class PathApproximation {
 public:
  PathApproximation() = default;
  PathApproximation(const Path& path, const double max_error)
      : _max_error(max_error), _max_sqr_error(max_error * max_error) {
    init(path);
  }
  double max_error() const { return _max_error; }
  const std::vector<int>& original_ids() const { return _original_ids; }
  const std::vector<apollo::common::math::LineSegment2d>& segments() const {
    return _segments;
  }

  bool get_projection(const Path& path,
                      const apollo::common::math::Vec2d& point,
                      double* accumulate_s, double* lateral,
                      double* distance) const;

  bool overlap_with(const Path& path, const apollo::common::math::Box2d& box,
                    double width) const;

 protected:
  void init(const Path& path);
  bool is_within_max_error(const Path& path, const int s, const int t);
  double compute_max_error(const Path& path, const int s, const int t);

  void init_dilute(const Path& path);
  void init_projections(const Path& path);

 protected:
  double _max_error = 0;
  double _max_sqr_error = 0;

  int _num_points = 0;
  std::vector<int> _original_ids;
  std::vector<apollo::common::math::LineSegment2d> _segments;
  std::vector<double> _max_error_per_segment;

  // TODO(@lianglia_apollo): use direction change checks to early stop.

  // Projection of points onto the diluated segments.
  std::vector<double> _projections;
  double _max_projection;
  int _num_projection_samples = 0;

  // The original_projection is the projection of original points onto the
  // diluated segments.
  std::vector<double> _original_projections;
  // max_p_to_left[i] = max(p[0], p[1], ... p[i]).
  // min_p_to_right[i] = min(p[i], p[i + 1], ... p[size - 1]).
  std::vector<double> _max_original_projections_to_left;
  std::vector<double> _min_original_projections_to_right;
  std::vector<int> _sampled_max_original_projections_to_left;
};

class InterpolatedIndex {
 public:
  InterpolatedIndex(int id, double offset) : id(id), offset(offset) {}
  int id = 0;
  double offset = 0.0;
};

class Path {
 public:
  Path() = default;
  Path(Path&&) = default;
  Path& operator=(Path&&) = default;

  explicit Path(std::vector<PathPoint> path_points);
  Path(std::vector<PathPoint> path_points,
       std::vector<LaneSegment> lane_segments);
  Path(std::vector<PathPoint> path_points,
       std::vector<LaneSegment> lane_segments,
       const double max_approximation_error);

  // Return smooth coordinate by interpolated index or accumulate_s.
  PathPoint get_smooth_point(const InterpolatedIndex& index) const;
  PathPoint get_smooth_point(double s) const;

  // Compute accumulate s value of the index.
  double get_s_from_index(const InterpolatedIndex& index) const;
  // Compute interpolated index by accumulate_s.
  InterpolatedIndex get_index_from_s(double s) const;

  bool get_nearest_point(const apollo::common::math::Vec2d& point,
                         double* accumulate_s, double* lateral) const;
  bool get_nearest_point(const apollo::common::math::Vec2d& point,
                         double* accumulate_s, double* lateral,
                         double* distance) const;
  bool get_projection(const apollo::common::math::Vec2d& point,
                      double* accumulate_s, double* lateral) const;
  bool get_projection(const apollo::common::math::Vec2d& point,
                      double* accumulate_s, double* lateral,
                      double* distance) const;

  bool get_heading_along_path(const apollo::common::math::Vec2d& point,
                              double* heading) const;

  int num_points() const { return _num_points; }
  int num_segments() const { return _num_segments; }
  const std::vector<PathPoint>& path_points() const { return _path_points; }
  const std::vector<LaneSegment>& lane_segments() const {
    return _lane_segments;
  }
  const std::vector<LaneSegment>& lane_segments_to_next_point() const {
    return _lane_segments_to_next_point;
  }
  const std::vector<apollo::common::math::Vec2d>& unit_directions() const {
    return _unit_directions;
  }
  const std::vector<double>& accumulated_s() const { return _accumulated_s; }
  const std::vector<apollo::common::math::LineSegment2d>& segments() const {
    return _segments;
  }
  const PathApproximation* approximation() const { return &_approximation; }
  double length() const { return _length; }

  const std::vector<PathOverlap>& lane_overlaps() const {
    return _lane_overlaps;
  }
  const std::vector<PathOverlap>& signal_overlaps() const {
    return _signal_overlaps;
  }
  const std::vector<PathOverlap>& yield_sign_overlaps() const {
    return _yield_sign_overlaps;
  }
  const std::vector<PathOverlap>& stop_sign_overlaps() const {
    return _stop_sign_overlaps;
  }
  const std::vector<PathOverlap>& crosswalk_overlaps() const {
    return _crosswalk_overlaps;
  }
  const std::vector<PathOverlap>& parking_space_overlaps() const {
    return _parking_space_overlaps;
  }
  const std::vector<PathOverlap>& junction_overlaps() const {
    return _junction_overlaps;
  }
  const std::vector<PathOverlap>& speed_bump_overlaps() const {
    return _speed_bump_overlaps;
  }

  double get_left_width(const double s) const;
  double get_right_width(const double s) const;
  bool get_width(const double s, double* left_width, double* right_width) const;

  bool is_on_path(const apollo::common::math::Vec2d& point) const;
  // requires all corners of the box on path.
  bool is_on_path(const apollo::common::math::Box2d& box) const;
  bool overlap_with(const apollo::common::math::Box2d& box, double width) const;

  std::string debug_string() const;

 protected:
  void init();
  void init_points();
  void init_lane_segments();
  void init_width();
  void init_point_index();
  void init_overlaps();

  double get_sample(const std::vector<double>& samples, const double s) const;

  using GetOverlapFromLaneFunc =
      std::function<const std::vector<const apollo::hdmap::OverlapInfo*>&(
          const apollo::hdmap::LaneInfo&)>;
  void get_all_overlaps(GetOverlapFromLaneFunc get_overlaps_from_lane,
                        std::vector<PathOverlap>* const overlaps) const;

 protected:
  int _num_points = 0;
  int _num_segments = 0;
  std::vector<PathPoint> _path_points;
  std::vector<LaneSegment> _lane_segments;
  std::vector<LaneSegment> _lane_segments_to_next_point;
  std::vector<apollo::common::math::Vec2d> _unit_directions;
  double _length = 0.0;
  std::vector<double> _accumulated_s;
  std::vector<apollo::common::math::LineSegment2d> _segments;
  bool _use_path_approximation = false;
  PathApproximation _approximation;

  // Sampled every fixed length.
  int _num_sample_points = 0;
  std::vector<double> _left_width;
  std::vector<double> _right_width;
  std::vector<int> _last_point_index;

  std::vector<PathOverlap> _lane_overlaps;
  std::vector<PathOverlap> _signal_overlaps;
  std::vector<PathOverlap> _yield_sign_overlaps;
  std::vector<PathOverlap> _stop_sign_overlaps;
  std::vector<PathOverlap> _crosswalk_overlaps;
  std::vector<PathOverlap> _parking_space_overlaps;
  std::vector<PathOverlap> _junction_overlaps;
  std::vector<PathOverlap> _speed_bump_overlaps;
};

}  // namespace hdmap
}  // namespace apollo

#endif  // MODULES_MAP_PNC_MAP_TRAJECTORY_H_
