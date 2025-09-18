OPENQASM 2.0;
include "qelib1.inc";
qreg q[32];
// Description:
//     This circuit contains 16 CZ-gates that can be performed parallel in one
//     go.
//
// Motivation:
//     Hence, it requires some proper routing of qubits because not for every
//     qubit the nearest site can be chosen.
//
//  q_0: ─■─
//        │
//  q_1: ─■─
//
//  q_2: ─■─
//        │
//  q_3: ─■─
//
//  q_4: ─■─
//        │
//  q_5: ─■─
//
// ...
//
// q_30: ─■─
//        │
// q_31: ─■─
cz q[0], q[1];
cz q[2], q[3];
cz q[4], q[5];
cz q[6], q[7];
cz q[8], q[9];
cz q[10], q[11];
cz q[12], q[13];
cz q[14], q[15];
cz q[16], q[17];
cz q[18], q[19];
cz q[20], q[21];
cz q[22], q[23];
cz q[24], q[25];
cz q[26], q[27];
cz q[28], q[29];
cz q[30], q[31];
