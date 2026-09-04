# Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
# Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

"""End-to-end tests for the public compiler entry point ``compile_subcircuit``.

These exercise the full routing + phase-optimization pipeline through the public
API using only ``torch`` (no Perceval / evaluation dependencies), so they run in
CI. What is checked here is the compiler's *logic*: that it converges on ideal
hardware (the returned phases realize the target unitary, i.e. the fidelity loss
is ~0), that its routing decision is correct (observable in the returned ports),
and that it is deterministic. The *physical* photon-loss / coincidence-rate
behavior, which needs the Perceval simulator, is tested in ``eval/ph/tests``.
"""

from __future__ import annotations

import math

import pytest

torch = pytest.importorskip("torch")

from mqt.qmap.ph.subcircuit_compilation import CompilationResult, OptimizationConfig, compile_subcircuit


def _reproducible_unitary(dim: int, seed: int) -> torch.Tensor:
    """Construct a reproducible ``dim x dim`` unitary target.

    The Q factor of a seeded complex Gaussian is unitary, which is all these
    tests need; the specific distribution is irrelevant.

    Args:
        dim: Dimension of the unitary.
        seed: Seed for the generator, for reproducibility.

    Returns:
        A ``(dim, dim)`` complex unitary tensor.
    """
    gen = torch.Generator().manual_seed(seed)
    z = torch.randn(dim, dim, generator=gen, dtype=torch.complex128)
    q, _ = torch.linalg.qr(z)
    return q


class TestCompileSubcircuitEndToEnd:
    """End-to-end tests of ``compile_subcircuit`` on ideal hardware."""

    @staticmethod
    def test_realizes_ideal_target(ideal_bs_chip4, ones_transmissions_chip4) -> None:
        """On ideal hardware the compiled phases realize a realizable 2x2 target (loss ~ 0).

        ``loss`` is the fidelity loss of the returned phases against the target,
        so a near-zero loss is a genuine functional statement that the whole
        pipeline (routing + optimization) produced phases that realize the
        target - not merely that it ran.
        """
        target = _reproducible_unitary(2, seed=1)
        torch.manual_seed(0)
        result = compile_subcircuit(
            beam_splitter_reflectivities=ideal_bs_chip4,
            input_transmissions=ones_transmissions_chip4,
            output_transmissions=ones_transmissions_chip4,
            target_unitary=target,
            config=OptimizationConfig(max_iterations=200),
        )
        assert isinstance(result, CompilationResult)
        # Ideal 4-mode hardware realizes any 2x2 target; the optimizer reaches ~1e-6.
        assert result.loss < 1e-3

    @staticmethod
    def test_result_structure(ideal_bs_chip4, ones_transmissions_chip4) -> None:
        """``compile_subcircuit`` returns well-formed phases and ports."""
        target = _reproducible_unitary(2, seed=2)
        chip_dim = len(ones_transmissions_chip4)
        target_dim = int(target.shape[0])

        torch.manual_seed(0)
        result = compile_subcircuit(
            beam_splitter_reflectivities=ideal_bs_chip4,
            input_transmissions=ones_transmissions_chip4,
            output_transmissions=ones_transmissions_chip4,
            target_unitary=target,
            config=OptimizationConfig(max_iterations=50),
        )

        # phases: chip_dim**2 finite floats.
        assert len(result.phases) == chip_dim**2
        assert all(isinstance(p, float) and math.isfinite(p) for p in result.phases)

        # ports: correct lengths, in range, distinct inputs.
        assert len(result.input_ports) == target_dim // 2
        assert len(result.output_ports) == target_dim
        assert all(0 <= p < chip_dim for p in result.input_ports)
        assert all(0 <= p < chip_dim for p in result.output_ports)
        assert len(set(result.input_ports)) == len(result.input_ports)

        # loss and compute time are sensible.
        assert math.isfinite(result.loss)
        assert result.loss >= 0.0
        assert result.compute_time >= 0.0

    @staticmethod
    def test_deterministic_with_seed(ideal_bs_chip4, ones_transmissions_chip4) -> None:
        """Identical inputs and torch seed produce identical phases and ports."""
        target = _reproducible_unitary(2, seed=3)

        def run() -> CompilationResult:
            torch.manual_seed(0)
            return compile_subcircuit(
                beam_splitter_reflectivities=ideal_bs_chip4,
                input_transmissions=ones_transmissions_chip4,
                output_transmissions=ones_transmissions_chip4,
                target_unitary=target,
                config=OptimizationConfig(max_iterations=50),
            )

        first = run()
        second = run()
        assert first.phases == second.phases
        assert first.input_ports == second.input_ports
        assert first.output_ports == second.output_ports


class TestCompileSubcircuitInputValidation:
    """The public entry point rejects malformed dimensions and characterization data."""

    @staticmethod
    def test_short_output_transmissions_raises(ideal_bs_chip4, ones_transmissions_chip4) -> None:
        """A too-short output-transmission vector raises instead of being silently accepted."""
        target = _reproducible_unitary(2, seed=1)
        with pytest.raises(ValueError, match="output_transmissions must have length"):
            compile_subcircuit(
                beam_splitter_reflectivities=ideal_bs_chip4,
                input_transmissions=ones_transmissions_chip4,
                output_transmissions=ones_transmissions_chip4[:-1],
                target_unitary=target,
            )

    @staticmethod
    def test_negative_transmission_raises(ideal_bs_chip4, ones_transmissions_chip4) -> None:
        """A negative transmission raises instead of becoming a NaN edge cost."""
        target = _reproducible_unitary(2, seed=1)
        bad = [*ones_transmissions_chip4[:-1], -0.1]
        with pytest.raises(ValueError, match=r"input_transmissions values must be finite and in \[0, 1\]"):
            compile_subcircuit(
                beam_splitter_reflectivities=ideal_bs_chip4,
                input_transmissions=bad,
                output_transmissions=ones_transmissions_chip4,
                target_unitary=target,
            )

    @staticmethod
    def test_short_beam_splitter_vector_raises(ideal_bs_chip4, ones_transmissions_chip4) -> None:
        """A too-short beam-splitter vector raises a clear error, not an incidental IndexError."""
        target = _reproducible_unitary(2, seed=1)
        with pytest.raises(ValueError, match="beam_splitter_reflectivities must have length"):
            compile_subcircuit(
                beam_splitter_reflectivities=ideal_bs_chip4[:-1],
                input_transmissions=ones_transmissions_chip4,
                output_transmissions=ones_transmissions_chip4,
                target_unitary=target,
            )

    @staticmethod
    def test_non_square_target_raises(ideal_bs_chip4, ones_transmissions_chip4) -> None:
        """A non-square target matrix raises."""
        target = torch.zeros((2, 3), dtype=torch.complex128)
        with pytest.raises(ValueError, match="square 2D matrix"):
            compile_subcircuit(
                beam_splitter_reflectivities=ideal_bs_chip4,
                input_transmissions=ones_transmissions_chip4,
                output_transmissions=ones_transmissions_chip4,
                target_unitary=target,
            )

    @staticmethod
    def test_odd_dimension_target_raises(ideal_bs_chip4, ones_transmissions_chip4) -> None:
        """An odd-dimensioned target (no dual-rail pairing) raises."""
        target = torch.eye(3, dtype=torch.complex128)
        with pytest.raises(ValueError, match="positive even number"):
            compile_subcircuit(
                beam_splitter_reflectivities=ideal_bs_chip4,
                input_transmissions=ones_transmissions_chip4,
                output_transmissions=ones_transmissions_chip4,
                target_unitary=target,
            )

    @staticmethod
    def test_real_dtype_target_raises(ideal_bs_chip4, ones_transmissions_chip4) -> None:
        """A real-dtype target raises (a unitary must be complex)."""
        target = torch.eye(2, dtype=torch.float64)
        with pytest.raises(ValueError, match="complex dtype"):
            compile_subcircuit(
                beam_splitter_reflectivities=ideal_bs_chip4,
                input_transmissions=ones_transmissions_chip4,
                output_transmissions=ones_transmissions_chip4,
                target_unitary=target,
            )

    @staticmethod
    def test_target_larger_than_chip_raises(ideal_bs_chip4, ones_transmissions_chip4) -> None:
        """A target wider than the chip raises."""
        target = _reproducible_unitary(6, seed=1)  # target_dim 6 > chip_dim 4
        with pytest.raises(ValueError, match="cannot exceed chip dimension"):
            compile_subcircuit(
                beam_splitter_reflectivities=ideal_bs_chip4,
                input_transmissions=ones_transmissions_chip4,
                output_transmissions=ones_transmissions_chip4,
                target_unitary=target,
            )


def test_output_ports_follow_extreme_routing(extreme_routing_chip) -> None:
    """End-to-end routing regression through ``compile_subcircuit``.

    On the extreme-routing chip the strong bar/cross contrast forces a unique
    optimal route that lands the computation window on modes [2, 3, 4, 5]. A
    graph-layer -> chip-layer mapping error (for example the ``+1`` sign bug in
    ``get_edge_cost_for_graph_layer`` in ``graph.py``) would instead route to
    window [0, 1, 2, 3]. This mirrors
    ``test_routing.TestRoutingLayerMappingRegression`` but exercises the whole
    public pipeline. Only a few optimizer iterations are needed because the
    routing decision - and therefore the ports - is independent of the phase
    optimization.
    """
    chip = extreme_routing_chip
    target = _reproducible_unitary(chip.target_dim, seed=10)

    torch.manual_seed(0)
    result = compile_subcircuit(
        beam_splitter_reflectivities=chip.bs,
        input_transmissions=[1.0] * chip.chip_dim,
        # A negligible output loss on the top modes breaks the two-fold routing
        # degeneracy so the intended [2, 3, 4, 5] window is the unique optimum.
        output_transmissions=chip.output_transmissions,
        target_unitary=target,
        config=OptimizationConfig(max_iterations=5),
    )

    assert result.input_ports == [0, 2]
    assert result.output_ports == [2, 3, 4, 5]
