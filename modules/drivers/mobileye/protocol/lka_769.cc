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

#include "modules/drivers/mobileye/protocol/lka_769.h"
#include "modules/drivers/canbus/common/byte.h"

namespace apollo {
namespace drivers {
namespace mobileye {

using ::apollo::drivers::canbus::Byte;

const int Lka769::ID = 0x769;

void Lka769::Parse(const uint8_t* bytes, int32_t length,
                   Mobileye* mobileye) const {
  mobileye->mutable_lka_769()->set_heading_angle(heading_angle(bytes, length));
  mobileye->mutable_lka_769()->set_view_range(view_range(bytes, length));
  mobileye->mutable_lka_769()->set_view_range_availability(
      is_view_range_availability(bytes, length));
}

// config detail: {'name': 'heading_angle', 'offset': -31.9990234375,
// 'precision': 0.0009765625, 'len': 16, 'f_type': 'value', 'is_signed_var':
// False, 'physical_range': '[-0.357|0.357]', 'bit': 0, 'type': 'double',
// 'order': 'intel', 'physical_unit': '"radians"'}
double Lka769::heading_angle(const uint8_t* bytes, int32_t length) const {
  Byte t0(bytes + 1);
  int32_t x = t0.get_byte(0, 8);

  Byte t1(bytes + 0);
  int32_t t = t1.get_byte(0, 8);
  x <<= 8;
  x |= t;

  return x * 0.000977 + -31.999023;
}

// config detail: {'name': 'view_range', 'offset': 0.0, 'precision': 0.00390625,
// 'len': 15, 'f_type': 'value', 'is_signed_var': False, 'physical_range':
// '[0|127.99609375]', 'bit': 16, 'type': 'double', 'order': 'intel',
// 'physical_unit': '"meter"'}
double Lka769::view_range(const uint8_t* bytes, int32_t length) const {
  Byte t0(bytes + 3);
  int32_t x = t0.get_byte(0, 7);

  Byte t1(bytes + 2);
  int32_t t = t1.get_byte(0, 8);
  x <<= 8;
  x |= t;

  return x * 0.003906;
}

// config detail: {'name': 'view_range_availability', 'offset': 0.0,
// 'precision': 1.0, 'len': 1, 'f_type': 'valid', 'is_signed_var': False,
// 'physical_range': '[0|0]', 'bit': 31, 'type': 'bool', 'order': 'intel',
// 'physical_unit': '""'}
bool Lka769::is_view_range_availability(const uint8_t* bytes,
                                        int32_t length) const {
  Byte frame(bytes + 3);
  return frame.is_bit_1(7);
}

}  // namespace mobileye
}  // namespace drivers
}  // namespace apollo
