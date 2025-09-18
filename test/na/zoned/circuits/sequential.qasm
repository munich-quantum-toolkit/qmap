OPENQASM 2.0;
include "qelib1.inc";
qreg q[32];
// Description:
//     This circuit contains 32 CZ-gates that can only be performed
//     sequentially. Always, one qubit of the previous gate can be reused.
//
// Motivation:
//     Since, one of the qubits is reused, many CZ-gates are executed in the
//     same site's pair (before the lookahead decides not to reuse the qubit).
//     While moving back, the qubits build a flock in the storage zone which
//     requires proper routing in the storage zone.
//
//  q_0: ─■─────────
//        │
//  q_1: ─■─■───────
//          │
//  q_2: ───■─■─────
//            │
//  q_3: ─────■─────
//
//           ...
//
// q_30: ─────────■─
//                │
// q_31: ─────────■─
cz q[0], q[1];
cz q[1], q[2];
cz q[2], q[3];
cz q[3], q[4];
cz q[4], q[5];
cz q[5], q[6];
cz q[6], q[7];
cz q[7], q[8];
cz q[8], q[9];
cz q[9], q[10];
cz q[10], q[11];
cz q[11], q[12];
cz q[12], q[13];
cz q[13], q[14];
cz q[14], q[15];
cz q[15], q[16];
cz q[16], q[17];
cz q[17], q[18];
cz q[18], q[19];
cz q[19], q[20];
cz q[20], q[21];
cz q[21], q[22];
cz q[22], q[23];
cz q[23], q[24];
cz q[24], q[25];
cz q[25], q[26];
cz q[26], q[27];
cz q[27], q[28];
cz q[28], q[29];
cz q[29], q[30];
cz q[30], q[31];
