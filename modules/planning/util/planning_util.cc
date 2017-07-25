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

#include "modules/planning/util/planning_util.h"

namespace apollo {
namespace planning {
namespace util {

using SpeedPoint = apollo::planning::SpeedPoint;

SpeedPoint MakeSpeedPoint(const double s, const double t, double v, double a,
                          double da) {
  SpeedPoint point;
  point.set_s(s);
  point.set_t(t);
  point.set_v(v);
  point.set_a(a);
  point.set_da(da);
  return point;
}

}  // namespace util
}  // namespace planning
}  // namespace apollo
