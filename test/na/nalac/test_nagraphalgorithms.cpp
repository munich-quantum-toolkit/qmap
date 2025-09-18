#include "datastructures/Layer.hpp"
#include "ir/Definitions.hpp"
#include "ir/QuantumComputation.hpp"
#include "ir/operations/OpType.hpp"
#include "na/nalac/NAGraphAlgorithms.hpp"

#include <algorithm>
#include <cstddef>
#include <gtest/gtest.h>
#include <iterator>
#include <numeric>
#include <stdexcept>
#include <tuple>
#include <unordered_set>
#include <vector>

class TestNAGraph : public testing::Test {
protected:
  qc::QuantumComputation qc;
  qc::Layer layer;
  na::nalac::InteractionGraph graph{};
  void SetUp() override {
    qc = qc::QuantumComputation(8);
    qc.cz(1, 2);
    qc.cz(1, 6);
    qc.cz(2, 7);
    qc.cz(3, 4);
    qc.cz(3, 5);
    qc.cz(4, 5);
    qc.cz(4, 7);
    qc.cz(5, 7);
    qc.cz(6, 7);
    layer = qc::Layer(qc);
    graph = layer.constructInteractionGraph(qc::OpType::Z, 1);
  }
};

TEST_F(TestNAGraph, Getter) {
  EXPECT_EQ(graph.getNEdges(), 9);
  EXPECT_EQ(graph.getNVertices(), 7);
  EXPECT_EQ(graph.getDegree(1), 2);
  EXPECT_EQ(graph.getDegree(2), 2);
  EXPECT_EQ(graph.getDegree(3), 2);
  EXPECT_EQ(graph.getDegree(4), 3);
  EXPECT_EQ(graph.getDegree(5), 3);
  EXPECT_EQ(graph.getDegree(6), 2);
  EXPECT_EQ(graph.getDegree(7), 4);
  EXPECT_THROW(std::ignore = graph.getDegree(0), std::invalid_argument);
  EXPECT_TRUE(graph.isAdjacent(1, 2));
  EXPECT_FALSE(graph.isAdjacent(1, 3));
  EXPECT_FALSE(graph.isAdjacent(1, 4));
  EXPECT_FALSE(graph.isAdjacent(1, 5));
  EXPECT_TRUE(graph.isAdjacent(1, 6));
  EXPECT_FALSE(graph.isAdjacent(1, 7));
  EXPECT_TRUE(graph.isAdjacent(2, 1));
  EXPECT_FALSE(graph.isAdjacent(2, 3));
  EXPECT_FALSE(graph.isAdjacent(2, 4));
  EXPECT_FALSE(graph.isAdjacent(2, 5));
  EXPECT_FALSE(graph.isAdjacent(2, 6));
  EXPECT_TRUE(graph.isAdjacent(2, 7));
  EXPECT_FALSE(graph.isAdjacent(3, 1));
  EXPECT_FALSE(graph.isAdjacent(3, 2));
  EXPECT_TRUE(graph.isAdjacent(3, 4));
  EXPECT_TRUE(graph.isAdjacent(3, 5));
  EXPECT_FALSE(graph.isAdjacent(3, 6));
  EXPECT_FALSE(graph.isAdjacent(3, 7));
  EXPECT_FALSE(graph.isAdjacent(4, 1));
  EXPECT_FALSE(graph.isAdjacent(4, 2));
  EXPECT_TRUE(graph.isAdjacent(4, 3));
  EXPECT_TRUE(graph.isAdjacent(4, 5));
  EXPECT_FALSE(graph.isAdjacent(4, 6));
  EXPECT_TRUE(graph.isAdjacent(4, 7));
  EXPECT_FALSE(graph.isAdjacent(5, 1));
  EXPECT_FALSE(graph.isAdjacent(5, 2));
  EXPECT_TRUE(graph.isAdjacent(5, 3));
  EXPECT_TRUE(graph.isAdjacent(5, 4));
  EXPECT_FALSE(graph.isAdjacent(5, 6));
  EXPECT_TRUE(graph.isAdjacent(5, 7));
  EXPECT_TRUE(graph.isAdjacent(6, 1));
  EXPECT_FALSE(graph.isAdjacent(6, 2));
  EXPECT_FALSE(graph.isAdjacent(6, 3));
  EXPECT_FALSE(graph.isAdjacent(6, 4));
  EXPECT_FALSE(graph.isAdjacent(6, 5));
  EXPECT_TRUE(graph.isAdjacent(6, 7));
  EXPECT_FALSE(graph.isAdjacent(7, 1));
  EXPECT_TRUE(graph.isAdjacent(7, 2));
  EXPECT_FALSE(graph.isAdjacent(7, 3));
  EXPECT_TRUE(graph.isAdjacent(7, 4));
  EXPECT_TRUE(graph.isAdjacent(7, 5));
  EXPECT_TRUE(graph.isAdjacent(7, 6));
}

TEST_F(TestNAGraph, MaxIndepSet) {
  const auto& maxIndepSet =
      na::nalac::NAGraphAlgorithms::getMaxIndependentSet(graph);
  EXPECT_EQ(maxIndepSet.size(), 3);
  EXPECT_TRUE(maxIndepSet.count(1));
  EXPECT_TRUE(maxIndepSet.count(3));
  EXPECT_TRUE(maxIndepSet.count(7));
}

TEST_F(TestNAGraph, CoveredEdges) {
  EXPECT_THROW(std::ignore = na::nalac::NAGraphAlgorithms::coveredEdges(
                   graph, std::unordered_set<qc::Qubit>{8}),
               std::invalid_argument);
}

TEST_F(TestNAGraph, Coloring) {
  const auto& maxIndepSet =
      na::nalac::NAGraphAlgorithms::getMaxIndependentSet(graph);
  std::vector queue(maxIndepSet.cbegin(), maxIndepSet.cend());
  // sort the vertices by degree in descending order
  std::sort(queue.begin(), queue.end(), [&](const auto& u, const auto& v) {
    return graph.getDegree(u) > graph.getDegree(v);
  });
  const auto& edges =
      na::nalac::NAGraphAlgorithms::coveredEdges(graph, maxIndepSet);
  const auto& coloring =
      na::nalac::NAGraphAlgorithms::colorEdges(graph, edges, queue);
  // check that adjacent edges have different colors
  for (const auto& [e, k] : coloring.first) {
    for (const auto& [f, l] : coloring.first) {
      if (e != f && (e.first == f.first || e.first == f.second ||
                     e.second == f.first || e.second == f.second)) {
        EXPECT_NE(k, l);
      }
    }
  }
  // check that all edges obey the topological sorting of the queue
  for (const auto& [e, k] : coloring.first) {
    for (const auto& [f, l] : coloring.first) {
      if (e != f) {
        qc::Qubit u = 0;
        qc::Qubit v = 0;
        if (e.first == f.first) {
          u = e.second;
          v = f.second;
        } else if (e.first == f.second) {
          u = e.second;
          v = f.first;
        } else if (e.second == f.first) {
          u = e.first;
          v = f.second;
        } else if (e.second == f.second) {
          u = e.first;
          v = f.first;
        } else {
          continue;
        }
        const auto& uIt = std::find(queue.cbegin(), queue.cend(), u);
        const auto& vIt = std::find(queue.cbegin(), queue.cend(), v);
        if (uIt != queue.cend() && vIt != queue.cend()) {
          if (std::distance(queue.cbegin(), uIt) <
              std::distance(queue.cbegin(), vIt)) {
            EXPECT_LT(k, l);
          } else {
            EXPECT_GT(k, l);
          }
        }
      }
    }
  }
}

TEST_F(TestNAGraph, SequenceOrdering) {
  const auto& sequence =
      na::nalac::NAGraphAlgorithms::computeSequence(graph, 20);
  const auto& moveable = sequence.first;
  // check that the order of moveable qubits is consistent
  auto order =
      std::accumulate(moveable[0].cbegin(), moveable[0].cend(),
                      std::vector<qc::Qubit>{}, [](auto& acc, const auto& p) {
                        acc.push_back(p.first);
                        return acc;
                      });
  std::sort(order.begin(), order.end(), [&](const auto& p, const auto& q) {
    return moveable[0].at(p) < moveable[0].at(q);
  });
  for (const auto& t : moveable) {
    EXPECT_EQ(t.size(), order.size());
    for (std::size_t i = 1; i < order.size(); ++i) {
      const auto& x1It = t.find(order[i - 1]);
      const auto& x2It = t.find(order[i]);
      EXPECT_NE(x1It, t.cend());
      EXPECT_NE(x2It, t.cend());
      EXPECT_LT(x1It->second, x2It->second);
    }
  }
}

TEST_F(TestNAGraph, InteractionExists) {
  const auto& sequence =
      na::nalac::NAGraphAlgorithms::computeSequence(graph, 20);
  const auto& moveable = sequence.first;
  const auto& fixed = sequence.second;
  // check that all interactions are part of the interaction graph
  for (const auto& seq : moveable) {
    for (const auto& s : seq) {
      const auto& p = s.first;
      const auto& x = s.second;
      const auto& qIt =
          std::find_if(fixed.cbegin(), fixed.cend(),
                       [&](const auto& q) { return q.second == x; });
      if (qIt != fixed.cend()) {
        EXPECT_TRUE(graph.isAdjacent(p, qIt->first));
      }
    }
  }
}

TEST_F(TestNAGraph, CoveredInteractions) {
  const auto& maxIndepSet =
      na::nalac::NAGraphAlgorithms::getMaxIndependentSet(graph);
  const auto& coveredEdges =
      na::nalac::NAGraphAlgorithms::coveredEdges(graph, maxIndepSet);
  // TODO for some reason this must be a vector, set gives an error
  std::vector coveredEdgesVec(coveredEdges.cbegin(), coveredEdges.cend());
  const auto& sequence =
      na::nalac::NAGraphAlgorithms::computeSequence(graph, 20);
  const auto& moveable = sequence.first;
  const auto& fixed = sequence.second;
  // check that all interactions that are covered by the independent set are
  // part of the sequence
  for (const auto& seq : moveable) {
    for (const auto& s : seq) {
      const auto p = s.first;
      const auto x = s.second;
      const auto& qIt =
          std::find_if(fixed.cbegin(), fixed.cend(),
                       [&](const auto& q) { return q.second == x; });
      if (qIt != fixed.cend()) {
        const auto q = qIt->first;
        coveredEdgesVec.erase(
            std::remove_if(coveredEdgesVec.begin(), coveredEdgesVec.end(),
                           [&](const auto& e) {
                             return (e.first == q && e.second == p) ||
                                    (e.first == p && e.second == q);
                           }),
            coveredEdgesVec.end());
      }
    }
  }
  // all edges are covered
  EXPECT_EQ(coveredEdgesVec.size(), 0);
}
