OPENQASM 2.0;
include "qelib1.inc";
qreg q[4];
// Description:
//     This circuit is a random compilation of all possible gates.
//
// Motivation:
//     Test the correct handling of a mixture of global, single-qubit and
//     two-qubit gates.
//     ┌───┐
//q_0: ┤   ├─■─────────────
//     │   │ │    ┌───────┐
//q_1: ┤   ├─■──■─┤ Rz(π) ├
//     │ Y │    │ ├───────┤
//q_2: ┤   ├────■─┤ Rz(π) ├
//     │   │      └───────┘
//q_3: ┤   ├───────────────
//     └───┘
y q;
cz q[0],q[1];
cz q[0],q[2];
z q[1];
z q[2];
