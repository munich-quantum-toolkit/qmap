/*
 * Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
 * Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

/** @file Device.hpp
 * @brief Neutral-atom FoMaC device session implementation.
 */

#pragma once

#include "fomac/FoMaC.hpp"
#include "na/qdmi/Configuration.hpp"

// NOLINTNEXTLINE(misc-include-cleaner)
#include <nlohmann/json.hpp>
#include <optional>
#include <vector>

namespace na {
/**
 * @brief Class representing the Session library with neutral atom extensions.
 * @see fomac::Session
 */
class Session {
public:
  /**
   * @brief Class representing a quantum device with neutral atom extensions.
   * @see fomac::Session::Device
   * @note Since it inherits from @ref na::Device, Device objects can be
   * converted to `nlohmann::json` objects.
   */
  class Device : public fomac::Device, na::Device {

    /**
     * @brief Initializes the name from the underlying QDMI device.
     */
    auto initNameFromDevice() -> void;

    /**
     * @brief Initializes the minimum atom distance from the underlying QDMI
     * device.
     */
    auto initMinAtomDistanceFromDevice() -> bool;

    /**
     * @brief Initializes the number of qubits from the underlying QDMI device.
     */
    auto initQubitsNumFromDevice() -> void;

    /**
     * @brief Initializes the length unit from the underlying QDMI device.
     */
    auto initLengthUnitFromDevice() -> bool;

    /**
     * @brief Initializes the duration unit from the underlying QDMI device.
     */
    auto initDurationUnitFromDevice() -> bool;

    /**
     * @brief Initializes the decoherence times from the underlying QDMI device.
     */
    auto initDecoherenceTimesFromDevice() -> bool;

    /**
     * @brief Initializes the trap lattices from the underlying QDMI device.
     * @details It reconstructs the entire lattice structure from the
     * information retrieved from the QDMI device, including lattice vectors,
     * sublattice offsets, and extent.
     * @see na::Device::Lattice
     */
    auto initTrapsfromDevice() -> bool;

    /**
     * @brief Initializes the all operations from the underlying QDMI device.
     */
    auto initOperationsFromDevice() -> bool;

    /**
     * @brief Constructs a Device object from a fomac::Session::Device object.
     * @param device The fomac::Session::Device object to wrap.
     * @note The constructor does not initialize the additional fields of this
     * class. For their initialization, the corresponding `init*FromDevice`
     * methods must be called, see @ref tryCreateFromDevice.
     */
    explicit Device(const fomac::Device& device)
        : fomac::Device(device), na::Device() {};

  public:
    /// @returns the length unit of the device.
    [[nodiscard]] auto getLengthUnit() const -> const Unit& {
      return lengthUnit;
    }

    /// @returns the duration unit of the device.
    [[nodiscard]] auto getDurationUnit() const -> const Unit& {
      return durationUnit;
    }

    /// @returns the decoherence times of the device.
    [[nodiscard]] auto getDecoherenceTimes() const -> const DecoherenceTimes& {
      return decoherenceTimes;
    }

    /// @returns the list of trap lattices of the device.
    [[nodiscard]] auto getTraps() const -> const std::vector<Lattice>& {
      return traps;
    }

    /**
     * @brief Try to create a Device object from a fomac::Session::Device
     * object.
     * @details This method attempts to create a Device object by initializing
     * all necessary fields from the provided fomac::Session::Device object. If
     * any required information is missing or invalid, the method returns
     * `std::nullopt`.
     * @param device is the fomac::Session::Device object to wrap.
     * @return An optional containing the instantiated device if compatible,
     * std::nullopt otherwise.
     */
    [[nodiscard]] static auto tryCreateFromDevice(const fomac::Device& device)
        -> std::optional<Device> {
      Device d(device);
      // The sequence of the following method calls does not matter.
      // They are independent of each other.
      if (!d.initMinAtomDistanceFromDevice()) {
        return std::nullopt;
      }
      if (!d.initLengthUnitFromDevice()) {
        return std::nullopt;
      }
      if (!d.initDurationUnitFromDevice()) {
        return std::nullopt;
      }
      if (!d.initDecoherenceTimesFromDevice()) {
        return std::nullopt;
      }
      if (!d.initTrapsfromDevice()) {
        return std::nullopt;
      }
      if (!d.initOperationsFromDevice()) {
        return std::nullopt;
      }
      d.initNameFromDevice();
      d.initQubitsNumFromDevice();
      return d;
    }

    // The following is the result of
    // NLOHMANN_DEFINE_DERIVED_TYPE_INTRUSIVE_ONLY_SERIALIZE(Device, na::Device)
    // without any new attributes, which is the reason the macro cannot be used.
    template <typename BasicJsonType>
    friend void to_json(BasicJsonType& nlohmannJsonJ,
                        const Device& nlohmannJsonT) {
      // NOLINTNEXTLINE(misc-include-cleaner)
      nlohmann::to_json(nlohmannJsonJ,
                        static_cast<const na::Device&>(nlohmannJsonT));
    }
  };

  /// @brief Deleted default constructor to prevent instantiation.
  Session() = delete;

  /// @see QDMI_SESSION_PROPERTY_DEVICES
  [[nodiscard]] static auto getDevices() -> std::vector<Device>;
};

} // namespace na
