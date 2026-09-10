# Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
# Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

"""Tests for unitary_to_phase_compilation.optimize_unitary_subcircuit_parameters."""

import pytest

torch = pytest.importorskip("torch")

from mqt.qmap.ph.unitary_to_phase_compilation import (
    build_unitary_selected_columns_from_components,
    fidelity_loss,
    optimize_unitary_subcircuit_parameters,
)


def _reference_unitary(bs: torch.Tensor, phases: torch.Tensor, exclude_corners: bool) -> torch.Tensor:
    """Propagate full component matrices as an independent small-mesh reference."""
    n = phases.shape[0]
    unitary = torch.eye(n, dtype=torch.complex128)
    offset = 0
    for layer in range(n):
        for component in (0, 1):
            for i, top in enumerate(range(layer % 2, n - 1, 2)):
                r = bs[offset + 2 * i + component]
                matrix = torch.eye(n, dtype=torch.complex128)
                matrix[top, top] = matrix[top + 1, top + 1] = torch.sqrt(r)
                matrix[top, top + 1] = matrix[top + 1, top] = 1j * torch.sqrt(1 - r)
                unitary = matrix @ unitary
            if component == 0:
                for mode in range(n):
                    if exclude_corners and layer == n - 1 and mode in {0, n - 1}:
                        continue
                    matrix = torch.eye(n, dtype=torch.complex128)
                    matrix[mode, mode] = torch.exp(1j * phases[mode, layer])
                    unitary = matrix @ unitary
        offset += 2 * len(range(layer % 2, n - 1, 2))
    return unitary


@pytest.mark.parametrize(
    ("num_modes", "columns", "exclude_corners"),
    [(2, [0, 1], False), (2, [1], True), (4, [0, 2], False), (6, [1, 4], True), (8, list(range(8)), False)],
)
def test_batched_propagation_and_gradients(num_modes: int, columns: list[int], exclude_corners: bool) -> None:
    """Layer batches preserve the unitary and phase gradients of component propagation."""
    rng = torch.Generator().manual_seed(12)
    bs = torch.rand(num_modes * (num_modes - 1), generator=rng, dtype=torch.float64)
    phases = torch.rand((num_modes, num_modes), generator=rng, dtype=torch.float64, requires_grad=True)
    expected = _reference_unitary(bs, phases, exclude_corners)[:, columns]
    actual = build_unitary_selected_columns_from_components(num_modes, bs, phases, columns, exclude_corners)
    torch.testing.assert_close(actual, expected, atol=1e-12, rtol=1e-12)
    weights = torch.randn(actual.shape, generator=rng, dtype=torch.complex128)
    expected_grad = torch.autograd.grad((expected * weights).real.sum(), phases)[0]
    actual_grad = torch.autograd.grad((actual * weights).real.sum(), phases)[0]
    torch.testing.assert_close(actual_grad, expected_grad, atol=1e-12, rtol=1e-12)


@pytest.mark.parametrize("budget", [0, 1, 3])
def test_gradient_step_budget_and_returned_loss(budget: int, monkeypatch: pytest.MonkeyPatch) -> None:
    """Count actual gradient steps and independently evaluate the returned best state."""
    calls = 0
    step = torch.optim.Adam.step

    def counted_step(self, *args, **kwargs):
        nonlocal calls
        calls += 1
        return step(self, *args, **kwargs)

    monkeypatch.setattr(torch.optim.Adam, "step", counted_step)
    bs = torch.full((12,), 0.5, dtype=torch.float64)
    target = torch.eye(4, dtype=torch.complex128)
    result = optimize_unitary_subcircuit_parameters(
        target, bs, max_iterations=budget, threshold=-1.0, early_stop_patience=0
    )
    assert calls == budget
    actual = _reference_unitary(bs, result.phase_shifter_params, exclude_corners=False)
    assert result.best_loss == pytest.approx(fidelity_loss(actual, target).item(), abs=1e-12)


def test_negative_iteration_budget_raises() -> None:
    """An invalid iteration budget fails before optimizer initialization."""
    with pytest.raises(ValueError, match="max_iterations must be nonnegative"):
        optimize_unitary_subcircuit_parameters(torch.eye(2), torch.tensor([0.5, 0.5]), max_iterations=-1)


class TestMaxIterationsContract:
    """``max_iterations`` is an exact bound on the number of gradient steps."""

    @staticmethod
    def _run(max_iterations: int):
        """Optimize a fixed 4-mode target for exactly ``max_iterations`` steps."""
        # chip_dim=4: MZIs per layer [2, 1, 2, 1] -> 6 MZIs -> 12 ideal reflectivities.
        bs = torch.as_tensor([0.5] * 12, dtype=torch.float64)
        # A generic, reproducible unitary target (Q factor of a seeded complex Gaussian).
        z = torch.randn(4, 4, generator=torch.Generator().manual_seed(1), dtype=torch.complex128)
        target_unitary, _ = torch.linalg.qr(z)
        # Seed immediately before each run so the random parameter init is identical.
        torch.manual_seed(0)
        return optimize_unitary_subcircuit_parameters(
            target_unitary=target_unitary,
            beam_splitter_reflectivities=bs,
            max_iterations=max_iterations,
        )

    def test_zero_iterations_performs_no_step(self) -> None:
        """``max_iterations=0`` returns the (evaluated) initial state, taking no step."""
        # Two zero-step runs are identical, and a single step changes the result -
        # so zero really is a no-optimization run, not silently bumped to one.
        assert torch.equal(self._run(0).phase_shifter_params, self._run(0).phase_shifter_params)
        assert not torch.equal(self._run(0).phase_shifter_params, self._run(1).phase_shifter_params)

    def test_each_step_counts_exactly(self) -> None:
        """0, 1, and 2 steps each produce a distinct result (no clamp collapsing 0/1 onto 2)."""
        zero, one, two = self._run(0), self._run(1), self._run(2)
        # The old floor-of-two clamp forced run(1) == run(2); an exact bound must not.
        assert not torch.equal(zero.phase_shifter_params, one.phase_shifter_params)
        assert not torch.equal(one.phase_shifter_params, two.phase_shifter_params)

    def test_more_steps_never_worsen_best_loss(self) -> None:
        """The returned best loss is non-increasing in the step budget."""
        losses = [self._run(mi).best_loss for mi in (0, 1, 2, 3)]
        assert losses == sorted(losses, reverse=True)
