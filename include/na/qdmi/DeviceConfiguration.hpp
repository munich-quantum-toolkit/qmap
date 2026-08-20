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

#include <filesystem>
#include <optional>
#include <qdmi/device.h>
#include <string>
#include <string_view>

namespace na::qdmi::detail {

/// JSON text selected for a provider session together with a safe source label.
struct LoadedDeviceConfiguration {
  std::string json;
  std::string source;
};

/**
 * @brief Validate and store a CUSTOM1/CUSTOM2 string parameter.
 *
 * A null value with size zero is a capability probe. Assignments contain one
 * trailing NUL and no embedded NUL. A single NUL clears the selected value.
 */
int setDeviceConfigurationParameter(QDMI_Device_Session_Parameter parameter,
                                    size_t size, const void* value,
                                    std::optional<std::string>& inlineJson,
                                    std::optional<std::filesystem::path>& file);

/**
 * @brief Select and load one runtime device description.
 *
 * A non-empty explicit inline value wins over an explicit file value. Without
 * an explicit source, exactly one technology-specific environment variable may
 * select inline JSON or a file. The final fallback is a file beside the shared
 * module containing @p anchor.
 */
std::optional<LoadedDeviceConfiguration> loadDeviceConfiguration(
    const std::optional<std::string>& inlineJson,
    const std::optional<std::filesystem::path>& file,
    std::string_view inlineEnvironment, std::string_view fileEnvironment,
    std::string_view bundledFilename, const void* anchor, int& status);

} // namespace na::qdmi::detail
