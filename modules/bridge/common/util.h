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

#pragma once

#include <cstring>
#include <memory>
#include <string>
#include "modules/bridge/common/bridge_buffer.h"

namespace apollo {
namespace bridge {

const int HEADER_BUF_SIZE = sizeof(size_t);
template <typename T>
void WriteToBuffer(BridgeBuffer<char> *buf, const std::shared_ptr<T> &pb_msg) {
  if (!buf) {
    return;
  }
  size_t msg_len = pb_msg->ByteSize();
  size_t total_size = HEADER_BUF_SIZE + msg_len;

  buf->reset(total_size);

  buf->write(0, reinterpret_cast<char *>(&msg_len), sizeof(size_t));
  pb_msg->SerializeToArray(reinterpret_cast<char *>(buf + sizeof(size_t)),
                           static_cast<int>(msg_len));
}

int GetProtoSize(const char *buf, size_t size);

}  // namespace bridge
}  // namespace apollo
