/*
 * Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
 * Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "datastructures/SymmetricMatrix.hpp"

#include <cstddef>
#include <gtest/gtest.h>
#include <string>

namespace qc {

TEST(SymmetricMatrix, Constructors) {
  SymmetricMatrix<int> const m(3);
  EXPECT_EQ(m.size(), 3);
  SymmetricMatrix<int> m2(3, 1);
  EXPECT_EQ(m2.size(), 3);
  for (size_t i = 0; i < m2.size(); ++i) {
    for (size_t j = 0; j <= i; ++j) {
      EXPECT_EQ(m2(i, j), 1);
      EXPECT_EQ(m2(j, i), 1);
    }
  }
}

TEST(SymmetricMatrix, DifferentDataTypes) {
  SymmetricMatrix<double> const m(3);
  EXPECT_EQ(m.size(), 3);
  SymmetricMatrix<std::string> const m2(3, "1");
  EXPECT_EQ(m2.size(), 3);
  EXPECT_EQ(m2(0, 2), m2(2, 0));
  EXPECT_EQ(m2(0, 2), "1");
  SymmetricMatrix<char> const m3(3, '1');
  EXPECT_EQ(m3.size(), 3);
  EXPECT_EQ(m3(0, 1), m3(1, 0));
  EXPECT_EQ(m3(0, 1), '1');
}

TEST(SymmetricMatrix, Assignment) {
  SymmetricMatrix<int> m(3);
  m(0, 1) = 1;
  m(1, 2) = 2;
  m(0, 2) = 3;
  EXPECT_EQ(m(0, 1), 1);
  EXPECT_EQ(m(1, 0), 1);
  EXPECT_EQ(m(1, 2), 2);
  EXPECT_EQ(m(2, 1), 2);
  EXPECT_EQ(m(0, 2), 3);
  EXPECT_EQ(m(2, 0), 3);
}

} // namespace qc
