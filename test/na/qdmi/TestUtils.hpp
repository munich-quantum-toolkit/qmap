/*
 * Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
 * Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

/** @file TestUtils.hpp
 * @brief Shared test utilities for QDMI components.
 */

#pragma once

#include <cstdlib>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

namespace mqt::test {

/// Temporarily sets an environment variable and restores its previous value.
class ScopedEnvironmentVariable {
public:
  ScopedEnvironmentVariable(std::string name, const std::string& value)
      : name_(std::move(name)) {
    if (const auto* previous = std::getenv(name_.c_str());
        previous != nullptr) {
      previous_ = previous;
    }
    set(value);
  }

  ~ScopedEnvironmentVariable() {
    if (previous_) {
      static_cast<void>(setWithoutChecking(*previous_));
    } else {
#ifdef _WIN32
      static_cast<void>(_putenv_s(name_.c_str(), ""));
#else
      // NOLINTNEXTLINE(misc-include-cleaner)
      static_cast<void>(unsetenv(name_.c_str()));
#endif
    }
  }

  ScopedEnvironmentVariable(const ScopedEnvironmentVariable&) = delete;
  auto operator=(const ScopedEnvironmentVariable&)
      -> ScopedEnvironmentVariable& = delete;
  ScopedEnvironmentVariable(ScopedEnvironmentVariable&&) = delete;
  auto operator=(ScopedEnvironmentVariable&&)
      -> ScopedEnvironmentVariable& = delete;

private:
  void set(const std::string& value) const {
    if (!setWithoutChecking(value)) {
      throw std::runtime_error("Failed to set environment variable " + name_);
    }
  }

  [[nodiscard]] auto setWithoutChecking(const std::string& value) const
      -> bool {
#ifdef _WIN32
    return _putenv_s(name_.c_str(), value.c_str()) == 0;
#else
    // NOLINTNEXTLINE(misc-include-cleaner)
    return setenv(name_.c_str(), value.c_str(), 1) == 0;
#endif
  }

  std::string name_;
  std::optional<std::string> previous_;
};

} // namespace mqt::test
