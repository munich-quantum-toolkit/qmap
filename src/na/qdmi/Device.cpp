/*
 * Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
 * Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

/** @file Device.cpp
 * @brief The MQT QDMI device implementation for neutral atom devices.
 */

#include "na/qdmi/Device.hpp"

#include "mqt_qmap_na_qdmi/device.h"
#include "na/qdmi/Configuration.hpp"
#include "qdmi/common/Common.hpp"
#include "qdmi/common/DeviceConfiguration.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <exception>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <new>
#include <optional>
#include <span>
#include <stdexcept>
#include <stdio.h>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace {
/// Writes one best-effort diagnostic to the process standard error stream.
///
/// The writer does not allocate memory. This property lets exception handlers
/// report allocation failures without throwing another exception.
void emitDiagnostic(
    const std::initializer_list<std::string_view> parts) noexcept {
#ifdef _WIN32
  _lock_file(stderr);
#else
  flockfile(stderr);
#endif
  const auto write = [](const std::string_view part) noexcept {
    if (!part.empty()) {
#ifdef _WIN32
      _fwrite_nolock(part.data(), sizeof(char), part.size(), stderr);
#else
      std::fwrite(part.data(), sizeof(char), part.size(), stderr);
#endif
    }
  };
  write("[mqt-qmap] [error] ");
  for (const auto part : parts) {
    write(part);
  }
#ifdef _WIN32
  _fputc_nolock('\n', stderr);
  _fflush_nolock(stderr);
  _unlock_file(stderr);
#else
  std::fputc('\n', stderr);
  std::fflush(stderr);
  funlockfile(stderr);
#endif
}

/// Maps exceptions from an allocation-capable device call to QDMI status codes.
template <class Callable>
[[nodiscard]] auto guardDeviceCall(const std::string_view action,
                                   Callable&& callable) noexcept -> int {
  try {
    return std::invoke(std::forward<Callable>(callable));
  } catch (const std::bad_alloc&) {
    emitDiagnostic({"Out of memory while ", action});
    return QDMI_ERROR_OUTOFMEM;
  } catch (const std::exception& error) {
    emitDiagnostic({"Failed while ", action, ": ", error.what()});
    return QDMI_ERROR_FATAL;
  } catch (...) {
    emitDiagnostic({"Failed while ", action, ": unknown exception"});
    return QDMI_ERROR_FATAL;
  }
}

[[nodiscard]] auto inside(const na::Device::Region& region, const int64_t x,
                          const int64_t y) -> bool {
  return region.origin.x <= x &&
         x <= region.origin.x + static_cast<int64_t>(region.size.width) &&
         region.origin.y <= y &&
         y <= region.origin.y + static_cast<int64_t>(region.size.height);
}

[[nodiscard]] auto magnitude(const int64_t value) -> uint64_t {
  return value >= 0
             ? static_cast<uint64_t>(value)
             : static_cast<uint64_t>(-(value + 1)) + static_cast<uint64_t>(1);
}

[[nodiscard]] auto coordinateDistance(const int64_t first, const int64_t second)
    -> uint64_t {
  if ((first < 0) == (second < 0)) {
    return first >= second ? static_cast<uint64_t>(first - second)
                           : static_cast<uint64_t>(second - first);
  }
  return magnitude(first) + magnitude(second);
}

[[nodiscard]] auto withinRadius(const int64_t firstX, const int64_t firstY,
                                const int64_t secondX, const int64_t secondY,
                                const uint64_t radius) -> bool {
  const auto deltaX = coordinateDistance(firstX, secondX);
  const auto deltaY = coordinateDistance(firstY, secondY);
  return std::hypot(static_cast<double>(deltaX), static_cast<double>(deltaY)) <=
         static_cast<double>(radius);
}
} // namespace

auto MQT_QMAP_NA_QDMI_Device_Session_impl_d::init() -> int {
  if (status_ != Status::ALLOCATED) {
    return QDMI_ERROR_BADSTATE;
  }
  std::optional<::qdmi::detail::LoadedDeviceConfiguration> loaded;
  try {
    int loadStatus = QDMI_SUCCESS;
    loaded = ::qdmi::detail::loadDeviceConfiguration(
        inlineConfiguration_, fileConfiguration_,
        "MQT_QMAP_QDMI_NA_CONFIG_JSON", "MQT_QMAP_QDMI_NA_CONFIG_FILE",
        "mqt-qmap-qdmi-na-device.json",
        reinterpret_cast<const void*>(&MQT_QMAP_NA_QDMI_device_initialize),
        loadStatus);
    if (!loaded) {
      return loadStatus;
    }
    const auto configuration = na::readJSON(loaded->json, loaded->source);
    std::vector<std::unique_ptr<MQT_QMAP_NA_QDMI_Site_impl_d>> newSiteStorage;
    std::vector<MQT_QMAP_NA_QDMI_Site> newSites;
    std::vector<MQT_QMAP_NA_QDMI_Site> globalMultiZones;
    std::vector<MQT_QMAP_NA_QDMI_Site> globalSingleZones;
    std::vector<MQT_QMAP_NA_QDMI_Site> shuttlingZones;
    const auto addZone = [&](const na::Device::Region& region) {
      auto site = std::make_unique<MQT_QMAP_NA_QDMI_Site_impl_d>(
          this, newSiteStorage.size(), region.origin.x, region.origin.y,
          region.size.width, region.size.height,
          configuration.decoherenceTimes.t1, configuration.decoherenceTimes.t2);
      auto* const handle = site.get();
      newSiteStorage.emplace_back(std::move(site));
      newSites.emplace_back(handle);
      return handle;
    };
    globalMultiZones.reserve(configuration.globalMultiQubitOperations.size());
    for (const auto& operation : configuration.globalMultiQubitOperations) {
      globalMultiZones.emplace_back(addZone(operation.region));
    }
    globalSingleZones.reserve(configuration.globalSingleQubitOperations.size());
    for (const auto& operation : configuration.globalSingleQubitOperations) {
      globalSingleZones.emplace_back(addZone(operation.region));
    }
    shuttlingZones.reserve(configuration.shuttlingUnits.size());
    for (const auto& unit : configuration.shuttlingUnits) {
      shuttlingZones.emplace_back(addZone(unit.region));
    }

    struct RegularSite {
      MQT_QMAP_NA_QDMI_Site handle;
      int64_t x;
      int64_t y;
    };
    std::vector<RegularSite> regularSites;
    na::forEachRegularSites(configuration.traps, [&](const na::SiteInfo& info) {
      auto site = std::make_unique<MQT_QMAP_NA_QDMI_Site_impl_d>(
          this, newSiteStorage.size(), info.moduleId, info.subModuleId, info.x,
          info.y, configuration.decoherenceTimes.t1,
          configuration.decoherenceTimes.t2);
      auto* const handle = site.get();
      newSiteStorage.emplace_back(std::move(site));
      newSites.emplace_back(handle);
      regularSites.push_back({.handle = handle, .x = info.x, .y = info.y});
    });

    std::vector<std::unique_ptr<MQT_QMAP_NA_QDMI_Operation_impl_d>>
        newOperationStorage;
    const auto addOperation = [&]<typename... Args>(Args&&... args) {
      newOperationStorage.emplace_back(
          std::make_unique<MQT_QMAP_NA_QDMI_Operation_impl_d>(
              this, std::forward<Args>(args)...));
    };
    for (size_t i = 0; i < configuration.globalSingleQubitOperations.size();
         ++i) {
      const auto& operation = configuration.globalSingleQubitOperations[i];
      addOperation(operation.name, operation.numParameters, 1U,
                   operation.duration, operation.fidelity,
                   globalSingleZones[i]);
    }
    for (size_t i = 0; i < configuration.globalMultiQubitOperations.size();
         ++i) {
      const auto& operation = configuration.globalMultiQubitOperations[i];
      addOperation(operation.name, operation.numParameters, operation.numQubits,
                   operation.duration, operation.fidelity,
                   operation.interactionRadius, operation.blockingRadius,
                   operation.idlingFidelity, globalMultiZones[i]);
    }
    for (const auto& operation : configuration.localSingleQubitOperations) {
      std::vector<MQT_QMAP_NA_QDMI_Site> supported;
      for (const auto& site : regularSites) {
        if (inside(operation.region, site.x, site.y)) {
          supported.emplace_back(site.handle);
        }
      }
      addOperation(operation.name, operation.numParameters, operation.duration,
                   operation.fidelity, std::move(supported));
    }
    for (const auto& operation : configuration.localMultiQubitOperations) {
      std::vector<std::pair<MQT_QMAP_NA_QDMI_Site, MQT_QMAP_NA_QDMI_Site>>
          supported;
      for (size_t first = 0; first < regularSites.size(); ++first) {
        if (!inside(operation.region, regularSites[first].x,
                    regularSites[first].y)) {
          continue;
        }
        for (size_t second = first + 1; second < regularSites.size();
             ++second) {
          if (inside(operation.region, regularSites[second].x,
                     regularSites[second].y) &&
              withinRadius(regularSites[first].x, regularSites[first].y,
                           regularSites[second].x, regularSites[second].y,
                           operation.interactionRadius)) {
            supported.emplace_back(regularSites[first].handle,
                                   regularSites[second].handle);
          }
        }
      }
      addOperation(operation.name, operation.numParameters, operation.numQubits,
                   operation.duration, operation.fidelity,
                   operation.interactionRadius, operation.blockingRadius,
                   std::move(supported));
    }
    for (size_t i = 0; i < configuration.shuttlingUnits.size(); ++i) {
      const auto& unit = configuration.shuttlingUnits[i];
      addOperation("load<" + std::to_string(unit.id) + ">", unit.numParameters,
                   unit.loadDuration, unit.loadFidelity, shuttlingZones[i]);
      addOperation("move<" + std::to_string(unit.id) + ">", unit.numParameters,
                   shuttlingZones[i], unit.meanShuttlingSpeed);
      addOperation("store<" + std::to_string(unit.id) + ">", unit.numParameters,
                   unit.storeDuration, unit.storeFidelity, shuttlingZones[i]);
    }
    std::vector<MQT_QMAP_NA_QDMI_Operation> newOperations;
    newOperations.reserve(newOperationStorage.size());
    std::ranges::transform(
        newOperationStorage, std::back_inserter(newOperations),
        [](const auto& operation) { return operation.get(); });

    name_ = configuration.name;
    qubitsNum_ = configuration.numQubits;
    lengthUnit_ = configuration.lengthUnit;
    durationUnit_ = configuration.durationUnit;
    minAtomDistance_ = configuration.minAtomDistance;
    siteStorage_ = std::move(newSiteStorage);
    sites_ = std::move(newSites);
    operationStorage_ = std::move(newOperationStorage);
    operations_ = std::move(newOperations);
    status_ = Status::INITIALIZED;
    return QDMI_SUCCESS;
  } catch (const std::bad_alloc&) {
    const std::string_view source =
        loaded ? std::string_view(loaded->source)
               : std::string_view("selected configuration");
    emitDiagnostic(
        {"Out of memory while initializing NA device from ", source});
    return QDMI_ERROR_OUTOFMEM;
  } catch (const std::invalid_argument& error) {
    const std::string_view source =
        loaded ? std::string_view(loaded->source)
               : std::string_view("selected configuration");
    emitDiagnostic(
        {"Invalid NA device configuration from ", source, ": ", error.what()});
    return QDMI_ERROR_INVALIDARGUMENT;
  } catch (const std::exception& error) {
    const std::string_view source =
        loaded ? std::string_view(loaded->source)
               : std::string_view("selected configuration");
    emitDiagnostic(
        {"Failed to initialize NA device from ", source, ": ", error.what()});
    return QDMI_ERROR_FATAL;
  }
}
auto MQT_QMAP_NA_QDMI_Device_Session_impl_d::setParameter(
    QDMI_Device_Session_Parameter param, const size_t size,
    const void* value) noexcept -> int {
  if ((value != nullptr && size == 0) ||
      IS_INVALID_ARGUMENT(param, QDMI_DEVICE_SESSION_PARAMETER)) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }
  if (status_ != Status::ALLOCATED) {
    return QDMI_ERROR_BADSTATE;
  }
  return guardDeviceCall("setting an NA device session parameter", [&] {
    return ::qdmi::detail::setDeviceConfigurationParameter(
        param, size, value, inlineConfiguration_, fileConfiguration_);
  });
}
auto MQT_QMAP_NA_QDMI_Device_Session_impl_d::createDeviceJob(
    // NOLINTNEXTLINE(readability-non-const-parameter)
    MQT_QMAP_NA_QDMI_Device_Job* job) noexcept -> int {
  if (job == nullptr) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }
  if (status_ == Status::ALLOCATED) {
    return QDMI_ERROR_BADSTATE;
  }
  *job = nullptr;
  return guardDeviceCall("creating an NA device job", [&] {
    auto uniqueJob = std::make_unique<MQT_QMAP_NA_QDMI_Device_Job_impl_d>(this);
    *job = jobs_.emplace(uniqueJob.get(), std::move(uniqueJob)).first->first;
    return QDMI_SUCCESS;
  });
}
void MQT_QMAP_NA_QDMI_Device_Session_impl_d::freeDeviceJob(
    MQT_QMAP_NA_QDMI_Device_Job job) {
  if (job != nullptr) {
    jobs_.erase(job);
  }
}
auto MQT_QMAP_NA_QDMI_Device_Session_impl_d::queryDeviceProperty(
    const QDMI_Device_Property prop, const size_t size, void* value,
    size_t* sizeRet) const -> int {
  if (status_ != Status::INITIALIZED) {
    return QDMI_ERROR_BADSTATE;
  }
  if ((value != nullptr && size == 0) ||
      IS_INVALID_ARGUMENT(prop, QDMI_DEVICE_PROPERTY)) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }
  ADD_STRING_PROPERTY(QDMI_DEVICE_PROPERTY_NAME, name_.c_str(), prop, size,
                      value, sizeRet)
  ADD_STRING_PROPERTY(QDMI_DEVICE_PROPERTY_VERSION, MQT_QMAP_VERSION, prop,
                      size, value, sizeRet)
  ADD_STRING_PROPERTY(QDMI_DEVICE_PROPERTY_LIBRARYVERSION, QDMI_VERSION, prop,
                      size, value, sizeRet)
  ADD_SINGLE_VALUE_PROPERTY(QDMI_DEVICE_PROPERTY_STATUS, QDMI_Device_Status,
                            QDMI_DEVICE_STATUS_IDLE, prop, size, value, sizeRet)
  ADD_SINGLE_VALUE_PROPERTY(QDMI_DEVICE_PROPERTY_QUBITSNUM, size_t, qubitsNum_,
                            prop, size, value, sizeRet)
  ADD_SINGLE_VALUE_PROPERTY(QDMI_DEVICE_PROPERTY_NEEDSCALIBRATION, size_t, 0,
                            prop, size, value, sizeRet)
  ADD_SINGLE_VALUE_PROPERTY(
      QDMI_DEVICE_PROPERTY_PULSESUPPORT, QDMI_Device_Pulse_Support_Level,
      QDMI_DEVICE_PULSE_SUPPORT_LEVEL_NONE, prop, size, value, sizeRet)
  ADD_STRING_PROPERTY(QDMI_DEVICE_PROPERTY_LENGTHUNIT, lengthUnit_.unit.c_str(),
                      prop, size, value, sizeRet)
  ADD_SINGLE_VALUE_PROPERTY(QDMI_DEVICE_PROPERTY_LENGTHSCALEFACTOR, double,
                            lengthUnit_.scaleFactor, prop, size, value, sizeRet)
  ADD_STRING_PROPERTY(QDMI_DEVICE_PROPERTY_DURATIONUNIT,
                      durationUnit_.unit.c_str(), prop, size, value, sizeRet)
  ADD_SINGLE_VALUE_PROPERTY(QDMI_DEVICE_PROPERTY_DURATIONSCALEFACTOR, double,
                            durationUnit_.scaleFactor, prop, size, value,
                            sizeRet)
  ADD_SINGLE_VALUE_PROPERTY(QDMI_DEVICE_PROPERTY_MINATOMDISTANCE, uint64_t,
                            minAtomDistance_, prop, size, value, sizeRet)
  ADD_LIST_PROPERTY(QDMI_DEVICE_PROPERTY_SITES, MQT_QMAP_NA_QDMI_Site, sites_,
                    prop, size, value, sizeRet)
  ADD_LIST_PROPERTY(QDMI_DEVICE_PROPERTY_OPERATIONS, MQT_QMAP_NA_QDMI_Operation,
                    operations_, prop, size, value, sizeRet)
  if (prop == QDMI_DEVICE_PROPERTY_SUPPORTEDPROGRAMFORMATS) {
    if (value != nullptr && size > 0) {
      return QDMI_ERROR_INVALIDARGUMENT;
    }
    if (sizeRet != nullptr) {
      *sizeRet = 0;
    }
    return QDMI_SUCCESS;
  }
  return QDMI_ERROR_NOTSUPPORTED;
}
auto MQT_QMAP_NA_QDMI_Device_Session_impl_d::querySiteProperty(
    MQT_QMAP_NA_QDMI_Site site, const QDMI_Site_Property prop,
    const size_t size, void* value, size_t* sizeRet) const -> int {
  if (site == nullptr || std::ranges::find(sites_, site) == sites_.end() ||
      !site->ownedBy(this)) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }
  if (status_ != Status::INITIALIZED) {
    return QDMI_ERROR_BADSTATE;
  }
  return site->queryProperty(prop, size, value, sizeRet);
}
auto MQT_QMAP_NA_QDMI_Device_Session_impl_d::queryOperationProperty(
    MQT_QMAP_NA_QDMI_Operation operation, const size_t numSites,
    const MQT_QMAP_NA_QDMI_Site* sites, const size_t numParams,
    const double* params, const QDMI_Operation_Property prop, const size_t size,
    void* value, size_t* sizeRet) const -> int {
  if (operation == nullptr ||
      std::ranges::find(operations_, operation) == operations_.end() ||
      !operation->ownedBy(this)) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }
  if (status_ != Status::INITIALIZED) {
    return QDMI_ERROR_BADSTATE;
  }
  if (sites != nullptr) {
    for (auto* const site : std::span{sites, numSites}) {
      if (site == nullptr || std::ranges::find(sites_, site) == sites_.end() ||
          !site->ownedBy(this)) {
        return QDMI_ERROR_INVALIDARGUMENT;
      }
    }
  }
  return operation->queryProperty(numSites, sites, numParams, params, prop,
                                  size, value, sizeRet);
}
void MQT_QMAP_NA_QDMI_Device_Job_impl_d::free() {
  session_->freeDeviceJob(this);
}
// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
auto MQT_QMAP_NA_QDMI_Device_Job_impl_d::setParameter(
    const QDMI_Device_Job_Parameter param, const size_t size, const void* value)
    -> int {
  if ((value != nullptr && size == 0) ||
      IS_INVALID_ARGUMENT(param, QDMI_DEVICE_JOB_PARAMETER)) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }
  return QDMI_ERROR_NOTSUPPORTED;
}
// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
auto MQT_QMAP_NA_QDMI_Device_Job_impl_d::queryProperty(
    // NOLINTNEXTLINE(readability-non-const-parameter)
    const QDMI_Device_Job_Property prop, const size_t size, void* value,
    [[maybe_unused]] size_t* sizeRet) -> int {
  if ((value != nullptr && size == 0) ||
      IS_INVALID_ARGUMENT(prop, QDMI_DEVICE_JOB_PROPERTY)) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }
  return QDMI_ERROR_NOTSUPPORTED;
}
// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
auto MQT_QMAP_NA_QDMI_Device_Job_impl_d::submit() -> int {
  return QDMI_ERROR_NOTSUPPORTED;
}
// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
auto MQT_QMAP_NA_QDMI_Device_Job_impl_d::cancel() -> int {
  return QDMI_ERROR_NOTSUPPORTED;
}
// NOLINTNEXTLINE(readability-non-const-parameter,readability-convert-member-functions-to-static)
auto MQT_QMAP_NA_QDMI_Device_Job_impl_d::check(QDMI_Job_Status* status) -> int {
  if (status == nullptr) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }
  return QDMI_ERROR_NOTSUPPORTED;
}
// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
auto MQT_QMAP_NA_QDMI_Device_Job_impl_d::wait(
    [[maybe_unused]] const size_t timeout) -> int {
  return QDMI_ERROR_NOTSUPPORTED;
}
// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
auto MQT_QMAP_NA_QDMI_Device_Job_impl_d::getResults(
    QDMI_Job_Result result,
    // NOLINTNEXTLINE(readability-non-const-parameter)
    const size_t size, void* data, [[maybe_unused]] size_t* sizeRet) -> int {
  if ((data != nullptr && size == 0) ||
      IS_INVALID_ARGUMENT(result, QDMI_JOB_RESULT)) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }
  return QDMI_ERROR_NOTSUPPORTED;
}
MQT_QMAP_NA_QDMI_Site_impl_d::MQT_QMAP_NA_QDMI_Site_impl_d(
    MQT_QMAP_NA_QDMI_Device_Session_impl_d* owner, const uint64_t id,
    const uint64_t module, const uint64_t subModule, const int64_t x,
    const int64_t y, const uint64_t t1, const uint64_t t2)
    : owner_(owner), id_(id), moduleId_(module), subModuleId_(subModule), x_(x),
      y_(y), decoherenceTimes_{.t1_ = t1, .t2_ = t2} {}
MQT_QMAP_NA_QDMI_Site_impl_d::MQT_QMAP_NA_QDMI_Site_impl_d(
    MQT_QMAP_NA_QDMI_Device_Session_impl_d* owner, const uint64_t id,
    const int64_t x, const int64_t y, const uint64_t width,
    const uint64_t height, const uint64_t t1, const uint64_t t2)
    : owner_(owner), id_(id), x_(x), y_(y), xExtent_(width), yExtent_(height),
      decoherenceTimes_{.t1_ = t1, .t2_ = t2}, isZone(true) {}
auto MQT_QMAP_NA_QDMI_Site_impl_d::queryProperty(const QDMI_Site_Property prop,
                                                 const size_t size, void* value,
                                                 size_t* sizeRet) const -> int {
  if ((value != nullptr && size == 0) ||
      IS_INVALID_ARGUMENT(prop, QDMI_SITE_PROPERTY)) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }
  ADD_SINGLE_VALUE_PROPERTY(QDMI_SITE_PROPERTY_INDEX, uint64_t, id_, prop, size,
                            value, sizeRet)
  ADD_SINGLE_VALUE_PROPERTY(QDMI_SITE_PROPERTY_XCOORDINATE, int64_t, x_, prop,
                            size, value, sizeRet)
  ADD_SINGLE_VALUE_PROPERTY(QDMI_SITE_PROPERTY_YCOORDINATE, int64_t, y_, prop,
                            size, value, sizeRet)
  ADD_SINGLE_VALUE_PROPERTY(QDMI_SITE_PROPERTY_T1, uint64_t,
                            decoherenceTimes_.t1_, prop, size, value, sizeRet)
  ADD_SINGLE_VALUE_PROPERTY(QDMI_SITE_PROPERTY_T2, uint64_t,
                            decoherenceTimes_.t2_, prop, size, value, sizeRet)
  ADD_SINGLE_VALUE_PROPERTY(QDMI_SITE_PROPERTY_ISZONE, bool, isZone, prop, size,
                            value, sizeRet)
  if (isZone) {
    ADD_SINGLE_VALUE_PROPERTY(QDMI_SITE_PROPERTY_XEXTENT, uint64_t, xExtent_,
                              prop, size, value, sizeRet)
    ADD_SINGLE_VALUE_PROPERTY(QDMI_SITE_PROPERTY_YEXTENT, uint64_t, yExtent_,
                              prop, size, value, sizeRet)
  } else {
    ADD_SINGLE_VALUE_PROPERTY(QDMI_SITE_PROPERTY_MODULEINDEX, uint64_t,
                              moduleId_, prop, size, value, sizeRet)
    ADD_SINGLE_VALUE_PROPERTY(QDMI_SITE_PROPERTY_SUBMODULEINDEX, uint64_t,
                              subModuleId_, prop, size, value, sizeRet)
  }
  return QDMI_ERROR_NOTSUPPORTED;
}
MQT_QMAP_NA_QDMI_Operation_impl_d::MQT_QMAP_NA_QDMI_Operation_impl_d(
    MQT_QMAP_NA_QDMI_Device_Session_impl_d* owner, std::string name,
    const size_t numParameters, const size_t numQubits, const uint64_t duration,
    const double fidelity, MQT_QMAP_NA_QDMI_Site zone)
    : owner_(owner), name_(std::move(name)), numParameters_(numParameters),
      numQubits_(numQubits), duration_(duration), fidelity_(fidelity),
      supportedSites_(std::vector<MQT_QMAP_NA_QDMI_Site>{zone}),
      isZoned_(true) {}
MQT_QMAP_NA_QDMI_Operation_impl_d::MQT_QMAP_NA_QDMI_Operation_impl_d(
    MQT_QMAP_NA_QDMI_Device_Session_impl_d* owner, std::string name,
    const size_t numParameters, const size_t numQubits, const uint64_t duration,
    const double fidelity, const uint64_t interactionRadius,
    uint64_t blockingRadius, const double idlingFidelity,
    MQT_QMAP_NA_QDMI_Site zone)
    : owner_(owner), name_(std::move(name)), numParameters_(numParameters),
      numQubits_(numQubits), duration_(duration), fidelity_(fidelity),
      interactionRadius_(interactionRadius), blockingRadius_(blockingRadius),
      idlingFidelity_(idlingFidelity),
      supportedSites_(std::vector<MQT_QMAP_NA_QDMI_Site>{zone}),
      isZoned_(true) {}
MQT_QMAP_NA_QDMI_Operation_impl_d::MQT_QMAP_NA_QDMI_Operation_impl_d(
    MQT_QMAP_NA_QDMI_Device_Session_impl_d* owner, std::string name,
    const size_t numParameters, const uint64_t duration, const double fidelity,
    std::vector<MQT_QMAP_NA_QDMI_Site> sites)
    : owner_(owner), name_(std::move(name)), numParameters_(numParameters),
      numQubits_(1), duration_(duration), fidelity_(fidelity),
      supportedSites_(std::move(sites)) {
  sortSites();
}
MQT_QMAP_NA_QDMI_Operation_impl_d::MQT_QMAP_NA_QDMI_Operation_impl_d(
    MQT_QMAP_NA_QDMI_Device_Session_impl_d* owner, std::string name,
    const size_t numParameters, const size_t numQubits, const uint64_t duration,
    const double fidelity, const uint64_t interactionRadius,
    uint64_t blockingRadius,
    std::vector<std::pair<MQT_QMAP_NA_QDMI_Site, MQT_QMAP_NA_QDMI_Site>> sites)
    : owner_(owner), name_(std::move(name)), numParameters_(numParameters),
      numQubits_(numQubits), duration_(duration), fidelity_(fidelity),
      interactionRadius_(interactionRadius), blockingRadius_(blockingRadius),
      supportedSites_(std::move(sites)) {
  sortSites();
}
MQT_QMAP_NA_QDMI_Operation_impl_d::MQT_QMAP_NA_QDMI_Operation_impl_d(
    MQT_QMAP_NA_QDMI_Device_Session_impl_d* owner, std::string name,
    const size_t numParameters, const uint64_t duration, const double fidelity,
    MQT_QMAP_NA_QDMI_Site zone)
    : owner_(owner), name_(std::move(name)), numParameters_(numParameters),
      duration_(duration), fidelity_(fidelity),
      supportedSites_(std::vector<MQT_QMAP_NA_QDMI_Site>{zone}),
      isZoned_(true) {}
MQT_QMAP_NA_QDMI_Operation_impl_d::MQT_QMAP_NA_QDMI_Operation_impl_d(
    MQT_QMAP_NA_QDMI_Device_Session_impl_d* owner, std::string name,
    const size_t numParameters, MQT_QMAP_NA_QDMI_Site zone,
    const uint64_t meanShuttlingSpeed)
    : owner_(owner), name_(std::move(name)), numParameters_(numParameters),
      meanShuttlingSpeed_(meanShuttlingSpeed),
      supportedSites_(std::vector<MQT_QMAP_NA_QDMI_Site>{zone}),
      isZoned_(true) {}
void MQT_QMAP_NA_QDMI_Operation_impl_d::sortSites() {
  std::visit(
      [this](auto& sites) {
        using T = std::decay_t<decltype(sites)>;
        if constexpr (std::is_same_v<T, std::vector<MQT_QMAP_NA_QDMI_Site>>) {
          // Single-qubit: sort flat list by pointer address
          std::ranges::sort(sites, std::less<MQT_QMAP_NA_QDMI_Site>{});
        } else if constexpr (std::is_same_v<T, std::vector<std::pair<
                                                   MQT_QMAP_NA_QDMI_Site,
                                                   MQT_QMAP_NA_QDMI_Site>>>) {
          // Two-qubit: normalize each pair (first < second)
          // Use std::less for proper total order (pointer comparison with
          // operator> invokes undefined behavior)
          std::ranges::for_each(sites, [](auto& p) {
            if (std::less<MQT_QMAP_NA_QDMI_Site>{}(p.second, p.first)) {
              std::swap(p.first, p.second);
            }
          });
          std::ranges::sort(sites);
          flattenedSupportedSites_.clear();
          flattenedSupportedSites_.reserve(sites.size() * 2);
          for (const auto& [first, second] : sites) {
            flattenedSupportedSites_.emplace_back(first);
            flattenedSupportedSites_.emplace_back(second);
          }
        }
        // more cases go here if needed in the future
      },
      supportedSites_);
}
auto MQT_QMAP_NA_QDMI_Operation_impl_d::queryProperty(
    const size_t numSites, const MQT_QMAP_NA_QDMI_Site* sites,
    const size_t numParams, const double* params,
    const QDMI_Operation_Property prop, const size_t size, void* value,
    size_t* sizeRet) const -> int {
  if ((sites != nullptr && numSites == 0) ||
      (params != nullptr && numParams == 0) ||
      (value != nullptr && size == 0) ||
      IS_INVALID_ARGUMENT(prop, QDMI_OPERATION_PROPERTY) ||
      (isZoned_ && numSites > 1) ||
      (!isZoned_ && numSites > 0 && numQubits_ != numSites)) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }
  if (sites != nullptr) {
    // If numQubits_ == 1 or isZoned_ == true
    if (numSites == 1) {
      // If the (single) site is not supported, return with an error
      const bool found = std::visit(
          [sites](const auto& storedSites) {
            using T = std::decay_t<decltype(storedSites)>;
            if constexpr (std::is_same_v<T,
                                         std::vector<MQT_QMAP_NA_QDMI_Site>>) {
              return std::ranges::binary_search(
                  storedSites, *sites, std::less<MQT_QMAP_NA_QDMI_Site>{});
            }
            return false; // Wrong variant type
          },
          supportedSites_);
      if (!found) {
        return QDMI_ERROR_NOTSUPPORTED;
      }
    } else if (numSites == 2) {
      // NOLINTBEGIN(cppcoreguidelines-pro-bounds-pointer-arithmetic)
      const std::pair needle =
          std::less<MQT_QMAP_NA_QDMI_Site>{}(sites[0], sites[1])
              ? std::make_pair(sites[0], sites[1])
              : std::make_pair(sites[1], sites[0]);
      // NOLINTEND(cppcoreguidelines-pro-bounds-pointer-arithmetic)
      // if the pair of sites is not supported, return with an error
      const bool found = std::visit(
          [&needle](const auto& storedSites) {
            using T = std::decay_t<decltype(storedSites)>;
            if constexpr (std::is_same_v<
                              T,
                              std::vector<std::pair<MQT_QMAP_NA_QDMI_Site,
                                                    MQT_QMAP_NA_QDMI_Site>>>) {
              return std::ranges::binary_search(storedSites, needle);
            }
            return false; // Wrong variant type
          },
          supportedSites_);
      if (!found) {
        return QDMI_ERROR_NOTSUPPORTED;
      }
    } // this device does not support operations with more than two qubits
  }
  ADD_STRING_PROPERTY(QDMI_OPERATION_PROPERTY_NAME, name_.c_str(), prop, size,
                      value, sizeRet)
  ADD_SINGLE_VALUE_PROPERTY(QDMI_OPERATION_PROPERTY_PARAMETERSNUM, size_t,
                            numParameters_, prop, size, value, sizeRet)

  if (prop == QDMI_OPERATION_PROPERTY_SITES) {
    return std::visit(
        [&](const auto& storedSites) {
          using T = std::decay_t<decltype(storedSites)>;
          if constexpr (std::is_same_v<T, std::vector<MQT_QMAP_NA_QDMI_Site>>) {
            // Single-qubit: return flat array
            ADD_LIST_PROPERTY(QDMI_OPERATION_PROPERTY_SITES,
                              MQT_QMAP_NA_QDMI_Site, storedSites, prop, size,
                              value, sizeRet)
          } else if constexpr (std::is_same_v<T, std::vector<std::pair<
                                                     MQT_QMAP_NA_QDMI_Site,
                                                     MQT_QMAP_NA_QDMI_Site>>>) {
            // Local multi-qubit sites are exposed as a flat list of tuples.
            ADD_LIST_PROPERTY(QDMI_OPERATION_PROPERTY_SITES,
                              MQT_QMAP_NA_QDMI_Site, flattenedSupportedSites_,
                              prop, size, value, sizeRet)
          }
          // more cases go here if needed in the future
          return QDMI_ERROR_NOTSUPPORTED;
        },
        supportedSites_);
  }
  if (interactionRadius_) {
    ADD_SINGLE_VALUE_PROPERTY(QDMI_OPERATION_PROPERTY_INTERACTIONRADIUS,
                              uint64_t, *interactionRadius_, prop, size, value,
                              sizeRet)
  }
  if (blockingRadius_) {
    ADD_SINGLE_VALUE_PROPERTY(QDMI_OPERATION_PROPERTY_BLOCKINGRADIUS, uint64_t,
                              *blockingRadius_, prop, size, value, sizeRet)
  }
  if (meanShuttlingSpeed_) {
    ADD_SINGLE_VALUE_PROPERTY(QDMI_OPERATION_PROPERTY_MEANSHUTTLINGSPEED,
                              uint64_t, *meanShuttlingSpeed_, prop, size, value,
                              sizeRet)
  }
  if (duration_) {
    ADD_SINGLE_VALUE_PROPERTY(QDMI_OPERATION_PROPERTY_DURATION, uint64_t,
                              *duration_, prop, size, value, sizeRet)
  }
  if (fidelity_) {
    ADD_SINGLE_VALUE_PROPERTY(QDMI_OPERATION_PROPERTY_FIDELITY, double,
                              *fidelity_, prop, size, value, sizeRet)
  }
  if (numQubits_) {
    ADD_SINGLE_VALUE_PROPERTY(QDMI_OPERATION_PROPERTY_QUBITSNUM, size_t,
                              *numQubits_, prop, size, value, sizeRet)
  }
  if (idlingFidelity_) {
    ADD_SINGLE_VALUE_PROPERTY(QDMI_OPERATION_PROPERTY_IDLINGFIDELITY, double,
                              *idlingFidelity_, prop, size, value, sizeRet)
  }
  ADD_SINGLE_VALUE_PROPERTY(QDMI_OPERATION_PROPERTY_ISZONED, bool, isZoned_,
                            prop, size, value, sizeRet)
  return QDMI_ERROR_NOTSUPPORTED;
}

auto MQT_QMAP_NA_QDMI_device_initialize() -> int { return QDMI_SUCCESS; }

auto MQT_QMAP_NA_QDMI_device_finalize() -> int { return QDMI_SUCCESS; }

auto MQT_QMAP_NA_QDMI_device_session_alloc(
    MQT_QMAP_NA_QDMI_Device_Session* session) -> int {
  if (session == nullptr) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }
  // QDMI transfers ownership through its opaque C handle.
  // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
  *session = new (std::nothrow) MQT_QMAP_NA_QDMI_Device_Session_impl_d;
  return *session == nullptr ? QDMI_ERROR_OUTOFMEM : QDMI_SUCCESS;
}

auto MQT_QMAP_NA_QDMI_device_session_init(
    MQT_QMAP_NA_QDMI_Device_Session session) -> int {
  if (session == nullptr) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }
  return session->init();
}

void MQT_QMAP_NA_QDMI_device_session_free(
    MQT_QMAP_NA_QDMI_Device_Session session) {
  // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
  delete session;
}

auto MQT_QMAP_NA_QDMI_device_session_set_parameter(
    MQT_QMAP_NA_QDMI_Device_Session session,
    QDMI_Device_Session_Parameter param, const size_t size, const void* value)
    -> int {
  if (session == nullptr) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }
  return session->setParameter(param, size, value);
}

auto MQT_QMAP_NA_QDMI_device_session_create_device_job(
    MQT_QMAP_NA_QDMI_Device_Session session, MQT_QMAP_NA_QDMI_Device_Job* job)
    -> int {
  if (session == nullptr) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }
  return session->createDeviceJob(job);
}

auto MQT_QMAP_NA_QDMI_device_session_retrieve_device_job_by_id(
    MQT_QMAP_NA_QDMI_Device_Session session, const char* jobId,
    MQT_QMAP_NA_QDMI_Device_Job* job) -> int {
  if (session == nullptr || jobId == nullptr || jobId[0] == '\0' ||
      job == nullptr) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }
  return QDMI_ERROR_NOTSUPPORTED;
}

void MQT_QMAP_NA_QDMI_device_job_free(MQT_QMAP_NA_QDMI_Device_Job job) {
  if (job != nullptr) {
    job->free();
  }
}

auto MQT_QMAP_NA_QDMI_device_job_set_parameter(
    MQT_QMAP_NA_QDMI_Device_Job job, const QDMI_Device_Job_Parameter param,
    const size_t size, const void* value) -> int {
  if (job == nullptr) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }
  return job->setParameter(param, size, value);
}

auto MQT_QMAP_NA_QDMI_device_job_query_property(
    MQT_QMAP_NA_QDMI_Device_Job job, const QDMI_Device_Job_Property prop,
    const size_t size, void* value, size_t* sizeRet) -> int {
  if (job == nullptr) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }
  return job->queryProperty(prop, size, value, sizeRet);
}

auto MQT_QMAP_NA_QDMI_device_job_submit(MQT_QMAP_NA_QDMI_Device_Job job)
    -> int {
  if (job == nullptr) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }

  return job->submit();
}

auto MQT_QMAP_NA_QDMI_device_job_cancel(MQT_QMAP_NA_QDMI_Device_Job job)
    -> int {
  if (job == nullptr) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }
  return job->cancel();
}

auto MQT_QMAP_NA_QDMI_device_job_check(MQT_QMAP_NA_QDMI_Device_Job job,
                                       QDMI_Job_Status* status) -> int {
  if (job == nullptr) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }
  return job->check(status);
}

auto MQT_QMAP_NA_QDMI_device_job_wait(MQT_QMAP_NA_QDMI_Device_Job job,
                                      const size_t timeout) -> int {
  if (job == nullptr) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }
  return job->wait(timeout);
}

auto MQT_QMAP_NA_QDMI_device_job_get_results(MQT_QMAP_NA_QDMI_Device_Job job,
                                             QDMI_Job_Result result,
                                             const size_t size, void* data,
                                             size_t* sizeRet) -> int {
  if (job == nullptr) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }
  return job->getResults(result, size, data, sizeRet);
}

auto MQT_QMAP_NA_QDMI_device_session_query_device_property(
    MQT_QMAP_NA_QDMI_Device_Session session, const QDMI_Device_Property prop,
    const size_t size, void* value, size_t* sizeRet) -> int {
  if (session == nullptr) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }
  return session->queryDeviceProperty(prop, size, value, sizeRet);
}

auto MQT_QMAP_NA_QDMI_device_session_query_site_property(
    MQT_QMAP_NA_QDMI_Device_Session session, MQT_QMAP_NA_QDMI_Site site,
    const QDMI_Site_Property prop, const size_t size, void* value,
    size_t* sizeRet) -> int {
  if (session == nullptr) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }
  return session->querySiteProperty(site, prop, size, value, sizeRet);
}

auto MQT_QMAP_NA_QDMI_device_session_query_operation_property(
    MQT_QMAP_NA_QDMI_Device_Session session,
    MQT_QMAP_NA_QDMI_Operation operation, const size_t numSites,
    const MQT_QMAP_NA_QDMI_Site* sites, const size_t numParams,
    const double* params, const QDMI_Operation_Property prop, const size_t size,
    void* value, size_t* sizeRet) -> int {
  if (session == nullptr) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }
  return session->queryOperationProperty(operation, numSites, sites, numParams,
                                         params, prop, size, value, sizeRet);
}
