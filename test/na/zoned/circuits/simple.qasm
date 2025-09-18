OPENQASM 2.0;
include "qelib1.inc";
qreg q[1];
// Description:
//     This circuit contains 1 RZ-gate.
//
// Motivation:
//     Test the correct handling of single-qubit gates.
//       ┌──────────┐
// q_0: ─┤ Rz(3.14) ├─
//       └──────────┘
rz(3.14) q[0];
