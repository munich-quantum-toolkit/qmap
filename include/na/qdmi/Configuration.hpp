/*
 * Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
 * Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

/** @file Configuration.hpp
 * @brief Neutral-atom QDMI device configuration.
 */

#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <istream>
#include <nlohmann/json.hpp> // NOLINT(misc-include-cleaner)
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace na {
/**
 * @brief Represents a neutral atom device configuration.
 * @details This struct defines the schema for the JSON representation of a
 * neutral atom device configuration. This struct, including all its
 * sub-structs, implements functions to serialize and deserialize to and from
 * JSON using the nlohmann::json library.
 * @note All duration and length values are in multiples of the time unit and
 * the length unit, respectively.
 */
struct Device {
  /// @brief Version of this device-description schema.
  uint64_t schemaVersion = 1;
  /// @brief The name of the device.
  std::string name;
  /// @brief The number of qubits in the device.
  uint64_t numQubits = 0;

  /// @brief Represents a 2D-vector.
  struct Vector {
    /// @brief The x-coordinate of the vector.
    int64_t x = 0;
    /// @brief The y-coordinate of the vector.
    int64_t y = 0;

    // NOLINTNEXTLINE(misc-include-cleaner)
    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(Vector, x, y)

    auto operator<=>(const Vector&) const = default;
  };
  /// @brief Represents a region in the device.
  struct Region {
    /// @brief The origin of the region.
    Vector origin;

    /// @brief The size of the region.
    struct Size {
      /// @brief The width of the region.
      uint64_t width = 0;
      /// @brief The height of the region.
      uint64_t height = 0;

      // NOLINTNEXTLINE(misc-include-cleaner)
      NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(Size, width, height)

      auto operator<=>(const Size&) const = default;
    };
    /// @brief The size of the region.
    Size size;

    // NOLINTNEXTLINE(misc-include-cleaner)
    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(Region, origin, size)

    auto operator<=>(const Region&) const = default;
  };
  /// @brief Represents a lattice of traps in the device.
  struct Lattice {
    /// @brief The origin of the lattice.
    Vector latticeOrigin;
    /**
     * @brief The first lattice vector.
     * @details Multiples of this vector are added to the lattice origin to
     * create the lattice structure.
     */
    Vector latticeVector1;
    /**
     * @brief The second lattice vector.
     * @details Multiples of this vector are added to the lattice origin and
     * multiples of the first lattice vector to create the lattice structure.
     */
    Vector latticeVector2;
    /**
     * @brief The offsets for each sublattice.
     * @details The actual locations of traps are calculated by adding the
     * each offset to the points in the lattice defined by the lattice
     * vectors, i.e., for each sublattice offset `offset` and indices `i` and
     * `j`, the trap location is `latticeOrigin + i * latticeVector1 + j *
     * latticeVector2 + offset`.
     */
    std::vector<Vector> sublatticeOffsets;
    /**
     * @brief The extent of the lattice.
     * @details The extent defines the boundary of the lattice in which traps
     * are placed. Only traps of the lattice that are within this extent
     * are considered valid.
     */
    Region extent;

    // NOLINTNEXTLINE(misc-include-cleaner)
    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(Lattice, latticeOrigin,
                                                latticeVector1, latticeVector2,
                                                sublatticeOffsets, extent)

    auto operator<=>(const Lattice&) const = default;
  };
  /// @brief The list of lattices (trap areas) in the device.
  std::vector<Lattice> traps;
  /**
   * @brief The minimum distance between atoms in the device that must be
   * respected.
   */
  uint64_t minAtomDistance = 0;

private:
  struct Operation {
    /// @brief The name of the operation.
    std::string name;
    /// @brief The region in which the operation can be performed.
    Region region;
    /// @brief The duration of the operation.
    uint64_t duration = 0;
    /// @brief The fidelity of the operation.
    double fidelity = 0.0;
    /// @brief The number of parameters the operation takes.
    uint64_t numParameters = 0;

    // NOLINTNEXTLINE(misc-include-cleaner)
    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(Operation, name, region,
                                                duration, fidelity,
                                                numParameters)
  };

public:
  /// @brief Represents a global single-qubit operation.
  struct GlobalSingleQubitOperation : Operation {};
  /// @brief The list of global single-qubit operations supported by the device.
  std::vector<GlobalSingleQubitOperation> globalSingleQubitOperations;

  /// @brief Represents a global multi-qubit operation.
  struct GlobalMultiQubitOperation : Operation {
    /**
     * @brief The interaction radius of the operation within which two qubits
     * can interact.
     */
    uint64_t interactionRadius = 0;
    /**
     * @brief The blocking radius of the operation within which no other
     * operation can be performed to avoid interference.
     */
    uint64_t blockingRadius = 0;
    /// @brief The fidelity of the operation when no qubits are interacting.
    double idlingFidelity = 0.0;
    /// @brief The number of qubits involved in the operation.
    uint64_t numQubits = 0;

    // NOLINTNEXTLINE(misc-include-cleaner)
    NLOHMANN_DEFINE_DERIVED_TYPE_INTRUSIVE_WITH_DEFAULT(
        GlobalMultiQubitOperation, Operation, interactionRadius, blockingRadius,
        idlingFidelity, numQubits)
  };
  /// @brief The list of global multi-qubit operations supported by the device.
  std::vector<GlobalMultiQubitOperation> globalMultiQubitOperations;

  /// @brief Represents a local single-qubit operation.
  struct LocalSingleQubitOperation : Operation {};
  /// @brief The list of local single-qubit operations supported by the device.
  std::vector<LocalSingleQubitOperation> localSingleQubitOperations;

  /// @brief Represents a local multi-qubit operation.
  struct LocalMultiQubitOperation : Operation {
    /**
     * @brief The interaction radius of the operation within which two qubits
     * can interact.
     */
    uint64_t interactionRadius = 0;
    /**
     * @brief The blocking radius of the operation within which no other
     * operation can be performed to avoid interference.
     */
    uint64_t blockingRadius = 0;
    /// @brief The number of qubits involved in the operation.
    uint64_t numQubits = 0;

    // NOLINTNEXTLINE(misc-include-cleaner)
    NLOHMANN_DEFINE_DERIVED_TYPE_INTRUSIVE_WITH_DEFAULT(
        LocalMultiQubitOperation, Operation, interactionRadius, blockingRadius,
        numQubits)
  };
  /// @brief The list of local multi-qubit operations supported by the device.
  std::vector<LocalMultiQubitOperation> localMultiQubitOperations;

  /// @brief Represents a shuttling unit in the device.
  struct ShuttlingUnit {
    size_t id = 0; ///< @brief Unique identifier for the shuttling unit.
    /// @brief The region in which the shuttling unit operates.
    Region region;
    /// @brief The duration of the load operation in the shuttling unit.
    uint64_t loadDuration = 0;
    /// @brief The duration of the store operation in the shuttling unit.
    uint64_t storeDuration = 0;
    /// @brief The fidelity of the load operation in the shuttling unit.
    double loadFidelity = 0.0;
    /// @brief The fidelity of the store operation in the shuttling unit.
    double storeFidelity = 0.0;
    /// @brief The number of parameters the shuttling unit takes.
    uint64_t numParameters = 0;
    /**
     * @brief The mean shuttling speed.
     * @note Only for shuttling operations.
     */
    uint64_t meanShuttlingSpeed = 0;

    // NOLINTNEXTLINE(misc-include-cleaner)
    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(ShuttlingUnit, region,
                                                loadDuration, storeDuration,
                                                loadFidelity, storeFidelity,
                                                numParameters,
                                                meanShuttlingSpeed)
  };
  /// @brief The list of shuttling units supported by the device.
  std::vector<ShuttlingUnit> shuttlingUnits;

  /// @brief Represents the decoherence times of the device.
  struct DecoherenceTimes {
    /// @brief The T1 time.
    uint64_t t1 = 0;
    /// @brief The T2 time.
    uint64_t t2 = 0;

    // NOLINTNEXTLINE(misc-include-cleaner)
    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(DecoherenceTimes, t1, t2)
  };
  /// @brief The decoherence times of the device.
  DecoherenceTimes decoherenceTimes;

  /// @brief Represents a unit of measurement for length and time.
  struct Unit {
    /// @brief The unit of measurement (e.g., "µm" for micrometers, "ns" for
    /// nanoseconds).
    std::string unit;
    /// @brief The factor of the unit.
    double scaleFactor = 0;

    // NOLINTNEXTLINE(misc-include-cleaner)
    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(Unit, scaleFactor, unit)
  };

  /// @brief The unit of measurement for lengths in the device.
  Unit lengthUnit = {.unit = "um", ///< Default is micrometers (um).
                     .scaleFactor = 1.0};

  /// @brief The unit of measurement for time in the device.
  Unit durationUnit = {.unit = "us", ///< Default is microseconds (us).
                       .scaleFactor = 1.0};

  /// Serialize the versioned runtime configuration.
  // NOLINTNEXTLINE(readability-identifier-naming,misc-include-cleaner)
  friend void to_json(nlohmann::json& json, const Device& device) {
    json = nlohmann::json{
        {"schema-version", device.schemaVersion},
        {"name", device.name},
        {"numQubits", device.numQubits},
        {"traps", device.traps},
        {"minAtomDistance", device.minAtomDistance},
        {"globalSingleQubitOperations", device.globalSingleQubitOperations},
        {"globalMultiQubitOperations", device.globalMultiQubitOperations},
        {"localSingleQubitOperations", device.localSingleQubitOperations},
        {"localMultiQubitOperations", device.localMultiQubitOperations},
        {"shuttlingUnits", device.shuttlingUnits},
        {"decoherenceTimes", device.decoherenceTimes},
        {"lengthUnit", device.lengthUnit},
        {"durationUnit", device.durationUnit}};
  }

  // the name of the following function is given by the nlohmann::json library
  // and must not be changed
  template <typename BasicJsonType>
  // NOLINTNEXTLINE(readability-identifier-naming)
  friend void from_json(const BasicJsonType& json, Device& device) {
    const Device defaultDevice{};
    device.name = !json.is_null() ? json.value("name", defaultDevice.name)
                                  : defaultDevice.name;
    device.numQubits = !json.is_null()
                           ? json.value("numQubits", defaultDevice.numQubits)
                           : defaultDevice.numQubits;
    device.traps = !json.is_null() ? json.value("traps", defaultDevice.traps)
                                   : defaultDevice.traps;
    device.minAtomDistance =
        !json.is_null()
            ? json.value("minAtomDistance", defaultDevice.minAtomDistance)
            : defaultDevice.minAtomDistance;
    device.globalSingleQubitOperations =
        !json.is_null() ? json.value("globalSingleQubitOperations",
                                     defaultDevice.globalSingleQubitOperations)
                        : defaultDevice.globalSingleQubitOperations;
    device.globalMultiQubitOperations =
        !json.is_null() ? json.value("globalMultiQubitOperations",
                                     defaultDevice.globalMultiQubitOperations)
                        : defaultDevice.globalMultiQubitOperations;
    device.localSingleQubitOperations =
        !json.is_null() ? json.value("localSingleQubitOperations",
                                     defaultDevice.localSingleQubitOperations)
                        : defaultDevice.localSingleQubitOperations;
    device.localMultiQubitOperations =
        !json.is_null() ? json.value("localMultiQubitOperations",
                                     defaultDevice.localMultiQubitOperations)
                        : defaultDevice.localMultiQubitOperations;
    device.shuttlingUnits =
        !json.is_null()
            ? json.value("shuttlingUnits", defaultDevice.shuttlingUnits)
            : defaultDevice.shuttlingUnits;
    std::ranges::for_each(device.shuttlingUnits,
                          [i = 0UL](auto& unit) mutable { unit.id = i++; });
    device.decoherenceTimes =
        !json.is_null()
            ? json.value("decoherenceTimes", defaultDevice.decoherenceTimes)
            : defaultDevice.decoherenceTimes;
    device.lengthUnit = !json.is_null()
                            ? json.value("lengthUnit", defaultDevice.lengthUnit)
                            : defaultDevice.lengthUnit;
    constexpr std::array allowedLengthUnits = {"mm", "um", "nm"};
    if (std::ranges::find(allowedLengthUnits, device.lengthUnit.unit) ==
        allowedLengthUnits.end()) {
      std::ostringstream ss;
      ss << "Invalid length unit: " << device.lengthUnit.unit
         << ". Supported units are: ";
      std::ranges::for_each(allowedLengthUnits,
                            [&ss](const char* unit) { ss << unit << ", "; });
      ss.seekp(-2, std::ostringstream::cur); // Remove the last comma and space
      ss << ".";
      throw std::runtime_error(ss.str());
    }
    device.durationUnit =
        !json.is_null() ? json.value("durationUnit", defaultDevice.durationUnit)
                        : defaultDevice.durationUnit;
    constexpr std::array allowedDurationUnits = {"ms", "us", "ns"};
    if (std::ranges::find(allowedDurationUnits, device.durationUnit.unit) ==
        allowedDurationUnits.end()) {
      std::ostringstream ss;
      ss << "Invalid duration unit: " << device.durationUnit.unit
         << ". Supported units are: ";
      std::ranges::for_each(allowedDurationUnits,
                            [&ss](const char* unit) { ss << unit << ", "; });
      ss.seekp(-2, std::ostringstream::cur); // Remove the last comma and space
      ss << ".";
      throw std::runtime_error(ss.str());
    }
  }
};

/**
 * @brief Parses the device configuration from an input stream.
 * @param is is the input stream containing the JSON representation of the
 * device configuration.
 * @returns The parsed device configuration as a Protobuf message.
 * @throws std::runtime_error if the JSON cannot be parsed.
 */
[[nodiscard]] Device readJSON(std::istream& is);

/// Parse and validate JSON text while using @p source in diagnostics.
[[nodiscard]] Device readJSON(std::string_view json, std::string_view source);

/**
 * @brief Parses the device configuration from a JSON file.
 * @param path is the path to the JSON file containing the device configuration.
 * @returns The parsed device configuration as a Protobuf message.
 * @throws std::runtime_error if the JSON file does not exist, or the JSON file
 * cannot be parsed.
 */
[[nodiscard]] Device readJSON(const std::string& path);

/**
 * @brief Information about a regular site in a lattice.
 * @details This struct encapsulates all relevant information about a site
 * for use in the forEachRegularSites callback.
 */
struct SiteInfo {
  /// @brief The unique identifier of the site.
  uint64_t id;
  /// @brief The x-coordinate of the site.
  int64_t x;
  /// @brief The y-coordinate of the site.
  int64_t y;
  /// @brief The identifier of the lattice (module) the site belongs to.
  uint64_t moduleId;
  /// @brief The identifier of the sublattice (submodule) the site belongs to.
  uint64_t subModuleId;
};

/**
 * @brief Iterates over all regular sites created by the given lattices and
 * calls the given function for each site.
 * @param lattices is the list of lattices to iterate over.
 * @param f is the function to call for each regular site, receiving a SiteInfo
 * struct containing all site information.
 * @param startId is the starting identifier for the sites. Default is 0.
 * @throws std::runtime_error if lattice vectors are degenerate (i.e., the
 * determinant of the lattice vector matrix is near zero, causing the system
 * of equations to have no unique solution).
 */
void forEachRegularSites(const std::vector<Device::Lattice>& lattices,
                         const std::function<void(const SiteInfo&)>& f,
                         size_t startId = 0);

} // namespace na
