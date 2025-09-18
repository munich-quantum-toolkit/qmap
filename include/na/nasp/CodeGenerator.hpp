#pragma once

#include "Solver.hpp"
#include "ir/QuantumComputation.hpp"
#include "na/NAComputation.hpp"
#include "na/entities/Location.hpp"

#include <cstdint>

namespace na {
using namespace qc;

class CodeGenerator {
private:
  /**
   * @brief Calculate the coordinates of a point in the 2D grid from its
   * discrete coordinates.
   *
   * @details The parameters specify the measures that were used for the
   * abstraction of the 2D grid. In the abstraction the 2D plane is divided into
   * interaction sites. Each site is identified by a discrete coordinate. The
   * exact (discrete) position of an atom in a site is given by the discrete
   * offset from the sites' origin.
   *
   * @param q A qubit that contains the discrete coordinates of the interaction
   * site together with the discrete offset.
   * @param maxHOffset The maximal possible horizontal offset of an atom in a
   * site. This value determines the width of a site in x-direction.
   * @param maxVOffset The maximal possible vertical offset of an atom in a
   * site. This value determines the height of a site in y-direction.
   * @param minEntanglingY The minimal y-coordinate of the entangling zone. All
   * discrete y-coordinates smaller than this value are considered to be in the
   * top storage zone. If this value is 0, there is no top storage zone.
   * @param maxEntanglingY The maximal y-coordinate of the entangling zone. All
   * discrete y-coordinates greater than this value are considered to be in the
   * bottom storage zone.
   * @param minAtomDist The minimal distance between two atoms in the 2D grid,
   * i.e., between the discrete sites in one interaction site.
   * @param noInteractionRadius The radius of atoms in order to avoid
   * interactions. This is used as the minimal distance between two atoms in two
   * different interaction sites.
   * @param zoneDist The distance between the top storage zone and the
   * entangling zone and between the entangling zone and the bottom storage
   * zone. This distance already includes the minimal distance between two atoms
   * and the no interaction radius, i.e., it is the minimal distance between two
   * atoms in different zones.
   * @return The coordinates of the point in the 2D grid.
   */
  static auto coordFromDiscrete(NASolver::Result::Qubit q, int64_t maxHOffset,
                                int64_t maxVOffset, int64_t minEntanglingY,
                                int64_t maxEntanglingY, int64_t minAtomDist,
                                int64_t noInteractionRadius, int64_t zoneDist)
      -> Location;

public:
  /**
   * @brief Generate a NAComputation from a QuantumComputation and a NASolver
   * result.
   *
   * @details The function generates a NAComputation from a QuantumComputation
   * and a NASolver result. The solver works with an abstraction of the 2D grid.
   * In the abstraction the 2D plane is divided into
   * interaction sites. Each site is identified by a discrete coordinate. The
   * exact (discrete) position of an atom in a site is given by the discrete
   * offset from the sites' origin. This function calculates the exact position
   * of each qubit in the 2D grid from the discrete coordinates and offsets. For
   * the calculation, the measures used for the abstraction are required.
   *
   * @warning This function expects a QuantumComputation that was used as input
   * for the NASolver. Additionally, this function assumes the quantum circuit
   * represented by the QuantumComputation to be of the following form:
   * First, all qubits are initialized in the |+> state by applying a Hadamard
   * gate to each qubit. Then, a set of entangling gates (CZ) is applied to the
   * qubits. Finally, hadamard gates are applied to some qubits. Unfortunately,
   * the function cannot deal with arbitrary quantum circuits as the NASolver
   * cannot do either.
   *
   * @param input The QuantumComputation that was used as input for the
   * NASolver.
   * @param result The result of the NASolver.
   * @param maxHOffset The maximal possible horizontal offset of an atom in a
   * site. This value determines the width of a site in x-direction.
   * @param maxVOffset The maximal possible vertical offset of an atom in a
   * site. This value determines the height of a site in y-direction.
   * @param minEntanglingY The minimal y-coordinate of the entangling zone. All
   * discrete y-coordinates smaller than this value are considered to be in the
   * top storage zone. If this value is 0, there is no top storage zone.
   * @param maxEntanglingY The maximal y-coordinate of the entangling zone. All
   * discrete y-coordinates greater than this value are considered to be in the
   * bottom storage zone.
   * @param minAtomDist The minimal distance between two atoms in the 2D grid,
   * i.e., between the discrete sites in one interaction site.
   * @param noInteractionRadius The radius of atoms in order to avoid
   * interactions. This is used as the minimal distance between two atoms in two
   * different interaction sites.
   * @param zoneDist The distance between the top storage zone and the
   * entangling zone and between the entangling zone and the bottom storage
   * zone. This distance already includes the minimal distance between two atoms
   * and the no interaction radius, i.e., it is the minimal distance between two
   * atoms in different zones.
   * @return The generated NAComputation.
   */
  [[nodiscard]] static auto generate(const QuantumComputation& input,
                                     const NASolver::Result& result,
                                     uint16_t minAtomDist = 1,
                                     uint16_t noInteractionRadius = 10,
                                     uint16_t zoneDist = 24) -> NAComputation;
};
} // namespace na
