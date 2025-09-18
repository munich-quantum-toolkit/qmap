/*
 * Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
 * Copyright (c) 2025 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "sc/Architecture.hpp"
#include "sc/utils.hpp"

#include <fstream>
#include <gtest/gtest.h>
#include <vector>

TEST(General, LoadCouplingMapNonexistentFile) {
  EXPECT_THROW(Architecture("path/that/does/not/exist"), QMAPException);
}

TEST(General, LoadCouplingMapEmptyFile) {
  std::ofstream ofs("test.arch");
  ofs.close();
  EXPECT_THROW(Architecture("test.arch"), QMAPException);
}

TEST(General, LoadCouplingMapNoQubitCount) {
  std::ofstream ofs("test.arch");
  ofs << "noqubits\n";
  ofs.close();
  EXPECT_THROW(Architecture("test.arch"), QMAPException);
}

TEST(General, LoadCouplingMapNoEdge) {
  std::ofstream ofs("test.arch");
  ofs << "1\n"
      << "noedge\n";
  ofs.close();
  EXPECT_THROW(Architecture("test.arch"), QMAPException);
}

TEST(General, LoadCalibrationDataNonexistentFile) {
  std::ofstream ofs("test.arch");
  ofs << "2\n"
      << "0 1\n";
  ofs.close();
  EXPECT_THROW(Architecture("test.arch", "path/that/does/not/exist"),
               QMAPException);
}

TEST(General, TestLineParsing) {
  const std::string line =
      "Entry1;Entry2;\"EscapedEntry1;EscapedEntry2\";Entry3";

  std::vector<std::string> data{};
  parseLine(line, ';', {'\"'}, {'\\'}, data);

  EXPECT_EQ(data[1], "Entry2");
  EXPECT_EQ(data[2], "EscapedEntry1;EscapedEntry2");
}

TEST(General, Dijkstra) {
  /*
            (6)
        .----------.
       \/          \/
  0 <-> 1 --> 2 <-> 3
    (1)   (2)   (3)
  */

  const CouplingMap cm = {{0, 1}, {1, 0}, {1, 2}, {2, 3},
                          {3, 2}, {1, 3}, {3, 1}};

  const Matrix edgeWeights = {
      {0, 1, 0, 0}, {1, 0, 2, 6}, {0, 15, 0, 3}, {0, 6, 3, 0}};

  const Matrix targetTable1 = {
      {0, 1, 3, 6}, {1, 0, 2, 5}, {10, 9, 0, 3}, {7, 6, 3, 0}};
  Matrix distanceTable{};
  Dijkstra::buildTable(cm, distanceTable, edgeWeights);
  EXPECT_EQ(distanceTable, targetTable1);
}

TEST(General, DijkstraCNOTReversal) {
  /*
  0 -> 1 <- 2 -> 3 -> 4
  */

  const CouplingMap cm = {{0, 1}, {2, 1}, {2, 3}, {3, 4}};

  const Matrix edgeWeights = {{0, 3, 0, 0, 0},
                              {3, 0, 3, 0, 0},
                              {0, 3, 0, 3, 0},
                              {0, 0, 3, 0, 3},
                              {0, 0, 0, 3, 0}};
  Matrix simpleDistanceTable{};
  Dijkstra::buildTable(cm, simpleDistanceTable, edgeWeights);
  Matrix fullDistanceTable{};
  Dijkstra::buildSingleEdgeSkipTable(simpleDistanceTable, cm, 1.,
                                     fullDistanceTable);

  const Matrix targetTable2 = {{0, 0, 3, 6, 9},
                               {1, 0, 1, 3, 6},
                               {3, 0, 0, 0, 3},
                               {6, 3, 1, 0, 0},
                               {9, 6, 4, 1, 0}};
  EXPECT_EQ(fullDistanceTable, targetTable2);
}

TEST(General, DijkstraSkipEdges) {
  /*
          0 -[2]- 1 -[5]- 2
          |               |
         [4]             [1]
          |               |
  6 -[2]- 5 -[2]- 4 -[2]- 3
  |               |
 [1]             [9]
  |               |
  7 -[1]- 8 -[1]- 9
  */

  const CouplingMap cm = {
      {0, 1}, {1, 0}, {1, 2}, {2, 1}, {2, 3}, {3, 2}, {3, 4}, {4, 3},
      {4, 5}, {5, 4}, {5, 0}, {0, 5}, {5, 6}, {6, 5}, {6, 7}, {7, 6},
      {7, 8}, {8, 7}, {8, 9}, {9, 8}, {9, 4}, {4, 9},
  };

  const Matrix edgeWeights = {
      {0, 2, 0, 0, 0, 4, 0, 0, 0, 0}, {2, 0, 5, 0, 0, 0, 0, 0, 0, 0},
      {0, 5, 0, 1, 0, 0, 0, 0, 0, 0}, {0, 0, 1, 0, 2, 0, 0, 0, 0, 0},
      {0, 0, 0, 2, 0, 2, 0, 0, 0, 9}, {4, 0, 0, 0, 2, 0, 2, 0, 0, 0},
      {0, 0, 0, 0, 0, 2, 0, 1, 0, 0}, {0, 0, 0, 0, 0, 0, 1, 0, 1, 0},
      {0, 0, 0, 0, 0, 0, 0, 1, 0, 1}, {0, 0, 0, 0, 9, 0, 0, 0, 1, 0}};

  const Matrix targetTable = {
      {0, 2, 7, 8, 6, 4, 6, 7, 8, 9},  {2, 0, 5, 6, 8, 6, 8, 9, 10, 11},
      {7, 5, 0, 1, 3, 5, 7, 8, 9, 10}, {8, 6, 1, 0, 2, 4, 6, 7, 8, 9},
      {6, 8, 3, 2, 0, 2, 4, 5, 6, 7},  {4, 6, 5, 4, 2, 0, 2, 3, 4, 5},
      {6, 8, 7, 6, 4, 2, 0, 1, 2, 3},  {7, 9, 8, 7, 5, 3, 1, 0, 1, 2},
      {8, 10, 9, 8, 6, 4, 2, 1, 0, 1}, {9, 11, 10, 9, 7, 5, 3, 2, 1, 0}};
  Matrix distanceTable{};
  Dijkstra::buildTable(cm, distanceTable, edgeWeights);
  EXPECT_EQ(distanceTable, targetTable);

  const std::vector<Matrix> edgeSkipTargetTable = {
      targetTable,
      {{0, 0, 2, 3, 2, 0, 2, 3, 4, 5},
       {0, 0, 0, 1, 3, 2, 4, 5, 6, 7},
       {2, 0, 0, 0, 1, 3, 5, 5, 4, 3},
       {3, 1, 0, 0, 0, 2, 4, 4, 3, 2},
       {2, 3, 1, 0, 0, 0, 2, 2, 1, 0},
       {0, 2, 3, 2, 0, 0, 0, 1, 2, 2},
       {2, 4, 5, 4, 2, 0, 0, 0, 1, 2},
       {3, 5, 5, 4, 2, 1, 0, 0, 0, 1},
       {4, 6, 4, 3, 1, 2, 1, 0, 0, 0},
       {5, 7, 3, 2, 0, 2, 2, 1, 0, 0}},
      {{0, 0, 0, 1, 0, 0, 0, 1, 2, 2},
       {0, 0, 0, 0, 1, 0, 2, 3, 4, 3},
       {0, 0, 0, 0, 0, 1, 3, 3, 2, 1},
       {1, 0, 0, 0, 0, 0, 2, 2, 1, 0},
       {0, 1, 0, 0, 0, 0, 0, 1, 0, 0},
       {0, 0, 1, 0, 0, 0, 0, 0, 1, 0},
       {0, 2, 3, 2, 0, 0, 0, 0, 0, 1},
       {1, 3, 3, 2, 1, 0, 0, 0, 0, 0},
       {2, 4, 2, 1, 0, 1, 0, 0, 0, 0},
       {2, 3, 1, 0, 0, 0, 1, 0, 0, 0}},
      {{0, 0, 0, 0, 0, 0, 0, 0, 1, 0},
       {0, 0, 0, 0, 0, 0, 0, 1, 2, 1},
       {0, 0, 0, 0, 0, 0, 1, 2, 1, 0},
       {0, 0, 0, 0, 0, 0, 0, 1, 0, 0},
       {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
       {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
       {0, 0, 1, 0, 0, 0, 0, 0, 0, 0},
       {0, 1, 2, 1, 0, 0, 0, 0, 0, 0},
       {1, 2, 1, 0, 0, 0, 0, 0, 0, 0},
       {0, 1, 0, 0, 0, 0, 0, 0, 0, 0}},
      {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
       {0, 0, 0, 0, 0, 0, 0, 0, 1, 0},
       {0, 0, 0, 0, 0, 0, 0, 1, 0, 0},
       {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
       {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
       {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
       {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
       {0, 0, 1, 0, 0, 0, 0, 0, 0, 0},
       {0, 1, 0, 0, 0, 0, 0, 0, 0, 0},
       {0, 0, 0, 0, 0, 0, 0, 0, 0, 0}}};
  std::vector<Matrix> edgeSkipDistanceTable = {};
  Dijkstra::buildEdgeSkipTable(cm, edgeSkipDistanceTable, edgeWeights);
  EXPECT_EQ(edgeSkipDistanceTable, edgeSkipTargetTable);
}
