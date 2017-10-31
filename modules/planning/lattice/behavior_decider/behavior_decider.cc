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
 * @file decision_analyzer.h
 **/

#ifndef MODULES_PLANNING_LATTICE_BEHAVIOR_DECIDER_BEHAVIOR_DECIDER_H
#define MODULES_PLANNING_LATTICE_BEHAVIOR_DECIDER_BEHAVIOR_DECIDER_H

#include "gflags/gflags.h"

#include "modules/planning/common/planning_gflags.h"
#include "modules/planning/lattice/lattice_params.h"
#include "modules/planning/lattice/reference_line_matcher.h"
#include "modules/planning/lattice/behavior_decider/behavior_decider.h"
#include "modules/planning/lattice/lattice_util.h"
#include "modules/common/log.h"
#include "modules/common/proto/geometry.pb.h"

namespace apollo {
namespace planning {

using apollo::common::PointENU;
using apollo::common::PathPoint;

BehaviorDecider::BehaviorDecider() {}

PlanningTarget BehaviorDecider::Analyze(
    Frame* frame, const common::TrajectoryPoint& init_planning_point,
    const std::array<double, 3>& lon_init_state,
    const std::vector<ReferenceLine>& candidate_reference_lines) {

  PlanningTarget ret;
  CHECK(frame != nullptr);
  // Only handles one reference line
  CHECK_GT(candidate_reference_lines.size(), 0);
  const ReferenceLine& ref_line = candidate_reference_lines[0];
  const std::vector<ReferencePoint>& ref_points = ref_line.reference_points();

  std::vector<common::PathPoint> discretized_ref_points =
      apollo::planning::ToDiscretizedReferenceLine(ref_points);

  for (size_t i = 0; i < discretized_ref_points.size(); ++i) {
    ret.mutable_discretized_reference_line()
        ->add_discretized_reference_line_point()
        ->CopyFrom(discretized_ref_points[i]);
  }

  LatticeSamplingConfig* lattice_sampling_config =
      ret.mutable_lattice_sampling_config();
  LonSampleConfig* lon_sample_config =
      lattice_sampling_config->mutable_lon_sample_config();
  LatSampleConfig* lat_sample_config =
      lattice_sampling_config->mutable_lat_sample_config();
  // lon_sample_config->mutable_lon_end_condition()->set_s(0.0);
  lon_sample_config->mutable_lon_end_condition()->set_ds(
      FLAGS_default_cruise_speed);
  lon_sample_config->mutable_lon_end_condition()->set_dds(0.0);
  ret.set_decision_type(PlanningTarget::GO);
  return ret;
}

PlanningTarget BehaviorDecider::Analyze(
    Frame* frame, const common::TrajectoryPoint& init_planning_point,
    const std::array<double, 3>& lon_init_state,
    const std::vector<common::PathPoint>& discretized_reference_line) {
  PlanningTarget ret;
  CHECK(frame != nullptr);
  // Only handles one reference line
  CHECK_GT(discretized_reference_line.size(), 0);

  for (size_t i = 0; i < discretized_reference_line.size(); ++i) {
    ret.mutable_discretized_reference_line()
        ->add_discretized_reference_line_point()
        ->CopyFrom(discretized_reference_line[i]);
  }

  LatticeSamplingConfig* lattice_sampling_config =
      ret.mutable_lattice_sampling_config();
  LonSampleConfig* lon_sample_config =
      lattice_sampling_config->mutable_lon_sample_config();
  LatSampleConfig* lat_sample_config =
      lattice_sampling_config->mutable_lat_sample_config();
  // lon_sample_config->mutable_lon_end_condition()->set_s(0.0);
  lon_sample_config->mutable_lon_end_condition()->set_ds(
      FLAGS_default_cruise_speed);
  lon_sample_config->mutable_lon_end_condition()->set_dds(0.0);
  ret.set_decision_type(PlanningTarget::GO);

  if (StopDecisionNearDestination(frame, lon_init_state,
      discretized_reference_line, lon_sample_config, &ret)) {
    AINFO << "STOP decision when near the routing end.";
  }

  return ret;
}

bool BehaviorDecider::StopDecisionNearDestination(
    Frame* frame,
    const std::array<double, 3>& lon_init_state,
    const std::vector<common::PathPoint>& discretized_reference_line,
    LonSampleConfig* lon_sample_config,
    PlanningTarget* planning_target) {

  PointENU routing_end = frame->GetRoutingDestination();
  PathPoint routing_end_on_ref_line =
      ReferenceLineMatcher::match_to_reference_line(
          discretized_reference_line, routing_end.x(), routing_end.y());

  double dist_x = routing_end.x() - routing_end_on_ref_line.x();
  double dist_y = routing_end.y() - routing_end_on_ref_line.y();
  double dist = std::hypot(dist_x, dist_y);
  if (dist > dist_thred_omit_routing_end) {
    return false;
  }

  if (routing_end_on_ref_line.s() <
        discretized_reference_line.back().s() - 0.2) {
    double res_s = routing_end_on_ref_line.s() - lon_init_state[0];
    double v = lon_init_state[1];
    double target_acc = (v * v) / (2.0 * res_s);
    if (target_acc > stop_acc_thred) {
      lon_sample_config->mutable_lon_end_condition()->set_s(
          routing_end_on_ref_line.s());
      lon_sample_config->mutable_lon_end_condition()->set_ds(0.0);
      lon_sample_config->mutable_lon_end_condition()->set_dds(0.0);
      planning_target->set_decision_type(PlanningTarget::STOP);
      return true;
    }
  }
  return false;
}

void BehaviorDecider::GetNearbyObstacles(
    const common::TrajectoryPoint& init_planning_point,
    const Frame* frame,
    const std::vector<common::PathPoint>& discretized_reference_line,
    std::array<double, 3>* forward_state,
    std::array<double, 3>* backward_state) {
  // TODO(kechxu) implement
}

}  // namespace planning
}  // namespace apollo

#endif /* MODULES_PLANNING_DECISION_ANALYZER_H */
