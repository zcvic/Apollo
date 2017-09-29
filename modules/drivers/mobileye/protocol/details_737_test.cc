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

#include "modules/drivers/mobileye/protocol/details_737.h"

#include "gtest/gtest.h"

namespace apollo {
namespace drivers {
namespace mobileye {

class Details737Test : public ::testing::Test {
 public:
  virtual void SetUp() {}
};

TEST_F(Details737Test, Parse) {
  Details737 details_737;
  int32_t length = 8;
  Mobileye mobileye;
  uint8_t bytes[8];

  bytes[0] = 0b00000000;
  bytes[1] = 0b01000000;
  bytes[2] = 0b00100000;
  bytes[3] = 0b00000010;
  bytes[4] = 0b00000000;
  bytes[5] = 0b00000000;
  bytes[6] = 0b00000000;
  bytes[7] = 0b00000000;

  details_737.Parse(bytes, length, &mobileye);
  EXPECT_NEAR(0.065536, mobileye.details_737().lane_curvature(), 1e-6);
  EXPECT_NEAR(0.272, mobileye.details_737().lane_heading(), 1e-6);
}

}  // namespace mobileye
}  // namespace drivers
}  // namespace apollo
