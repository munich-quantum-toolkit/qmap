/*
 * Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
 * Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#pragma once

/** @file
 * @brief The MQT QDMI device implementation for neutral atom devices.
 */

#include "mqt_qmap_na_qdmi/device.h"
#include "na/qdmi/Configuration.hpp"
#include "qdmi/common/Common.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

/**
 * @brief Implementation of the MQT_QMAP_NA_QDMI_Device_Session structure.
 */
struct MQT_QMAP_NA_QDMI_Device_Session_impl_d {
private:
  /// The status of the session.
  enum class Status : uint8_t {
    ALLOCATED,   ///< The session has been allocated but not initialized
    INITIALIZED, ///< The session has been initialized and is ready for use
  };
  /// @brief The current status of the session.
  Status status_ = Status::ALLOCATED;
  std::optional<std::string> inlineConfiguration_;
  std::optional<std::filesystem::path> fileConfiguration_;
  std::string name_;
  size_t qubitsNum_ = 0;
  na::Device::Unit lengthUnit_;
  na::Device::Unit durationUnit_;
  uint64_t minAtomDistance_ = 0;
  std::vector<std::unique_ptr<MQT_QMAP_NA_QDMI_Site_impl_d>> siteStorage_;
  std::vector<MQT_QMAP_NA_QDMI_Site> sites_;
  std::vector<std::unique_ptr<MQT_QMAP_NA_QDMI_Operation_impl_d>>
      operationStorage_;
  std::vector<MQT_QMAP_NA_QDMI_Operation> operations_;
  /// @brief The device jobs associated with this session.
  std::unordered_map<MQT_QMAP_NA_QDMI_Device_Job,
                     std::unique_ptr<MQT_QMAP_NA_QDMI_Device_Job_impl_d>>
      jobs_;

public:
  /**
   * @brief Initializes the device session.
   * @see MQT_QMAP_NA_QDMI_device_session_init
   */
  int init();

  /**
   * @brief Sets a parameter for the device session.
   * @see MQT_QMAP_NA_QDMI_device_session_set_parameter
   */
  int setParameter(QDMI_Device_Session_Parameter param, size_t size,
                   const void* value) noexcept;

  /**
   * @brief Create a new device job.
   * @see MQT_QMAP_NA_QDMI_device_session_create_device_job
   */
  int createDeviceJob(MQT_QMAP_NA_QDMI_Device_Job* job) noexcept;

  /**
   * @brief Frees the device job.
   * @see MQT_QMAP_NA_QDMI_device_job_free
   */
  void freeDeviceJob(MQT_QMAP_NA_QDMI_Device_Job job);

  /**
   * @brief Forwards a query of a device property to the device.
   * @see MQT_QMAP_NA_QDMI_device_session_query_device_property
   */
  int queryDeviceProperty(QDMI_Device_Property prop, size_t size, void* value,
                          size_t* sizeRet) const;

  /**
   * @brief Forwards a query of a site property to the site.
   * @see MQT_QMAP_NA_QDMI_device_session_query_site_property
   */
  int querySiteProperty(MQT_QMAP_NA_QDMI_Site site, QDMI_Site_Property prop,
                        size_t size, void* value, size_t* sizeRet) const;

  /**
   * @brief Forwards a query of an operation property to the operation.
   * @see MQT_QMAP_NA_QDMI_device_session_query_operation_property
   */
  int queryOperationProperty(MQT_QMAP_NA_QDMI_Operation operation,
                             size_t numSites,
                             const MQT_QMAP_NA_QDMI_Site* sites,
                             size_t numParams, const double* params,
                             QDMI_Operation_Property prop, size_t size,
                             void* value, size_t* sizeRet) const;
};

/**
 * @brief Implementation of the MQT_QMAP_NA_QDMI_Device_Job structure.
 */
struct MQT_QMAP_NA_QDMI_Device_Job_impl_d {
private:
  /// @brief The device session associated with the job.
  MQT_QMAP_NA_QDMI_Device_Session_impl_d* session_;

public:
  /// @brief Constructor for the MQT_QMAP_NA_QDMI_Device_Job_impl_d.
  explicit MQT_QMAP_NA_QDMI_Device_Job_impl_d(
      MQT_QMAP_NA_QDMI_Device_Session_impl_d* session)
      : session_(session) {}
  /**
   * @brief Frees the device job.
   * @note This function just forwards to the session's @ref
   * MQT_QMAP_NA_QDMI_Device_Session_impl_d::freeDeviceJob function. This
   * function is needed because the interface only provides the job handle to
   * the @ref QDMI_job_free function and the job's session handle is private.
   */
  void free();

  /**
   * @brief Sets a parameter for the job.
   * @see MQT_QMAP_NA_QDMI_device_job_set_parameter
   */
  int setParameter(QDMI_Device_Job_Parameter param, size_t size,
                   const void* value);

  /**
   * @brief Queries a property of the job.
   * @see MQT_QMAP_NA_QDMI_device_job_query_property
   */
  int queryProperty(QDMI_Device_Job_Property prop, size_t size, void* value,
                    size_t* sizeRet);

  /**
   * @brief Submits the job to the device.
   * @see MQT_QMAP_NA_QDMI_device_job_submit
   */
  int submit();

  /**
   * @brief Cancels the job.
   * @see MQT_QMAP_NA_QDMI_device_job_cancel
   */
  int cancel();

  /**
   * @brief Checks the status of the job.
   * @see MQT_QMAP_NA_QDMI_device_job_check
   */
  int check(QDMI_Job_Status* status);

  /**
   * @brief Waits for the job to complete but at most for the specified timeout.
   * @see MQT_QMAP_NA_QDMI_device_job_wait
   */
  int wait(size_t timeout);

  /**
   * @brief Gets the results of the job.
   * @see MQT_QMAP_NA_QDMI_device_job_get_results
   */
  int getResults(QDMI_Job_Result result, size_t size, void* data,
                 [[maybe_unused]] size_t* sizeRet);
};

/**
 * @brief Implementation of the MQT_QMAP_NA_QDMI_Device_Site structure.
 */
struct MQT_QMAP_NA_QDMI_Site_impl_d {
  friend MQT_QMAP_NA_QDMI_Operation_impl_d;

private:
  MQT_QMAP_NA_QDMI_Device_Session_impl_d* owner_ = nullptr;
  uint64_t id_ = 0;       ///< Unique identifier of the site
  uint64_t moduleId_ = 0; ///< Identifier of the module the site belongs to
  /// Identifier of the submodule the site belongs to
  uint64_t subModuleId_ = 0;
  int64_t x_ = 0;        ///< X coordinate of the site in the lattice
  int64_t y_ = 0;        ///< Y coordinate of the site in the lattice
  uint64_t xExtent_ = 0; ///< Width of the site in the lattice (for zone sites)
  uint64_t yExtent_ = 0; ///< Height of the site in the lattice (for zone sites)
  /// @brief Collects decoherence times for the device.
  struct DecoherenceTimes {
    uint64_t t1_ = 0; ///< T1 time
    uint64_t t2_ = 0; ///< T2 time
  };
  /// @brief The decoherence times of the device.
  DecoherenceTimes decoherenceTimes_{};
  bool isZone = false; ///< Indicates if the site is a zone site

public:
  /// @brief Constructor for regular sites.
  MQT_QMAP_NA_QDMI_Site_impl_d(MQT_QMAP_NA_QDMI_Device_Session_impl_d* owner,
                               uint64_t id, uint64_t moduleId,
                               uint64_t subModuleId, int64_t x, int64_t y,
                               uint64_t t1, uint64_t t2);
  /// @brief Constructor for zone sites.
  MQT_QMAP_NA_QDMI_Site_impl_d(MQT_QMAP_NA_QDMI_Device_Session_impl_d* owner,
                               uint64_t id, int64_t x, int64_t y,
                               uint64_t width, uint64_t height, uint64_t t1,
                               uint64_t t2);

  /**
   * @brief Queries a property of the site.
   * @see MQT_QMAP_NA_QDMI_device_session_query_site_property
   */
  int queryProperty(QDMI_Site_Property prop, size_t size, void* value,
                    size_t* sizeRet) const;
  [[nodiscard]] bool
  ownedBy(const MQT_QMAP_NA_QDMI_Device_Session_impl_d* session) const {
    return owner_ == session;
  }
};

/**
 * @brief Implementation of the MQT_QMAP_NA_QDMI_Device_Operation structure.
 */
struct MQT_QMAP_NA_QDMI_Operation_impl_d {
private:
  MQT_QMAP_NA_QDMI_Device_Session_impl_d* owner_ = nullptr;
  std::string name_;     ///< Name of the operation
  size_t numParameters_; ///< Number of parameters for the operation
  /**
   * @brief Number of qubits involved in the operation
   * @note This number is only valid if the operation is a multi-qubit
   * operation.
   */
  std::optional<size_t> numQubits_ = std::nullopt;
  /// Duration of the operation
  std::optional<uint64_t> duration_ = std::nullopt;
  std::optional<double> fidelity_ = std::nullopt; ///< Fidelity of the operation
  /// Interaction radius for multi-qubit operations
  std::optional<uint64_t> interactionRadius_ = std::nullopt;
  /// Blocking radius for multi-qubit operations
  std::optional<uint64_t> blockingRadius_ = std::nullopt;
  /// Mean shuttling speed
  std::optional<uint64_t> meanShuttlingSpeed_ = std::nullopt;
  /// Idling fidelity
  std::optional<double> idlingFidelity_ = std::nullopt;

  /**
   * @brief Storage for individual sites and site pairs.
   * @details Uses std::variant to preserve the tuple structure of the operation
   * sites:
   * - Single-qubit and zoned operations: vector<Site>
   * - Local two-qubit operations: vector<pair<Site, Site>>
   * This maintains type safety and QDMI specification compliance, which states
   * that operation sites should be "a list of tuples" for local multi-qubit
   * operations.
   */
  using SitesStorage = std::variant<
      std::vector<MQT_QMAP_NA_QDMI_Site>,
      std::vector<std::pair<MQT_QMAP_NA_QDMI_Site, MQT_QMAP_NA_QDMI_Site>>>;

  /// The operation's supported sites
  SitesStorage supportedSites_;
  /// Flattened site tuples used by the QDMI list-property representation.
  std::vector<MQT_QMAP_NA_QDMI_Site> flattenedSupportedSites_;
  /// Indicates if this operation is zoned (global)
  bool isZoned_ = false;

public:
  /// @brief Constructor for the global single-qubit.
  MQT_QMAP_NA_QDMI_Operation_impl_d(
      MQT_QMAP_NA_QDMI_Device_Session_impl_d* owner, std::string name,
      size_t numParameters, size_t numQubits, uint64_t duration,
      double fidelity, MQT_QMAP_NA_QDMI_Site zone);
  /// @brief Constructor for the global multi-qubit operations.
  MQT_QMAP_NA_QDMI_Operation_impl_d(
      MQT_QMAP_NA_QDMI_Device_Session_impl_d* owner, std::string name,
      size_t numParameters, size_t numQubits, uint64_t duration,
      double fidelity, uint64_t interactionRadius, uint64_t blockingRadius,
      double idlingFidelity, MQT_QMAP_NA_QDMI_Site zone);
  /// @brief Constructor for the single-qubit operations.
  MQT_QMAP_NA_QDMI_Operation_impl_d(
      MQT_QMAP_NA_QDMI_Device_Session_impl_d* owner, std::string name,
      size_t numParameters, uint64_t duration, double fidelity,
      std::vector<MQT_QMAP_NA_QDMI_Site> sites);
  /// @brief Constructor for the local two-qubit operations.
  MQT_QMAP_NA_QDMI_Operation_impl_d(
      MQT_QMAP_NA_QDMI_Device_Session_impl_d* owner, std::string name,
      size_t numParameters, size_t numQubits, uint64_t duration,
      double fidelity, uint64_t interactionRadius, uint64_t blockingRadius,
      std::vector<std::pair<MQT_QMAP_NA_QDMI_Site, MQT_QMAP_NA_QDMI_Site>>
          sites);
  /// @brief Constructor for load and store operations.
  MQT_QMAP_NA_QDMI_Operation_impl_d(
      MQT_QMAP_NA_QDMI_Device_Session_impl_d* owner, std::string name,
      size_t numParameters, uint64_t duration, double fidelity,
      MQT_QMAP_NA_QDMI_Site zone);
  /// @brief Constructor for the shuttling operations.
  MQT_QMAP_NA_QDMI_Operation_impl_d(
      MQT_QMAP_NA_QDMI_Device_Session_impl_d* owner, std::string name,
      size_t numParameters, MQT_QMAP_NA_QDMI_Site zone,
      uint64_t meanShuttlingSpeed);

private:
  /// @brief Sort the sites such that the occurrence of a given site can be
  /// determined in O(log n) time.
  void sortSites();

public:
  [[nodiscard]] bool
  ownedBy(const MQT_QMAP_NA_QDMI_Device_Session_impl_d* session) const {
    return owner_ == session;
  }

  /**
   * @brief Queries a property of the operation.
   * @see MQT_QMAP_NA_QDMI_device_session_query_operation_property
   */
  int queryProperty(size_t numSites, const MQT_QMAP_NA_QDMI_Site* sites,
                    size_t numParams, const double* params,
                    QDMI_Operation_Property prop, size_t size, void* value,
                    size_t* sizeRet) const;
};
