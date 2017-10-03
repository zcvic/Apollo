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
 * @file
 **/
#include "modules/planning/math/smoothing_spline/spline_1d_kernel.h"

#include "glog/logging.h"
#include "gtest/gtest.h"

namespace apollo {
namespace planning {

TEST(Spline1dKernel, add_regularization) {
  std::vector<double> x_knots = {0.0, 1.0, 2.0, 3.0};
  int32_t spline_order = 4;
  Spline1dKernel kernel(x_knots, spline_order);

  std::vector<double> x_coord = {0.0, 1.0, 2.0, 3.0};
  kernel.AddRegularization(0.2);

  EXPECT_EQ(kernel.kernel_matrix().rows(), kernel.kernel_matrix().cols());
  EXPECT_EQ(kernel.kernel_matrix().rows(), spline_order * (x_knots.size() - 1));
  Eigen::MatrixXd ref_kernel_matrix = Eigen::MatrixXd::Zero(12, 12);
  // clang-format off
  ref_kernel_matrix <<
      0.2,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
        0, 0.2,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
        0,   0, 0.2,   0,   0,   0,   0,   0,   0,   0,   0,   0,
        0,   0,   0, 0.2,   0,   0,   0,   0,   0,   0,   0,   0,
        0,   0,   0,   0, 0.2,   0,   0,   0,   0,   0,   0,   0,
        0,   0,   0,   0,   0, 0.2,   0,   0,   0,   0,   0,   0,
        0,   0,   0,   0,   0,   0, 0.2,   0,   0,   0,   0,   0,
        0,   0,   0,   0,   0,   0,   0, 0.2,   0,   0,   0,   0,
        0,   0,   0,   0,   0,   0,   0,   0, 0.2,   0,   0,   0,
        0,   0,   0,   0,   0,   0,   0,   0,   0, 0.2,   0,   0,
        0,   0,   0,   0,   0,   0,   0,   0,   0,   0, 0.2,   0,
        0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, 0.2;
  // clang-format on

  for (int i = 0; i < kernel.kernel_matrix().rows(); ++i) {
    for (int j = 0; j < kernel.kernel_matrix().cols(); ++j) {
      EXPECT_DOUBLE_EQ(kernel.kernel_matrix()(i, j), ref_kernel_matrix(i, j));
    }
  }

  Eigen::MatrixXd ref_offset = Eigen::MatrixXd::Zero(12, 1);

  for (int i = 0; i < kernel.offset().rows(); ++i) {
    for (int j = 0; j < kernel.offset().cols(); ++j) {
      EXPECT_DOUBLE_EQ(kernel.offset()(i, j), ref_offset(i, j));
    }
  }
}

TEST(Spline1dKernel, add_reference_line_kernel) {
  std::vector<double> x_knots = {0.0, 1.0, 2.0, 3.0};
  int32_t spline_order = 5;
  Spline1dKernel kernel(x_knots, spline_order);
  Eigen::IOFormat OctaveFmt(Eigen::StreamPrecision, 0, ", ", ";\n", "", "", "[",
                            "]");

  std::vector<double> x_coord = {0.0, 1.0, 2.0, 3.0};
  std::vector<double> ref_x = {0.0, 0.5, 0.6, 2.0};
  kernel.AddReferenceLineKernelMatrix(x_coord, ref_x, 1.0);

  Eigen::MatrixXd ref_kernel_matrix = Eigen::MatrixXd::Zero(15, 15);
  ref_kernel_matrix << 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 1, 1, 1, 1, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1,
      1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 1, 1, 1, 1, 1;

  for (int i = 0; i < kernel.kernel_matrix().rows(); ++i) {
    for (int j = 0; j < kernel.kernel_matrix().cols(); ++j) {
      EXPECT_DOUBLE_EQ(kernel.kernel_matrix()(i, j), ref_kernel_matrix(i, j));
    }
  }

  Eigen::MatrixXd ref_offset = Eigen::MatrixXd::Zero(15, 1);
  ref_offset << 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, -5.2, -4, -4, -4, -4;
  for (int i = 0; i < kernel.offset().rows(); ++i) {
    for (int j = 0; j < kernel.offset().cols(); ++j) {
      EXPECT_DOUBLE_EQ(kernel.offset()(i, j), ref_offset(i, j));
    }
  }
}

TEST(Spline1dKernel, add_derivative_kernel_matrix_01) {
  // please see the document at docs/specs/qp_spline_path_optimizer.md for
  // details.
  std::vector<double> x_knots = {0.0, 1.0};
  int32_t spline_order = 6;
  Spline1dKernel kernel(x_knots, spline_order);
  kernel.AddDerivativeKernelMatrix(1.0);

  EXPECT_EQ(kernel.kernel_matrix().rows(), kernel.kernel_matrix().cols());
  EXPECT_EQ(kernel.kernel_matrix().rows(), spline_order * (x_knots.size() - 1));
  Eigen::MatrixXd ref_kernel_matrix = Eigen::MatrixXd::Zero(6, 6);

  // clang-format off
  ref_kernel_matrix <<
      0,       0,       0,       0,       0,       0,
      0,       1,       1,       1,       1,       1,
      0,       1, 1.33333,     1.5,     1.6, 1.66667,
      0,       1,     1.5,     1.8,       2, 2.14286,
      0,       1,     1.6,       2, 2.28571,     2.5,
      0,       1, 1.66667, 2.14286,     2.5, 2.77778;
  // clang-format on

  for (int i = 0; i < kernel.kernel_matrix().rows(); ++i) {
    for (int j = 0; j < kernel.kernel_matrix().cols(); ++j) {
      EXPECT_NEAR(kernel.kernel_matrix()(i, j), ref_kernel_matrix(i, j), 1e-5);
    }
  }

  Eigen::MatrixXd ref_offset = Eigen::MatrixXd::Zero(6, 1);

  for (int i = 0; i < kernel.offset().rows(); ++i) {
    for (int j = 0; j < kernel.offset().cols(); ++j) {
      EXPECT_DOUBLE_EQ(kernel.offset()(i, j), ref_offset(i, j));
    }
  }
}

TEST(Spline1dKernel, add_derivative_kernel_matrix_02) {
  // please see the document at docs/specs/qp_spline_path_optimizer.md for
  // details.
  std::vector<double> x_knots = {0.0, 1.0, 2.0};
  int32_t spline_order = 6;
  Spline1dKernel kernel(x_knots, spline_order);
  kernel.AddDerivativeKernelMatrix(1.0);

  EXPECT_EQ(kernel.kernel_matrix().rows(), kernel.kernel_matrix().cols());
  EXPECT_EQ(kernel.kernel_matrix().rows(), spline_order * (x_knots.size() - 1));
  Eigen::MatrixXd ref_kernel_matrix = Eigen::MatrixXd::Zero(
      spline_order * (x_knots.size() - 1), spline_order * (x_knots.size() - 1));

  // clang-format off
  ref_kernel_matrix <<
      0,       0,       0,       0,       0,       0,       0,       0,       0,       0,       0,       0,  // NOLINT
      0,       1,       1,       1,       1,       1,       0,       0,       0,       0,       0,       0,  // NOLINT
      0,       1, 1.33333,     1.5,     1.6, 1.66667,       0,       0,       0,       0,       0,       0,  // NOLINT
      0,       1,     1.5,     1.8,       2, 2.14286,       0,       0,       0,       0,       0,       0,  // NOLINT
      0,       1,     1.6,       2, 2.28571,     2.5,       0,       0,       0,       0,       0,       0,  // NOLINT
      0,       1, 1.66667, 2.14286,     2.5, 2.77778,       0,       0,       0,       0,       0,       0,  // NOLINT
      0,       0,       0,       0,       0,       0,       0,       0,       0,       0,       0,       0,  // NOLINT
      0,       0,       0,       0,       0,       0,       0,       1,       1,       1,       1,       1,  // NOLINT
      0,       0,       0,       0,       0,       0,       0,       1, 1.33333,     1.5,     1.6, 1.66667,  // NOLINT
      0,       0,       0,       0,       0,       0,       0,       1,     1.5,     1.8,       2, 2.14286,  // NOLINT
      0,       0,       0,       0,       0,       0,       0,       1,     1.6,       2, 2.28571,     2.5,  // NOLINT
      0,       0,       0,       0,       0,       0,       0,       1, 1.66667, 2.14286,     2.5, 2.77778;  // NOLINT
  // clang-format on

  for (int i = 0; i < kernel.kernel_matrix().rows(); ++i) {
    for (int j = 0; j < kernel.kernel_matrix().cols(); ++j) {
      EXPECT_NEAR(kernel.kernel_matrix()(i, j), ref_kernel_matrix(i, j), 1e-5);
    }
  }

  Eigen::MatrixXd ref_offset = Eigen::MatrixXd::Zero(kernel.offset().rows(), 1);

  for (int i = 0; i < kernel.offset().rows(); ++i) {
    for (int j = 0; j < kernel.offset().cols(); ++j) {
      EXPECT_DOUBLE_EQ(kernel.offset()(i, j), ref_offset(i, j));
    }
  }
}

TEST(Spline1dKernel, add_derivative_kernel_matrix_03) {
  // please see the document at docs/specs/qp_spline_path_optimizer.md for
  // details.
  std::vector<double> x_knots = {0.0, 0.5};
  int32_t spline_order = 6;
  Spline1dKernel kernel(x_knots, spline_order);
  kernel.AddDerivativeKernelMatrix(1.0);

  EXPECT_EQ(kernel.kernel_matrix().rows(), kernel.kernel_matrix().cols());
  EXPECT_EQ(kernel.kernel_matrix().rows(), spline_order * (x_knots.size() - 1));
  Eigen::MatrixXd ref_kernel_matrix = Eigen::MatrixXd::Zero(6, 6);

  // clang-format off
  ref_kernel_matrix <<
      0,       0,       0,       0,       0,       0,
      0,       1,       1,       1,       1,       1,
      0,       1, 1.33333,     1.5,     1.6, 1.66667,
      0,       1,     1.5,     1.8,       2, 2.14286,
      0,       1,     1.6,       2, 2.28571,     2.5,
      0,       1, 1.66667, 2.14286,     2.5, 2.77778;
  // clang-format on

  for (int i = 0; i < kernel.kernel_matrix().rows(); ++i) {
    for (int j = 0; j < kernel.kernel_matrix().cols(); ++j) {
      double param = std::pow(0.5, i + j - 1);
      EXPECT_NEAR(kernel.kernel_matrix()(i, j), param * ref_kernel_matrix(i, j),
                  1e-5);
    }
  }

  Eigen::MatrixXd ref_offset = Eigen::MatrixXd::Zero(6, 1);

  for (int i = 0; i < kernel.offset().rows(); ++i) {
    for (int j = 0; j < kernel.offset().cols(); ++j) {
      EXPECT_DOUBLE_EQ(kernel.offset()(i, j), ref_offset(i, j));
    }
  }
}

TEST(Spline1dKernel, add_second_derivative_kernel_matrix_01) {
  // please see the document at docs/specs/qp_spline_path_optimizer.md for
  // details.
  std::vector<double> x_knots = {0.0, 0.5};
  int32_t spline_order = 6;
  Spline1dKernel kernel(x_knots, spline_order);
  kernel.AddSecondOrderDerivativeMatrix(1.0);

  EXPECT_EQ(kernel.kernel_matrix().rows(), kernel.kernel_matrix().cols());
  EXPECT_EQ(kernel.kernel_matrix().rows(), spline_order * (x_knots.size() - 1));
  Eigen::MatrixXd ref_kernel_matrix = Eigen::MatrixXd::Zero(
      spline_order * (x_knots.size() - 1), spline_order * (x_knots.size() - 1));

  // clang-format off
  ref_kernel_matrix <<
      0,       0,       0,       0,       0,       0,
      0,       0,       0,       0,       0,       0,
      0,       0,       4,       6,       8,      10,
      0,       0,       6,      12,      18,      24,
      0,       0,       8,      18,    28.8,      40,
      0,       0,      10,      24,      40, 57.1429;
  // clang-format on

  for (int i = 0; i < kernel.kernel_matrix().rows(); ++i) {
    for (int j = 0; j < kernel.kernel_matrix().cols(); ++j) {
      const double param = std::pow(0.5, std::max(0, i + j - 3));
      EXPECT_NEAR(kernel.kernel_matrix()(i, j), param * ref_kernel_matrix(i, j),
                  1e-5);
    }
  }

  Eigen::MatrixXd ref_offset = Eigen::MatrixXd::Zero(kernel.offset().rows(), 1);

  for (int i = 0; i < kernel.offset().rows(); ++i) {
    for (int j = 0; j < kernel.offset().cols(); ++j) {
      EXPECT_DOUBLE_EQ(kernel.offset()(i, j), ref_offset(i, j));
    }
  }
}

TEST(Spline1dKernel, add_second_derivative_kernel_matrix_02) {
  // please see the document at docs/specs/qp_spline_path_optimizer.md for
  // details.
  std::vector<double> x_knots = {0.0, 0.5, 1.0};
  int32_t spline_order = 6;
  Spline1dKernel kernel(x_knots, spline_order);
  kernel.AddSecondOrderDerivativeMatrix(1.0);

  EXPECT_EQ(kernel.kernel_matrix().rows(), kernel.kernel_matrix().cols());
  EXPECT_EQ(kernel.kernel_matrix().rows(), spline_order * (x_knots.size() - 1));
  Eigen::MatrixXd ref_kernel_matrix = Eigen::MatrixXd::Zero(
      spline_order * (x_knots.size() - 1), spline_order * (x_knots.size() - 1));

  // clang-format off
  ref_kernel_matrix <<
     0, 0,  0,  0,    0,       0, 0, 0,  0,  0,    0,       0,
     0, 0,  0,  0,    0,       0, 0, 0,  0,  0,    0,       0,
     0, 0,  4,  6,    8,      10, 0, 0,  0,  0,    0,       0,
     0, 0,  6, 12,   18,      24, 0, 0,  0,  0,    0,       0,
     0, 0,  8, 18, 28.8,      40, 0, 0,  0,  0,    0,       0,
     0, 0, 10, 24,   40, 57.1429, 0, 0,  0,  0,    0,       0,
     0, 0,  0,  0,    0,       0, 0, 0,  0,  0,    0,       0,
     0, 0,  0,  0,    0,       0, 0, 0,  0,  0,    0,       0,
     0, 0,  0,  0,    0,       0, 0, 0,  4,  6,    8,      10,
     0, 0,  0,  0,    0,       0, 0, 0,  6, 12,   18,      24,
     0, 0,  0,  0,    0,       0, 0, 0,  8, 18, 28.8,      40,
     0, 0,  0,  0,    0,       0, 0, 0, 10, 24,   40, 57.1429;
  // clang-format on

  for (int i = 0; i < kernel.kernel_matrix().rows(); ++i) {
    for (int j = 0; j < kernel.kernel_matrix().cols(); ++j) {
      const double param = std::pow(0.5, std::max(0, i % 6 + j % 6 - 3));
      EXPECT_NEAR(kernel.kernel_matrix()(i, j), param * ref_kernel_matrix(i, j),
                  1e-6);
    }
  }

  Eigen::MatrixXd ref_offset = Eigen::MatrixXd::Zero(kernel.offset().rows(), 1);

  for (int i = 0; i < kernel.offset().rows(); ++i) {
    for (int j = 0; j < kernel.offset().cols(); ++j) {
      EXPECT_DOUBLE_EQ(kernel.offset()(i, j), ref_offset(i, j));
    }
  }
}

TEST(Spline1dKernel, add_third_derivative_kernel_matrix_01) {
  std::vector<double> x_knots = {0.0, 1.5};
  int32_t spline_order = 6;
  Spline1dKernel kernel(x_knots, spline_order);
  kernel.AddThirdOrderDerivativeMatrix(1.0);

  EXPECT_EQ(kernel.kernel_matrix().rows(), kernel.kernel_matrix().cols());
  EXPECT_EQ(kernel.kernel_matrix().rows(), spline_order * (x_knots.size() - 1));
  Eigen::MatrixXd ref_kernel_matrix = Eigen::MatrixXd::Zero(
      spline_order * (x_knots.size() - 1), spline_order * (x_knots.size() - 1));

  // clang-format off
  ref_kernel_matrix <<
      0,   0,   0,   0,   0,   0,
      0,   0,   0,   0,   0,   0,
      0,   0,   0,   0,   0,   0,
      0,   0,   0,  36,  72, 120,
      0,   0,   0,  72, 192, 360,
      0,   0,   0, 120, 360, 720;
  // clang-format on

  for (int i = 0; i < kernel.kernel_matrix().rows(); ++i) {
    for (int j = 0; j < kernel.kernel_matrix().cols(); ++j) {
      const double param = std::pow(1.5, std::max(0, i % 6 + j % 6 - 5));
      EXPECT_NEAR(kernel.kernel_matrix()(i, j), param * ref_kernel_matrix(i, j),
                  1e-6);
    }
  }

  Eigen::MatrixXd ref_offset = Eigen::MatrixXd::Zero(kernel.offset().rows(), 1);

  for (int i = 0; i < kernel.offset().rows(); ++i) {
    for (int j = 0; j < kernel.offset().cols(); ++j) {
      EXPECT_DOUBLE_EQ(kernel.offset()(i, j), ref_offset(i, j));
    }
  }
}

TEST(Spline1dKernel, add_third_derivative_kernel_matrix_02) {
  std::vector<double> x_knots = {0.0, 1.5, 3.0};
  int32_t spline_order = 6;
  Spline1dKernel kernel(x_knots, spline_order);
  kernel.AddThirdOrderDerivativeMatrix(1.0);

  EXPECT_EQ(kernel.kernel_matrix().rows(), kernel.kernel_matrix().cols());
  EXPECT_EQ(kernel.kernel_matrix().rows(), spline_order * (x_knots.size() - 1));
  Eigen::MatrixXd ref_kernel_matrix =
      Eigen::MatrixXd::Zero(spline_order, spline_order);

  // clang-format off
  ref_kernel_matrix <<
      0,   0,   0,   0,   0,   0,
      0,   0,   0,   0,   0,   0,
      0,   0,   0,   0,   0,   0,
      0,   0,   0,  36,  72, 120,
      0,   0,   0,  72, 192, 360,
      0,   0,   0, 120, 360, 720;
  // clang-format on

  for (int i = 0; i < kernel.kernel_matrix().rows(); ++i) {
    for (int j = 0; j < kernel.kernel_matrix().cols(); ++j) {
      if ((i >= 6 && j < 6) || (i < 6 && j >= 6)) {
        EXPECT_DOUBLE_EQ(kernel.kernel_matrix()(i, j), 0.0);
      } else {
        const double param = std::pow(1.5, std::max(0, i % 6 + j % 6 - 5));
        EXPECT_NEAR(kernel.kernel_matrix()(i, j),
                    param * ref_kernel_matrix(i % 6, j % 6), 1e-6);
      }
    }
  }

  Eigen::MatrixXd ref_offset = Eigen::MatrixXd::Zero(kernel.offset().rows(), 1);

  for (int i = 0; i < kernel.offset().rows(); ++i) {
    for (int j = 0; j < kernel.offset().cols(); ++j) {
      EXPECT_DOUBLE_EQ(kernel.offset()(i, j), ref_offset(i, j));
    }
  }
}

}  // namespace planning
}  // namespace apollo
