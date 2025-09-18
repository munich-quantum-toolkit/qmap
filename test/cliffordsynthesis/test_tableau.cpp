//
// This file is part of the MQT QMAP library released under the MIT license.
// See README.md or go to https://github.com/cda-tum/qmap for more information.
//

#include "cliffordsynthesis/Tableau.hpp"
#include "ir/QuantumComputation.hpp"
#include "ir/operations/CompoundOperation.hpp"
#include "ir/operations/Control.hpp"
#include "ir/operations/OpType.hpp"
#include "ir/operations/StandardOperation.hpp"

#include <bitset>
#include <cstddef>
#include <gtest/gtest.h>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>

namespace cs {

class TestTableau : public ::testing::Test {
protected:
  // 0 0 | 1 0 | 0
  // 0 0 | 0 1 | 0
  Tableau tableau{2};

  Tableau fullTableau{2, true};
};

TEST_F(TestTableau, InitialTableau) {
  EXPECT_EQ(tableau.getQubitCount(), 2);
  // X part is zero
  EXPECT_EQ(tableau[0][0], 0);
  EXPECT_EQ(tableau[0][1], 0);
  EXPECT_EQ(tableau[1][0], 0);
  EXPECT_EQ(tableau[1][1], 0);
  // Z part is identity
  EXPECT_EQ(tableau[0][2], 1);
  EXPECT_EQ(tableau[0][3], 0);
  EXPECT_EQ(tableau[1][2], 0);
  EXPECT_EQ(tableau[1][3], 1);
  // R part is zero
  EXPECT_EQ(tableau[0][4], 0);
  EXPECT_EQ(tableau[1][4], 0);

  EXPECT_EQ(fullTableau[0][0], 1);
  EXPECT_EQ(fullTableau[0][1], 0);
  EXPECT_EQ(fullTableau[1][0], 0);
  EXPECT_EQ(fullTableau[1][1], 1);

  EXPECT_EQ(fullTableau[0][2], 0);
  EXPECT_EQ(fullTableau[0][3], 0);
  EXPECT_EQ(fullTableau[1][2], 0);
  EXPECT_EQ(fullTableau[1][3], 0);

  EXPECT_EQ(fullTableau[2][0], 0);
  EXPECT_EQ(fullTableau[2][1], 0);
  EXPECT_EQ(fullTableau[3][0], 0);
  EXPECT_EQ(fullTableau[3][1], 0);

  EXPECT_EQ(fullTableau[2][2], 1);
  EXPECT_EQ(fullTableau[2][3], 0);
  EXPECT_EQ(fullTableau[3][2], 0);
  EXPECT_EQ(fullTableau[3][3], 1);

  EXPECT_EQ(fullTableau[0][4], 0);
  EXPECT_EQ(fullTableau[1][4], 0);
  EXPECT_EQ(fullTableau[2][4], 0);
  EXPECT_EQ(fullTableau[3][4], 0);

  std::stringstream ss;
  ss << tableau;

  const std::string representation = "0;0;1;0;0;\n"
                                     "0;0;0;1;0;\n";
  EXPECT_EQ(ss.str(), representation);

  std::stringstream fullss;
  fullss << fullTableau;

  const std::string fullRepresentation = "1;0;0;0;0;\n"
                                         "0;1;0;0;0;\n"
                                         "0;0;1;0;0;\n"
                                         "0;0;0;1;0;\n";
  EXPECT_EQ(fullss.str(), fullRepresentation);

  const auto tFromStr = Tableau(representation);
  EXPECT_EQ(tableau, tFromStr);

  const auto fullTFromStr = Tableau(fullRepresentation);
  EXPECT_EQ(fullTableau, fullTFromStr);

  const std::string stabilizers = "[+ZI, +IZ]";
  const auto tFromStabilizers = Tableau(stabilizers);
  EXPECT_EQ(tableau, tFromStabilizers);

  const std::string destabilizers = "[+XI, +IX]";
  const auto fullTFromStabilizersAndDestabilizers =
      Tableau(stabilizers, destabilizers);
  EXPECT_EQ(fullTableau, fullTFromStabilizersAndDestabilizers);
}

TEST_F(TestTableau, H) {
  // H on |0> is |+>, which is stabilized by X
  tableau.applyH(0);
  std::string expected = "1;0;0;0;0;\n"
                         "0;0;0;1;0;\n";
  EXPECT_EQ(tableau, Tableau(expected));
  EXPECT_EQ(tableau, Tableau("[+XI, +IZ]"));

  tableau.applyH(1);
  expected = "1;0;0;0;0;\n"
             "0;1;0;0;0;\n";
  EXPECT_EQ(tableau, Tableau(expected));
  EXPECT_EQ(tableau, Tableau("[+XI, +IX]"));

  tableau.applyH(1);
  expected = "1;0;0;0;0;\n"
             "0;0;0;1;0;\n";
  EXPECT_EQ(tableau, Tableau(expected));
  EXPECT_EQ(tableau, Tableau("[+XI, +IZ]"));

  tableau.applyH(0);
  expected = "0;0;1;0;0;\n"
             "0;0;0;1;0;\n";
  EXPECT_EQ(tableau, Tableau(expected));
  EXPECT_EQ(tableau, Tableau("[+ZI, +IZ]"));
}

TEST_F(TestTableau, FullH) {
  // H on |0> is |+>, which is stabilized by X
  fullTableau.applyH(0);
  std::string expected = "0;0;1;0;0;\n"
                         "0;1;0;0;0;\n"
                         "1;0;0;0;0;\n"
                         "0;0;0;1;0;\n";
  EXPECT_EQ(fullTableau, Tableau(expected));
  EXPECT_EQ(fullTableau, Tableau("[+XI, +IZ]", "[+ZI, +IX]"));

  fullTableau.applyH(1);
  expected = "0;0;1;0;0;\n"
             "0;0;0;1;0;\n"
             "1;0;0;0;0;\n"
             "0;1;0;0;0;\n";
  EXPECT_EQ(fullTableau, Tableau(expected));
  EXPECT_EQ(fullTableau, Tableau("[+XI, +IX]", "[+ZI, +IZ]"));

  fullTableau.applyH(1);
  expected = "0;0;1;0;0;\n"
             "0;1;0;0;0;\n"
             "1;0;0;0;0;\n"
             "0;0;0;1;0;\n";
  EXPECT_EQ(fullTableau, Tableau(expected));
  EXPECT_EQ(fullTableau, Tableau("[+XI, +IZ]", "[+ZI, +IX]"));

  fullTableau.applyH(0);
  expected = "1;0;0;0;0;\n"
             "0;1;0;0;0;\n"
             "0;0;1;0;0;\n"
             "0;0;0;1;0;\n";
  EXPECT_EQ(fullTableau, Tableau(expected));
  EXPECT_EQ(fullTableau, Tableau("[+ZI, +IZ]", "[+XI, +IX]"));
}

TEST_F(TestTableau, X) {
  // X on |0> is |1>, which is stabilized by -Z
  tableau.applyX(0);
  std::string expected = "0;0;1;0;1;\n"
                         "0;0;0;1;0;\n";
  EXPECT_EQ(tableau, Tableau(expected));
  EXPECT_EQ(tableau, Tableau("[-ZI, +IZ]"));

  tableau.applyX(1);
  expected = "0;0;1;0;1;\n"
             "0;0;0;1;1;\n";
  EXPECT_EQ(tableau, Tableau(expected));
  EXPECT_EQ(tableau, Tableau("[-ZI, -IZ]"));

  tableau.applyX(1);
  expected = "0;0;1;0;1;\n"
             "0;0;0;1;0;\n";
  EXPECT_EQ(tableau, Tableau(expected));
  EXPECT_EQ(tableau, Tableau("[-ZI, +IZ]"));

  tableau.applyX(0);
  expected = "0;0;1;0;0;\n"
             "0;0;0;1;0;\n";
  EXPECT_EQ(tableau, Tableau(expected));
  EXPECT_EQ(tableau, Tableau("[+ZI, +IZ]"));
}

TEST_F(TestTableau, FullX) {
  // X on |0> is |1>, which is stabilized by -Z
  fullTableau.applyX(0);
  std::string expected = "1;0;0;0;0;\n"
                         "0;1;0;0;0;\n"
                         "0;0;1;0;1;\n"
                         "0;0;0;1;0;\n";
  EXPECT_EQ(fullTableau, Tableau(expected));
  EXPECT_EQ(fullTableau, Tableau("[-ZI, +IZ]", "[+XI, +IX]"));

  fullTableau.applyX(1);
  expected = "1;0;0;0;0;\n"
             "0;1;0;0;0;\n"
             "0;0;1;0;1;\n"
             "0;0;0;1;1;\n";
  EXPECT_EQ(fullTableau, Tableau(expected));
  EXPECT_EQ(fullTableau, Tableau("[-ZI, -IZ]", "[+XI, +IX]"));

  fullTableau.applyX(1);
  expected = "1;0;0;0;0;\n"
             "0;1;0;0;0;\n"
             "0;0;1;0;1;\n"
             "0;0;0;1;0;\n";
  EXPECT_EQ(fullTableau, Tableau(expected));
  EXPECT_EQ(fullTableau, Tableau("[-ZI, +IZ]", "[+XI, +IX]"));

  fullTableau.applyX(0);
  expected = "1;0;0;0;0;\n"
             "0;1;0;0;0;\n"
             "0;0;1;0;0;\n"
             "0;0;0;1;0;\n";
  EXPECT_EQ(fullTableau, Tableau(expected));
  EXPECT_EQ(fullTableau, Tableau("[+ZI, +IZ]", "[+XI, +IX]"));
}

TEST_F(TestTableau, S) {
  // S on |0> is |0>, which is stabilized by +Z
  tableau.applyS(0);
  std::string expected = "0;0;1;0;0;\n"
                         "0;0;0;1;0;\n";
  EXPECT_EQ(tableau, Tableau(expected));
  EXPECT_EQ(tableau, Tableau("[+ZI, +IZ]"));

  // S on |1> is i|1>, which is stabilized by -Z
  tableau.applyX(0);
  tableau.applyS(0);
  expected = "0;0;1;0;1;\n"
             "0;0;0;1;0;\n";
  EXPECT_EQ(tableau, Tableau(expected));
  EXPECT_EQ(tableau, Tableau("[-ZI, +IZ]"));

  // S on |+> is |R> = 1/sqrt(2) (|0> + i|1>), which is stabilized by Y
  tableau.applyH(1);
  tableau.applyS(1);
  expected = "0;0;1;0;1;\n"
             "0;1;0;1;0;\n";
  EXPECT_EQ(tableau, Tableau(expected));
  EXPECT_EQ(tableau, Tableau("[-ZI, +IY]"));

  tableau.applySdag(1);
  expected = "0;0;1;0;1;\n"
             "0;1;0;0;0;\n";
  EXPECT_EQ(tableau, Tableau(expected));
  EXPECT_EQ(tableau, Tableau("[-ZI, +IX]"));
}

TEST_F(TestTableau, FullS) {
  // S on |0> is |0>, which is stabilized by +Z
  fullTableau.applyS(0);
  std::string expected = "1;0;1;0;0;\n"
                         "0;1;0;0;0;\n"
                         "0;0;1;0;0;\n"
                         "0;0;0;1;0;\n";
  EXPECT_EQ(fullTableau, Tableau(expected));
  EXPECT_EQ(fullTableau, Tableau("[+ZI, +IZ]", "[+YI, +IX]"));

  // S on |1> is i|1>, which is stabilized by -Z
  fullTableau.applyX(0);
  fullTableau.applyS(0);
  expected = "1;0;0;0;0;\n"
             "0;1;0;0;0;\n"
             "0;0;1;0;1;\n"
             "0;0;0;1;0;\n";
  EXPECT_EQ(fullTableau, Tableau(expected));
  EXPECT_EQ(fullTableau, Tableau("[-ZI, +IZ]", "[+XI, +IX]"));

  // S on |+> is |R> = 1/sqrt(2) (|0> + i|1>), which is stabilized by Y
  fullTableau.applyH(1);
  fullTableau.applyS(1);
  expected = "1;0;0;0;0;\n"
             "0;0;0;1;0;\n"
             "0;0;1;0;1;\n"
             "0;1;0;1;0;\n";
  EXPECT_EQ(fullTableau, Tableau(expected));
  EXPECT_EQ(fullTableau, Tableau("[-ZI, +IY]", "[+XI, +IZ]"));

  fullTableau.applySdag(1);
  expected = "1;0;0;0;0;\n"
             "0;0;0;1;0;\n"
             "0;0;1;0;1;\n"
             "0;1;0;0;0;\n";
  EXPECT_EQ(fullTableau, Tableau(expected));
  EXPECT_EQ(fullTableau, Tableau("[-ZI, +IX]", "[+XI, +IZ]"));
}

TEST_F(TestTableau, Z) {
  // Z on |0> is |0>, which is stabilized by +Z
  tableau.applyZ(0);
  std::string expected = "0;0;1;0;0;\n"
                         "0;0;0;1;0;\n";
  EXPECT_EQ(tableau, Tableau(expected));
  EXPECT_EQ(tableau, Tableau("[+ZI, +IZ]"));

  // Z on |1> is -|1>, which is stabilized by -Z
  tableau.applyX(0);
  tableau.applyZ(0);
  expected = "0;0;1;0;1;\n"
             "0;0;0;1;0;\n";
  EXPECT_EQ(tableau, Tableau(expected));
  EXPECT_EQ(tableau, Tableau("[-ZI, +IZ]"));

  // Z on |+> is |->, which is stabilized by -X
  tableau.applyH(1);
  tableau.applyZ(1);
  expected = "0;0;1;0;1;\n"
             "0;1;0;0;1;\n";
  EXPECT_EQ(tableau, Tableau(expected));
  EXPECT_EQ(tableau, Tableau("[-ZI, -IX]"));
}

TEST_F(TestTableau, FullZ) {
  // Z on |0> is |0>, which is stabilized by +Z
  fullTableau.applyZ(0);
  std::string expected = "1;0;0;0;1;\n"
                         "0;1;0;0;0;\n"
                         "0;0;1;0;0;\n"
                         "0;0;0;1;0;\n";
  EXPECT_EQ(fullTableau, Tableau(expected));
  EXPECT_EQ(fullTableau, Tableau("[+ZI, +IZ]", "[-XI, +IX]"));

  // Z on |1> is -|1>, which is stabilized by -Z
  fullTableau.applyX(0);
  fullTableau.applyZ(0);
  expected = "1;0;0;0;0;\n"
             "0;1;0;0;0;\n"
             "0;0;1;0;1;\n"
             "0;0;0;1;0;\n";
  EXPECT_EQ(fullTableau, Tableau(expected));
  EXPECT_EQ(fullTableau, Tableau("[-ZI, +IZ]", "[+XI, +IX]"));

  // Z on |+> is |->, which is stabilized by -X
  fullTableau.applyH(1);
  fullTableau.applyZ(1);
  expected = "1;0;0;0;0;\n"
             "0;0;0;1;0;\n"
             "0;0;1;0;1;\n"
             "0;1;0;0;1;\n";
  EXPECT_EQ(fullTableau, Tableau(expected));
  EXPECT_EQ(fullTableau, Tableau("[-ZI, -IX]", "[+XI, +IZ]"));
}

TEST_F(TestTableau, Sx) {
  // Applying two Sx gates on |0> is equivalent to applying an X gate
  tableau.applySx(0);
  tableau.applySx(0);

  std::string expected = "0;0;1;0;1;\n"
                         "0;0;0;1;0;\n";
  EXPECT_EQ(tableau, Tableau(expected));
  EXPECT_EQ(tableau, Tableau("[-ZI, +IZ]"));

  tableau.applySxdag(0);
  tableau.applySxdag(0);
  expected = "0;0;1;0;0;\n"
             "0;0;0;1;0;\n";
  EXPECT_EQ(tableau, Tableau(expected));
  EXPECT_EQ(tableau, Tableau("[+ZI, +IZ]"));
}

TEST_F(TestTableau, FullSx) {
  // Applying two Sx gates on |0> is equivalent to applying an X gate
  fullTableau.applySx(0);
  fullTableau.applySx(0);

  std::string expected = "1;0;0;0;0;\n"
                         "0;1;0;0;0;\n"
                         "0;0;1;0;1;\n"
                         "0;0;0;1;0;\n";
  EXPECT_EQ(fullTableau, Tableau(expected));
  EXPECT_EQ(fullTableau, Tableau("[-ZI, +IZ]", "[+XI, +IX]"));

  fullTableau.applySxdag(0);
  fullTableau.applySxdag(0);
  expected = "1;0;0;0;0;\n"
             "0;1;0;0;0;\n"
             "0;0;1;0;0;\n"
             "0;0;0;1;0;\n";
  EXPECT_EQ(fullTableau, Tableau(expected));
  EXPECT_EQ(fullTableau, Tableau("[+ZI, +IZ]", "[+XI, +IX]"));
}

TEST_F(TestTableau, Y) {
  // Y on |0> is i|1>, which is stabilized by -Z
  tableau.applyY(0);
  std::string expected = "0;0;1;0;1;\n"
                         "0;0;0;1;0;\n";
  EXPECT_EQ(tableau, Tableau(expected));
  EXPECT_EQ(tableau, Tableau("[-ZI, +IZ]"));

  // Y on |1> is -i|0>, which is stabilized by +Z
  tableau.applyX(1);
  tableau.applyY(1);
  expected = "0;0;1;0;1;\n"
             "0;0;0;1;0;\n";
  EXPECT_EQ(tableau, Tableau(expected));
  EXPECT_EQ(tableau, Tableau("[-ZI, +IZ]"));
}

TEST_F(TestTableau, FullY) {
  // Y on |0> is i|1>, which is stabilized by -Z
  fullTableau.applyY(0);
  std::string expected = "1;0;0;0;1;\n"
                         "0;1;0;0;0;\n"
                         "0;0;1;0;1;\n"
                         "0;0;0;1;0;\n";
  EXPECT_EQ(fullTableau, Tableau(expected));
  EXPECT_EQ(fullTableau, Tableau("[-ZI, +IZ]", "[-XI, +IX]"));

  // Y on |1> is -i|0>, which is stabilized by +Z
  fullTableau.applyX(1);
  fullTableau.applyY(1);
  expected = "1;0;0;0;1;\n"
             "0;1;0;0;1;\n"
             "0;0;1;0;1;\n"
             "0;0;0;1;0;\n";
  EXPECT_EQ(fullTableau, Tableau(expected));
  EXPECT_EQ(fullTableau, Tableau("[-ZI, +IZ]", "[-XI, -IX]"));
}

TEST_F(TestTableau, CX) {
  // CX is stabilized by +ZI, +ZZ
  tableau.applyCX(0, 1);
  std::string expected = "0;0;1;0;0;\n"
                         "0;0;1;1;0;\n";
  EXPECT_EQ(tableau, Tableau(expected));
  EXPECT_EQ(tableau, Tableau("[+ZI, +ZZ]"));

  // undo CX
  tableau.applyCX(0, 1);
  expected = "0;0;1;0;0;\n"
             "0;0;0;1;0;\n";
  EXPECT_EQ(tableau, Tableau(expected));
  EXPECT_EQ(tableau, Tableau("[+ZI, +IZ]"));

  // CX on |10> is |11>, which is stabilized by -ZI, +ZZ
  tableau.applyX(0);
  tableau.applyCX(0, 1);
  expected = "0;0;1;0;1;\n"
             "0;0;1;1;0;\n";
  EXPECT_EQ(tableau, Tableau(expected));
  EXPECT_EQ(tableau, Tableau("[-ZI, +ZZ]"));
}

TEST_F(TestTableau, FullCX) {
  // CX is stabilized by +ZI, +ZZ
  fullTableau.applyCX(0, 1);
  std::string expected = "1;1;0;0;0;\n"
                         "0;1;0;0;0;\n"
                         "0;0;1;0;0;\n"
                         "0;0;1;1;0;\n";
  EXPECT_EQ(fullTableau, Tableau(expected));
  EXPECT_EQ(fullTableau, Tableau("[+ZI, +ZZ]", "[+XX, +IX]"));

  // undo CX
  fullTableau.applyCX(0, 1);
  expected = "1;0;0;0;0;\n"
             "0;1;0;0;0;\n"
             "0;0;1;0;0;\n"
             "0;0;0;1;0;\n";
  EXPECT_EQ(fullTableau, Tableau(expected));
  EXPECT_EQ(fullTableau, Tableau("[+ZI, +IZ]", "[+XI, +IX]"));

  // CX on |10> is |11>, which is stabilized by -ZI, +ZZ
  fullTableau.applyX(0);
  fullTableau.applyCX(0, 1);
  expected = "1;1;0;0;0;\n"
             "0;1;0;0;0;\n"
             "0;0;1;0;1;\n"
             "0;0;1;1;0;\n";
  EXPECT_EQ(fullTableau, Tableau(expected));
  EXPECT_EQ(fullTableau, Tableau("[-ZI, +ZZ]", "[+XX, +IX]"));
}

TEST_F(TestTableau, BellState) {
  // |00> + |11> is stabilized by +XX, +ZZ
  tableau.applyH(0);
  tableau.applyCX(0, 1);
  const std::string expected = "1;1;0;0;0;\n"
                               "0;0;1;1;0;\n";
  EXPECT_EQ(tableau, Tableau(expected));
  EXPECT_EQ(tableau, Tableau("[+XX, +ZZ]"));
}

TEST_F(TestTableau, FullBellState) {
  // |00> + |11> is stabilized by +XX, +ZZ
  fullTableau.applyH(0);
  fullTableau.applyCX(0, 1);
  const std::string expected = "0;0;1;0;0;\n"
                               "0;1;0;0;0;\n"
                               "1;1;0;0;0;\n"
                               "0;0;1;1;0;\n";
  EXPECT_EQ(fullTableau, Tableau(expected));
  EXPECT_EQ(fullTableau, Tableau("[+XX, +ZZ]", "[+ZI, +IX]"));
}

TEST_F(TestTableau, CircuitTranslation) {
  using namespace qc::literals;

  qc::QuantumComputation qc(2U);
  qc.i(0);
  qc.x(0);
  qc.y(0);
  qc.z(0);
  qc.h(0);
  qc.s(0);
  qc.sdg(0);
  qc.sx(0);
  qc.sxdg(0);
  qc.cx(0_pc, 1);
  qc.cy(0_pc, 1);
  qc.cz(0_pc, 1);
  qc.swap(0, 1);
  qc.iswap(0, 1);
  qc.dcx(0, 1);
  qc.ecr(0, 1);

  auto compOP = std::make_unique<qc::CompoundOperation>();
  compOP->emplace_back<qc::StandardOperation>(0, qc::H);
  compOP->emplace_back<qc::StandardOperation>(0_pc, 1, qc::X);
  qc.emplace_back(compOP);

  EXPECT_NO_THROW(tableau = cs::Tableau(qc););
  EXPECT_NO_THROW(fullTableau = cs::Tableau(qc, true));
}

TEST_F(TestTableau, UnsupportedOperations) {
  using namespace qc::literals;

  qc::QuantumComputation qc(3U);

  // three-qubit operation not supported
  qc.mcx({1_pc, 2_pc}, 0);
  EXPECT_THROW(tableau = cs::Tableau(qc), std::runtime_error);

  // single-qubit gate not supported
  qc.clear();
  qc.t(0);
  EXPECT_THROW(tableau = cs::Tableau(qc), std::runtime_error);

  // controlled two-qubit gate not supported
  qc.clear();
  qc.cs(1_pc, 0);
  EXPECT_THROW(tableau = cs::Tableau(qc), std::runtime_error);
}

TEST_F(TestTableau, BVAccess) {
  const auto bv0 = 0b01;
  const auto bv1 = 0b10;
  const auto bv2 = 0b00;

  tableau.populateTableauFrom(bv0, 2, 0);
  tableau.populateTableauFrom(bv1, 2, 1);
  tableau.populateTableauFrom(bv2, 2, 2);
  tableau.populateTableauFrom(bv2, 2, 3);
  tableau.populateTableauFrom(bv2, 2, 4);

  const auto col0 = tableau.getBVFrom(0);
  const auto col1 = tableau.getBVFrom(1);
  const auto col2 = tableau.getBVFrom(2);
  const auto col3 = tableau.getBVFrom(3);
  const auto col4 = tableau.getBVFrom(4);

  EXPECT_EQ(col0, bv0);
  EXPECT_EQ(col1, bv1);
  EXPECT_EQ(col2, bv2);
  EXPECT_EQ(col3, bv2);
  EXPECT_EQ(col4, bv2);

  const std::string expected = "1;0;0;0;0;\n"
                               "0;1;0;0;0;\n";

  EXPECT_EQ(tableau, Tableau(expected));
}

TEST_F(TestTableau, FullBVAccess) {
  const auto bv0 = 0b1000;
  const auto bv1 = 0b0100;
  const auto bv2 = 0b0010;
  const auto bv3 = 0b0001;
  const auto bv4 = 0b0000;

  fullTableau.populateTableauFrom(bv0, 4, 0);
  fullTableau.populateTableauFrom(bv1, 4, 1);
  fullTableau.populateTableauFrom(bv2, 4, 2);
  fullTableau.populateTableauFrom(bv3, 4, 3);
  fullTableau.populateTableauFrom(bv4, 4, 4);

  const auto col0 = fullTableau.getBVFrom(0);
  const auto col1 = fullTableau.getBVFrom(1);
  const auto col2 = fullTableau.getBVFrom(2);
  const auto col3 = fullTableau.getBVFrom(3);
  const auto col4 = fullTableau.getBVFrom(4);

  EXPECT_EQ(col0, bv0);
  EXPECT_EQ(col1, bv1);
  EXPECT_EQ(col2, bv2);
  EXPECT_EQ(col3, bv3);
  EXPECT_EQ(col4, bv4);

  const std::string expected = "0;0;0;1;0;\n"
                               "0;0;1;0;0;\n"
                               "0;1;0;0;0;\n"
                               "1;0;0;0;0;\n";

  EXPECT_EQ(fullTableau, Tableau(expected));
}

TEST_F(TestTableau, LargeBV) {
  // Assert that a tableau for 128 qubits can be properly created
  tableau = Tableau(128);
  for (std::size_t i = 0U; i < 128U; ++i) {
    EXPECT_EQ(tableau.getBVFrom<128>(128U + i), std::bitset<128>().set(i));
  }

  // Set the phase for all qubits to 1
  tableau.populateTableauFrom(std::bitset<128>().flip(), 128, 256);
  for (std::size_t i = 0U; i < 128U; ++i) {
    EXPECT_EQ(tableau[i][256], 1U);
  }
}

TEST_F(TestTableau, TableauIO) {
  const std::string filename = "tableau.txt";
  tableau.dump(filename);
  auto tableau2 = Tableau{};
  tableau2.import(filename);
  EXPECT_EQ(tableau, tableau2);

  const std::string filename2 = "fullTableau.txt";
  fullTableau.dump(filename2);
  tableau2 = Tableau{};
  tableau2.import(filename2);
  EXPECT_EQ(fullTableau, tableau2);
}

TEST_F(TestTableau, InvalidInput) {
  EXPECT_THROW(tableau = Tableau("[ZZX, aXy]"), std::invalid_argument);
  EXPECT_THROW(tableau = Tableau("[ZZ__I, XXY]"), std::invalid_argument);
  EXPECT_THROW(tableau = Tableau("[ZZI, -XY]"), std::invalid_argument);
  EXPECT_THROW(tableau = Tableau("XY, XY]"), std::invalid_argument);
  EXPECT_THROW(tableau = Tableau("[XY, XY"), std::invalid_argument);
  EXPECT_THROW(tableau = Tableau("[XY, XY"), std::invalid_argument);
  EXPECT_THROW(tableau = Tableau("[XY; XY"), std::invalid_argument);
  EXPECT_THROW(tableau = Tableau("['XY, XY]"), std::invalid_argument);
}

TEST_F(TestTableau, ApplyCXH) {
  tableau = Tableau(3);
  tableau.applyCX(1, 2);
  std::string expected = "0;0;0;1;0;0;0\n0;0;0;0;1;0;0\n0;0;0;0;1;1;0";
  EXPECT_EQ(tableau, Tableau(expected));
  tableau.applyH(2);
  expected = "0;0;0;1;0;0;0\n0;0;0;0;1;0;0\n0;0;1;0;1;0;0";
  EXPECT_EQ(tableau, Tableau(expected));
  tableau.applyH(1);
  expected = "0;0;0;1;0;0;0\n0;1;0;0;0;0;0\n0;1;1;0;0;0;0";
  EXPECT_EQ(tableau, Tableau(expected));
  tableau.applyH(2);
  expected = "0;0;0;1;0;0;0\n0;1;0;0;0;0;0\n0;1;0;0;0;1;0";
  EXPECT_EQ(tableau, Tableau(expected));
  tableau.applyCX(0, 2);
  expected = "0;0;0;1;0;0;0\n0;1;0;0;0;0;0\n0;1;0;1;0;1;0";
  EXPECT_EQ(tableau, Tableau(expected));
  tableau.applyCX(0, 1);
  expected = "0;0;0;1;0;0;0\n0;1;0;0;0;0;0\n0;1;0;1;0;1;0";
  EXPECT_EQ(tableau, Tableau(expected));
  EXPECT_EQ(tableau, Tableau("[+ZII, +IXI, +ZXZ]"));
}

} // namespace cs
