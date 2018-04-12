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
 * @file
 **/

#include "modules/planning/lattice/behavior/feasible_region.h"

#include <cmath>

#include "glog/logging.h"
#include "modules/planning/common/planning_gflags.h"

namespace apollo {
namespace planning {

FeasibleRegion::FeasibleRegion(const std::array<double, 3>& init_s,
                               const double speed_limit) {
  Setup(init_s, speed_limit);
}

void FeasibleRegion::Setup(const std::array<double, 3>& init_s,
                           const double speed_limit) {
  init_s_ = init_s;
  speed_limit_ = speed_limit;

  double v = init_s[1];
  CHECK_GE(v, 0.0);

  const double max_deceleration = -FLAGS_longitudinal_acceleration_lower_bound;
  t_at_zero_speed_ = v / max_deceleration;
  s_at_zero_speed_ = init_s[0] + v * v / (2.0 * max_deceleration);

  double delta_v = speed_limit - v;
  if (delta_v < 0.0) {
    t_at_speed_limit_ = delta_v / FLAGS_longitudinal_acceleration_lower_bound;
    s_at_speed_limit_ = init_s_[0] + v * t_at_speed_limit_ +
                        0.5 * FLAGS_longitudinal_acceleration_lower_bound *
                            t_at_speed_limit_ * t_at_speed_limit_;
  } else {
    t_at_speed_limit_ = delta_v / FLAGS_longitudinal_acceleration_upper_bound;
    s_at_speed_limit_ = init_s_[0] + v * t_at_speed_limit_ +
                        0.5 * FLAGS_longitudinal_acceleration_upper_bound *
                            t_at_speed_limit_ * t_at_speed_limit_;
  }
}

double FeasibleRegion::SUpper(const double t) const {
  CHECK(t >= 0.0);

  if (init_s_[1] < speed_limit_) {
    if (t < t_at_speed_limit_) {
      return init_s_[0] + init_s_[1] * t +
             0.5 * FLAGS_longitudinal_acceleration_upper_bound * t * t;
    } else {
      return s_at_speed_limit_ + speed_limit_ * (t - t_at_speed_limit_);
    }
  } else {
    return init_s_[0] + init_s_[1] * t;
  }
}

double FeasibleRegion::SLower(const double t) const {
  if (t < t_at_zero_speed_) {
    return init_s_[0] + init_s_[1] * t +
           0.5 * FLAGS_longitudinal_acceleration_lower_bound * t * t;
  }
  return s_at_zero_speed_;
}

double FeasibleRegion::VUpper(const double t) const {
  return std::max(
      init_s_[1],
      std::min(init_s_[1] + FLAGS_longitudinal_acceleration_upper_bound * t,
               speed_limit_));
}

double FeasibleRegion::VLower(const double t) const {
  return t < t_at_zero_speed_
             ? init_s_[1] + FLAGS_longitudinal_acceleration_lower_bound * t
             : 0.0;
}

double FeasibleRegion::TLower(const double s) const {
  CHECK(s >= init_s_[0]);

  if (init_s_[1] < speed_limit_) {
    if (s < s_at_speed_limit_) {
      double delta =
          init_s_[1] * init_s_[1] -
          2.0 * FLAGS_longitudinal_acceleration_upper_bound * (init_s_[0] - s);
      return (std::sqrt(delta) - init_s_[1]) /
             FLAGS_longitudinal_acceleration_upper_bound;
    } else {
      return t_at_speed_limit_ + (s - s_at_speed_limit_) / speed_limit_;
    }
  } else {
    return (s - init_s_[0]) / init_s_[1];
  }
}

}  // namespace planning
}  // namespace apollo
