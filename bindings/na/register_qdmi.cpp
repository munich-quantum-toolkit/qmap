/*
 * Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
 * Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "na/fomac/Device.hpp"
#include "na/qdmi/Configuration.hpp"

#include <nanobind/nanobind.h>
#include <nanobind/operators.h>
#include <nanobind/stl/optional.h> // NOLINT(misc-include-cleaner)
#include <nanobind/stl/string.h>   // NOLINT(misc-include-cleaner)
#include <nanobind/stl/vector.h>   // NOLINT(misc-include-cleaner)
#include <string>
#include <vector>

namespace nb = nanobind;
using namespace nb::literals;

namespace {

template <typename T>
concept pyClass = requires(T t) { nb::cast(t); };
template <pyClass T> [[nodiscard]] auto repr(T c) -> std::string {
  return nb::repr(nb::cast(c)).c_str();
}

/**
 * @brief Register the packaged neutral-atom device with the QDMI driver.
 * @details The driver only discovers devices next to its own library, so a
 * device shipped in this package is never found. Registering a definition does
 * not load the library.
 * @return The identifier of the registered device, empty if it is not
 * installed.
 */
[[nodiscard]] auto registerPackagedDevice() -> std::string {
  const auto paths = nb::module_::import_("mqt.qmap._qdmi_paths");
  const auto library =
      nb::cast<std::string>(paths.attr("NA_QDMI_DEVICE_LIBRARY_PATH"));
  auto id = nb::cast<std::string>(paths.attr("NA_QDMI_DEVICE_ID"));
  if (library.empty()) {
    return {};
  }
  const auto driver = nb::module_::import_("mqt.core.qdmi.driver");
  const auto definition = driver.attr("DeviceDefinition")(
      id, library, paths.attr("NA_QDMI_PREFIX"));
  driver.attr("register_device_if_absent")(definition);
  return id;
}

} // namespace

// NOLINTNEXTLINE(misc-use-internal-linkage)
void registerQdmi(nb::module_& m) {
  m.doc() = R"pb(Neutral-atom view of a QDMI device.)pb";

  // The generic QDMI device base class is registered by MQT Core. Importing the
  // module ensures the base type is known before the derived type is bound.
  nb::module_::import_("mqt.core.qdmi");

  const auto deviceId = registerPackagedDevice();
  m.attr("DEVICE_ID") = deviceId;

  auto device = nb::class_<na::Session::Device, na::qdmi_client::Device>(
      m, "Device", "Represents a device with a lattice of traps.");

  auto lattice = nb::class_<na::Device::Lattice>(
      device, "Lattice", "Represents a lattice of traps in the device.");

  auto vector = nb::class_<na::Device::Vector>(lattice, "Vector",
                                               "Represents a 2D vector.");
  vector.def_ro("x", &na::Device::Vector::x, "The x-coordinate of the vector.");
  vector.def_ro("y", &na::Device::Vector::y, "The y-coordinate of the vector.");
  vector.def("__repr__", [](const na::Device::Vector& v) {
    return "<Vector x=" + std::to_string(v.x) + " y=" + std::to_string(v.y) +
           ">";
  });
  // nanobind uses these intentionally self-comparative expressions to bind
  // Python rich comparison. NOLINTBEGIN(misc-redundant-expression)
  vector.def(nb::self == nb::self,
             nb::sig("def __eq__(self, arg: object, /) -> bool"));
  vector.def(nb::self != nb::self,
             nb::sig("def __ne__(self, arg: object, /) -> bool"));

  auto region = nb::class_<na::Device::Region>(
      lattice, "Region", "Represents a region in the device.");

  auto size = nb::class_<na::Device::Region::Size>(
      region, "Size", "Represents the size of a region.");
  size.def_ro("width", &na::Device::Region::Size::width,
              "The width of the region.");
  size.def_ro("height", &na::Device::Region::Size::height,
              "The height of the region.");
  size.def("__repr__", [](const na::Device::Region::Size& s) {
    return "<Size width=" + std::to_string(s.width) +
           " height=" + std::to_string(s.height) + ">";
  });
  size.def(nb::self == nb::self,
           nb::sig("def __eq__(self, arg: object, /) -> bool"));
  size.def(nb::self != nb::self,
           nb::sig("def __ne__(self, arg: object, /) -> bool"));

  region.def_ro("origin", &na::Device::Region::origin,
                "The origin of the region.");
  region.def_ro("size", &na::Device::Region::size, "The size of the region.");
  region.def("__repr__", [](const na::Device::Region& r) {
    return "<Region origin=" + repr(r.origin) + " size=" + repr(r.size) + ">";
  });
  region.def(nb::self == nb::self,
             nb::sig("def __eq__(self, arg: object, /) -> bool"));
  region.def(nb::self != nb::self,
             nb::sig("def __ne__(self, arg: object, /) -> bool"));

  lattice.def_ro("lattice_origin", &na::Device::Lattice::latticeOrigin,
                 "The origin of the lattice.");
  lattice.def_ro("lattice_vector_1", &na::Device::Lattice::latticeVector1,
                 "The first lattice vector.");
  lattice.def_ro("lattice_vector_2", &na::Device::Lattice::latticeVector2,
                 "The second lattice vector.");
  lattice.def_ro("sublattice_offsets", &na::Device::Lattice::sublatticeOffsets,
                 "The offsets of the sublattices.");
  lattice.def_ro("extent", &na::Device::Lattice::extent,
                 "The extent of the lattice.");
  lattice.def("__repr__", [](const na::Device::Lattice& l) {
    return "<Lattice origin=" + repr(l.latticeOrigin) + ">";
  });
  lattice.def(nb::self == nb::self,
              nb::sig("def __eq__(self, arg: object, /) -> bool"));
  lattice.def(nb::self != nb::self,
              nb::sig("def __ne__(self, arg: object, /) -> bool"));

  device.def_prop_ro("traps", &na::Session::Device::getTraps,
                     nb::rv_policy::reference_internal,
                     "The list of trap positions in the device.");
  device.def_prop_ro(
      "t1",
      [](const na::Session::Device& dev) {
        return dev.getDecoherenceTimes().t1;
      },
      "The T1 time of the device.");
  device.def_prop_ro(
      "t2",
      [](const na::Session::Device& dev) {
        return dev.getDecoherenceTimes().t2;
      },
      "The T2 time of the device.");
  device.def("__repr__", [](const na::qdmi_client::Device& dev) {
    return "<Device name=\"" + dev.getName() + "\">";
  });
  device.def_static(
      "try_create_from_device", &na::Session::Device::tryCreateFromDevice,
      "device"_a,
      R"pb(Create a neutral-atom device from a generic QDMI device.

Args:
    device: The generic QDMI device to convert.

Returns:
    The converted neutral-atom device or None if the conversion is not possible.)pb");
  device.def(nb::self == nb::self,
             nb::sig("def __eq__(self, arg: object, /) -> bool"));
  device.def(nb::self != nb::self,
             nb::sig("def __ne__(self, arg: object, /) -> bool"));
  // NOLINTEND(misc-redundant-expression)

  // A device registered at runtime is reachable by its identifier but does not
  // appear in the device list of a QDMI session, so the packaged device is
  // opened by identifier.
  m.def(
      "devices",
      [deviceId]() -> std::vector<na::Session::Device> {
        if (deviceId.empty()) {
          return {};
        }
        const auto driver = nb::module_::import_("mqt.core.qdmi.driver");
        const auto opened = driver.attr("open_device")(deviceId);
        auto device = na::Session::Device::tryCreateFromDevice(
            nb::cast<na::qdmi_client::Device>(opened));
        if (!device.has_value()) {
          return {};
        }
        return {*device};
      },
      "Returns a list of available devices.");
}
