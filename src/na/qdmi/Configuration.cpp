/*
 * Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
 * Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

/** @file Configuration.cpp
 * @brief Runtime configuration parsing for neutral-atom QDMI devices.
 */

#include "na/qdmi/Configuration.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <fstream>
#include <functional>
#include <initializer_list>
#include <istream>
#include <limits>
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace na {
namespace {
using Json = nlohmann::json;

[[noreturn]] void validationError(const std::string_view source,
                                  const std::string_view pointer,
                                  const std::string_view message) {
  throw std::invalid_argument(std::string(source) + ":" + std::string(pointer) +
                              " " + std::string(message));
}

void rejectUnknownKeys(const Json& value,
                       const std::initializer_list<std::string_view> allowed,
                       const std::string_view source,
                       const std::string_view pointer) {
  for (const auto& [key, unused] : value.items()) {
    static_cast<void>(unused);
    if (std::ranges::find(allowed, std::string_view{key}) == allowed.end()) {
      validationError(source, pointer, "contains unknown key '" + key + "'");
    }
  }
}

void requireObjectKeys(const Json& value,
                       const std::initializer_list<std::string_view> required,
                       const std::string_view source,
                       const std::string& pointer) {
  if (!value.is_object()) {
    validationError(source, pointer, "must be an object");
  }
  rejectUnknownKeys(value, required, source, pointer);
  for (const auto key : required) {
    if (!value.contains(key)) {
      validationError(source, pointer + "/" + std::string(key), "is required");
    }
  }
}

void requireUnsigned(const Json& value, const std::string_view key,
                     const std::string_view source,
                     const std::string& pointer) {
  const auto& field = value.at(key);
  if (!field.is_number_unsigned() &&
      (!field.is_number_integer() || field.get<int64_t>() < 0)) {
    validationError(source, pointer + "/" + std::string(key),
                    "must be a non-negative integer");
  }
}

void requireNumber(const Json& value, const std::string_view key,
                   const std::string_view source, const std::string& pointer) {
  if (!value.at(key).is_number()) {
    validationError(source, pointer + "/" + std::string(key),
                    "must be a number");
  }
}

void requireString(const Json& value, const std::string_view key,
                   const std::string_view source, const std::string& pointer) {
  if (!value.at(key).is_string()) {
    validationError(source, pointer + "/" + std::string(key),
                    "must be a string");
  }
}

void validateVector(const Json& value, const std::string_view source,
                    const std::string& pointer) {
  requireObjectKeys(value, {"x", "y"}, source, pointer);
  for (const auto* const key : {"x", "y"}) {
    const auto& coordinate = value.at(key);
    const auto outOfRangeUnsigned =
        coordinate.is_number_unsigned() &&
        coordinate.get<uint64_t>() >
            static_cast<uint64_t>(std::numeric_limits<int64_t>::max());
    if (!coordinate.is_number_integer() || outOfRangeUnsigned) {
      validationError(source, pointer + "/" + key,
                      "must be a signed 64-bit integer");
    }
  }
}

void validateRegion(const Json& value, const std::string_view source,
                    const std::string& pointer) {
  requireObjectKeys(value, {"origin", "size"}, source, pointer);
  validateVector(value.at("origin"), source, pointer + "/origin");
  requireObjectKeys(value.at("size"), {"width", "height"}, source,
                    pointer + "/size");
  requireUnsigned(value.at("size"), "width", source, pointer + "/size");
  requireUnsigned(value.at("size"), "height", source, pointer + "/size");
  const auto width = value.at("size").at("width").get<uint64_t>();
  const auto height = value.at("size").at("height").get<uint64_t>();
  if (width == 0 || height == 0 ||
      width > static_cast<uint64_t>(std::numeric_limits<int64_t>::max()) ||
      height > static_cast<uint64_t>(std::numeric_limits<int64_t>::max())) {
    validationError(source, pointer + "/size",
                    "width and height must be positive signed 64-bit values");
  }
  const auto originX = value.at("origin").at("x").get<int64_t>();
  const auto originY = value.at("origin").at("y").get<int64_t>();
  if (originX >
          std::numeric_limits<int64_t>::max() - static_cast<int64_t>(width) ||
      originY >
          std::numeric_limits<int64_t>::max() - static_cast<int64_t>(height)) {
    validationError(source, pointer,
                    "origin plus size exceeds the coordinate range");
  }
}

void validateOperation(const Json& value, const std::string_view source,
                       const std::string& pointer, const bool multiQubit) {
  if (multiQubit) {
    requireObjectKeys(value,
                      {"name", "region", "duration", "fidelity",
                       "numParameters", "interactionRadius", "blockingRadius",
                       "numQubits"},
                      source, pointer);
  } else {
    requireObjectKeys(
        value, {"name", "region", "duration", "fidelity", "numParameters"},
        source, pointer);
  }
  validateRegion(value.at("region"), source, pointer + "/region");
  requireString(value, "name", source, pointer);
  requireUnsigned(value, "duration", source, pointer);
  requireNumber(value, "fidelity", source, pointer);
  requireUnsigned(value, "numParameters", source, pointer);
  if (multiQubit) {
    requireUnsigned(value, "interactionRadius", source, pointer);
    requireUnsigned(value, "blockingRadius", source, pointer);
    requireUnsigned(value, "numQubits", source, pointer);
  }
}

void validateNestedSchema(const Json& json, const std::string_view source) {
  const auto validateArray = [&](const std::string_view key,
                                 const auto& validateElement) {
    const auto& array = json.at(key);
    if (!array.is_array()) {
      validationError(source, "$/" + std::string(key), "must be an array");
    }
    for (size_t i = 0; i < array.size(); ++i) {
      validateElement(array.at(i),
                      "$/" + std::string(key) + "/" + std::to_string(i));
    }
  };

  validateArray("traps", [&](const Json& trap, const std::string& pointer) {
    requireObjectKeys(trap,
                      {"latticeOrigin", "latticeVector1", "latticeVector2",
                       "sublatticeOffsets", "extent"},
                      source, pointer);
    validateVector(trap.at("latticeOrigin"), source,
                   pointer + "/latticeOrigin");
    validateVector(trap.at("latticeVector1"), source,
                   pointer + "/latticeVector1");
    validateVector(trap.at("latticeVector2"), source,
                   pointer + "/latticeVector2");
    const auto& offsets = trap.at("sublatticeOffsets");
    if (!offsets.is_array() || offsets.empty()) {
      validationError(source, pointer + "/sublatticeOffsets",
                      "must be a non-empty array");
    }
    for (size_t i = 0; i < offsets.size(); ++i) {
      validateVector(offsets.at(i), source,
                     pointer + "/sublatticeOffsets/" + std::to_string(i));
    }
    validateRegion(trap.at("extent"), source, pointer + "/extent");
  });

  const auto validateOperations = [&](const std::string_view key,
                                      const bool multiQubit) {
    validateArray(key, [&](const Json& operation, const std::string& pointer) {
      validateOperation(operation, source, pointer, multiQubit);
    });
  };
  validateOperations("globalSingleQubitOperations", false);
  validateOperations("localSingleQubitOperations", false);
  validateOperations("localMultiQubitOperations", true);

  validateArray("globalMultiQubitOperations", [&](const Json& operation,
                                                  const std::string& pointer) {
    requireObjectKeys(operation,
                      {"name", "region", "duration", "fidelity",
                       "numParameters", "interactionRadius", "blockingRadius",
                       "idlingFidelity", "numQubits"},
                      source, pointer);
    validateRegion(operation.at("region"), source, pointer + "/region");
    requireString(operation, "name", source, pointer);
    requireUnsigned(operation, "duration", source, pointer);
    requireNumber(operation, "fidelity", source, pointer);
    requireUnsigned(operation, "numParameters", source, pointer);
    requireUnsigned(operation, "interactionRadius", source, pointer);
    requireUnsigned(operation, "blockingRadius", source, pointer);
    requireNumber(operation, "idlingFidelity", source, pointer);
    requireUnsigned(operation, "numQubits", source, pointer);
  });

  validateArray(
      "shuttlingUnits", [&](const Json& unit, const std::string& pointer) {
        requireObjectKeys(unit,
                          {"region", "loadDuration", "storeDuration",
                           "loadFidelity", "storeFidelity", "numParameters",
                           "meanShuttlingSpeed"},
                          source, pointer);
        validateRegion(unit.at("region"), source, pointer + "/region");
        for (const auto* const key : {"loadDuration", "storeDuration",
                                      "numParameters", "meanShuttlingSpeed"}) {
          requireUnsigned(unit, key, source, pointer);
        }
        requireNumber(unit, "loadFidelity", source, pointer);
        requireNumber(unit, "storeFidelity", source, pointer);
      });
  requireObjectKeys(json.at("decoherenceTimes"), {"t1", "t2"}, source,
                    "$/decoherenceTimes");
  requireUnsigned(json.at("decoherenceTimes"), "t1", source,
                  "$/decoherenceTimes");
  requireUnsigned(json.at("decoherenceTimes"), "t2", source,
                  "$/decoherenceTimes");
  requireObjectKeys(json.at("lengthUnit"), {"scaleFactor", "unit"}, source,
                    "$/lengthUnit");
  requireNumber(json.at("lengthUnit"), "scaleFactor", source, "$/lengthUnit");
  requireString(json.at("lengthUnit"), "unit", source, "$/lengthUnit");
  requireObjectKeys(json.at("durationUnit"), {"scaleFactor", "unit"}, source,
                    "$/durationUnit");
  requireNumber(json.at("durationUnit"), "scaleFactor", source,
                "$/durationUnit");
  requireString(json.at("durationUnit"), "unit", source, "$/durationUnit");
}

[[nodiscard]] auto parseAndValidate(const Json& json,
                                    const std::string_view source) -> Device {
  if (!json.is_object()) {
    validationError(source, "$", "must be an object");
  }
  requireObjectKeys(json,
                    {"schema-version", "name", "numQubits", "traps",
                     "minAtomDistance", "globalSingleQubitOperations",
                     "globalMultiQubitOperations", "localSingleQubitOperations",
                     "localMultiQubitOperations", "shuttlingUnits",
                     "decoherenceTimes", "lengthUnit", "durationUnit"},
                    source, "$");
  validateNestedSchema(json, source);
  requireString(json, "name", source, "$");
  requireUnsigned(json, "numQubits", source, "$");
  requireUnsigned(json, "minAtomDistance", source, "$");
  const auto& schemaVersion = json.at("schema-version");
  if (!schemaVersion.is_number_unsigned() &&
      !schemaVersion.is_number_integer()) {
    validationError(source, "$/schema-version", "must be the integer 1");
  }
  const auto supportedSchemaVersion = schemaVersion.is_number_unsigned()
                                          ? schemaVersion.get<uint64_t>() == 1
                                          : schemaVersion.get<int64_t>() == 1;
  if (!supportedSchemaVersion) {
    validationError(source, "$/schema-version", "must be 1");
  }
  Device device;
  try {
    device = json.get<Device>();
  } catch (const Json::exception& error) {
    validationError(source, "$",
                    "has an invalid value: " + std::string(error.what()));
  } catch (const std::exception& error) {
    validationError(source, "$",
                    "has an invalid value: " + std::string(error.what()));
  }
  device.schemaVersion = 1;
  if (device.name.empty() || device.numQubits == 0 ||
      device.minAtomDistance == 0) {
    validationError(source, "$",
                    "requires a non-empty name, positive numQubits, and "
                    "positive minAtomDistance");
  }
  const auto validUnit = [](const Device::Unit& unit) -> bool {
    return std::isfinite(unit.scaleFactor) && unit.scaleFactor > 0.;
  };
  if (!validUnit(device.lengthUnit) || !validUnit(device.durationUnit)) {
    validationError(source, "$/*Unit",
                    "scaleFactor must be positive and finite");
  }
  const auto validFidelity = [](const double fidelity) -> bool {
    return std::isfinite(fidelity) && fidelity >= 0. && fidelity <= 1.;
  };
  const auto validateOperations = [&](const auto& operations,
                                      const std::string_view pointer) -> void {
    std::set<std::string> names;
    for (const auto& operation : operations) {
      if (operation.name.empty() || operation.duration == 0 ||
          !validFidelity(operation.fidelity) ||
          !names.emplace(operation.name).second) {
        validationError(source, pointer,
                        "operations require a unique name, positive duration, "
                        "and fidelity in [0, 1]");
      }
    }
  };
  validateOperations(device.globalSingleQubitOperations,
                     "$/globalSingleQubitOperations");
  validateOperations(device.globalMultiQubitOperations,
                     "$/globalMultiQubitOperations");
  validateOperations(device.localSingleQubitOperations,
                     "$/localSingleQubitOperations");
  validateOperations(device.localMultiQubitOperations,
                     "$/localMultiQubitOperations");
  for (const auto& operation : device.globalMultiQubitOperations) {
    if (!validFidelity(operation.idlingFidelity) || operation.numQubits <= 1 ||
        operation.interactionRadius == 0 || operation.blockingRadius == 0) {
      validationError(source, "$/globalMultiQubitOperations",
                      "requires arity greater than one, positive radii, and "
                      "valid idlingFidelity");
    }
  }
  for (const auto& operation : device.localMultiQubitOperations) {
    if (operation.numQubits != 2 || operation.interactionRadius == 0 ||
        operation.blockingRadius == 0) {
      validationError(source, "$/localMultiQubitOperations",
                      "requires positive radii and exactly two qubits");
    }
  }
  for (const auto& unit : device.shuttlingUnits) {
    if (!validFidelity(unit.loadFidelity) ||
        !validFidelity(unit.storeFidelity) || unit.loadDuration == 0 ||
        unit.storeDuration == 0 || unit.meanShuttlingSpeed == 0) {
      validationError(source, "$/shuttlingUnits",
                      "durations and speed must be positive and fidelities "
                      "must be finite and in [0, 1]");
    }
  }
  if (device.decoherenceTimes.t1 == 0 || device.decoherenceTimes.t2 == 0) {
    validationError(source, "$/decoherenceTimes", "t1 and t2 must be positive");
  }
  std::set<std::pair<int64_t, int64_t>> uniqueCoordinates;
  std::vector<std::pair<int64_t, int64_t>> coordinates;
  size_t generatedSites = 0;
  try {
    forEachRegularSites(device.traps, [&](const SiteInfo& site) {
      if (!uniqueCoordinates.emplace(site.x, site.y).second) {
        validationError(source, "$/traps",
                        "generates duplicate site coordinates");
      }
      coordinates.emplace_back(site.x, site.y);
      ++generatedSites;
    });
  } catch (const std::invalid_argument&) {
    throw;
  } catch (const std::exception& error) {
    validationError(source, "$/traps", error.what());
  }
  if (generatedSites < device.numQubits) {
    validationError(source, "$/traps",
                    "does not generate enough sites for numQubits");
  }
  constexpr size_t maxLocalPairCandidates = 10'000'000;
  size_t remainingPairCandidates = maxLocalPairCandidates;
  for (const auto& operation : device.localMultiQubitOperations) {
    for (size_t first = 0; first < coordinates.size(); ++first) {
      const auto& [x, y] = coordinates[first];
      if (operation.region.origin.x <= x &&
          x <= operation.region.origin.x +
                   static_cast<int64_t>(operation.region.size.width) &&
          operation.region.origin.y <= y &&
          y <= operation.region.origin.y +
                   static_cast<int64_t>(operation.region.size.height)) {
        const auto candidates = coordinates.size() - first - 1;
        if (candidates > remainingPairCandidates) {
          validationError(source, "$/localMultiQubitOperations",
                          "expands to more than 10000000 candidate site pairs");
        }
        remainingPairCandidates -= candidates;
      }
    }
  }
  return device;
}

/**
 * @brief Solves a 2D linear equation system.
 * @details The equation has the following form:
 * @code
 * x1 * i + x2 * j = x0
 * y1 * i + y2 * j = y0
 * @endcode
 * The free variables are i and j.
 * @param x1 Coefficient for x in the first equation.
 * @param x2 Coefficient for y in the first equation.
 * @param y1 Coefficient for x in the second equation.
 * @param y2 Coefficient for y in the second equation.
 * @param x0 Right-hand side of the first equation.
 * @param y0 Right-hand side of the second equation.
 * @returns A pair containing the solution (i, j).
 * @throws std::runtime_error if the system has no unique solution.
 */
[[noreturn]] auto arithmeticOverflow() -> void {
  throw std::overflow_error(
      "lattice arithmetic exceeds the signed 64-bit range");
}

[[nodiscard]] auto checkedAdd(const int64_t left, const int64_t right)
    -> int64_t {
  if ((right > 0 && left > std::numeric_limits<int64_t>::max() - right) ||
      (right < 0 && left < std::numeric_limits<int64_t>::min() - right)) {
    arithmeticOverflow();
  }
  return left + right;
}

[[nodiscard]] auto checkedSubtract(const int64_t left, const int64_t right)
    -> int64_t {
  if ((right > 0 && left < std::numeric_limits<int64_t>::min() + right) ||
      (right < 0 && left > std::numeric_limits<int64_t>::max() + right)) {
    arithmeticOverflow();
  }
  return left - right;
}

[[nodiscard]] auto checkedMultiply(const int64_t left, const int64_t right)
    -> int64_t {
  if (left == 0 || right == 0) {
    return 0;
  }
  if ((left == -1 && right == std::numeric_limits<int64_t>::min()) ||
      (right == -1 && left == std::numeric_limits<int64_t>::min())) {
    arithmeticOverflow();
  }
  bool overflow = false;
  if (left > 0) {
    overflow = right > 0 ? left > std::numeric_limits<int64_t>::max() / right
                         : right < std::numeric_limits<int64_t>::min() / left;
  } else {
    overflow = right > 0 ? left < std::numeric_limits<int64_t>::min() / right
                         : left < std::numeric_limits<int64_t>::max() / right;
  }
  if (overflow) {
    arithmeticOverflow();
  }
  return left * right;
}

// Preserve the extended precision offered by platforms where it is available.
using LatticeFloat = long double; // NOLINT(google-runtime-float)

[[nodiscard]] auto solve2DLinearEquation(const int64_t x1, const int64_t x2,
                                         const int64_t y1, const int64_t y2,
                                         const int64_t x0, const int64_t y0)
    -> std::pair<double, double> {
  const auto asLatticeFloat = [](const int64_t value) {
    return static_cast<LatticeFloat>(value);
  };
  const auto det = (asLatticeFloat(x1) * asLatticeFloat(y2)) -
                   (asLatticeFloat(x2) * asLatticeFloat(y1));
  if (constexpr auto epsilon = 1e-10L; std::abs(det) < epsilon) {
    throw std::runtime_error("The system of equations has no unique solution.");
  }
  const auto detX = (asLatticeFloat(x0) * asLatticeFloat(y2)) -
                    (asLatticeFloat(x2) * asLatticeFloat(y0));
  const auto detY = (asLatticeFloat(x1) * asLatticeFloat(y0)) -
                    (asLatticeFloat(x0) * asLatticeFloat(y1));
  return {static_cast<double>(detX / det), static_cast<double>(detY / det)};
}

[[nodiscard]] auto floorToInt64(const double value) -> int64_t {
  const auto floored = std::floor(value);
  constexpr auto minInt64 = -0x1p63;
  constexpr auto maxInt64Exclusive = 0x1p63;
  if (!std::isfinite(floored) || floored < minInt64 ||
      floored >= maxInt64Exclusive) {
    throw std::overflow_error("lattice index exceeds the signed 64-bit range");
  }
  return static_cast<int64_t>(floored);
}

[[nodiscard]] auto coordinate(const int64_t origin, const int64_t offset,
                              const int64_t firstIndex,
                              const int64_t firstVector,
                              const int64_t secondIndex,
                              const int64_t secondVector) -> int64_t {
  return checkedAdd(checkedAdd(origin, offset),
                    checkedAdd(checkedMultiply(firstIndex, firstVector),
                               checkedMultiply(secondIndex, secondVector)));
}

/**
 * @brief Increments the indices in lexicographic order.
 * @details This function increments the first index that is less than its
 * limit, resets all previous indices to their minimum values.
 * @param indices The vector of indices to increment.
 * @param minima The minimum values for each index (used when resetting).
 * @param limits The limits for each index.
 * @returns true if the increment was successful, false if all indices have
 * reached their limits.
 */
[[nodiscard]] auto increment(std::vector<int64_t>& indices,
                             const std::vector<int64_t>& minima,
                             const std::vector<int64_t>& limits) -> bool {
  size_t i = 0;
  for (; i < indices.size() && indices[i] == limits[i]; ++i) {
  }
  if (i == indices.size()) {
    // all indices are at their limits
    return false;
  }
  for (size_t j = 0; j < i; ++j) {
    indices[j] = minima[j]; // Reset all previous indices to their minima
  }
  ++indices[i]; // Increment the next index
  return true;
}
} // namespace

[[nodiscard]] auto readJSON(std::istream& is) -> Device {
  // Read the device configuration from the input stream
  nlohmann::json json;
  try {
    is >> json;
    // NOLINTNEXTLINE(misc-include-cleaner)
  } catch (const nlohmann::detail::parse_error& e) {
    std::stringstream ss;
    ss << "Failed to parse JSON string: " << e.what();
    throw std::runtime_error(ss.str());
  }
  return parseAndValidate(json, "input");
}

auto readJSON(const std::string_view json, const std::string_view source)
    -> Device {
  try {
    return parseAndValidate(Json::parse(json), source);
  } catch (const Json::parse_error& error) {
    throw std::invalid_argument(std::string(source) +
                                ": invalid JSON: " + error.what());
  }
}

[[nodiscard]] auto readJSON(const std::string& path) -> Device {
  // Read the device configuration from a JSON file
  std::ifstream ifs(path);
  if (!ifs.good()) {
    throw std::runtime_error("Failed to open JSON file: " + std::string(path));
  }
  const auto& device = readJSON(ifs);
  ifs.close();
  return device;
}

void forEachRegularSites(const std::vector<Device::Lattice>& lattices,
                         const std::function<void(const SiteInfo&)>& f,
                         const size_t startId) {
  size_t count = startId;
  size_t moduleCount = 0;
  for (const auto& [latticeOrigin, latticeVector1, latticeVector2,
                    sublatticeOffsets, extent] : lattices) {
    size_t subModuleCount = 0;
    const auto& [origin, size] = extent;
    const auto extentWidth = static_cast<int64_t>(size.width);
    const auto extentHeight = static_cast<int64_t>(size.height);
    const auto solve = [&](const int64_t x, const int64_t y) {
      return solve2DLinearEquation(latticeVector1.x, latticeVector2.x,
                                   latticeVector1.y, latticeVector2.y, x, y);
    };
    const auto maximumX = checkedAdd(origin.x, extentWidth);
    const auto maximumY = checkedAdd(origin.y, extentHeight);

    // indices of the bottom left corner
    const auto& [bottomLeftI, bottomLeftJ] =
        solve(checkedSubtract(origin.x, latticeOrigin.x),
              checkedSubtract(origin.y, latticeOrigin.y));

    // indices of the bottom right corner
    const auto& [bottomRightI, bottomRightJ] =
        solve(checkedSubtract(maximumX, latticeOrigin.x),
              checkedSubtract(origin.y, latticeOrigin.y));

    // indices of the top left corner
    const auto& [topLeftI, topLeftJ] =
        solve(checkedSubtract(origin.x, latticeOrigin.x),
              checkedSubtract(maximumY, latticeOrigin.y));

    // indices of the top right corner
    const auto& [topRightI, topRightJ] =
        solve(checkedSubtract(maximumX, latticeOrigin.x),
              checkedSubtract(maximumY, latticeOrigin.y));

    const auto minI = floorToInt64(
        std::min({bottomLeftI, bottomRightI, topLeftI, topRightI}));
    const auto minJ = floorToInt64(
        std::min({bottomLeftJ, bottomRightJ, topLeftJ, topRightJ}));
    const auto maxI = floorToInt64(
        std::max({bottomLeftI, bottomRightI, topLeftI, topRightI}));
    const auto maxJ = floorToInt64(
        std::max({bottomLeftJ, bottomRightJ, topLeftJ, topRightJ}));

    constexpr size_t maxCandidateSites = 10'000'000;
    const auto spanI = checkedAdd(checkedSubtract(maxI, minI), 1);
    const auto spanJ = checkedAdd(checkedSubtract(maxJ, minJ), 1);
    if (std::cmp_greater(spanI, maxCandidateSites) ||
        std::cmp_greater(spanJ, maxCandidateSites) ||
        spanI > static_cast<int64_t>(maxCandidateSites) / spanJ ||
        sublatticeOffsets.size() >
            maxCandidateSites /
                (static_cast<size_t>(spanI) * static_cast<size_t>(spanJ))) {
      throw std::length_error(
          "lattice expands to more than 10000000 candidate sites");
    }

    const std::vector minima{minI, minJ};
    const std::vector limits{maxI, maxJ};
    std::vector indices{minI, minJ};
    for (bool loop = true; loop;
         loop = increment(indices, minima, limits), ++subModuleCount) {
      // For every sublattice offset, add a site for repetition indices
      for (const auto& [xOffset, yOffset] : sublatticeOffsets) {
        if (count == std::numeric_limits<size_t>::max()) {
          throw std::overflow_error("generated site identifier overflow");
        }
        const auto id = count++;
        const auto x =
            coordinate(latticeOrigin.x, xOffset, indices[0], latticeVector1.x,
                       indices[1], latticeVector2.x);
        const auto y =
            coordinate(latticeOrigin.y, yOffset, indices[0], latticeVector1.y,
                       indices[1], latticeVector2.y);
        if (origin.x <= x && x <= origin.x + extentWidth && origin.y <= y &&
            y <= origin.y + extentHeight) {
          // Only add the site if it is within the extent of the lattice
          f(SiteInfo{.id = id,
                     .x = x,
                     .y = y,
                     .moduleId = moduleCount,
                     .subModuleId = subModuleCount});
        }
      }
    }
    ++moduleCount;
  }
}
} // namespace na
