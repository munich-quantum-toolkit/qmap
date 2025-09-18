/*
 * Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
 * Copyright (c) 2025 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "na/zoned/layout_synthesizer/placer/VertexMatchingPlacer.hpp"

#include "ir/Definitions.hpp"
#include "na/zoned/Architecture.hpp"
#include "na/zoned/Types.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <functional>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <queue>
#include <sstream>
#include <stdexcept>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace na::zoned {
auto VertexMatchingPlacer::minimumWeightFullBipartiteMatching(
    const std::vector<std::vector<std::optional<double>>>& costMatrix)
    -> std::vector<std::size_t> {
  const std::size_t sizeX = costMatrix.size();
  if (sizeX == 0) {
    return {};
  }
  auto it = costMatrix.cbegin();
  const std::size_t sizeY = it->size();
  if (sizeX > sizeY) {
    throw std::invalid_argument(
        "Input matrix must have more columns than rows");
  }
  if (std::all_of(it->cbegin(), it->cend(), [](const std::optional<double>& o) {
        return o == std::nullopt;
      })) {
    throw std::invalid_argument("Input matrix must not contain empty rows");
  }
  // check the rectangular shape of input matrix, i.e., check whether all
  // consecutive rows have the same size
  for (++it; it != costMatrix.cend(); ++it) {
    if (it->size() != sizeY) {
      throw std::invalid_argument("Input matrix must be rectangular");
    }
    if (std::all_of(
            it->cbegin(), it->cend(),
            [](const std::optional<double>& o) { return o == std::nullopt; })) {
      throw std::invalid_argument("Input matrix must not contain empty rows");
    }
  }
  // for all x lists all neighbors y in increasing order of c(x, y)
  std::vector list(sizeX, std::vector<std::size_t>{});
  for (std::size_t x = 0; x < sizeX; ++x) {
    for (std::size_t y = 0; y < sizeY; ++y) {
      if (costMatrix[x][y]) {
        list[x].emplace_back(y);
      }
    }
    std::sort(list[x].begin(), list[x].end(),
              [x, &costMatrix](const std::size_t a, const std::size_t b) {
                return costMatrix[x][a] < costMatrix[x][b];
              });
  }
  // initialize the set of free sources
  std::vector freeSources(sizeX, true);
  // initialize the set of free targets
  std::vector freeDestinations(sizeY, true);
  // initialize the matching
  std::vector<std::optional<std::size_t>> invMatching(sizeY, std::nullopt);
  std::size_t sizeMatching = 0;
  std::vector quantitiesX(sizeX, 0.0);
  std::vector quantitiesY(sizeY, 0.0);
  std::vector potentialsX(sizeX, 0.0);
  std::vector potentialsY(sizeY, 0.0);
  double maxPotential = 0.0;
  while (sizeMatching < sizeX) {
    std::vector<std::size_t> pathSetX(sizeX, 0);
    std::vector<std::size_t> pathSetY(sizeY, 0);
    // items have the form (<special>, <x>, <y>, <cost>)
    // if special, the iterator to the edge in list is stored in the
    // optional
    std::priority_queue<
        std::tuple<double, std::size_t, std::size_t,
                   std::optional<std::vector<std::size_t>::const_iterator>>,
        std::vector<std::tuple<
            double, std::size_t, std::size_t,
            std::optional<std::vector<std::size_t>::const_iterator>>>,
        std::greater<>>
        queue{};
    std::vector residueSetX = freeSources;
    std::vector residueSetY(sizeY, false);
    for (std::size_t x = 0; x < sizeX; ++x) {
      if (residueSetX[x]) {
        quantitiesX[x] = 0.0;
        const auto listIt = list[x].cbegin();
        const auto y = *listIt;
        queue.emplace(quantitiesX[x] + *costMatrix[x][y] + potentialsX[x] -
                          maxPotential,
                      x, y, listIt);
      }
    }
    // intersection of `remainder_set` and `freeDestinations` is empty
    std::vector intersection(sizeY, false);
    std::size_t x = 0;
    std::size_t y = 0;
    while (std::all_of(intersection.cbegin(), intersection.cend(),
                       [](const bool b) { return !b; })) {
      // select regular item from queue
      bool special = false;
      do {
        const auto& itm = queue.top();
        auto optIt = std::get<3>(itm);
        special = optIt.has_value();
        x = std::get<1>(itm);
        y = std::get<2>(itm);
        queue.pop();
        if (special) {
          if (list[x].back() != y) {
            const auto w = *(++(*optIt));
            queue.emplace(quantitiesX[x] + *costMatrix[x][w] + potentialsX[x] -
                              maxPotential,
                          x, w, *optIt);
          }
          queue.emplace(quantitiesX[x] + *costMatrix[x][y] + potentialsX[x] -
                            potentialsY[y],
                        x, y, std::nullopt);
        }
      } while (special || invMatching[y] == x);
      // select regular item from queue - done
      if (!residueSetY[y]) {
        pathSetY[y] = x;
        residueSetY[y] = true;
        intersection[y] = freeDestinations[y];
        quantitiesY[y] = quantitiesX[x] + *costMatrix[x][y] + potentialsX[x] -
                         potentialsY[y];
        if (!freeDestinations[y]) {
          const auto v = *invMatching[y];
          pathSetX[v] = y;
          residueSetX[v] = true;
          quantitiesX[v] = quantitiesY[y];
          const auto itW = list[v].cbegin();
          const auto w = *itW;
          queue.emplace(quantitiesX[v] + *costMatrix[v][w] + potentialsX[v] -
                            maxPotential,
                        v, w, itW);
        }
      }
    }
    // reset maxPotential and update quantities and potentials
    // afterward maxPotential will have the correct value again
    maxPotential = std::numeric_limits<double>::min();
    for (std::size_t v = 0; v < sizeY; ++v) {
      if (!residueSetY[v]) {
        quantitiesY[v] = quantitiesY[y];
      }
      potentialsY[v] += quantitiesY[v];
      maxPotential = std::max(potentialsY[v], maxPotential);
    }
    for (std::size_t v = 0; v < sizeX; ++v) {
      if (!residueSetX[v]) {
        quantitiesX[v] = quantitiesY[y];
      }
      potentialsX[v] += quantitiesX[v];
      maxPotential = std::max(potentialsX[v], maxPotential);
    }
    freeDestinations[y] = false;
    ++sizeMatching;
    while (true) {
      x = pathSetY[y];
      invMatching[y] = x;
      if (freeSources[x]) {
        freeSources[x] = false;
        break;
      }
      y = pathSetX[x];
    }
  }
  std::vector<std::size_t> matching(sizeX, 0);
  for (std::size_t y = 0; y < sizeY; ++y) {
    if (const auto optX = invMatching[y]) {
      matching[*optX] = y;
    }
  }
  return matching;
}
auto VertexMatchingPlacer::computeMovementCostBetweenPlacements(
    const Placement& placementBefore, const Placement& placementAfter) const
    -> double {
  double cost = 0;
  std::unordered_map<std::pair<size_t, size_t>, double>
      movementParallelMovement;

  for (std::size_t q = 0; q < placementBefore.size(); ++q) {
    const auto& [slm1, r1, c1] = placementBefore[q];
    const auto& [slm2, r2, c2] = placementAfter[q];
    if (&slm1.get() != &slm2.get() || r1 != r2 || c1 != c2) {
      const auto y1 = architecture_.get().exactSLMLocation(slm1, r1, c1).second;
      const auto y2 = architecture_.get().exactSLMLocation(slm2, r2, c2).second;
      const double dis =
          architecture_.get().distance(slm1, r1, c1, slm2, r2, c2);
      if (const auto& [it, inserted] =
              movementParallelMovement.try_emplace(std::pair{y1, y2}, dis);
          !inserted) {
        it->second = std::max(it->second, dis);
      }
    }
  }
  for (const auto& [_, value] : movementParallelMovement) {
    cost += std::sqrt(value);
  }
  return cost;
}
auto VertexMatchingPlacer::computeLayersMovementCost(
    const Placement& placementBefore, const Placement& placementBetween,
    const Placement& placementAfter) const -> double {
  return computeMovementCostBetweenPlacements(placementBefore,
                                              placementBetween) +
         computeMovementCostBetweenPlacements(placementBetween, placementAfter);
}
auto VertexMatchingPlacer::filterMapping(
    const Placement& previousGatePlacement,
    const std::pair<Placement, Placement>& placementsWithoutReuse,
    const std::pair<Placement, Placement>& placementsWithReuse) const
    -> std::pair<Placement, Placement> {
  const auto& [qubitPlacementWithoutReuse, gatePlacementWithoutReuse] =
      placementsWithoutReuse;
  const double costNoReuse = computeLayersMovementCost(
      previousGatePlacement, qubitPlacementWithoutReuse,
      gatePlacementWithoutReuse);
  // now calculate cost with reuse
  const auto& [qubitPlacementWithReuse, gatePlacementWithReuse] =
      placementsWithReuse;
  const double costReuse = computeLayersMovementCost(
      previousGatePlacement, qubitPlacementWithReuse, gatePlacementWithReuse);
  const auto nQubit = previousGatePlacement.size();
  if (costAtomTransfer_ * std::pow((1 - (costNoReuse / 1.5e6)), nQubit) >
      std::pow((1 - (costReuse / 1.5e6)), nQubit)) {
    return {qubitPlacementWithoutReuse, gatePlacementWithoutReuse};
  }
  return {qubitPlacementWithReuse, gatePlacementWithReuse};
}
auto VertexMatchingPlacer::placeGatesInEntanglementZone(
    const Placement& previousQubitPlacement,
    const std::unordered_set<qc::Qubit>& reuseQubits,
    const TwoQubitGateLayer& twoQubitGates,
    const TwoQubitGateLayer& nextTwoQubitGates, const bool reuse) const
    -> Placement {
  std::unordered_map<qc::Qubit, qc::Qubit> dictReuseQubitNeighbor;
  if (!nextTwoQubitGates.empty() and reuse) {
    for (const auto q : reuseQubits) {
      for (const auto& gate : nextTwoQubitGates) {
        if (q == gate.front()) {
          dictReuseQubitNeighbor.emplace(q, gate.back());
          break;
        }
        if (q == gate.back()) {
          dictReuseQubitNeighbor.emplace(q, gate.front());
          break;
        }
      }
    }
  }
  const auto expandFactor =
      static_cast<size_t>(std::ceil(std::sqrt(twoQubitGates.size() / 2)));
  std::unordered_map<
      std::tuple<std::reference_wrapper<const std::array<SLM, 2>>, size_t,
                 size_t>,
      size_t>
      siteRydbergToIdx;
  std::vector<std::tuple<std::reference_wrapper<const std::array<SLM, 2>>,
                         size_t, size_t>>
      listRydberg;
  // WARNING: The role of listColCoo and listRowCoo is swapped compared to
  // the original implementation because our matching algorithm only
  // supports a single direction, i.e., fewer rows than columns.
  std::vector<size_t> listRowCoo;
  std::vector<size_t> listColCoo;
  std::vector<double> listData;
  for (size_t i = 0; i < twoQubitGates.size(); ++i) {
    const auto& [q1, q2] = twoQubitGates[i];
    // a set of possible locations sites for one operand of the gate
    std::unordered_set<std::tuple<
        std::reference_wrapper<const std::array<SLM, 2>>, size_t, size_t>>
        nearestSites;
    if (reuse && reuseQubits.find(q1) != reuseQubits.end()) {
      const auto& [slm, r, c] = previousQubitPlacement[q1];
      assert(slm.get().isEntanglement());
      nearestSites.emplace(*slm.get().entanglementZone_, r, c);
    } else if (reuse && reuseQubits.find(q2) != reuseQubits.end()) {
      const auto& [slm, r, c] = previousQubitPlacement[q2];
      assert(slm.get().isEntanglement());
      nearestSites.emplace(*slm.get().entanglementZone_, r, c);
    } else {
      const auto& [slm1, r1, c1] = previousQubitPlacement[q1];
      const auto& [slm2, r2, c2] = previousQubitPlacement[q2];
      // add the nearest entanglement site as well as the site in the same
      // column on top and bottom to the set
      const auto& [nearestSLM, nearestR, nearestC] =
          architecture_.get().nearestEntanglementSite(slm1, r1, c1, slm2, r2,
                                                      c2);
      nearestSites.emplace(*nearestSLM.get().entanglementZone_, nearestR,
                           nearestC);
      const auto& [topSLM, topR, topC] =
          architecture_.get().nearestEntanglementSite(slm1, 0, c1, slm2, 0, c2);
      nearestSites.emplace(*topSLM.get().entanglementZone_, topR, topC);
      const auto& [bottomSLM, bottomR, bottomC] =
          architecture_.get().nearestEntanglementSite(
              slm1, slm1.get().nRows - 1, c1, slm2, slm2.get().nRows - 1, c2);
      nearestSites.emplace(*bottomSLM.get().entanglementZone_, bottomR,
                           bottomC);
      for (const auto& nearestSite : nearestSites) {
        nearestSites.emplace(nearestSite);
        const auto& [slm, slm_r, slm_c] = nearestSite;
        auto low_r = slm_r > expandFactor ? slm_r - expandFactor : 0;
        auto high_r =
            std::min(slm.get().front().nRows, slm_r + expandFactor + 1);
        auto low_c = slm_c > expandFactor ? slm_c - expandFactor : 0;
        auto high_c =
            std::min(slm.get().front().nCols, slm_c + expandFactor + 1);
        if (high_c - low_c < 2 * expandFactor) {
          const auto heightGap = static_cast<size_t>(std::ceil(
                                     static_cast<double>(twoQubitGates.size()) /
                                     static_cast<double>(high_c - low_c))) -
                                 expandFactor;
          low_r = low_r > (heightGap / 2) ? low_r - (heightGap / 2) : 0;
          high_r = std::min(slm.get().front().nRows,
                            low_r + heightGap + expandFactor);
        }
        if (high_r - low_r < 2 * expandFactor) {
          const auto widthGap = static_cast<size_t>(std::ceil(
                                    static_cast<double>(twoQubitGates.size()) /
                                    static_cast<double>(high_r - low_r))) -
                                expandFactor;
          low_c = low_c > (widthGap / 2) ? low_c - (widthGap / 2) : 0;
          high_c = std::min(slm.get().front().nCols,
                            low_c + widthGap + expandFactor);
        }
        for (size_t r = low_r; r < high_r; ++r) {
          for (size_t c = low_c; c < high_c; ++c) {
            nearestSites.emplace(slm, r, c);
          }
        }
      }
    }
    for (const auto& site : nearestSites) {
      if (siteRydbergToIdx.find(site) == siteRydbergToIdx.end()) {
        siteRydbergToIdx.emplace(site, listRydberg.size());
        listRydberg.emplace_back(site);
      }
      const auto idxRydberg = siteRydbergToIdx.at(site);
      const auto& [slm, r, c] = site;
      const auto& [slm1, r1, c1] = previousQubitPlacement[q1];
      const auto& [slm2, r2, c2] = previousQubitPlacement[q2];
      const double dis1 =
          architecture_.get().distance(slm1, r1, c1, slm.get().front(), r, c);
      const double dis2 =
          architecture_.get().distance(slm2, r2, c2, slm.get().front(), r, c);
      // lookahead for the next gate
      double dis3 = 0;
      std::optional<size_t> q3;
      if (dictReuseQubitNeighbor.find(q1) != dictReuseQubitNeighbor.end()) {
        q3 = dictReuseQubitNeighbor.at(q1);
      } else if (dictReuseQubitNeighbor.find(q2) !=
                 dictReuseQubitNeighbor.end()) {
        q3 = dictReuseQubitNeighbor.at(q2);
      }
      if (q3) {
        const auto& [slm3, r3, c3] = previousQubitPlacement[*q3];
        dis3 =
            architecture_.get().distance(slm3, r3, c3, slm.get().front(), r, c);
      }
      listColCoo.emplace_back(idxRydberg);
      listRowCoo.emplace_back(i);
      if (&slm1.get() == &slm2.get() && r1 == r2) {
        listData.emplace_back(std::sqrt(std::max(dis1, dis2)) +
                              std::sqrt(dis3));
      } else {
        listData.emplace_back(std::sqrt(dis1) + std::sqrt(dis2) +
                              std::sqrt(dis3));
      }
    }
  }

  if (listRydberg.size() < twoQubitGates.size()) {
    std::stringstream ss;
    ss << "[Error] QMAP: Minimum-weight-full-matching-based intermediate "
          "placement: No enough sites for gates ("
       << listRydberg.size() << " vs " << twoQubitGates.size() << ").";
    throw std::invalid_argument(ss.str());
  }

  std::vector matrix(
      twoQubitGates.size(),
      std::vector<std::optional<double>>(listRydberg.size(), std::nullopt));
  for (size_t i = 0; i < listRowCoo.size(); ++i) {
    matrix[listRowCoo[i]][listColCoo[i]] = listData[i];
  }
  const auto& matching = minimumWeightFullBipartiteMatching(matrix);
  double cost = 0;
  for (size_t i = 0; i < matching.size(); ++i) {
    cost = cost + matrix[i][matching[i]].value();
  }
  auto newPlacement = previousQubitPlacement;
  for (size_t idxGate = 0; idxGate < matching.size(); ++idxGate) {
    const auto q0 = twoQubitGates[idxGate].front();
    const auto q1 = twoQubitGates[idxGate].back();
    const auto& [zone, r, c] = listRydberg[matching[idxGate]];
    if (reuse && (reuseQubits.find(q0) != reuseQubits.end())) {
      // q0 remains at its current location, place q1 at the other site of
      // this pair of entanglement sites
      if (const auto& leftSite = std::tie(zone.get().front(), r, c);
          leftSite == previousQubitPlacement[q0]) {
        newPlacement[q1] = std::tie(zone.get().back(), r, c);
      } else {
        newPlacement[q1] = leftSite;
      }
    } else if (reuse && (reuseQubits.find(q1) != reuseQubits.end())) {
      // q1 remains at its current location, place q0 at the other site of
      // this pair of entanglement sites
      if (const auto& leftSite = std::tie(zone.get().front(), r, c);
          leftSite == previousQubitPlacement[q1]) {
        newPlacement[q0] = std::tie(zone.get().back(), r, c);
      } else {
        newPlacement[q0] = leftSite;
      }
    } else {
      // avoid crossings, i.e., place the left qubit in the left site and the
      // right qubit in the right site
      const auto c0 = std::get<2>(previousQubitPlacement[q0]);
      const auto c1 = std::get<2>(previousQubitPlacement[q1]);
      if (c0 < c1) {
        newPlacement[q0] = std::tie(zone.get().front(), r, c);
        newPlacement[q1] = std::tie(zone.get().back(), r, c);
      } else {
        newPlacement[q0] = std::tie(zone.get().back(), r, c);
        newPlacement[q1] = std::tie(zone.get().front(), r, c);
      }
    }
  }
  return newPlacement;
}
auto VertexMatchingPlacer::placeAtomsInStorageZone(
    const Placement& initialPlacement, const Placement& previousGatePlacement,
    const std::unordered_set<qc::Qubit>& reuseQubits,
    const TwoQubitGateLayer& nextTwoQubitGates, const bool reuse) const
    -> Placement {
  // for each storage SLM array, construct a matrix indicating occupancy of
  // sites
  SLMMap<std::vector<std::vector<bool>>> isEmptyStorageSite{};
  // for each SLM array, initialize the site as empty
  for (const auto& slm : architecture_.get().storageZones) {
    isEmptyStorageSite.emplace(
        *slm, std::vector(slm->nRows, std::vector(slm->nCols, true)));
  }
  // qubits that need to be placed, in particular, this does not include
  // qubits that are reused NOTE: the indices stored in qubitToPlace are
  // the indices of the lastGateMapping and do not refer to the actual
  // index of a qubit
  std::vector<qc::Qubit> qubitToPlace{};
  // go through the placement of qubits after the last gate
  for (qc::Qubit q = 0; q < previousGatePlacement.size(); ++q) {
    const auto& [slm, r, c] = previousGatePlacement[q];
    if (isEmptyStorageSite.find(slm) != isEmptyStorageSite.end()) {
      // the mapped qubit is in the storage zone, set the site as occupied
      isEmptyStorageSite.at(slm)[r][c] = false;
    } else if (!reuse || reuseQubits.find(q) == reuseQubits.end()) {
      // the mapped qubit is in the entangling zone and must be placed (if
      // not reused)
      qubitToPlace.emplace_back(q);
    }
  }
  // occupied sites in the initial mapping that are currently unoccupied;
  // those are used as candidate sites for qubit placement as it is
  // guaranteed that they exist
  SiteSet commonSite{};
  for (const auto& [slm, r, c] : initialPlacement) {
    if (isEmptyStorageSite.find(slm) != isEmptyStorageSite.end()) {
      if (isEmptyStorageSite.at(slm)[r][c]) {
        // add site to common sites if it is currently unoccupied
        commonSite.emplace(slm, r, c);
      }
    } else {
      // site is in an entangling zone and not yet added to
      // isEmptyStorageSite
      isEmptyStorageSite.emplace(
          slm,
          std::vector(slm.get().nRows, std::vector(slm.get().nCols, true)));
      commonSite.emplace(slm, r, c);
    }
  }
  // dictionary to store the qubit interactions with other qubits
  std::unordered_map<qc::Qubit, std::vector<qc::Qubit>> dictQubitInteraction{};
  for (const auto q : qubitToPlace) {
    dictQubitInteraction.emplace(q, std::vector<qc::Qubit>{});
  }
  if (!nextTwoQubitGates.empty()) {
    for (const auto& gate : nextTwoQubitGates) {
      if (dictQubitInteraction.find(gate.front()) !=
              dictQubitInteraction.end() &&
          ((!reuse) || (reuseQubits.find(gate.back()) == reuseQubits.end()))) {
        dictQubitInteraction[gate.front()].emplace_back(gate.back());
      }
      if (dictQubitInteraction.find(gate.back()) !=
              dictQubitInteraction.end() &&
          ((!reuse) || (reuseQubits.find(gate.front()) == reuseQubits.end()))) {
        dictQubitInteraction[gate.back()].emplace_back(gate.front());
      }
    }
  }
  const size_t expandFactor = 1;

  SiteMap<size_t> siteStorageToIdx{};
  std::vector<Site> listStorage{};
  std::vector<size_t> listColCoo{};
  std::vector<size_t> listRowCoo{};
  std::vector<double> listData{};

  for (size_t i = 0; i < qubitToPlace.size(); ++i) {
    const auto q = qubitToPlace[i];
    SLMMap<std::tuple<size_t, size_t, size_t, size_t>> dictBoudingbox{};
    const auto& [slm, r, c] = initialPlacement[q];
    auto lowerRow = r;
    auto upperRow = lowerRow;
    auto leftCol = c;
    auto rightCol = leftCol;
    const std::pair<size_t, size_t>& exactLocQ =
        architecture_.get().exactSLMLocation(slm, r, c);
    const auto& [initSLM, initRow, initCol] = initialPlacement[q];
    const std::pair<size_t, size_t>& exactLocGate =
        architecture_.get().exactSLMLocation(initSLM, initRow, initCol);
    if (exactLocGate.second < exactLocQ.second) {
      lowerRow = 0;
    } else {
      upperRow = slm.get().nRows;
    }

    dictBoudingbox.emplace(std::get<0>(initialPlacement[q]),
                           std::tuple{lowerRow, upperRow, leftCol, rightCol});
    for (const qc::Qubit neighbor : dictQubitInteraction.at(q)) {
      const auto& [tmpSLM, tmpRow, tmpCol] = previousGatePlacement[neighbor];
      const auto& [neighborSLM, neighborRow, neighborCol] =
          tmpSLM.get().isEntanglement()
              ? architecture_.get().nearestStorageSite(tmpSLM, tmpRow, tmpCol)
              : previousGatePlacement[neighbor];
      if (const auto& it = dictBoudingbox.find(neighborSLM);
          it != dictBoudingbox.end()) {
        const auto& [lower, upper, left, right] = it->second;
        it->second = std::tuple{
            std::min(neighborRow, lower), std::max(neighborRow, upper),
            std::min(neighborCol, left), std::max(neighborCol, right)};
      } else {
        lowerRow = neighborRow;
        upperRow = neighborRow;
        leftCol = neighborCol;
        rightCol = neighborCol;
        const auto y =
            architecture_.get()
                .exactSLMLocation(neighborSLM, neighborRow, neighborCol)
                .second;
        if (exactLocGate.second < y) {
          lowerRow = 0;
        } else {
          upperRow = neighborSLM.get().nRows;
        }
        it->second = std::tuple{lowerRow, upperRow, leftCol, rightCol};
      }
    }
    const auto& [gateSLM, gateRow, gateCol] = previousGatePlacement[q];
    const auto& [nearestSLM, nearestRow, nearestCol] =
        architecture_.get().nearestStorageSite(gateSLM, gateRow, gateCol);
    // todo: what is ratio?
    const size_t ratio = 3;
    if (const auto it = dictBoudingbox.find(nearestSLM);
        it != dictBoudingbox.end()) {
      const auto& [lower, upper, left, right] = it->second;
      it->second = std::tuple{std::min(nearestRow - ratio, lower),
                              std::max(nearestRow + ratio, upper),
                              std::min(nearestCol - ratio, left),
                              std::max(nearestCol + ratio, right)};
    } else {
      it->second = std::tuple{nearestRow - ratio, nearestRow + ratio,
                              nearestCol - ratio, nearestCol + ratio};
    }
    auto setNearbySite = commonSite;
    if (isEmptyStorageSite.at(initSLM)[initRow][initCol]) {
      setNearbySite.emplace(initialPlacement[q]);
    }

    for (auto& [slmId, boundingBox] : dictBoudingbox) {
      auto& [lower, upper, left, right] = boundingBox;
      lower = lower > expandFactor ? lower - expandFactor : 0;
      upper = std::min(upper + expandFactor + 1, slmId.get().nRows);
      left = left > expandFactor ? left - expandFactor : 0;
      right = std::min(right + expandFactor + 1, slmId.get().nCols);
      for (auto row = lower; row < upper; ++row) {
        for (auto col = left; col < right; ++col) {
          if (isEmptyStorageSite.find(slmId) == isEmptyStorageSite.end() ||
              isEmptyStorageSite.at(slmId)[row][col]) {
            setNearbySite.emplace(slmId, row, col);
          }
        }
      }
    }

    for (const auto& site : setNearbySite) {
      const auto& [it, success] =
          siteStorageToIdx.try_emplace(site, listStorage.size());
      if (success) {
        listStorage.emplace_back(site);
      }
      const auto idxStorage = it->second;
      const auto& [siteSLM, siteRow, siteCol] = site;
      const double dis = architecture_.get().distance(
          gateSLM, gateRow, gateCol, siteSLM, siteRow, siteCol);
      double lookaheadCost = 0;
      for (const auto& neighbor : dictQubitInteraction.at(q)) {
        const auto& [neighborSLM, neighborRow, neighborCol] =
            previousGatePlacement[neighbor];
        if (neighborSLM.get().isStorage()) {
          lookaheadCost += architecture_.get().nearestEntanglementSiteDistance(
              siteSLM, siteRow, siteCol, neighborSLM, neighborRow, neighborCol);
        } else {
          const std::pair<size_t, size_t>& exactLocNeighborQ =
              architecture_.get().exactSLMLocation(neighborSLM, neighborRow,
                                                   neighborCol);
          const auto dx = static_cast<std::int64_t>(exactLocNeighborQ.first) -
                          static_cast<std::int64_t>(exactLocQ.first);
          const auto dy = static_cast<std::int64_t>(exactLocNeighborQ.second) -
                          static_cast<std::int64_t>(exactLocQ.second);
          lookaheadCost += std::sqrt(std::sqrt((dx * dx) + (dy * dy)));
        }
      }
      const double cost = std::sqrt(dis) + (0.1 * lookaheadCost);
      listColCoo.emplace_back(idxStorage);
      listRowCoo.emplace_back(i);
      listData.emplace_back(cost);
    }
  }

  // to construct from three arrays:
  // - listData the entries of the matrix, in any order
  // - listRowCoo the row indices of the matrix entries
  // - listColCoo the column indices of the matrix entries
  // Where A[listRowCoo[k], listColCoo[k]] = listData[k].
  std::vector costMatrix(
      qubitToPlace.size(),
      std::vector<std::optional<double>>(listStorage.size(), std::nullopt));
  for (size_t k = 0; k < listData.size(); ++k) {
    costMatrix[listRowCoo[k]][listColCoo[k]] = listData[k];
  }
  const auto& matching = minimumWeightFullBipartiteMatching(costMatrix);
  auto newPlacement = previousGatePlacement;
  for (size_t j = 0; j < matching.size(); ++j) {
    newPlacement[qubitToPlace[j]] = listStorage[matching[j]];
  }
  return newPlacement;
}
VertexMatchingPlacer::VertexMatchingPlacer(const Architecture& architecture,
                                           const Config& config)
    : architecture_(architecture), config_(config) {
  // get first storage SLM and first entanglement SLM
  const auto& firstStorageSLM = *architecture_.get().storageZones.front();
  const auto& firstEntanglementSLM =
      architecture_.get().entanglementZones.front()->front();
  // check which side of the first storage SLM is closer to the entanglement
  // SLM
  if (firstStorageSLM.location.second < firstEntanglementSLM.location.second) {
    // if the entanglement SLM is closer to the last row of the storage SLM
    // start initial placement of the atoms in the last row instead of the
    // first and hence revert initial placement
    reverseInitialPlacement_ = true;
  }
}
auto VertexMatchingPlacer::place(
    const size_t nQubits,
    const std::vector<TwoQubitGateLayer>& twoQubitGateLayers,
    const std::vector<std::unordered_set<qc::Qubit>>& reuseQubits)
    -> std::vector<Placement> {
  std::vector<Placement> placement;
  placement.reserve((2 * twoQubitGateLayers.size()) + 1);
  placement.emplace_back(makeInitialPlacement(nQubits));
  // early return if no two-qubit gates are present
  if (twoQubitGateLayers.empty()) {
    return placement;
  }
  placement.emplace_back(placeGatesInEntanglementZone(
      placement.front(), std::unordered_set<qc::Qubit>{},
      twoQubitGateLayers.front(),
      twoQubitGateLayers.size() > 1 ? twoQubitGateLayers[1]
                                    : TwoQubitGateLayer{},
      false));
  for (size_t layer = 0; layer < twoQubitGateLayers.size(); ++layer) {
    // first compute the next qubit and gate placement without reusing atoms
    Placement qubitPlacementWithoutReuse;
    if (config_.dynamicPlacement) {
      qubitPlacementWithoutReuse = placeAtomsInStorageZone(
          placement.front(), placement.back(),
          layer < reuseQubits.size() ? reuseQubits[layer]
                                     : std::unordered_set<qc::Qubit>{},
          layer + 1 < twoQubitGateLayers.size() ? twoQubitGateLayers[layer + 1]
                                                : TwoQubitGateLayer{},
          false);
    } else {
      // keep the initial mapping for static placement
      qubitPlacementWithoutReuse = placement.front();
    }
    if (layer + 1 < twoQubitGateLayers.size()) {
      const auto& gatePlacementWithoutReuse = placeGatesInEntanglementZone(
          qubitPlacementWithoutReuse, reuseQubits[layer],
          twoQubitGateLayers[layer + 1],
          layer + 2 < twoQubitGateLayers.size() ? twoQubitGateLayers[layer + 2]
                                                : TwoQubitGateLayer{},
          false);
      // then compute the next qubit and gate placement with reusing atoms
      if (!reuseQubits[layer].empty()) {
        Placement qubitPlacementWithReuse;
        if (config_.dynamicPlacement) {
          qubitPlacementWithReuse = placeAtomsInStorageZone(
              placement.front(), placement.back(), reuseQubits[layer],
              layer + 1 < twoQubitGateLayers.size()
                  ? twoQubitGateLayers[layer + 1]
                  : TwoQubitGateLayer{},
              true);
        } else {
          // keep the initial mapping for static placement
          qubitPlacementWithReuse = placement.front();
          for (const auto q : reuseQubits[layer]) {
            qubitPlacementWithReuse[q] = placement.back()[q];
          }
        }
        const auto& gatePlacementWithReuse = placeGatesInEntanglementZone(
            qubitPlacementWithReuse, reuseQubits[layer],
            twoQubitGateLayers[layer + 1],
            layer + 2 < twoQubitGateLayers.size()
                ? twoQubitGateLayers[layer + 2]
                : TwoQubitGateLayer{},
            true);
        // keep the mapping with shorter distance
        const auto& [gatePlacement, qubitPlacement] = filterMapping(
            placement.back(),
            std::pair{qubitPlacementWithoutReuse, gatePlacementWithoutReuse},
            std::pair{qubitPlacementWithReuse, gatePlacementWithReuse});
        placement.emplace_back(gatePlacement);
        placement.emplace_back(qubitPlacement);
      } else {
        placement.emplace_back(qubitPlacementWithoutReuse);
        placement.emplace_back(gatePlacementWithoutReuse);
      }
    } else {
      placement.emplace_back(qubitPlacementWithoutReuse);
    }
  }
  return placement;
}
auto VertexMatchingPlacer::makeInitialPlacement(const size_t nQubits) const
    -> Placement {
  auto slmIt = architecture_.get().storageZones.cbegin();
  std::size_t c = 0;
  std::int64_t r = reverseInitialPlacement_
                       ? static_cast<std::int64_t>((*slmIt)->nRows) - 1
                       : 0;
  const std::int64_t step = reverseInitialPlacement_ ? -1 : 1;
  Placement initialPlacement;
  initialPlacement.reserve(nQubits);
  for (qc::Qubit qubit = 0; qubit < nQubits; ++qubit) {
    initialPlacement.emplace_back(**slmIt, r, c++);
    if (c == (*slmIt)->nCols) {
      // the end of the row reached, go to the next row
      r += step;
      c = 0;
      if (r == static_cast<std::int64_t>((*slmIt)->nRows)) {
        // the end of the slm reached, go to the next slm
        ++slmIt;
        if (step > 0) {
          r = static_cast<std::int64_t>((*slmIt)->nRows) - 1;
        } else {
          r = 0;
        }
      }
    }
  }
  return initialPlacement;
}
} // namespace na::zoned
