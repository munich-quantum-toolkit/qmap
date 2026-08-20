# Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
# Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

"""Test NADevice class creation."""

from __future__ import annotations

import pathlib
from json import load
from typing import TYPE_CHECKING, Any

import pytest
from mqt.core.qdmi.driver import open_device, registered_device_ids

from mqt.qmap.na.qdmi import DEVICE_ID, devices

if TYPE_CHECKING:
    from collections.abc import Mapping

    from mqt.qmap.na.qdmi import Device


@pytest.fixture
def device_tuple() -> tuple[Device, Mapping[str, Any]]:
    """Return a neutral-atom QDMI device instance."""
    with pathlib.Path("json/na/mqt-qmap-qdmi-na-device.json").open(encoding="utf-8") as f:
        device_dict = load(f)
    return next(iter(devices())), device_dict


def test_device_is_registered() -> None:
    """Importing the module registers the packaged device with the QDMI driver."""
    assert DEVICE_ID in registered_device_ids()


def test_device_is_the_packaged_one() -> None:
    """The reported device is the one that this package ships, not another provider's."""
    assert [device.name() for device in devices()] == [open_device(DEVICE_ID).name()]


def test_name(device_tuple: tuple[Device, Mapping[str, Any]]) -> None:
    """Test retrieving the name of the device."""
    device, device_dict = device_tuple
    assert device.name() == device_dict["name"]


def test_num_qubits(device_tuple: tuple[Device, Mapping[str, Any]]) -> None:
    """Test retrieving the number of qubits of the device."""
    device, device_dict = device_tuple
    assert device.qubits_num() == device_dict["numQubits"]


def test_length_unit(device_tuple: tuple[Device, Mapping[str, Any]]) -> None:
    """Test retrieving the length unit of the device."""
    device, device_dict = device_tuple
    assert device.length_unit() == device_dict["lengthUnit"]["unit"]


def test_length_scale_factor(device_tuple: tuple[Device, Mapping[str, Any]]) -> None:
    """Test retrieving the length scale factor of the device."""
    device, device_dict = device_tuple
    assert device.length_scale_factor() == device_dict["lengthUnit"]["scaleFactor"]


def test_duration_unit(device_tuple: tuple[Device, Mapping[str, Any]]) -> None:
    """Test retrieving the duration unit of the device."""
    device, device_dict = device_tuple
    assert device.duration_unit() == device_dict["durationUnit"]["unit"]


def test_duration_scale_factor(device_tuple: tuple[Device, Mapping[str, Any]]) -> None:
    """Test retrieving the duration scale factor of the device."""
    device, device_dict = device_tuple
    assert device.duration_scale_factor() == device_dict["durationUnit"]["scaleFactor"]


def test_min_atom_distance(device_tuple: tuple[Device, Mapping[str, Any]]) -> None:
    """Test retrieving the minimum atom distance of the device."""
    device, device_dict = device_tuple
    assert device.min_atom_distance() == device_dict["minAtomDistance"]


def test_t1(device_tuple: tuple[Device, Mapping[str, Any]]) -> None:
    """Test retrieving the T1 time of the device."""
    device, device_dict = device_tuple
    assert device.t1 == device_dict["decoherenceTimes"]["t1"]


def test_t2(device_tuple: tuple[Device, Mapping[str, Any]]) -> None:
    """Test retrieving the T2 time of the device."""
    device, device_dict = device_tuple
    assert device.t2 == device_dict["decoherenceTimes"]["t2"]


def test_traps(device_tuple: tuple[Device, Mapping[str, Any]]) -> None:
    """Test retrieving the traps of the device."""
    device, device_dict = device_tuple
    for trap, trap_dict in zip(device.traps, device_dict["traps"], strict=False):
        assert trap.lattice_origin.x == trap_dict["latticeOrigin"]["x"]
        assert trap.lattice_origin.y == trap_dict["latticeOrigin"]["y"]
        assert {
            (trap.lattice_vector_1.x, trap.lattice_vector_1.y),
            (trap.lattice_vector_2.x, trap.lattice_vector_2.y),
        } == {
            (trap_dict["latticeVector1"]["x"], trap_dict["latticeVector1"]["y"]),
            (trap_dict["latticeVector2"]["x"], trap_dict["latticeVector2"]["y"]),
        }
        assert trap.extent.origin.x == trap_dict["extent"]["origin"]["x"]
        assert trap.extent.origin.y == trap_dict["extent"]["origin"]["y"]
        assert trap.extent.size.width == trap_dict["extent"]["size"]["width"]
        assert trap.extent.size.height == trap_dict["extent"]["size"]["height"]
        for offset, offset_dict in zip(trap.sublattice_offsets, trap_dict["sublatticeOffsets"], strict=False):
            assert offset.x == offset_dict["x"]
            assert offset.y == offset_dict["y"]


def calculate_extent_from_sites(sites: list[Device.Site]) -> tuple[int, int, int, int]:
    """Calculate the extent from a list of sites.

    Args:
        sites: List of sites.

    Returns:
        The extent as (min_x, min_y, width, height).
    """
    min_x = min(i for i in (site.x_coordinate() for site in sites) if i is not None)
    max_x = max(i for i in (site.x_coordinate() for site in sites) if i is not None)
    min_y = min(i for i in (site.y_coordinate() for site in sites) if i is not None)
    max_y = max(i for i in (site.y_coordinate() for site in sites) if i is not None)
    return min_x, min_y, max_x - min_x, max_y - min_y


def test_operations(device_tuple: tuple[Device, Mapping[str, Any]]) -> None:
    """Test retrieving the operation properties of the device."""
    device, device_dict = device_tuple
    for operation in device.operations():
        qubits_num = operation.qubits_num()
        if operation.is_zoned():
            sites = operation.sites()
            assert sites is not None
            sites = list(sites)
            assert len(sites) == 1
            site = sites[0]
            if qubits_num is None:
                assert operation.parameters_num() == device_dict["shuttlingUnits"][0]["numParameters"]
                assert site.x_coordinate() == device_dict["shuttlingUnits"][0]["region"]["origin"]["x"]
                assert site.y_coordinate() == device_dict["shuttlingUnits"][0]["region"]["origin"]["y"]
                assert site.x_extent() == device_dict["shuttlingUnits"][0]["region"]["size"]["width"]
                assert site.y_extent() == device_dict["shuttlingUnits"][0]["region"]["size"]["height"]
                if "load" in operation.name():
                    assert operation.duration() == device_dict["shuttlingUnits"][0]["loadDuration"]
                    assert operation.fidelity() == device_dict["shuttlingUnits"][0]["loadFidelity"]
                if "move" in operation.name():
                    assert operation.mean_shuttling_speed() == device_dict["shuttlingUnits"][0]["meanShuttlingSpeed"]
                if "store" in operation.name():
                    assert operation.duration() == device_dict["shuttlingUnits"][0]["storeDuration"]
                    assert operation.fidelity() == device_dict["shuttlingUnits"][0]["storeFidelity"]
            elif qubits_num == 1:
                assert operation.duration() == device_dict["globalSingleQubitOperations"][0]["duration"]
                assert operation.fidelity() == device_dict["globalSingleQubitOperations"][0]["fidelity"]
                assert operation.parameters_num() == device_dict["globalSingleQubitOperations"][0]["numParameters"]
                assert site.x_coordinate() == device_dict["globalSingleQubitOperations"][0]["region"]["origin"]["x"]
                assert site.y_coordinate() == device_dict["globalSingleQubitOperations"][0]["region"]["origin"]["y"]
                assert site.x_extent() == device_dict["globalSingleQubitOperations"][0]["region"]["size"]["width"]
                assert site.y_extent() == device_dict["globalSingleQubitOperations"][0]["region"]["size"]["height"]
            else:
                assert qubits_num > 1
                assert operation.duration() == device_dict["globalMultiQubitOperations"][0]["duration"]
                assert operation.fidelity() == device_dict["globalMultiQubitOperations"][0]["fidelity"]
                assert operation.parameters_num() == device_dict["globalMultiQubitOperations"][0]["numParameters"]
                assert (
                    operation.interaction_radius() == device_dict["globalMultiQubitOperations"][0]["interactionRadius"]
                )
                assert operation.blocking_radius() == device_dict["globalMultiQubitOperations"][0]["blockingRadius"]
                assert operation.idling_fidelity() == device_dict["globalMultiQubitOperations"][0]["idlingFidelity"]
                assert operation.qubits_num() == device_dict["globalMultiQubitOperations"][0]["numQubits"]
                assert site.x_coordinate() == device_dict["globalMultiQubitOperations"][0]["region"]["origin"]["x"]
                assert site.y_coordinate() == device_dict["globalMultiQubitOperations"][0]["region"]["origin"]["y"]
                assert site.x_extent() == device_dict["globalMultiQubitOperations"][0]["region"]["size"]["width"]
                assert site.y_extent() == device_dict["globalMultiQubitOperations"][0]["region"]["size"]["height"]
        else:
            assert qubits_num is not None
            if qubits_num == 1:
                assert operation.duration() == device_dict["localSingleQubitOperations"][0]["duration"]
                assert operation.fidelity() == device_dict["localSingleQubitOperations"][0]["fidelity"]
                assert operation.parameters_num() == device_dict["localSingleQubitOperations"][0]["numParameters"]
                sites = operation.sites()
                assert sites is not None
                sites = list(sites)
                assert calculate_extent_from_sites(sites) == (
                    device_dict["localSingleQubitOperations"][0]["region"]["origin"]["x"],
                    device_dict["localSingleQubitOperations"][0]["region"]["origin"]["y"],
                    device_dict["localSingleQubitOperations"][0]["region"]["size"]["width"],
                    device_dict["localSingleQubitOperations"][0]["region"]["size"]["height"],
                )
            else:
                assert qubits_num > 1
                assert operation.duration() == device_dict["localMultiQubitOperations"][0]["duration"]
                assert operation.fidelity() == device_dict["localMultiQubitOperations"][0]["fidelity"]
                assert operation.parameters_num() == device_dict["localMultiQubitOperations"][0]["numParameters"]
                assert (
                    operation.interaction_radius() == device_dict["localMultiQubitOperations"][0]["interactionRadius"]
                )
                assert operation.blocking_radius() == device_dict["localMultiQubitOperations"][0]["blockingRadius"]
                assert operation.qubits_num() == device_dict["localMultiQubitOperations"][0]["numQubits"]

                site_pairs = operation.site_pairs()
                assert site_pairs is not None
                site_pairs_list = list(site_pairs)
                assert len(site_pairs_list) > 0

                for site1, site2 in site_pairs_list:
                    assert site1.index() >= 0
                    assert site2.index() >= 0

                sites = operation.sites()
                assert sites is not None
                sites = list(sites)
                assert len(sites) == len(site_pairs_list) * 2
                assert calculate_extent_from_sites(sites) == (
                    device_dict["localMultiQubitOperations"][0]["region"]["origin"]["x"],
                    device_dict["localMultiQubitOperations"][0]["region"]["origin"]["y"],
                    device_dict["localMultiQubitOperations"][0]["region"]["size"]["width"],
                    device_dict["localMultiQubitOperations"][0]["region"]["size"]["height"],
                )


def test_qubits_zones_separation(device_tuple: tuple[Device, Mapping[str, Any]]) -> None:
    """Test that regular sites (qubits) and zones are properly separated."""
    device, device_dict = device_tuple

    all_sites = list(device.sites())
    qubits = list(device.regular_sites())
    zones = list(device.zones())

    assert len(all_sites) == len(qubits) + len(zones)

    assert len(qubits) == device.qubits_num()
    assert len(qubits) == device_dict["numQubits"]

    for qubit in qubits:
        assert not qubit.is_zone()
        assert qubit.module_index() is not None
        assert qubit.submodule_index() is not None

    for zone in zones:
        assert zone.is_zone()
        assert zone.x_extent() is not None
        assert zone.y_extent() is not None

    qubit_indices = {q.index() for q in qubits}
    zone_indices = {z.index() for z in zones}
    assert len(qubit_indices & zone_indices) == 0
