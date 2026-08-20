/*
 * Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
 * Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "na/qdmi/DeviceConfiguration.hpp"

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <ios>
#include <iterator>
#include <optional>
#include <qdmi/device.h>
#include <span>
#include <spdlog/spdlog.h>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

#ifdef _WIN32
#include <memory>
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace na::qdmi::detail {
namespace {
[[nodiscard]] std::optional<std::string>
environment(const std::string_view name) {
#ifdef _WIN32
  char* raw = nullptr;
  size_t size = 0;
  const std::string ownedName(name);
  if (_dupenv_s(&raw, &size, ownedName.c_str()) != 0 || raw == nullptr) {
    return std::nullopt;
  }
  const std::unique_ptr<char, decltype(&std::free)> value(raw, &std::free);
  if (*value == '\0') {
    return std::nullopt;
  }
  return std::string(value.get());
#else
  const std::string ownedName(name);
  const auto* value = std::getenv(ownedName.c_str());
  if (value == nullptr || *value == '\0') {
    return std::nullopt;
  }
  return std::string(value);
#endif
}

[[nodiscard]] std::filesystem::path moduleDirectory(const void* anchor) {
#ifdef _WIN32
  HMODULE module = nullptr;
  if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                             GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                         reinterpret_cast<LPCWSTR>(anchor), &module) == 0) {
    return {};
  }
  std::wstring buffer(MAX_PATH, L'\0');
  while (true) {
    const auto length = GetModuleFileNameW(module, buffer.data(),
                                           static_cast<DWORD>(buffer.size()));
    if (length == 0) {
      return {};
    }
    if (length < buffer.size()) {
      buffer.resize(length);
      return std::filesystem::path(buffer).parent_path();
    }
    buffer.resize(buffer.size() * 2);
  }
#else
  Dl_info info{};
  if (dladdr(anchor, &info) == 0 || info.dli_fname == nullptr) {
    return {};
  }
  return std::filesystem::path(info.dli_fname).parent_path();
#endif
}

[[nodiscard]] std::optional<LoadedDeviceConfiguration>
readFile(const std::filesystem::path& path, const bool bundled, int& status) {
  std::error_code error;
  const auto exists = std::filesystem::exists(path, error);
  if (error) {
    status = bundled ? QDMI_ERROR_FATAL : QDMI_ERROR_PERMISSIONDENIED;
    SPDLOG_ERROR("Cannot inspect QDMI device configuration '{}': {}",
                 path.string(), error.message());
    return std::nullopt;
  }
  if (!exists) {
    status = bundled ? QDMI_ERROR_FATAL : QDMI_ERROR_NOTFOUND;
    SPDLOG_ERROR("QDMI device configuration '{}' does not exist",
                 path.string());
    return std::nullopt;
  }
  errno = 0;
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    status = bundled ? QDMI_ERROR_FATAL : QDMI_ERROR_PERMISSIONDENIED;
    SPDLOG_ERROR("Cannot read QDMI device configuration '{}': {}",
                 path.string(), std::strerror(errno));
    return std::nullopt;
  }
  std::string json{std::istreambuf_iterator<char>(input),
                   std::istreambuf_iterator<char>()};
  if (!input.eof() && input.fail()) {
    status = bundled ? QDMI_ERROR_FATAL : QDMI_ERROR_PERMISSIONDENIED;
    SPDLOG_ERROR("Failed while reading QDMI device configuration '{}'",
                 path.string());
    return std::nullopt;
  }
  status = QDMI_SUCCESS;
  return LoadedDeviceConfiguration{.json = std::move(json),
                                   .source = path.string()};
}
} // namespace

int setDeviceConfigurationParameter(
    const QDMI_Device_Session_Parameter parameter, const size_t size,
    const void* value, std::optional<std::string>& inlineJson,
    std::optional<std::filesystem::path>& file) {
  if (parameter != QDMI_DEVICE_SESSION_PARAMETER_CUSTOM1 &&
      parameter != QDMI_DEVICE_SESSION_PARAMETER_CUSTOM2) {
    return QDMI_ERROR_NOTSUPPORTED;
  }
  if (value == nullptr) {
    return size == 0 ? QDMI_SUCCESS : QDMI_ERROR_INVALIDARGUMENT;
  }
  if (size == 0) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }
  const std::span bytes{static_cast<const char*>(value), size};
  if (bytes.back() != '\0' ||
      std::memchr(bytes.data(), '\0', bytes.size() - 1) != nullptr) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }
  const std::string stringValue(bytes.data(), bytes.size() - 1);
  if (parameter == QDMI_DEVICE_SESSION_PARAMETER_CUSTOM1) {
    inlineJson =
        stringValue.empty() ? std::nullopt : std::optional{stringValue};
  } else {
    file = stringValue.empty()
               ? std::nullopt
               : std::optional{std::filesystem::path(stringValue)};
  }
  return QDMI_SUCCESS;
}

std::optional<LoadedDeviceConfiguration>
loadDeviceConfiguration(const std::optional<std::string>& inlineJson,
                        const std::optional<std::filesystem::path>& file,
                        const std::string_view inlineEnvironment,
                        const std::string_view fileEnvironment,
                        const std::string_view bundledFilename,
                        const void* anchor, int& status) {
  if (inlineJson) {
    status = QDMI_SUCCESS;
    return LoadedDeviceConfiguration{.json = *inlineJson,
                                     .source = "inline session configuration"};
  }
  if (file) {
    return readFile(*file, false, status);
  }
  const auto environmentJson = environment(inlineEnvironment);
  const auto environmentFile = environment(fileEnvironment);
  if (environmentJson && environmentFile) {
    SPDLOG_ERROR("Both {} and {} are set", inlineEnvironment, fileEnvironment);
    status = QDMI_ERROR_INVALIDARGUMENT;
    return std::nullopt;
  }
  if (environmentJson) {
    status = QDMI_SUCCESS;
    return LoadedDeviceConfiguration{.json = *environmentJson,
                                     .source = std::string(inlineEnvironment)};
  }
  if (environmentFile) {
    return readFile(std::filesystem::path(*environmentFile), false, status);
  }
  const auto directory = moduleDirectory(anchor);
  if (directory.empty()) {
    SPDLOG_ERROR("Cannot locate the QDMI provider module");
    status = QDMI_ERROR_FATAL;
    return std::nullopt;
  }
  return readFile(directory / bundledFilename, true, status);
}
} // namespace na::qdmi::detail
