/******************************************************************************
 * Copyright 2018 The Apollo Authors. All Rights Reserved.
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

#include "modules/perception/inference/paddlepaddle/paddle_net.h"

#include "cyber/common/log.h"

namespace apollo {
namespace perception {
namespace inference {

PaddleNet::PaddleNet(const std::string &model_file, const std::string &param_file,
                     const std::vector<std::string> &outputs)
    : model_file_(model_file), param_file_(param_file), output_names_(outputs) {}

bool PaddleNet::Init(const std::map<std::string, std::vector<int>> &shapes) {
  paddle::AnalysisConfig config;
  config.SetModel(model_file_, param_file_);
  config.SwitchUseFeedFetchOps(false);
  if (gpu_id_ >= 0) {
    config.EnableUseGpu(MemoryPoolInitSizeMb, gpu_id_);
  }

  // init Net
  predictor_ = paddle::CreatePaddlePredictor(config);
  if (predictor_ == nullptr) {
    return false;
  }
  // TODO[KAWAI]: shapes should only include input blobs. 
  //              & a way to process multiple inputs.
  std::vector<int> input_shape;
  for (auto name_shape : shapes) {
    std::shared_ptr<paddle::ZeroCopyTensor> blob =
      predictor_->GetInputTensor(name_map_[name_shape.first]);
    if (blob != nullptr) {
      blob->Reshape(name_shape.second);
    }
    input_shape = name_shape.second;
  }
  /////////////////
  int input_num = std::accumulate(input_shape.begin(),
                                  input_shape.end(),
                                  1,
                                  std::multiplies<int>()); 
  std::vector<std::vector<float>> input_data(1);
  input_data[0].resize(input_num);
  for (int i = 0; i < input_num; i++) {
    input_data[0][i] = static_cast<float>(i % 255);
  }
  // Prepare inputs
  auto input_names = predictor_->GetInputNames();
  int index = 0;
  for (auto& name : input_names) {
    auto input_t = predictor_->GetInputTensor(name);
    input_t->Reshape(input_shape);
    input_t->copy_from_cpu(input_data[index].data());
    index += 1;
  }
  AINFO << input_shape[0];
  AINFO << input_shape[1];
  AINFO << input_shape[2];
  AINFO << input_shape[3];
  CHECK(predictor_->ZeroCopyRun());
  for (auto name : output_names_) {
    AINFO << name;
    if (name_map_.find(name) == name_map_.end()) {
      continue;
    }
    AINFO << name;
    auto paddle_blob = predictor_->GetOutputTensor(name_map_[name]);
    AINFO << name;
    std::shared_ptr<apollo::perception::base::Blob<float>> blob;
    AINFO << name;
    blob.reset(new apollo::perception::base::Blob<float>(paddle_blob->shape()));
    AINFO << name;
    AINFO << name_map_[name];
    AINFO << paddle_blob->shape()[0];
    AINFO << paddle_blob->shape()[1];
    AINFO << paddle_blob->shape()[2];
    AINFO << paddle_blob->shape()[3];
    blobs_.insert(std::make_pair(name, blob));
    AINFO << name;
  }
  for (auto name : input_names_) {
    AINFO << name;
    auto paddle_blob = predictor_->GetInputTensor(name_map_[name]);
    AINFO << name;
    if (paddle_blob == nullptr) {
      continue;
    }
    AINFO << name;
    std::shared_ptr<apollo::perception::base::Blob<float>> blob;
    AINFO << name;
    blob.reset(new apollo::perception::base::Blob<float>(paddle_blob->shape()));
    AINFO << name;
    blobs_.insert(std::make_pair(name, blob));
    AINFO << name;
  }
  return true;
}

PaddleNet::PaddleNet(const std::string &model_file, const std::string &param_file,
                     const std::vector<std::string> &outputs,
                     const std::vector<std::string> &inputs)
                    : model_file_(model_file),
                      param_file_(param_file),
                      output_names_(outputs),
                      input_names_(inputs) {}

std::shared_ptr<apollo::perception::base::Blob<float>> PaddleNet::get_blob(
    const std::string &name) {
  AINFO <<  "check1";
  AINFO << name;
  auto iter = blobs_.find(name);
  if (iter == blobs_.end()) {
    return nullptr;
  }
  return iter->second;
}

bool PaddleNet::reshape() {
  for (auto name : input_names_) {
    auto blob = this->get_blob(name);
    auto paddle_blob = predictor_->GetInputTensor(name_map_[name]);
    if (paddle_blob != nullptr && blob != nullptr) {
      paddle_blob->Reshape(blob->shape());
      std::vector<int> paddle_blob_shape = paddle_blob->shape();
      int count = std::accumulate(paddle_blob_shape.begin(),
                                  paddle_blob_shape.end(),
                                  1, 
                                  std::multiplies<int>());
      cudaMemcpy(paddle_blob->mutable_data<float>(paddle::PaddlePlace::kGPU),
                 blob->gpu_data(),
                 count * sizeof(float), cudaMemcpyDeviceToDevice);
    }
  }

  return true;
}

void PaddleNet::Infer() {
  // reshape and get input data from blob to paddle_blob.
  this->reshape();
  // If `out_blob->mutable_cpu_data()` is invoked outside,
  // HEAD will be set to CPU, and `out_blob->mutable_gpu_data()`
  // after `enqueue` will copy data from CPU to GPU,
  // which will overwrite the `inference` results.
  // `out_blob->gpu_data()` will set HEAD to SYNCED,
  // then no copy happends after `enqueue`.
  for (auto name : output_names_) {
    AINFO << "check2";
    AINFO << name;
    auto blob = get_blob(name);
    if (blob != nullptr) {
      AINFO << "check3";
      blob->gpu_data();
      AINFO << "check3";
    }
    AINFO << "check4";
  }

  predictor_->ZeroCopyRun();
  for (auto name : output_names_) {
    if (name_map_.find(name) == name_map_.end()) {
      continue;
    }
    AINFO << name;
    auto blob = get_blob(name);
    auto paddle_blob = predictor_->GetOutputTensor(name_map_[name]);
    if (paddle_blob != nullptr && blob != nullptr) {
      blob->Reshape(paddle_blob->shape());
      // TODO[KaWai] : use the output_size as the count;
      std::vector<int> paddle_blob_shape = paddle_blob->shape();
      int count = std::accumulate(paddle_blob_shape.begin(),
                                  paddle_blob_shape.end(),
                                  1,
                                  std::multiplies<int>());
       AINFO << name;
       AINFO << name_map_[name];
       AINFO << paddle_blob_shape[0];
       AINFO << paddle_blob_shape[1];
       AINFO << paddle_blob_shape[2];
       AINFO << paddle_blob_shape[3];
      //int output_size;
      //paddle::PaddlePlace* place;
      cudaMemcpy(blob->mutable_gpu_data(),
                 paddle_blob->mutable_data<float>(paddle::PaddlePlace::kGPU),
                                          //&output_size),
                 count * sizeof(float), cudaMemcpyDeviceToDevice);
      AINFO << name;
    }
  }
}

bool PaddleNet::shape(const std::string &name, std::vector<int> *res) {
  bool in_input = false;
  bool in_output = false;
  if (std::find(input_names_.begin(),
                input_names_.end(),
                name) != input_names_.end()){
    in_input = true;
  } else if (std::find(output_names_.begin(),
                       output_names_.end(),
                       name) != output_names_.end()) {
    in_output = true;
  }
  if (~in_input && ~in_output) {
    return false;
  }

  auto blob = in_input? predictor_->GetInputTensor(name_map_[name]):
                        predictor_->GetOutputTensor(name_map_[name]);
  if (blob == nullptr) {
    return false;
  }
  *res = blob->shape();
  return true;
}

}  // namespace inference
}  // namespace perception
}  // namespace apollo
