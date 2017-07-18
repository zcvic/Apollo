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

#include "modules/prediction/common/prediction_gflags.h"

// System gflags
DEFINE_string(prediction_module_name, "prediction",
    "Default prediciton module name");
DEFINE_string(prediction_conf_file, "modules/prediction/conf/prediction_conf.pb.txt",
    "Default conf file for prediction");

// Features
DEFINE_double(double_precision, 1e-6, "precision of double");
DEFINE_double(max_acc, 4.0, "Upper bound of acceleration");
DEFINE_double(min_acc, -4.0, "Lower bound of deceleration");
DEFINE_double(q_var, 0.01, "Processing noise covariance");
DEFINE_double(r_var, 0.25, "Measurement noise covariance");
DEFINE_double(p_var, 0.1, "Error covariance");
DEFINE_double(go_approach_rate, 0.995,
    "The rate to approach to the reference line of going straight");
DEFINE_double(cutin_approach_rate, 0.9,
    "The rate to approach to the reference line of lane change");