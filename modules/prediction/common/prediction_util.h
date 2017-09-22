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

#ifndef MODULES_PREDICTION_COMMON_PREDICTION_UTIL_H_
#define MODULES_PREDICTION_COMMON_PREDICTION_UTIL_H_

#include <utility>
#include <vector>

#include "modules/common/proto/pnc_point.pb.h"

namespace apollo {
namespace prediction {
namespace util {
/**
 * @brief Normalize the value by specified mean and standard deviation.
 * @param value The value to be normalized.
 * @param mean The mean used for normalization.
 * @param std The standard deviation used for normalization.
 * @return The normalized value.
 */
double Normalize(const double value, const double mean, const double std);

/**
 * @brief Sigmoid function used in neural networks as an activation function.
 * @param value The input.
 * @return The output of sigmoid function.
 */
double Sigmoid(const double value);

/**
 * @brief RELU function used in neural networks as an activation function.
 * @param value The input.
 * @return The output of RELU function.
 */
double Relu(const double value);

/**
 * @brief Solve quadratic equation.
 * @param coefficients The coefficients of quadratic equation.
 * @param roots Two roots of the equation if any.
 * @return An integer indicating the success of solving equation.
 */
int SolveQuadraticEquation(const std::vector<double>& coefficients,
                           std::pair<double, double>* roots);

/**
 * @brief Translate a point.
 * @param translate_x The translation along x-axis.
 * @param translate_y The translation along y-axis.
 * @param point The point to be translated.
 */
void TranslatePoint(const double translate_x, const double translate_y,
                    apollo::common::TrajectoryPoint* point);

}  // namespace util
}  // namespace prediction
}  // namespace apollo

#endif  // MODULES_PREDICTION_COMMON_PREDICTION_UTIL_H_
