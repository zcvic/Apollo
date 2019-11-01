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

#include "modules/control/common/mrac_controller.h"

#include <iostream>
#include <string>

#include "cyber/common/file.h"
#include "gtest/gtest.h"
#include "modules/control/proto/control_conf.pb.h"
#include "modules/control/proto/mrac_conf.pb.h"

namespace apollo {
namespace control {

using Matrix = Eigen::MatrixXd;

class MracControllerTest : public ::testing::Test {
 public:
  virtual void SetUp() {
    std::string control_conf_file =
        "/apollo/modules/control/testdata/conf/control_conf.pb.txt";
    CHECK(cyber::common::GetProtoFromFile(control_conf_file, &control_conf_));
    lat_controller_conf_ = control_conf_.lat_controller_conf();
  }

 protected:
  ControlConf control_conf_;
  LatControllerConf lat_controller_conf_;
};

// test the input-output of the model reference adaptive controller
TEST_F(MracControllerTest, MracControl) {
  double dt = 0.01;
  Matrix state = Matrix::Zero(1, 1);
  MracConf mrac_conf = lat_controller_conf_.steer_mrac_conf();
  MracController mrac_controller;
  mrac_controller.Init(mrac_conf, dt);
  mrac_controller.Reset();
  double limit = 100.0;
  double rate_limit = 100.0 / dt;
  state(0, 0) = 6.0;
  EXPECT_NEAR(mrac_controller.Control(18.0, state, dt, limit, rate_limit), 0.0,
              1e-6);
  mrac_controller.Reset();
  state(0, 0) = 10.0;
  EXPECT_NEAR(mrac_controller.Control(18.0, state, dt, limit, rate_limit),
              -8.48, 1e-6);
  EXPECT_NEAR(mrac_controller.CurrentReferenceState(), 6.0, 1e-6);
  EXPECT_NEAR(mrac_controller.CurrentStateAdaptionGain(), -0.2, 1e-6);
  EXPECT_NEAR(mrac_controller.CurrentInputAdaptionGain(), -0.36, 1e-6);
  EXPECT_NEAR(mrac_controller.Control(18.0, state, dt, limit, rate_limit),
              -8.48, 1e-6);
  EXPECT_NEAR(mrac_controller.CurrentReferenceState(), 14.0, 1e-6);
  EXPECT_NEAR(mrac_controller.CurrentStateAdaptionGain(), -0.2, 1e-6);
  EXPECT_NEAR(mrac_controller.CurrentInputAdaptionGain(), -0.36, 1e-6);
  mrac_controller.Reset();
  state(0, 0) = -10.0;
  double control_value =
      mrac_controller.Control(-18.0, state, dt, limit, rate_limit);
  EXPECT_NEAR(control_value, 8.48, 1e-6);
  dt = 0.0;
  EXPECT_EQ(mrac_controller.Control(-18.0, state, dt, limit, rate_limit),
            control_value);
  mrac_controller.Reset();
  // test the bounded conditions of the system output
  dt = 0.01;
  mrac_controller.Init(mrac_conf, dt);
  state(0, 0) = 10.0;
  EXPECT_NEAR(mrac_controller.Control(18.0, state, dt, 100.0, 1.0 / dt), -1.0,
              1e-6);
  EXPECT_EQ(mrac_controller.ReferenceSaturationStatus(), 2);
  EXPECT_EQ(mrac_controller.ControlSaturationStatus(), -2);
  mrac_controller.Reset();
  state(0, 0) = 10.0;
  EXPECT_NEAR(mrac_controller.Control(18.0, state, dt, 10.0, 100.0 / dt), -8.48,
              1e-6);
  EXPECT_NEAR(mrac_controller.CurrentReferenceState(), 6.0, 1e-6);
  EXPECT_EQ(mrac_controller.ReferenceSaturationStatus(), 0);
  EXPECT_EQ(mrac_controller.ControlSaturationStatus(), 0);
  EXPECT_NEAR(mrac_controller.Control(18.0, state, dt, 10.0, 100.0 / dt), -10.0,
              1e-6);
  EXPECT_NEAR(mrac_controller.CurrentReferenceState(), 10.0, 1e-6);
  EXPECT_EQ(mrac_controller.ReferenceSaturationStatus(), 1);
  EXPECT_EQ(mrac_controller.ControlSaturationStatus(), -1);
}

// test the judgement of the symetric positive definite solution of the Lyapunov
// equation
TEST_F(MracControllerTest, CheckLyapunovPD) {
  double dt = 0.01;
  MracConf mrac_conf = lat_controller_conf_.steer_mrac_conf();
  MracController mrac_controller;
  mrac_controller.Init(mrac_conf, dt);
  // test on 1st order adaption dynamics
  Matrix matrix_a = Matrix::Zero(1, 1);
  Matrix matrix_p = Matrix::Zero(1, 1);
  matrix_a(0, 0) = -100.0;
  matrix_p(0, 0) = 1.0;
  EXPECT_EQ(mrac_controller.CheckLyapunovPD(matrix_a, matrix_p), true);
  matrix_a(0, 0) = -100.0;
  matrix_p(0, 0) = -1.0;
  EXPECT_EQ(mrac_controller.CheckLyapunovPD(matrix_a, matrix_p), false);
  // test on 2nd order adaption dynamics
  matrix_a = Matrix::Zero(2, 2);
  matrix_p = Matrix::Zero(2, 2);
  matrix_a(0, 1) = 1.0;
  matrix_a(1, 0) = -100.0;
  matrix_a(1, 1) = -18.0;
  matrix_p(0, 0) = 1.0;
  matrix_p(1, 1) = 1.0;
  EXPECT_EQ(mrac_controller.CheckLyapunovPD(matrix_a, matrix_p), false);
  matrix_p(0, 0) = 1.0;
  matrix_p(1, 1) = 0.01;
  EXPECT_EQ(mrac_controller.CheckLyapunovPD(matrix_a, matrix_p), false);
  matrix_p(0, 0) = 10.0;
  matrix_p(0, 1) = 0.1;
  matrix_p(1, 0) = 0.1;
  matrix_p(1, 1) = 0.1;
  EXPECT_EQ(mrac_controller.CheckLyapunovPD(matrix_a, matrix_p), true);
}

}  // namespace control
}  // namespace apollo
