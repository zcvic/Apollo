/******************************************************************************
 * Copyright 2019 The Apollo Authors. All Rights Reserved.
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

#include "modules/planning/tasks/deciders/path_assessment_decider.h"

namespace apollo {
namespace planning {

using apollo::common::ErrorCode;
using apollo::common::Status;

PathAssessmentDecider::PathAssessmentDecider(const TaskConfig& config)
    : Decider(config) {}

Status PathAssessmentDecider::Process(
    Frame* const frame, ReferenceLineInfo* const reference_line_info) {
  // Sanity checks.
  CHECK_NOTNULL(frame);
  CHECK_NOTNULL(reference_line_info);

  // Check the validity of paths (the optimization output).
  // 1. First check the regular path's validity.
  const PathData* regular_path_data = reference_line_info->mutable_path_data();
  bool is_valid_regular_path = IsValidPath(regular_path_data);
  // 2. If the regular path is not valid, check the validity of the
  //    fallback path.
  bool is_valid_fallback_path = false;
  if (!is_valid_regular_path) {
    const PathData* fallback_path_data =
        reference_line_info->mutable_fallback_path_data();
    is_valid_fallback_path = IsValidPath(fallback_path_data);
  }
  // 3. If neither is valid, use the reference_line as the ultimate fallback.
  if (!is_valid_regular_path && !is_valid_fallback_path) {
    const std::string msg =
        "Neither regular nor fallback path is valid.";
    AERROR << msg;
    return Status(ErrorCode::PLANNING_ERROR, msg);
  }

  // Analyze and add important info for speed decider to use.

  return Status::OK();
}

bool PathAssessmentDecider::IsValidPath(
    const PathData* path_data) {
  return true;
}

void PathAssessmentDecider::SetPathInfo() {
  return;
}

}  // namespace planning
}  // namespace apollo
