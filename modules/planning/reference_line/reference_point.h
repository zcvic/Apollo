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
 * @file reference_point.h
 **/

#ifndef MODULES_PLANNING_REFERENCE_LINE_REFERENCE_POINT_H_
#define MODULES_PLANNING_REFERENCE_LINE_REFERENCE_POINT_H_

#include "modules/map/pnc_map/path.h"

namespace apollo {
namespace planning {

class ReferencePoint : public hdmap::PathPoint {
 public:
  ReferencePoint() = default;

  ReferencePoint(const common::math::Vec2d& point, const double heading,
                 const double kappa, const double dkappa,
                 const double lower_bound, const double upper_bound);

  ReferencePoint(const common::math::Vec2d& point, const double heading,
                 const hdmap::LaneWaypoint lane_waypoint);

  ReferencePoint(const common::math::Vec2d& point, const double heading,
                 const double kappa, const double dkappa,
                 const hdmap::LaneWaypoint lane_waypoint);

  ReferencePoint(const common::math::Vec2d& point, const double heading,
                 const double kappa, const double dkappa,
                 const std::vector<hdmap::LaneWaypoint>& lane_waypoints);

  void set_kappa(const double kappa);
  void set_dkappa(const double dkappa);
  void set_lower_bound(const double lower_bound);
  void set_upper_bound(const double upper_bound);

  double kappa() const;
  double dkappa() const;

  double lower_bound() const;
  double upper_bound() const;

 private:
  double kappa_ = 0.0;
  double dkappa_ = 0.0;
  // boundary relative to the reference point
  double lower_bound_ = 0.0;
  double upper_bound_ = 0.0;
};

}  // namespace planning
}  // namespace apollo

#endif  // MODULES_PLANNING_REFERENCE_LINE_REFERENCE_POINT_H_
