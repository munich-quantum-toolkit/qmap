# Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
# Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

"""Neutral-atom view of a QDMI device."""

import mqt.core.qdmi

class Device(mqt.core.qdmi.Device):
    """Represents a device with a lattice of traps."""

    class Lattice:
        """Represents a lattice of traps in the device."""

        class Vector:
            """Represents a 2D vector."""

            @property
            def x(self) -> int:
                """The x-coordinate of the vector."""

            @property
            def y(self) -> int:
                """The y-coordinate of the vector."""

            def __eq__(self, arg: object, /) -> bool: ...
            def __ne__(self, arg: object, /) -> bool: ...

        class Region:
            """Represents a region in the device."""

            class Size:
                """Represents the size of a region."""

                @property
                def width(self) -> int:
                    """The width of the region."""

                @property
                def height(self) -> int:
                    """The height of the region."""

                def __eq__(self, arg: object, /) -> bool: ...
                def __ne__(self, arg: object, /) -> bool: ...

            @property
            def origin(self) -> Device.Lattice.Vector:
                """The origin of the region."""

            @property
            def size(self) -> Device.Lattice.Region.Size:
                """The size of the region."""

            def __eq__(self, arg: object, /) -> bool: ...
            def __ne__(self, arg: object, /) -> bool: ...

        @property
        def lattice_origin(self) -> Device.Lattice.Vector:
            """The origin of the lattice."""

        @property
        def lattice_vector_1(self) -> Device.Lattice.Vector:
            """The first lattice vector."""

        @property
        def lattice_vector_2(self) -> Device.Lattice.Vector:
            """The second lattice vector."""

        @property
        def sublattice_offsets(self) -> list[Device.Lattice.Vector]:
            """The offsets of the sublattices."""

        @property
        def extent(self) -> Device.Lattice.Region:
            """The extent of the lattice."""

        def __eq__(self, arg: object, /) -> bool: ...
        def __ne__(self, arg: object, /) -> bool: ...

    @property
    def traps(self) -> list[Device.Lattice]:
        """The list of trap positions in the device."""

    @property
    def t1(self) -> int:
        """The T1 time of the device."""

    @property
    def t2(self) -> int:
        """The T2 time of the device."""

    @staticmethod
    def try_create_from_device(device: mqt.core.qdmi.Device) -> Device | None:
        """Create a neutral-atom device from a generic QDMI device.

        Args:
            device: The generic QDMI device to convert.

        Returns:
            The converted neutral-atom device or None if the conversion is not possible.
        """

    def __eq__(self, arg: object, /) -> bool: ...
    def __ne__(self, arg: object, /) -> bool: ...

def devices() -> list[Device]:
    """Returns a list of available devices."""
