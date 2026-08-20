/*
 * Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
 * Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "fomac/FoMaC.hpp"
#include "na/fomac/Device.hpp"
#include "na/qdmi/Configuration.hpp"

#include <nanobind/nanobind.h>
#include <nanobind/operators.h>
#include <nanobind/stl/optional.h> // NOLINT(misc-include-cleaner)
#include <nanobind/stl/string.h>   // NOLINT(misc-include-cleaner)
#include <nanobind/stl/vector.h>   // NOLINT(misc-include-cleaner)
#include <string>

namespace nb = nanobind;
using namespace nb::literals;

namespace {

template <typename T>
concept pyClass = requires(T t) { nb::cast(t); };
template <pyClass T> [[nodiscard]] auto repr(T c) -> std::string {
  return nb::repr(nb::cast(c)).c_str();
}

} // namespace

// NOLINTNEXTLINE(misc-use-internal-linkage)
void registerQdmi(nb::module_& m) {
  m.doc() = R"pb(Neutral-atom view of a QDMI device.)pb";

  // The FoMaC device base class is registered by MQT Core. Importing the module
  // ensures the base type is known before the derived type is bound here.
  nb::module_::import_("mqt.core.qdmi");

  auto device = nb::class_<na::Session::Device, fomac::Device>(
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
  device.def("__repr__", [](const fomac::Device& dev) {
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

  m.def("devices", &na::Session::getDevices, nb::rv_policy::reference_internal,
        "Returns a list of available devices.");
}
