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

#ifndef MODULES_PLANNING_LATTICE_BEHAVIOR_DECIDER_CONDITION_FILTER_H_
#define MODULES_PLANNING_LATTICE_BEHAVIOR_DECIDER_CONDITION_FILTER_H_

#include <array>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "modules/planning/lattice/behavior_decider/path_time_neighborhood.h"
#include "modules/planning/lattice/behavior_decider/feasible_region.h"
#include "modules/planning/proto/lattice_structure.pb.h"
#include "modules/planning/proto/planning_internal.pb.h"

namespace apollo {
namespace planning {

class ConditionFilter {
 public:
  ConditionFilter(
    const Frame* frame, const std::array<double, 3>& init_s,
    const double speed_limit, const ReferenceLine& reference_line,
    const std::vector<common::PathPoint>& discretized_ref_points,
    std::shared_ptr<PathTimeNeighborhood> path_time_neighborhood);

  std::vector<SampleBound> QuerySampleBounds(const double t) const;

  std::vector<SampleBound> QuerySampleBounds() const;

  std::vector<std::pair<PathTimePoint, PathTimePoint>>
  QueryPathTimeObstacleIntervals(const double t) const;

  bool GenerateLatticeStPixels(
    apollo::planning_internal::LatticeStTraining* st_data,
    double timestamp,
    std::string st_img_name);

  bool WithinObstacleSt(double s, double t);

 private:
  void Init();

  // Return true only if t is within the range of time slot,
  // but will output block interval anyway(maybe be extension)
  std::pair<PathTimePoint, PathTimePoint> QueryPathTimeObstacleIntervals(
      const double t, const PathTimeObstacle& critical_condition) const;

  std::set<double> CriticalTimeStamps() const;

  std::vector<double> UniformTimeStamps(
      const std::size_t num_of_time_segments) const;

 private:
  FeasibleRegion feasible_region_;
<<<<<<< HEAD
  std::vector<PathTimeObstacle> path_time_obstacles_;
  std::shared_ptr<PathTimeNeighborhood> path_time_neighborhood_;
=======

  std::shared_ptr<PathTimeNeighborhood> ptr_path_time_neighborhood_;
>>>>>>> planning: changed module interfaces for lattice planner
};

}  // namespace planning
}  // namespace apollo

#endif  // MODULES_PLANNING_LATTICE_BEHAVIOR_DECIDER_CONDITION_FILTER_H_
