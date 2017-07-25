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
 * @file path_point_util.h
 **/

#ifndef MODULES_PLANNING_COMMON_PATH_PATH_POINT_H_
#define MODULES_PLANNING_COMMON_PATH_PATH_POINT_H_

#include "Eigen/Dense"

#include "modules/common/proto/path_point.pb.h"

namespace apollo {
namespace planning {
namespace util {

apollo::common::PathPoint interpolate(
    const apollo::common::PathPoint &p0,
    const apollo::common::PathPoint &p1, const double s);

// @ weight shall between 1 and 0
apollo::common::PathPoint interpolate_linear_approximation(
    const apollo::common::PathPoint &p0,
    const apollo::common::PathPoint &p1,
    const double s);

apollo::common::TrajectoryPoint interpolate(
    const apollo::common::TrajectoryPoint &tp0,
    const apollo::common::TrajectoryPoint &tp1,
    const double t);

apollo::common::TrajectoryPoint interpolate_linear_approximation(
    const apollo::common::TrajectoryPoint &tp0,
    const apollo::common::TrajectoryPoint &tp1,
    const double t);

}  // namespace util
}  // namespace planning
}  // namespace apollo

#endif  // MODULES_PLANNING_COMMON_PATH_PATH_POINT_H_
