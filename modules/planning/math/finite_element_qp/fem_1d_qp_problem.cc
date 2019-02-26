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

#include "modules/planning/math/finite_element_qp/fem_1d_qp_problem.h"

#include <algorithm>
#include <chrono>

#include "cyber/common/log.h"

#include "modules/planning/common/planning_gflags.h"

namespace apollo {
namespace planning {

namespace {
constexpr double kMaxVariableRange = 1e10;
}  // namespace

Fem1dQpProblem::Fem1dQpProblem(const size_t num_of_knots,
                               const std::array<double, 3>& x_init,
                               const double delta_s,
                               const std::array<double, 5>& w,
                               const double max_x_third_order_derivative) {
  CHECK_GE(num_of_knots, 4);
  num_of_knots_ = num_of_knots;

  x_init_ = x_init;

  weight_.x_w = w[0];
  weight_.x_derivative_w = w[1];
  weight_.x_second_order_derivative_w = w[2];
  weight_.x_third_order_derivative_w = w[3];
  weight_.x_mid_line_w = w[4];

  max_x_third_order_derivative_ = max_x_third_order_derivative;

  delta_s_ = delta_s;
  delta_s_sq_ = delta_s * delta_s;

  x_bounds_.resize(num_of_knots_,
                   std::make_pair(-kMaxVariableRange, kMaxVariableRange));

  dx_bounds_.resize(num_of_knots_,
                    std::make_pair(-kMaxVariableRange, kMaxVariableRange));

  ddx_bounds_.resize(num_of_knots_,
                     std::make_pair(-kMaxVariableRange, kMaxVariableRange));
}

bool Fem1dQpProblem::OptimizeWithOsqp(
    const size_t kernel_dim, const size_t num_affine_constraint,
    std::vector<c_float>& P_data, std::vector<c_int>& P_indices,    // NOLINT
    std::vector<c_int>& P_indptr, std::vector<c_float>& A_data,     // NOLINT
    std::vector<c_int>& A_indices, std::vector<c_int>& A_indptr,    // NOLINT
    std::vector<c_float>& lower_bounds,                             // NOLINT
    std::vector<c_float>& upper_bounds,                             // NOLINT
    std::vector<c_float>& q, OSQPData* data, OSQPWorkspace** work,  // NOLINT
    OSQPSettings* settings) {
  // Define Solver settings as default
  osqp_set_default_settings(settings);
  settings->alpha = 1.0;  // Change alpha parameter
  settings->eps_abs = 1.0e-05;
  settings->eps_rel = 1.0e-05;
  settings->max_iter = 5000;
  settings->polish = true;
  settings->verbose = FLAGS_enable_osqp_debug;

  data->n = kernel_dim;
  data->m = num_affine_constraint;
  data->P = csc_matrix(data->n, data->n, P_data.size(), P_data.data(),
                       P_indices.data(), P_indptr.data());
  data->q = q.data();
  data->A = csc_matrix(data->m, data->n, A_data.size(), A_data.data(),
                       A_indices.data(), A_indptr.data());
  data->l = lower_bounds.data();
  data->u = upper_bounds.data();

  CHECK_EQ(upper_bounds.size(), lower_bounds.size());

  *work = osqp_setup(data, settings);

  // Solve Problem
  osqp_solve(*work);
  return true;
}

void Fem1dQpProblem::SetZeroOrderBounds(
    std::vector<std::pair<double, double>> x_bounds) {
  CHECK_EQ(x_bounds.size(), num_of_knots_);
  x_bounds_ = std::move(x_bounds);
}

void Fem1dQpProblem::SetFirstOrderBounds(
    std::vector<std::pair<double, double>> dx_bounds) {
  CHECK_EQ(dx_bounds.size(), num_of_knots_);
  dx_bounds_ = std::move(dx_bounds);
}

void Fem1dQpProblem::SetSecondOrderBounds(
    std::vector<std::pair<double, double>> d2x_bounds) {
  CHECK_EQ(d2x_bounds.size(), num_of_knots_);
  ddx_bounds_ = std::move(d2x_bounds);
}

void Fem1dQpProblem::SetZeroOrderBounds(const double x_bound) {
  CHECK_GT(x_bound, 0.0);
  for (auto& x : x_bounds_) {
    x.first = -x_bound;
    x.second = x_bound;
  }
}

void Fem1dQpProblem::SetFirstOrderBounds(const double dx_bound) {
  CHECK_GT(dx_bound, 0.0);
  for (auto& x : dx_bounds_) {
    x.first = -dx_bound;
    x.second = dx_bound;
  }
}

void Fem1dQpProblem::SetSecondOrderBounds(const double ddx_bound) {
  CHECK_GT(ddx_bound, 0.0);
  for (auto& x : ddx_bounds_) {
    x.first = -ddx_bound;
    x.second = ddx_bound;
  }
}

void Fem1dQpProblem::ProcessBound(
    const std::vector<std::tuple<double, double, double>>& src,
    std::vector<std::pair<double, double>>* dst) {
  DCHECK_NOTNULL(dst);

  *dst = std::vector<std::pair<double, double>>(
      num_of_knots_, std::make_pair(-kMaxVariableRange, kMaxVariableRange));

  for (size_t i = 0; i < src.size(); ++i) {
    size_t index = static_cast<size_t>(std::get<0>(src[i]) / delta_s_ + 0.5);
    if (index < dst->size()) {
      dst->at(index).first =
          std::max(dst->at(index).first, std::get<1>(src[i]));
      dst->at(index).second =
          std::min(dst->at(index).second, std::get<2>(src[i]));
    }
  }
}

// x_bounds: tuple(s, lower_bounds, upper_bounds)
void Fem1dQpProblem::SetVariableBounds(
    const std::vector<std::tuple<double, double, double>>& x_bounds) {
  ProcessBound(x_bounds, &x_bounds_);
}

// dx_bounds: tuple(s, lower_bounds, upper_bounds)
void Fem1dQpProblem::SetVariableDerivativeBounds(
    const std::vector<std::tuple<double, double, double>>& dx_bounds) {
  ProcessBound(dx_bounds, &dx_bounds_);
}

// ddx_bounds: tuple(s, lower_bounds, upper_bounds)
void Fem1dQpProblem::SetVariableSecondOrderDerivativeBounds(
    const std::vector<std::tuple<double, double, double>>& ddx_bounds) {
  ProcessBound(ddx_bounds, &ddx_bounds_);
}

void Fem1dQpProblem::SetOutputResolution(const double resolution) {
  // It is assumed that the third order derivative of x is const between each s
  // positions
  const double kEps = 1e-12;
  if (resolution < kEps || x_.empty()) {
    return;
  }
  std::vector<double> new_x;
  std::vector<double> new_dx;
  std::vector<double> new_ddx;
  std::vector<double> new_dddx;

  const double total_s = delta_s_ * (static_cast<double>(x_.size()) - 1.0);
  for (double s = resolution; s < total_s; s += resolution) {
    const size_t idx = static_cast<size_t>(std::floor(s / delta_s_));
    const double ds = s - delta_s_ * static_cast<double>(idx);

    double x = 0.0;
    double dx = 0.0;
    double d2x = 0.0;
    double d3x = 0.0;

    if (idx == 0) {
      d3x = dddx_.front();
      d2x = x_init_[2] + d3x * ds;
      dx = x_init_[1] + x_init_[2] * ds + 0.5 * d3x * ds * ds;
      x = x_init_[0] + x_init_[1] * ds + 0.5 * x_init_[2] * ds * ds +
          d3x * ds * ds * ds / 6.0;
    } else {
      d3x = dddx_[idx - 1];
      d2x = ddx_[idx - 1] + d3x * ds;
      dx = dx_[idx - 1] + ddx_[idx - 1] * ds + 0.5 * d3x * ds * ds;
      x = x_[idx - 1] + dx_[idx - 1] * ds + 0.5 * ddx_[idx - 1] * ds * ds +
          d3x * ds * ds * ds / 6.0;
    }

    new_x.push_back(x);
    new_dx.push_back(dx);
    new_ddx.push_back(d2x);
    new_ddx.push_back(d3x);
  }
  x_ = new_x;
  dx_ = new_dx;
  ddx_ = new_ddx;
  dddx_ = new_dddx;
}

bool Fem1dQpProblem::Optimize() {
  // calculate kernel
  std::vector<c_float> P_data;
  std::vector<c_int> P_indices;
  std::vector<c_int> P_indptr;
  auto start_time = std::chrono::system_clock::now();
  CalculateKernel(&P_data, &P_indices, &P_indptr);
  auto end_time1 = std::chrono::system_clock::now();
  std::chrono::duration<double> diff = end_time1 - start_time;
  ADEBUG << "Set Kernel used time: " << diff.count() * 1000 << " ms.";

  // calculate affine constraints
  std::vector<c_float> A_data;
  std::vector<c_int> A_indices;
  std::vector<c_int> A_indptr;
  std::vector<c_float> lower_bounds;
  std::vector<c_float> upper_bounds;
  CalculateAffineConstraint(&A_data, &A_indices, &A_indptr, &lower_bounds,
                            &upper_bounds);
  auto end_time2 = std::chrono::system_clock::now();
  diff = end_time2 - end_time1;
  ADEBUG << "CalculateAffineConstraint used time: " << diff.count() * 1000
         << " ms.";

  // calculate offset
  std::vector<c_float> q;
  CalculateOffset(&q);
  auto end_time3 = std::chrono::system_clock::now();
  diff = end_time3 - end_time2;
  ADEBUG << "CalculateOffset used time: " << diff.count() * 1000 << " ms.";

  OSQPData* data = reinterpret_cast<OSQPData*>(c_malloc(sizeof(OSQPData)));
  OSQPSettings* settings =
      reinterpret_cast<OSQPSettings*>(c_malloc(sizeof(OSQPSettings)));
  OSQPWorkspace* work = nullptr;

  OptimizeWithOsqp(3 * num_of_knots_, lower_bounds.size(), P_data, P_indices,
                   P_indptr, A_data, A_indices, A_indptr, lower_bounds,
                   upper_bounds, q, data, &work, settings);
  if (work == nullptr || work->solution == nullptr) {
    AERROR << "Failed to find QP solution.";
    return false;
  }

  // extract primal results
  x_.resize(num_of_knots_);
  dx_.resize(num_of_knots_);
  ddx_.resize(num_of_knots_);
  for (size_t i = 0; i < num_of_knots_; ++i) {
    x_.at(i) = work->solution->x[i];
    dx_.at(i) = work->solution->x[i + num_of_knots_];
    ddx_.at(i) = work->solution->x[i + 2 * num_of_knots_];
  }
  dx_.back() = 0.0;
  ddx_.back() = 0.0;

  // Cleanup
  osqp_cleanup(work);
  c_free(data->A);
  c_free(data->P);
  c_free(data);
  c_free(settings);
  auto end_time4 = std::chrono::system_clock::now();
  diff = end_time4 - end_time3;
  ADEBUG << "Run OptimizeWithOsqp used time: " << diff.count() * 1000 << " ms.";

  return true;
}

void Fem1dQpProblem::CalculateKernel(std::vector<c_float>* P_data,
                                     std::vector<c_int>* P_indices,
                                     std::vector<c_int>* P_indptr) {
  const int N = static_cast<int>(num_of_knots_);
  const int kNumParam = 3 * N;
  P_data->resize(kNumParam);
  P_indices->resize(kNumParam);
  P_indptr->resize(kNumParam + 1);

  for (int i = 0; i < kNumParam; ++i) {
    if (i < N) {
      P_data->at(i) = 2.0 * (weight_.x_w + weight_.x_mid_line_w);
    } else if (i < 2 * N) {
      P_data->at(i) = 2.0 * weight_.x_derivative_w;
    } else {
      P_data->at(i) = 2.0 * weight_.x_second_order_derivative_w;
    }
    P_indices->at(i) = i;
    P_indptr->at(i) = i;
  }
  P_indptr->at(kNumParam) = kNumParam;
  CHECK_EQ(P_data->size(), P_indices->size());
}

void Fem1dQpProblem::CalculateAffineConstraint(
    std::vector<c_float>* A_data, std::vector<c_int>* A_indices,
    std::vector<c_int>* A_indptr, std::vector<c_float>* lower_bounds,
    std::vector<c_float>* upper_bounds) {
  // 3N params bounds on x, x', x''
  // 3(N-1) constraints on x, x', x''
  // 3 constraints on x_init_
  const int N = static_cast<int>(num_of_knots_);
  const int kNumParam = 3 * N;
  const int kNumConstraint = kNumParam + 3 * (N - 1) + 3;
  lower_bounds->resize(kNumConstraint);
  upper_bounds->resize(kNumConstraint);

  std::vector<std::vector<std::pair<c_int, c_float>>> columns;
  columns.resize(kNumParam);
  int constraint_index = 0;

  // set x, x', x'' bounds
  for (int i = 0; i < kNumParam; ++i) {
    columns[i].emplace_back(constraint_index, 1.0);
    if (i < N) {
      lower_bounds->at(constraint_index) = std::get<0>(x_bounds_[i]);
      upper_bounds->at(constraint_index) = std::get<1>(x_bounds_[i]);
    } else if (i < 2 * N) {
      lower_bounds->at(constraint_index) = std::get<0>(dx_bounds_[i - N]);
      upper_bounds->at(constraint_index) = std::get<1>(dx_bounds_[i - N]);
    } else {
      lower_bounds->at(constraint_index) = std::get<0>(ddx_bounds_[i - 2 * N]);
      upper_bounds->at(constraint_index) = std::get<1>(ddx_bounds_[i - 2 * N]);
    }
    ++constraint_index;
  }
  CHECK_EQ(constraint_index, kNumParam);

  // x(i+1)'' - x(i)'' - x(i)''' * delta_s = 0
  for (int i = 0; i + 1 < N; ++i) {
    columns[2 * N + i].emplace_back(constraint_index, -1.0);
    columns[2 * N + i + 1].emplace_back(constraint_index, 1.0);
    lower_bounds->at(constraint_index) =
        -max_x_third_order_derivative_ * delta_s_;
    upper_bounds->at(constraint_index) =
        max_x_third_order_derivative_ * delta_s_;
    ++constraint_index;
  }

  // x(i+1)' - x(i)' - 0.5 * delta_s * (x(i+1)'' + x(i)'') = 0
  for (int i = 0; i + 1 < N; ++i) {
    columns[N + i].emplace_back(constraint_index, -1.0);
    columns[N + i + 1].emplace_back(constraint_index, 1.0);
    columns[2 * N + i].emplace_back(constraint_index, -0.5 * delta_s_);
    columns[2 * N + i + 1].emplace_back(constraint_index, -0.5 * delta_s_);
    lower_bounds->at(constraint_index) = 0.0;
    upper_bounds->at(constraint_index) = 0.0;
    ++constraint_index;
  }

  // x(i+1) - x(i) - x(i)'*delta_s - 1/3*x(i)''*delta_s^2 - 1/6*x(i)''*delta_s^2
  for (int i = 0; i + 1 < N; ++i) {
    columns[i].emplace_back(constraint_index, -1.0);
    columns[i + 1].emplace_back(constraint_index, 1.0);
    columns[N + i].emplace_back(constraint_index, -delta_s_);
    columns[2 * N + i].emplace_back(constraint_index, -delta_s_sq_ / 3.0);
    columns[2 * N + i + 1].emplace_back(constraint_index, -delta_s_sq_ / 6.0);

    lower_bounds->at(constraint_index) = 0.0;
    upper_bounds->at(constraint_index) = 0.0;
    ++constraint_index;
  }

  // constrain on x_init
  columns[0].emplace_back(constraint_index, 1.0);
  lower_bounds->at(constraint_index) = x_init_[0];
  upper_bounds->at(constraint_index) = x_init_[0];
  ++constraint_index;

  columns[N].emplace_back(constraint_index, 1.0);
  lower_bounds->at(constraint_index) = x_init_[1];
  upper_bounds->at(constraint_index) = x_init_[1];
  ++constraint_index;

  columns[2 * N].emplace_back(constraint_index, 1.0);
  lower_bounds->at(constraint_index) = x_init_[2];
  upper_bounds->at(constraint_index) = x_init_[2];
  ++constraint_index;

  CHECK_EQ(constraint_index, kNumConstraint);

  int ind_p = 0;
  for (int i = 0; i < kNumParam; ++i) {
    A_indptr->push_back(ind_p);
    for (const auto& row_data_pair : columns[i]) {
      A_data->push_back(row_data_pair.second);
      A_indices->push_back(row_data_pair.first);
      ++ind_p;
    }
  }
  A_indptr->push_back(ind_p);
}

void Fem1dQpProblem::CalculateOffset(std::vector<c_float>* q) {
  CHECK_NOTNULL(q);
  const int N = static_cast<int>(num_of_knots_);
  const int kNumParam = 3 * N;
  q->resize(kNumParam);
  for (int i = 0; i < kNumParam; ++i) {
    if (i < N) {
      q->at(i) = -2.0 * weight_.x_mid_line_w *
                 (std::get<0>(x_bounds_[i]) + std::get<1>(x_bounds_[i]));
    } else {
      q->at(i) = 0.0;
    }
  }
}

}  // namespace planning
}  // namespace apollo
