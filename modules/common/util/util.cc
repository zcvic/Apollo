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

#include "modules/common/util/util.h"

#include <cmath>

namespace apollo {
namespace common {
namespace util {

using SLPoint = apollo::common::SLPoint;

bool EndWith(const std::string& original, const std::string& pattern) {
  return original.length() >= pattern.length() &&
         original.substr(original.length() - pattern.length()) == pattern;
}

SLPoint MakeSLPoint(const double s, const double l) {
  SLPoint sl;
  sl.set_s(s);
  sl.set_l(l);
  return sl;
}

double Distance2D(const PathPoint& a, const PathPoint& b) {
  return std::hypot(a.x() - b.x(), a.y() - b.y());
}

}  // namespace util
}  // namespace common
}  // namespace apollo
