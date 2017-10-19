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

#include "modules/planning/lattice/trajectory1d_generator.h"

#include <cmath>
#include "modules/planning/lattice/standing_still_trajectory1d.h"
#include "modules/planning/lattice/constant_deceleration_trajectory1d.h"
#include "modules/planning/lattice/lattice_params.h"
#include "modules/planning/math/curve1d/quartic_polynomial_curve1d.h"
#include "modules/planning/math/curve1d/quintic_polynomial_curve1d.h"
#include "modules/planning/common/planning_gflags.h"
#include "modules/common/log.h"

namespace apollo {
namespace planning {

void Trajectory1dGenerator::GenerateTrajectoryBundles(
    const PlanningObject& planning_objective,
    const std::array<double, 3>& lon_init_state,
    const std::array<double, 3>& lat_init_state,
    std::vector<std::shared_ptr<Curve1d> >& lon_trajectory_bundle,
    std::vector<std::shared_ptr<Curve1d> >& lat_trajectory_bundle) {

  const LatticeSamplingConfig& lattice_sampling_config =
    planning_objective.lattice_sampling_config();
  const LonSampleConfig& lon_sample_config = lattice_sampling_config.lon_sample_config();
  const LatSampleConfig& lat_sample_config = lattice_sampling_config.lat_sample_config();

  if (planning_objective.decision_type() == PlanningObject::STOP &&
      enable_stop_handling == true) {
    double stop_position = lon_sample_config.lon_end_condition().s();
    double distance = stop_position - lon_init_state[0];
    double s_dot = lon_init_state[1];

    // if the stop point is close enough and vehicle speed is close to zero.
    if (distance < stop_margin && s_dot < stop_speed_threshold) {
      std::shared_ptr<Curve1d> ptr_lon_trajectory =
          std::shared_ptr<Curve1d>(new StandingStillTrajectory1d(
              lon_init_state[0], planned_trajectory_time));
      lon_trajectory_bundle.push_back(ptr_lon_trajectory);

      std::shared_ptr<Curve1d> ptr_lat_trajectory =
          std::shared_ptr<Curve1d>(new StandingStillTrajectory1d(
              lat_init_state[0], planned_trajectory_time));
      lat_trajectory_bundle.push_back(ptr_lat_trajectory);
      return;
    }

    // if the stop point is close enough and vehicle speed is slow, e.g., < 0.5 m/s.
    if (distance < stop_margin && s_dot < low_speed_threshold) {
      double comfort_deceleration =
          std::fabs(longitudinal_acceleration_comfort_factor *
                    FLAGS_longitudinal_acceleration_lower_bound);

      std::shared_ptr<Curve1d> ptr_lon_trajectory =
          std::shared_ptr<Curve1d>(new ConstantDecelerationTrajectory1d(
              lon_init_state[0], lon_init_state[1], comfort_deceleration));
      lon_trajectory_bundle.push_back(ptr_lon_trajectory);

      std::shared_ptr<Curve1d> ptr_lat_trajectory =
          std::shared_ptr<Curve1d>(new StandingStillTrajectory1d(
              lat_init_state[0], planned_trajectory_time));
      lat_trajectory_bundle.push_back(ptr_lat_trajectory);
      return;
    }
  }

  // generate the trajectory bundles using polynomial methods.
  GenerateLongitudinalTrajectoryBundle(
      planning_objective, lon_init_state, lon_trajectory_bundle);
  GenerateLateralTrajectoryBundle(
      lat_init_state, lat_trajectory_bundle);
  return;
}
void Trajectory1dGenerator::GenerateLongitudinalTrajectoryBundle(
  const PlanningObject& planning_objective,
  const std::array<double, 3>& init_state,
  std::vector<std::shared_ptr<Curve1d>>& lon_trajectory_bundle
  ) const {

  const LatticeSamplingConfig& lattice_sampling_config =
    planning_objective.lattice_sampling_config();
  const LonSampleConfig& lon_sample_config = lattice_sampling_config.lon_sample_config();
  const LatSampleConfig& lat_sample_config = lattice_sampling_config.lat_sample_config();

  double s_target = lon_sample_config.lon_end_condition().s();
  double ds_target = lon_sample_config.lon_end_condition().ds();
  double dds_target = lon_sample_config.lon_end_condition().dds();

  if (s_target <= std::numeric_limits<double>::epsilon()) {
    // Quartic
    // To Be Uniformly Replaced By LatticeSampleConfig
    GenerateSpeedProfilesForCruising(
        init_state, lon_sample_config, lon_trajectory_bundle);
  } else if (planning_objective.decision_type() == PlanningObject::GO) {
    GenerateSpeedProfilesForFollowing(
        init_state, lon_sample_config, lon_trajectory_bundle);
  } else {
    CHECK(planning_objective.decision_type() == PlanningObject::STOP);
    GenerateSpeedProfilesForStopping(
      init_state,
      lon_sample_config,
      lon_trajectory_bundle);
    GenerateSpeedProfilesForCruising(
      init_state,
      lon_sample_config,
      lon_trajectory_bundle);
  }
  return;
}

void Trajectory1dGenerator::GenerateSpeedProfilesForCruising(
    const std::array<double, 3>& init_state,
    const LonSampleConfig& lon_sample_config,
    std::vector<std::shared_ptr<Curve1d>>& lon_trajectory_bundle) const {

  double s_target = lon_sample_config.lon_end_condition().s();
  double ds_target = lon_sample_config.lon_end_condition().ds();
  double dds_target = lon_sample_config.lon_end_condition().dds();

  double cruise_speed = ds_target;

  std::vector<std::pair<std::array<double, 3>, double>> end_conditions;
  end_condition_sampler_.SampleLonEndConditionsForCruising(
      init_state, cruise_speed, end_conditions);

  for (const auto& end_condition : end_conditions) {
    // Only the last two elements in the end_condition are useful.
    std::array<double, 2> end_state;
    end_state[0] = end_condition.first[1];
    end_state[1] = end_condition.first[2];

    std::shared_ptr<Curve1d> ptr_lon_trajectory =
        std::shared_ptr<Curve1d>(new QuarticPolynomialCurve1d(
            init_state, end_state, end_condition.second));

    lon_trajectory_bundle.push_back(ptr_lon_trajectory);
  }
}

void Trajectory1dGenerator::GenerateSpeedProfilesForFollowing(
    const std::array<double, 3>& init_state,
    const LonSampleConfig& lon_sample_config,
    std::vector<std::shared_ptr<Curve1d>>& lon_trajectory_bundle) const {

  double target_position = lon_sample_config.lon_end_condition().s();
  double target_velocity = lon_sample_config.lon_end_condition().ds();

  std::vector<std::pair<std::array<double, 3>, double>> end_conditions;
  end_condition_sampler_.SampleLonEndConditionsForFollowing(init_state,
      target_position, target_velocity, end_conditions);

  for (const auto& end_condition : end_conditions) {
    std::shared_ptr<Curve1d> ptr_lon_trajectory =
        std::shared_ptr<Curve1d>(new QuinticPolynomialCurve1d(
            init_state, end_condition.first, end_condition.second));

    lon_trajectory_bundle.push_back(ptr_lon_trajectory);
  }
}

void Trajectory1dGenerator::GenerateSpeedProfilesForStopping(
    const std::array<double, 3>& init_state,
    const LonSampleConfig& lon_sample_config,
    std::vector<std::shared_ptr<Curve1d>>& lon_trajectory_bundle) const {
  double stop_position = lon_sample_config.lon_end_condition().s();
  std::vector<std::pair<std::array<double, 3>, double>> end_conditions;
  end_condition_sampler_.SampleLonEndConditionsForStopping(
      init_state, stop_position, end_conditions);

  for (const auto& end_condition : end_conditions) {
    std::shared_ptr<Curve1d> ptr_lon_trajectory =
        std::shared_ptr<Curve1d>(new QuinticPolynomialCurve1d(
            init_state, end_condition.first, end_condition.second));

    lon_trajectory_bundle.push_back(ptr_lon_trajectory);
  }
}

void Trajectory1dGenerator::GenerateLateralTrajectoryBundle(
    const std::array<double, 3>& init_state,
    std::vector<std::shared_ptr<Curve1d>>& lat_trajectory_bundle) const {

  std::vector<std::pair<std::array<double, 3>, double>> end_conditions;
  end_condition_sampler_.SampleLatEndConditions(
      init_state, end_conditions);

  for (const auto& end_condition : end_conditions) {
    std::shared_ptr<Curve1d> ptr_lat_trajectory =
        std::shared_ptr<Curve1d>(new QuinticPolynomialCurve1d(
            init_state, end_condition.first, end_condition.second));

    lat_trajectory_bundle.push_back(ptr_lat_trajectory);
  }
}

} //namespace planning
} //namespace apollo
