/*
 * Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
 * Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "ir/QuantumComputation.hpp"
#include "na/zoned/decomposer/NativeGateDecomposer.hpp"
#include "na/zoned/scheduler/ASAPScheduler.hpp"

#include <gmock/gmock-matchers.h>
#include <gmock/gmock-more-matchers.h>
#include <gtest/gtest.h>
#include <utility>
#include <vector>

//
// Created by cpsch on 08.04.2026.
//
namespace na::zoned {
constexpr std::string_view architectureJson = R"({
  "name": "asap_scheduler_architecture",
  "storage_zones": [{
    "zone_id": 0,
    "slms": [{"id": 0, "site_separation": [3, 3], "r": 20, "c": 20, "location": [0, 0]}],
    "offset": [0, 0],
    "dimension": [60, 60]
  }],
  "entanglement_zones": [{
    "zone_id": 0,
    "slms": [
      {"id": 1, "site_separation": [12, 10], "r": 4, "c": 4, "location": [5, 70]},
      {"id": 2, "site_separation": [12, 10], "r": 4, "c": 4, "location": [7, 70]}
    ],
    "offset": [5, 70],
    "dimension": [50, 40]
  }],
  "aods":[{"id": 0, "site_separation": 2, "r": 20, "c": 20}],
  "rydberg_range": [[[5, 70], [55, 110]]]
})";

class ThetaOptTest : public ::testing::Test {
protected:
  Architecture architecture;
  ASAPScheduler::Config schedulerConfig{.maxFillingFactor = .8};
  ASAPScheduler scheduler;
  NativeGateDecomposer::Config decomposerConfig{.thetaOptSchedule = true,
                                                .checkFinalCond = false};
  NativeGateDecomposer decomposer;
  ThetaOptTest()
      : architecture(Architecture::fromJSONString(architectureJson)),
        scheduler(architecture, schedulerConfig),
        decomposer(architecture, decomposerConfig) {}
};

constexpr static qc::fp epsilon = std::numeric_limits<qc::fp>::epsilon() * 1024;

TEST_F(ThetaOptTest, GraphTest) {
  // Circuit
  //        ┌───────┐       ┌───────┐
  //  q_0: ─┤   X   ├───■───┤   Z   ├─
  //        └───────┘   │   └───────┘
  //                    │   ┌───────┐
  //  q_1: ─────────────■───┤   X   ├─
  //                        └───────┘

  size_t n = 2;
  qc::QuantumComputation qc(n);
  qc.x(0);
  qc.cz(0, 1);
  qc.z(0);
  qc.x(1);

  auto schedule = scheduler.schedule(qc);
  auto one_qubit_gates = NativeGateDecomposer::transformToU3(schedule.first, n);
  auto graph = NativeGateDecomposer::convert_circ_to_dag(
      {one_qubit_gates, schedule.second}, n);
  EXPECT_EQ(
      std::get<NativeGateDecomposer::StructU3>(graph.get_Node_Value(0)).qubit,
      0);
  EXPECT_THAT(
      std::get<NativeGateDecomposer::StructU3>(graph.get_Node_Value(0)).angles,
      ::testing::ElementsAre(
          ::testing::DoubleNear(one_qubit_gates.front().front().angles.at(0),
                                epsilon),
          ::testing::DoubleNear(one_qubit_gates.front().front().angles.at(1),
                                epsilon),
          ::testing::DoubleNear(one_qubit_gates.front().front().angles.at(2),
                                epsilon)));
  EXPECT_THAT(graph.get_adjacent(0),
              ::testing::ElementsAre(std::pair<std::size_t, double>(1, 1.0)));

  auto tqg = std::get<std::array<qc::Qubit, 2>>(graph.get_Node_Value(1));
  EXPECT_THAT(tqg, ::testing::ElementsAre(static_cast<qc::Qubit>(0),
                                          static_cast<qc::Qubit>(1)));
  EXPECT_THAT(graph.get_adjacent(1),
              ::testing::ElementsAre(std::pair<std::size_t, double>(2, 1.0),
                                     std::pair<std::size_t, double>(3, 1.0)));

  EXPECT_EQ(
      std::get<NativeGateDecomposer::StructU3>(graph.get_Node_Value(2)).qubit,
      0);
  EXPECT_THAT(
      std::get<NativeGateDecomposer::StructU3>(graph.get_Node_Value(2)).angles,
      ::testing::ElementsAre(
          ::testing::DoubleNear(one_qubit_gates.at(1).front().angles.at(0),
                                epsilon),
          ::testing::DoubleNear(one_qubit_gates.at(1).front().angles.at(1),
                                epsilon),
          ::testing::DoubleNear(one_qubit_gates.at(1).front().angles.at(2),
                                epsilon)));
  EXPECT_THAT(graph.get_adjacent(2), ::testing::IsEmpty());

  EXPECT_EQ(
      std::get<NativeGateDecomposer::StructU3>(graph.get_Node_Value(3)).qubit,
      1);
  EXPECT_THAT(
      std::get<NativeGateDecomposer::StructU3>(graph.get_Node_Value(3)).angles,
      ::testing::ElementsAre(
          ::testing::DoubleNear(one_qubit_gates.at(1).at(1).angles.at(0),
                                epsilon),
          ::testing::DoubleNear(one_qubit_gates.at(1).at(1).angles.at(1),
                                epsilon),
          ::testing::DoubleNear(one_qubit_gates.at(1).at(1).angles.at(2),
                                epsilon)));
  EXPECT_THAT(graph.get_adjacent(3), ::testing::IsEmpty());
}

TEST_F(ThetaOptTest, SiftTest) {
  // Circuit
  //         ┌───────┐           ┌───────┐       ┌───────┐
  //  q_0: ──┤   X   ├───■───────┤   Z   ├───■───┤   Y   ├─
  //         └───────┘   │       └───────┘   │   └───────┘
  //                     │       ┌───────┐   │
  //  q_1: ──────────────■───■───┤   X   ├───│─────────────
  //                         │   └───────┘   │
  //         ┌───────┐       │   ┌───────┐   │
  //  q_1: ──┤   X   ├───────■───┤   Y   ├───■─────────────
  //         └───────┘           └───────┘

  size_t n = 3;
  qc::QuantumComputation qc(n);
  qc.x(0);
  qc.x(2);
  qc.cz(0, 1);
  qc.cz(1, 2);
  qc.z(0);
  qc.x(1);
  qc.y(2);
  qc.cz(0, 2);
  qc.y(0);

  auto schedule = scheduler.schedule(qc);
  auto one_qubit_gates = NativeGateDecomposer::transformToU3(schedule.first, n);
  auto graph = NativeGateDecomposer::convert_circ_to_dag(
      {one_qubit_gates, schedule.second}, n);

  // Perform Sift (multiple times???)
  std::vector<std::size_t> v_start = {0};
  for (std::size_t i = 0; i < graph.size(); ++i) {
    v_start.push_back(i);
  }
  std::array<std::vector<size_t>, 3> v =
      NativeGateDecomposer::sift(graph, v_start, n);

  EXPECT_THAT(v[0], ::testing::IsEmpty());
  EXPECT_THAT(v[1], ::testing::ElementsAre(0, 1));
  EXPECT_THAT(v[2], ::testing::ElementsAre(2, 3, 4, 5, 6, 7, 8));

  v = NativeGateDecomposer::sift(graph, v[2], n);

  EXPECT_THAT(v[0], ::testing::ElementsAre(2, 4));
  EXPECT_THAT(v[1], ::testing::ElementsAre(3, 5, 6));
  EXPECT_THAT(v[2], ::testing::ElementsAre(7, 8));

  v = NativeGateDecomposer::sift(graph, v[2], n);

  EXPECT_THAT(v[0], ::testing::ElementsAre(7));
  EXPECT_THAT(v[1], ::testing::ElementsAre(8));
  EXPECT_THAT(v[2], ::testing::IsEmpty());
}

TEST_F(ThetaOptTest, NextMomentsPushTest) {
  // Circuit
  //         ┌─────────────────┐             ┌───────┐
  //  q_0: ──┤ U(PI,Pi/2,PI/4) ├─────────■───┤   X   ├─────────■────
  //         └─────────────────┘         │   └───────┘         │
  //                                     │   ┌───────┐         │
  //  q_1: ──────────────────────────■───■───┤   Y   ├─────────│────
  //                                 │       └───────┘         │
  //         ┌───────────────────┐   │   ┌────────────────┐    │
  //  q_2: ──┤ U(PI/4,PI/4,PI/4) ├───■───┤ U(PI/2,0,PI/2) ├────■────
  //         └───────────────────┘       └────────────────┘
  size_t n = 3;
  qc::QuantumComputation qc(n);
  qc.u(qc::PI, qc::PI_2, qc::PI_4, 0);
  qc.u(qc::PI_4, qc::PI_4, qc::PI_4, 2);
  qc.cz(1, 2);
  qc.cz(0, 1);
  qc.x(0);
  qc.y(1);
  qc.u(qc::PI_2, 0.0, qc::PI_2, 2);
  qc.cz(0, 2);
  auto schedule = scheduler.schedule(qc);
  auto one_qubit_gates = NativeGateDecomposer::transformToU3(schedule.first, n);
  auto graph = NativeGateDecomposer::convert_circ_to_dag(
      {one_qubit_gates, schedule.second}, n);
  auto v = NativeGateDecomposer::sift(graph, {0, 1, 2, 3, 4, 5, 6, 7, 8}, n);
  NativeGateDecomposer::DiGraph<
      std::pair<std::vector<std::size_t>, std::vector<std::size_t>>>
      subproblem_graph{};
  subproblem_graph.add_Node(
      std::pair<std::vector<std::size_t>, std::vector<std::size_t>>({}, {}));
  auto v_new = NativeGateDecomposer::sift(graph, v[2], n);
  auto moments =
      NativeGateDecomposer::get_possible_moments(graph, v[1], v_new, false);

  EXPECT_EQ(moments.size(), 2);

  EXPECT_THAT(moments[0].second, ::testing::DoubleNear(qc::PI, epsilon));
  EXPECT_THAT(moments[0].first[0], ::testing::UnorderedElementsAre(0, 1));
  EXPECT_THAT(moments[0].first[1], ::testing::UnorderedElementsAre(2, 4));
  EXPECT_THAT(moments[0].first[2], ::testing::UnorderedElementsAre(3, 5, 6));
  EXPECT_THAT(moments[0].first[3], ::testing::UnorderedElementsAre(7));

  EXPECT_THAT(moments[1].second, ::testing::DoubleNear(qc::PI_4, epsilon));
  EXPECT_THAT(moments[1].first[0], ::testing::UnorderedElementsAre(1));
  EXPECT_THAT(moments[1].first[1], ::testing::UnorderedElementsAre(2));
  EXPECT_THAT(moments[1].first[2], ::testing::UnorderedElementsAre(3, 0));
  EXPECT_THAT(moments[1].first[3], ::testing::UnorderedElementsAre(4, 5, 6, 7));
}

TEST_F(ThetaOptTest, NextMomentsCond2Test) {
  // Circuit
  //         ┌─────────────────┐             ┌───────┐
  //  q_0: ──┤ U(PI,Pi/2,PI/4) ├─────■───■───┤   X   ├─────────■────
  //         └─────────────────┘     │   │   └───────┘         │
  //                                 │   │   ┌───────┐         │
  //  q_1: ──────────────────────────│───■───┤   Y   ├─────────│────
  //                                 │       └───────┘         │
  //         ┌───────────────────┐   │   ┌────────────────┐    │
  //  q_2: ──┤ U(PI/4,PI/4,PI/4) ├───■───┤ U(PI/2,0,PI/2) ├────■────
  //         └───────────────────┘       └────────────────┘
  size_t n = 3;
  qc::QuantumComputation qc(n);
  qc.u(qc::PI, qc::PI_2, qc::PI_4, 0);
  qc.u(qc::PI_4, qc::PI_4, qc::PI_4, 2);
  qc.cz(0, 2);
  qc.cz(0, 1);
  qc.x(0);
  qc.y(1);
  qc.u(qc::PI_2, 0.0, qc::PI_2, 2);
  qc.cz(0, 2);
  auto schedule = scheduler.schedule(qc);
  auto one_qubit_gates = NativeGateDecomposer::transformToU3(schedule.first, n);
  auto graph = NativeGateDecomposer::convert_circ_to_dag(
      {one_qubit_gates, schedule.second}, n);
  auto v = NativeGateDecomposer::sift(graph, {0, 1, 2, 3, 4, 5, 6, 7, 8}, n);
  NativeGateDecomposer::DiGraph<
      std::pair<std::vector<std::size_t>, std::vector<std::size_t>>>
      subproblem_graph{};
  subproblem_graph.add_Node(
      std::pair<std::vector<std::size_t>, std::vector<std::size_t>>({}, {}));
  auto v_new = NativeGateDecomposer::sift(graph, v[2], n);
  auto moments =
      NativeGateDecomposer::get_possible_moments(graph, v[1], v_new, false);

  EXPECT_EQ(moments.size(), 1);

  EXPECT_THAT(moments[0].second, ::testing::DoubleNear(qc::PI, epsilon));
  EXPECT_THAT(moments[0].first[0], ::testing::UnorderedElementsAre(0, 1));
  EXPECT_THAT(moments[0].first[1], ::testing::UnorderedElementsAre(2, 4));
  EXPECT_THAT(moments[0].first[2], ::testing::UnorderedElementsAre(3, 5, 6));
  EXPECT_THAT(moments[0].first[3], ::testing::UnorderedElementsAre(7));
}

TEST_F(ThetaOptTest, NextMomentsCond3Test) {
  // Circuit
  //         ┌──────────────────┐             ┌───────┐
  //  q_0: ──┤ U(-PI,Pi/2,PI/4) ├─────────■───┤   X   ├─────■────
  //         └──────────────────┘         │   └───────┘     │
  //                                      │   ┌───────┐     │
  //  q_1: ──────────────────────────■────■───┤   Y   ├─────│────
  //                                 │        └───────┘     │
  //         ┌───────────────────┐   │                      │
  //  q_2: ──┤ U(PI/4,PI/4,PI/4) ├───■──────────────────────■────
  //         └───────────────────┘
  size_t n = 3;
  qc::QuantumComputation qc(n);
  qc.u(-qc::PI, qc::PI_2, qc::PI_4, 0);
  qc.u(qc::PI_4, qc::PI_4, qc::PI_4, 2);
  qc.cz(1, 2);
  qc.cz(0, 1);
  qc.x(0);
  qc.y(1);
  qc.cz(0, 2);
  auto schedule = scheduler.schedule(qc);
  auto one_qubit_gates = NativeGateDecomposer::transformToU3(schedule.first, n);
  auto graph = NativeGateDecomposer::convert_circ_to_dag(
      {one_qubit_gates, schedule.second}, n);
  auto v = NativeGateDecomposer::sift(graph, {0, 1, 2, 3, 4, 5, 6, 7, 8}, n);
  NativeGateDecomposer::DiGraph<
      std::pair<std::vector<std::size_t>, std::vector<std::size_t>>>
      subproblem_graph{};
  subproblem_graph.add_Node(
      std::pair<std::vector<std::size_t>, std::vector<std::size_t>>({}, {}));
  auto v_new = NativeGateDecomposer::sift(graph, v[2], n);
  auto moments =
      NativeGateDecomposer::get_possible_moments(graph, v[1], v_new, false);

  EXPECT_EQ(moments.size(), 1);

  EXPECT_THAT(moments[0].second, ::testing::DoubleNear(qc::PI, epsilon));
  EXPECT_THAT(moments[0].first[0], ::testing::UnorderedElementsAre(0, 1));
  EXPECT_THAT(moments[0].first[1], ::testing::UnorderedElementsAre(2, 3));
  EXPECT_THAT(moments[0].first[2], ::testing::UnorderedElementsAre(4, 5));
  EXPECT_THAT(moments[0].first[3], ::testing::UnorderedElementsAre(6));
}

TEST_F(ThetaOptTest, RecursionBaseTest) {
  // Circuit
  //         ┌─────────────────┐             ┌───────┐
  //  q_0: ──┤ U(PI,Pi/2,PI/4) ├─────────■───┤   X   ├─────────■─ ─ ─
  //         └─────────────────┘         │   └───────┘         │
  //                                     │   ┌───────┐         │
  //  q_1: ──────────────────────────■───■───┤   Y   ├─────────│─ ─ ─
  //                                 │       └───────┘         │
  //         ┌───────────────────┐   │   ┌────────────────┐    │
  //  q_2: ──┤ U(PI/4,PI/4,PI/4) ├───■───┤ U(PI/2,0,PI/2) ├────■─ ─ ─
  //         └───────────────────┘       └────────────────┘
  //
  //            ┌─────────────────┐
  //  q_0: ─ ─ ─┤ U(PI/2,PI/2,PI) ├──
  //            └─────────────────┘
  //
  //  q_1: ─ ─ ──────────────────────
  //
  //  q_2: ─ ─ ──────────────────────

  size_t n = 3;
  qc::QuantumComputation qc(n);
  qc.u(qc::PI, qc::PI_2, qc::PI_4, 0);
  qc.u(qc::PI_4, qc::PI_4, qc::PI_4, 2);
  qc.cz(1, 2);
  qc.cz(0, 1);
  qc.x(0);
  qc.y(1);
  qc.u(qc::PI_2, 0.0, qc::PI_2, 2);
  qc.cz(0, 2);
  qc.y(0);
  qc.u(qc::PI_2, qc::PI_2, qc::PI, 0);

  auto schedule = scheduler.schedule(qc);
  auto one_qubit_gates = NativeGateDecomposer::transformToU3(schedule.first, n);
  auto graph = NativeGateDecomposer::convert_circ_to_dag(
      {one_qubit_gates, schedule.second}, n);
  auto v = NativeGateDecomposer::sift(graph, {0, 1, 2, 3, 4, 5, 6, 7, 8}, n);
  NativeGateDecomposer::DiGraph<
      std::pair<std::vector<std::size_t>, std::vector<std::size_t>>>
      subproblem_graph{};
  subproblem_graph.add_Node(
      std::pair<std::vector<std::size_t>, std::vector<std::size_t>>({}, {}));
  std::map<size_t, std::pair<size_t, std::array<double, 2>>> memo =
      std::map<size_t, std::pair<size_t, std::array<double, 2>>>();
  auto result = NativeGateDecomposer::schedule_remaining(
      v, graph, subproblem_graph, 0, n, false, memo);

  EXPECT_EQ(result, 5 * qc::PI_2);

  EXPECT_EQ(subproblem_graph.size(), 1 + 6);
  auto t = subproblem_graph.get_adjacent(0);
  EXPECT_THAT(subproblem_graph.get_adjacent(0),
              ::testing::UnorderedElementsAre(::testing::Pair(1, qc::PI),
                                              ::testing::Pair(4, qc::PI_4)));

  EXPECT_THAT(subproblem_graph.get_Node_Value(1).first, ::testing::IsEmpty());
  EXPECT_THAT(subproblem_graph.get_Node_Value(1).second,
              ::testing::UnorderedElementsAre(0, 1));
  EXPECT_THAT(subproblem_graph.get_adjacent(1),
              ::testing::UnorderedElementsAre(::testing::Pair(2, qc::PI)));
  EXPECT_THAT(subproblem_graph.get_Node_Value(2).first,
              ::testing::UnorderedElementsAre(2, 4));
  EXPECT_THAT(subproblem_graph.get_Node_Value(2).second,
              ::testing::UnorderedElementsAre(3, 5, 6));
  EXPECT_THAT(subproblem_graph.get_adjacent(2),
              ::testing::UnorderedElementsAre(::testing::Pair(3, qc::PI_2)));
  EXPECT_THAT(subproblem_graph.get_Node_Value(3).first,
              ::testing::UnorderedElementsAre(7));
  EXPECT_THAT(subproblem_graph.get_Node_Value(3).second,
              ::testing::UnorderedElementsAre(8));
  EXPECT_THAT(subproblem_graph.get_adjacent(3), ::testing::IsEmpty());

  EXPECT_THAT(subproblem_graph.get_Node_Value(4).first, ::testing::IsEmpty());
  EXPECT_THAT(subproblem_graph.get_Node_Value(4).second,
              ::testing::UnorderedElementsAre(1));
  EXPECT_THAT(subproblem_graph.get_adjacent(4),
              ::testing::UnorderedElementsAre(::testing::Pair(5, qc::PI)));
  EXPECT_THAT(subproblem_graph.get_Node_Value(5).first,
              ::testing::UnorderedElementsAre(2));
  EXPECT_THAT(subproblem_graph.get_Node_Value(5).second,
              ::testing::UnorderedElementsAre(0, 3));
  EXPECT_THAT(subproblem_graph.get_adjacent(5),
              ::testing::UnorderedElementsAre(::testing::Pair(6, qc::PI)));
  EXPECT_THAT(subproblem_graph.get_Node_Value(6).first,
              ::testing::UnorderedElementsAre(4));
  EXPECT_THAT(subproblem_graph.get_Node_Value(6).second,
              ::testing::UnorderedElementsAre(5, 6));
  EXPECT_THAT(subproblem_graph.get_adjacent(6),
              ::testing::UnorderedElementsAre(::testing::Pair(3, qc::PI_2)));
}

TEST_F(ThetaOptTest, CheapestPathTest) {
  // Subproblem graph!
  NativeGateDecomposer::DiGraph<
      std::pair<std::vector<std::size_t>, std::vector<std::size_t>>>
      subproblem_graph{};
  for (int i = 0; i < 14; i++) {
    subproblem_graph.add_Node(
        std::pair<std::vector<std::size_t>, std::vector<std::size_t>>({}, {}));
  }
  subproblem_graph.add_Edge(0, 1);
  subproblem_graph.add_Edge(0, 2);
  subproblem_graph.add_Edge(0, 3, 0.5);
  subproblem_graph.add_Edge(1, 4);
  subproblem_graph.add_Edge(2, 5);
  subproblem_graph.add_Edge(3, 6);
  subproblem_graph.add_Edge(3, 7);
  subproblem_graph.add_Edge(4, 8);
  subproblem_graph.add_Edge(5, 8);
  subproblem_graph.add_Edge(5, 9);
  subproblem_graph.add_Edge(6, 10);
  subproblem_graph.add_Edge(7, 11);
  subproblem_graph.add_Edge(8, 12);
  subproblem_graph.add_Edge(11, 13);
  auto leaf_nodes = NativeGateDecomposer::find_leaf_nodes(subproblem_graph);
  auto path =
      NativeGateDecomposer::find_cheapest_path(subproblem_graph, leaf_nodes);
  EXPECT_THAT(leaf_nodes, ::testing::ElementsAre(9, 10, 12, 13));
  EXPECT_THAT(path, ::testing::ElementsAre(3, 6, 10));
}

TEST_F(ThetaOptTest, BuildScheduleTest) {
  // Circuit
  //         ┌───────┐           ┌───────┐       ┌───────┐
  //  q_0: ──┤   X   ├───■───────┤   Z   ├───■───┤   Y   ├─
  //         └───────┘   │       └───────┘   │   └───────┘
  //                     │       ┌───────┐   │
  //  q_1: ──────────────■───■───┤   X   ├───│─────────────
  //                         │   └───────┘   │
  //         ┌───────┐       │   ┌───────┐   │
  //  q_2: ──┤   X   ├───────■───┤   Y   ├───■─────────────
  //         └───────┘           └───────┘

  size_t n = 3;
  qc::QuantumComputation qc(n);
  qc.x(0);
  qc.x(2);
  qc.cz(0, 1);
  qc.cz(1, 2);
  qc.z(0);
  qc.x(1);
  qc.y(2);
  qc.cz(0, 2);
  qc.y(0);

  auto asap_schedule = scheduler.schedule(qc);
  auto one_qubit_gates =
      NativeGateDecomposer::transformToU3(asap_schedule.first, n);
  auto graph = NativeGateDecomposer::convert_circ_to_dag(
      {one_qubit_gates, asap_schedule.second}, n);

  // Create Basic Subproblem graph from purely sifted schedule
  NativeGateDecomposer::DiGraph<
      std::pair<std::vector<std::size_t>, std::vector<std::size_t>>>
      subproblem_graph{};
  subproblem_graph.add_Node(
      std::pair<std::vector<std::size_t>, std::vector<std::size_t>>({}, {}));
  subproblem_graph.add_Node(
      std::pair<std::vector<std::size_t>, std::vector<std::size_t>>({},
                                                                    {0, 1}));
  subproblem_graph.add_Node(
      std::pair<std::vector<std::size_t>, std::vector<std::size_t>>({2, 4},
                                                                    {3, 5, 6}));
  subproblem_graph.add_Node(
      std::pair<std::vector<std::size_t>, std::vector<std::size_t>>({7}, {8}));
  subproblem_graph.add_Edge(0, 1,
                            NativeGateDecomposer::max_theta(graph, {0, 1}));
  subproblem_graph.add_Edge(1, 2,
                            NativeGateDecomposer::max_theta(graph, {3, 5, 6}));
  subproblem_graph.add_Edge(2, 3, NativeGateDecomposer::max_theta(graph, {8}));

  auto schedule = NativeGateDecomposer::build_schedule(graph, subproblem_graph);

  EXPECT_EQ(schedule.first.size(), 4);
  EXPECT_EQ(schedule.second.size(), 3);

  EXPECT_EQ(schedule.first.front().at(0).qubit, 0);
  EXPECT_THAT(schedule.first.front().at(0).angles,
              ::testing::ElementsAre(one_qubit_gates.at(0).at(0).angles[0],
                                     one_qubit_gates.at(0).at(0).angles[1],
                                     one_qubit_gates.at(0).at(0).angles[2]));
  EXPECT_EQ(schedule.first.at(0).at(1).qubit, 2);
  EXPECT_THAT(schedule.first.at(0).at(1).angles,
              ::testing::ElementsAre(one_qubit_gates.at(0).at(0).angles[0],
                                     one_qubit_gates.at(0).at(0).angles[1],
                                     one_qubit_gates.at(0).at(0).angles[2]));

  EXPECT_THAT(schedule.second.at(0).at(0), ::testing::ElementsAre(0, 1));

  EXPECT_TRUE(schedule.first.at(1).empty());

  EXPECT_THAT(schedule.second.at(1).at(0), ::testing::ElementsAre(1, 2));

  EXPECT_EQ(schedule.first.at(2).at(0).qubit, 0);
  EXPECT_THAT(schedule.first.at(2).at(0).angles,
              ::testing::ElementsAre(one_qubit_gates.at(1).at(0).angles[0],
                                     one_qubit_gates.at(1).at(0).angles[1],
                                     one_qubit_gates.at(1).at(0).angles[2]));
  // Two gates flipped??
  EXPECT_EQ(schedule.first.at(2).at(1).qubit, 1);
  EXPECT_THAT(schedule.first.at(2).at(1).angles,
              ::testing::ElementsAre(one_qubit_gates.at(2).at(0).angles[0],
                                     one_qubit_gates.at(2).at(0).angles[1],
                                     one_qubit_gates.at(2).at(0).angles[2]));
  EXPECT_EQ(schedule.first.at(2).at(2).qubit, 2);
  EXPECT_THAT(schedule.first.at(2).at(2).angles,
              ::testing::ElementsAre(one_qubit_gates.at(2).at(1).angles[0],
                                     one_qubit_gates.at(2).at(1).angles[1],
                                     one_qubit_gates.at(2).at(1).angles[2]));

  EXPECT_THAT(schedule.second.at(2).at(0), ::testing::ElementsAre(0, 2));

  EXPECT_EQ(schedule.first.at(3).at(0).qubit, 0);
  EXPECT_THAT(schedule.first.at(3).at(0).angles,
              ::testing::ElementsAre(one_qubit_gates.at(3).at(0).angles[0],
                                     one_qubit_gates.at(3).at(0).angles[1],
                                     one_qubit_gates.at(3).at(0).angles[2]));
}

TEST_F(ThetaOptTest, CompleteTestSmall) {
  // Circuit
  //         ┌─────────────────┐             ┌───────┐
  //  q_0: ──┤ U(PI,PI/2,PI/4) ├─────────■───┤   X   ├─────────■─ ─ ─
  //         └─────────────────┘         │   └───────┘         │
  //                                     │   ┌───────┐         │
  //  q_1: ──────────────────────────■───■───┤   Y   ├─────────│─ ─ ─
  //                                 │       └───────┘         │
  //         ┌───────────────────┐   │   ┌────────────────┐    │
  //  q_2: ──┤ U(PI/4,PI/4,PI/4) ├───■───┤ U(PI/2,0,PI/2) ├────■─ ─ ─
  //         └───────────────────┘       └────────────────┘
  //
  //            ┌─────────────────┐
  //  q_0: ─ ─ ─┤ U(PI/2,PI/2,PI) ├──
  //            └─────────────────┘
  //
  //  q_1: ─ ─ ──────────────────────
  //
  //  q_2: ─ ─ ──────────────────────

  size_t n = 3;
  qc::QuantumComputation qc(n);
  qc.u(qc::PI, qc::PI_2, qc::PI_4, 0);
  qc.u(qc::PI_4, qc::PI_4, qc::PI_4, 2);
  qc.cz(1, 2);
  qc.cz(0, 1);
  qc.x(0);
  qc.y(1);
  qc.u(qc::PI_2, 0.0, qc::PI_2, 2);
  qc.cz(0, 2);
  qc.y(0);
  qc.u(qc::PI_2, qc::PI_2, qc::PI, 0);

  auto schedule = scheduler.schedule(qc);
  auto one_qubit_gates = NativeGateDecomposer::transformToU3(schedule.first, n);
  auto theta_opt_schedule =
      decomposer.schedule_theta_opt({one_qubit_gates, schedule.second}, n);

  EXPECT_EQ(theta_opt_schedule.first.size(), 4);
  EXPECT_EQ(theta_opt_schedule.second.size(), 3);

  EXPECT_EQ(theta_opt_schedule.first.at(0).size(), 2);
  EXPECT_EQ(theta_opt_schedule.first.at(1).size(), 0);
  EXPECT_EQ(theta_opt_schedule.first.at(2).size(), 3);
  EXPECT_EQ(theta_opt_schedule.first.at(3).size(), 1);

  EXPECT_EQ(theta_opt_schedule.second.at(0).size(), 1);
  EXPECT_EQ(theta_opt_schedule.second.at(1).size(), 1);
  EXPECT_EQ(theta_opt_schedule.second.at(2).size(), 1);

  EXPECT_EQ(theta_opt_schedule.first.at(0).at(0).qubit, 0);
  // TODO: DOUBLE Nears!!!
  EXPECT_THAT(
      theta_opt_schedule.first.at(0).at(0).angles,
      ::testing::ElementsAre(
          ::testing::DoubleNear(one_qubit_gates.at(0).at(0).angles[0], epsilon),
          ::testing::DoubleNear(one_qubit_gates.at(0).at(0).angles[1], epsilon),
          ::testing::DoubleNear(one_qubit_gates.at(0).at(0).angles[2],
                                epsilon)));
  EXPECT_EQ(theta_opt_schedule.first.at(0).at(1).qubit, 2);
  EXPECT_THAT(
      theta_opt_schedule.first.at(0).at(1).angles,
      ::testing::ElementsAre(
          ::testing::DoubleNear(one_qubit_gates.at(0).at(1).angles[0], epsilon),
          ::testing::DoubleNear(one_qubit_gates.at(0).at(1).angles[1], epsilon),
          ::testing::DoubleNear(one_qubit_gates.at(0).at(1).angles[2],
                                epsilon)));

  EXPECT_THAT(theta_opt_schedule.second.at(0).front(),
              ::testing::ElementsAre(1, 2));

  EXPECT_THAT(theta_opt_schedule.second.at(1).front(),
              ::testing::ElementsAre(0, 1));
  // QUAT
  EXPECT_EQ(theta_opt_schedule.first.at(2).at(0).qubit, 2);
  EXPECT_THAT(
      theta_opt_schedule.first.at(2).at(0).angles,
      ::testing::ElementsAre(
          ::testing::DoubleNear(one_qubit_gates.at(1).at(0).angles[0], epsilon),
          ::testing::DoubleNear(one_qubit_gates.at(1).at(0).angles[1], epsilon),
          ::testing::DoubleNear(one_qubit_gates.at(1).at(0).angles[2],
                                epsilon)));
  EXPECT_EQ(theta_opt_schedule.first.at(2).at(1).qubit, 0);
  EXPECT_THAT(
      theta_opt_schedule.first.at(2).at(1).angles,
      ::testing::ElementsAre(
          ::testing::DoubleNear(one_qubit_gates.at(2).at(0).angles[0], epsilon),
          ::testing::DoubleNear(one_qubit_gates.at(2).at(0).angles[1], epsilon),
          ::testing::DoubleNear(one_qubit_gates.at(2).at(0).angles[2],
                                epsilon)));
  EXPECT_EQ(theta_opt_schedule.first.at(2).at(2).qubit, 1);
  EXPECT_THAT(
      theta_opt_schedule.first.at(2).at(2).angles,
      ::testing::ElementsAre(
          ::testing::DoubleNear(one_qubit_gates.at(2).at(1).angles[0], epsilon),
          ::testing::DoubleNear(one_qubit_gates.at(2).at(1).angles[1], epsilon),
          ::testing::DoubleNear(one_qubit_gates.at(2).at(1).angles[2],
                                epsilon)));

  EXPECT_THAT(theta_opt_schedule.second.at(2).front(),
              ::testing::ElementsAre(0, 2));

  EXPECT_EQ(theta_opt_schedule.first.at(3).at(0).qubit, 0);
  EXPECT_THAT(
      theta_opt_schedule.first.at(3).at(0).angles,
      ::testing::ElementsAre(
          ::testing::DoubleNear(one_qubit_gates.at(3).at(0).angles[0], epsilon),
          ::testing::DoubleNear(one_qubit_gates.at(3).at(0).angles[1], epsilon),
          ::testing::DoubleNear(one_qubit_gates.at(3).at(0).angles[2],
                                epsilon)));
}

TEST_F(ThetaOptTest, CompleteTest) {
  qc::QuantumComputation qc(4);
  qc.u(qc::PI, qc::PI_2, qc::PI_4, 0);
  qc.u(qc::PI_4, qc::PI_2, qc::PI_2, 1);
  qc.u(qc::PI_2, qc::PI_2, qc::PI_2, 2);
  qc.cz(1, 2);
  qc.cz(2, 3);
  qc.cz(0, 1);
  qc.u(qc::PI_2, qc::PI_4, qc::PI_2, 2);
  qc.u(qc::PI_2, qc::PI_2, qc::PI_4, 3);
  qc.cz(2, 3);
  qc.u(qc::PI, qc::PI_2, qc::PI_4, 2);
  auto schedule = scheduler.schedule(qc);
  auto res = decomposer.decompose(4, schedule);
  EXPECT_EQ(res.first.size(), 5);
}
} // namespace na::zoned
