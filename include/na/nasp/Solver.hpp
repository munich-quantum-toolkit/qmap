#pragma once

#include "ir/Definitions.hpp"
#include "ir/QuantumComputation.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <nlohmann/json_fwd.hpp>
#include <optional>
#include <string>
#include <utility>
#include <vector>
#include <z3++.h>

namespace na {
using namespace z3;

class NASolver {
private:
  /// Z3 context used throughout the solver instance
  std::shared_ptr<context> ctx;

  uint16_t maxX = 0; ///< maximum x-coordinate of an interaction site
  uint16_t maxY = 0; ///< maximum y-coordinate of an interaction site
  /**
   * @brief minimum y-coordinate of the entangling zone.
   * @details All discrete
   * y-coordinates smaller than this value are considered to be in the top
   * storage zone. If this value is 0, there is no top storage zone.
   */
  uint16_t minEntanglingY = 0;
  /**
   * @brief maximum y-coordinate of the entangling zone.
   * @details All discrete
   * y-coordinates greater than this value are considered to be in the bottom
   * storage zone. If this value is maxY, there is no bottom storage zone.
   */
  uint16_t maxEntanglingY = 0;
  /**
   * @brief maximum index of an AOD column.
   * @details Limits the number of AOD columns.
   */
  uint16_t maxC = 0;
  /**
   * @brief maximum index of an AOD row.
   * @details Limits the number of AOD rows.
   */
  uint16_t maxR = 0;
  /**
   * @brief maximum horizontal offset from the SLM trap.
   * @details Limits the columns within one interaction site. The number of
   * columns is 2 * maxHOffset + 1.
   */
  uint16_t maxHOffset = 0;
  /**
   * @brief maximum vertical offset from the SLM trap.
   * @details Limits the rows within one interaction site. The number of rows is
   * 2 * maxVOffset + 1.
   */
  uint16_t maxVOffset = 0;
  /**
   * @brief maximum horizontal distance between two atoms in order to interact.
   * @details The distance between two atoms in the 2D grid is at most (maxVDist
   * + maxHDist) * minAtomDist. If maxHDist = 1, this means that two atoms can
   * interact if they are in the same column or in adjacent columns.
   */
  uint16_t maxHDist = 0;
  /**
   * @brief maximum vertical distance between two atoms in order to interact.
   * @details The distance between two atoms in the 2D grid is at most (maxVDist
   * + maxHDist) * minAtomDist. If maxVDist = 1, this means that two atoms can
   * interact if they are in the same row or in adjacent rows.
   */
  uint16_t maxVDist = 0;

  enum class Storage : uint8_t { None, Bottom, TwoSided };

  Storage storage = Storage::None;

  static auto minBitsToRepresentUInt(uint16_t num) -> uint32_t;
  static auto minBitsToRepresentInt(int32_t num) -> uint32_t;

  /// A class to collect all variables associated with one qubit
  class Qubit {
  private:
    uint16_t id; ///< unique identifier of the qubit
    expr x;      ///< x-coordinate of the site the atom is loaded in
    expr y;      ///< y-coordinate of the site the atom is loaded in
    /**
     * @brief boolean variable to indicate whether the atom is loaded in an AOD,
     * SLM otherwise
     */
    expr a;
    /**
     * @brief if the atom is loaded in an AOD, this is the index of the AOD
     * column, otherwise it has no meaning
     */
    expr c;
    /**
     * @brief if the atom is loaded in an AOD, this is the index of the AOD row,
     * otherwise it has no meaning
     */
    expr r;
    /**
     * @brief denotes the horizontal offset from the SLM trap if the atom is
     * loaded in an AOD
     */
    expr h;
    /**
     * @brief denotes the vertical offset from the SLM trap if the atom is
     * loaded in an AOD
     */
    expr v;

  public:
    /**
     * @brief Construct a new Qubit object
     * @param ctx the solvers context
     * @param idx the unique identifier of the qubit
     * @param t the stage the qubit is in
     * @param maxX the maximum possible discrete x-coordinate used to determine
     * the bit width for x
     * @param maxY the maximum possible discrete y-coordinate used to determine
     * the bit width for y
     * @param maxC the maximum possible AOD column index used to determine the
     * bit width for c
     * @param maxR the maximum possible AOD row index used to determine the bit
     * width for r
     * @param maxHOffset the maximum possible horizontal offset used to
     * determine the bit width for h
     * @param maxVOffset the maximum possible vertical offset used to determine
     * the bit width for v
     */
    [[nodiscard]] Qubit(context& ctx, uint16_t idx, uint16_t t, uint16_t maxX,
                        uint16_t maxY, uint16_t maxC, uint16_t maxR,
                        uint16_t maxHOffset, uint16_t maxVOffset);

    /// @see id
    [[nodiscard]] auto getId() const -> uint16_t { return id; }

    /// @see x
    [[nodiscard]] auto getX() const -> const expr& { return x; }

    /// @see y
    [[nodiscard]] auto getY() const -> const expr& { return y; }

    /// @see a
    [[nodiscard]] auto getA() const -> const expr& { return a; }

    /// @see c
    [[nodiscard]] auto getC() const -> const expr& { return c; }

    /// @see r
    [[nodiscard]] auto getR() const -> const expr& { return r; }

    /// @see h
    [[nodiscard]] auto getH() const -> const expr& { return h; }

    /// @see v
    [[nodiscard]] auto getV() const -> const expr& { return v; }
  };

  class Stage {
  private:
    uint16_t t;                ///< the index of the stage
    std::vector<Qubit> qubits; ///< the location of all qubits in this stage
    /**
     * @brief boolean variables to indicate whether the column is loaded at this
     * stage.
     * @details The index of the vector corresponds to the column index.
     * When a column is loaded at a certain stage, then all atoms on this column
     * must be loaded at this stage.
     *
     * @note For an in-detail explanation of the purpose of this member and the
     * members @p loadRows, @p storeCols, @p storeRows, please refer to the
     * corresponding article "Optimal State Preparation for Logical Arrays on
     * Zoned Neutral Atom Quantum Computers" that can be obtained under the
     * following link:
     * https://www.cda.cit.tum.de/files/eda/2025_date_optimal_state_preparation_for_logical_arrays_on_zoned_neutral_atom_quantum_computers.pdf
     */
    std::vector<expr> loadCols;
    /**
     * @brief boolean variables to indicate whether the row is loaded at this
     * stage.
     * @details The index of the vector corresponds to the row index.
     * When a row is loaded at a certain stage, then all atoms on this row
     * must be loaded at this stage.
     */
    std::vector<expr> loadRows;
    /**
     * @brief boolean variables to indicate whether the column is stored at this
     * stage.
     * @details The index of the vector corresponds to the column index.
     * When a column is stored at a certain stage, then all atoms on this column
     * must be stored at this stage.
     */
    std::vector<expr> storeCols;
    /**
     * @brief boolean variables to indicate whether the row is stored at this
     * stage.
     * @details The index of the vector corresponds to the row index.
     * When a row is stored at a certain stage, then all atoms on this row
     * must be stored at this stage.
     */
    std::vector<expr> storeRows;

  public:
    [[nodiscard]] explicit Stage(context& ctx, uint16_t timestep,
                                 uint16_t numQubits, uint16_t maxX,
                                 uint16_t maxY, uint16_t maxC, uint16_t maxR,
                                 uint16_t maxHOffset, uint16_t maxVOffset);

    [[nodiscard]] auto getT() const -> uint16_t { return t; }

    [[nodiscard]] auto getQubit(const size_t i) const -> const Qubit& {
      return qubits[i];
    }

    [[nodiscard]] auto numQubits() const {
      return static_cast<uint16_t>(qubits.size());
    }

    [[nodiscard]] auto getLoadCol(const size_t i) const -> const expr& {
      return loadCols[i];
    }

    [[nodiscard]] auto getLoadRow(const size_t i) const -> const expr& {
      return loadRows[i];
    }

    [[nodiscard]] auto getStoreCol(const size_t i) const -> const expr& {
      return storeCols[i];
    }

    [[nodiscard]] auto getStoreRow(const size_t i) const -> const expr& {
      return storeRows[i];
    }
  };

  uint16_t numQubits = 0;
  uint16_t numStages = 0;
  std::optional<uint16_t> numTransfers = std::nullopt;
  std::vector<Stage> stages;
  std::vector<expr> transfers;
  std::vector<expr> gates;

  /// Initializes the variables for all stages and all qubits
  auto initVariables() -> void;

  /* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
   * EXPLANATION OF CONSTRAINTS
   *
   * For a detailed explanation of all constraints, please refer to the
   * corresponding article "Optimal State Preparation for Logical Arrays on
   * Zoned Neutral Atom Quantum Computers" that can be obtained under the
   * following link:
   * https://www.cda.cit.tum.de/files/eda/2025_date_optimal_state_preparation_for_logical_arrays_on_zoned_neutral_atom_quantum_computers.pdf
   * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
  /// Return constraints ensuring that exactly @code numTransfers@endcode
  /// transfers take place
  [[nodiscard]] auto getExactNumTransfersConstraints() const
      -> std::vector<expr>;

  /// Returns the constraint @code (x_t^(q0) = x_t^(q1)) ∧ (y_t^(q0) = y_t^(q1))
  /// @endcode
  [[nodiscard]] auto getHaveSamePositionConstraint(uint16_t q0, uint16_t q1,
                                                   uint16_t t) const -> expr;

  /// Returns the constraint @code (x_t^(q0) ≠ x_t^(q1)) ∨ (y_t^(q0) ≠ y_t^(q1))
  /// @endcode
  [[nodiscard]] auto
  getHaveDifferentPositionConstraint(uint16_t q0, uint16_t q1, uint16_t t) const
      -> expr;

  /// Return constraints ensuring that the qubits is in the entangling zone at
  /// stage t
  [[nodiscard]] auto getAffectedByRydbergBeamConstraint(uint16_t q,
                                                        uint16_t t) const
      -> expr;

  /// Return constraints ensuring that the qubits is in the entangling zone at
  /// stage t
  [[nodiscard]] auto getShieldedFromRydbergBeamConstraint(uint16_t q,
                                                          uint16_t t) const
      -> expr;

  /// Returns a vector of constraints ensuring that transition from a Rydberg
  /// stage to the next stage is valid
  [[nodiscard]] auto getValidRydbergTransitionConstraints(uint16_t t) const
      -> std::vector<expr>;

  /// Returns a vector of constraints ensuring that transition from a Transfer
  /// stage to the next stage is valid
  [[nodiscard]] auto getValidTransferTransitionConstraints(uint16_t t) const
      -> std::vector<expr>;

  /**
   * @brief Returns the constraints extracted from the quantum circuit to ensure
   * execution of each gate and not more.
   * @details Creates the variables @code gate_i@endcode for every gate between
   * the qubits @code q0, q1@endcode and returns the following constraints:
   * @code
   * (0 ≤ gate_i) ∧ (gate_i < numStages) for all i
   *
   * (gate_i = t) ⟷ rydbergStage(t) ∧ haveSamePosition(q0, q1, t) for all i, t
   * and (q0, q1) is a gate_i
   *
   * rydbergStage(t) ⟶ haveDifferentPosition(q, q', t) for all q, q', t where
   * (q, q') is not a gate
   * @endcode
   * @return a vector of the constraints described above
   */
  [[nodiscard]] auto getCircuitExecutionConstraints(
      const std::vector<std::pair<qc::Qubit, qc::Qubit>>& ops,
      bool mindOpsOrder, bool shieldIdleAtoms) -> std::vector<expr>;

  /// Returns a constraint expressing that this stage is a Rydberg stage, that
  /// is if
  /// @code numTransfers_{t-1} = numTransfers_t @endcode
  [[nodiscard]] auto getRydbergStageConstraint(uint16_t t) const -> expr;

  /// Returns a constraint expressing that this stage is a Transfer stage, that
  /// is if
  /// @code numTransfers_{t-1} + 1 = numTransfers_t @endcode
  [[nodiscard]] auto getTransferStageConstraint(uint16_t t) const -> expr;

  /// Returns constraints ensuring that the state at the given stage is valid
  [[nodiscard]] auto getValidStageConstraints(uint16_t t) const
      -> std::vector<expr>;

public:
  /**
   * @brief Construct a new NASolver object with the given parameters that
   * define the abstraction of the 2D grid used by the solver.
   * @param newMaxX is the maximal discrete x-coordinate of an interaction site
   * @param newMaxY is the maximal discrete y-coordinate of an interaction site
   * @param newMaxC is the maximal index of an AOD column, i.e., it limits the
   * number of AOD columns
   * @param newMaxR is the maximal index of an AOD row, i.e., it limits the
   * number of AOD rows
   * @param newMaxHOffset is the maximal horizontal offset from the SLM trap
   * @param newMaxVOffset is the maximal vertical offset from the SLM trap
   * @param newMaxHDist is the maximal horizontal distance between two atoms in
   * order to interact
   * @param newMaxVDist is the maximal vertical distance between two atoms in
   * order to interact
   * @param newMinEntanglingY is the minimal y-coordinate of the entangling
   * zone, i.e., all discrete y-coordinates smaller than this value are
   * considered to be in the top storage zone. If this value is 0, there is no
   * top storage zone.
   * @param newMaxEntanglingY is the maximal y-coordinate of the entangling
   * zone, i.e., all discrete y-coordinates greater than this value are
   * considered to be in the bottom storage zone. If this value is maxY, there
   * is no bottom storage zone.
   * @throws illegal_argument if newMinEntanglingY > newMaxEntanglingY
   */
  [[nodiscard]] NASolver(uint16_t newMaxX, uint16_t newMaxY, uint16_t newMaxC,
                         uint16_t newMaxR, uint16_t newMaxHOffset,
                         uint16_t newMaxVOffset, uint16_t newMaxHDist,
                         uint16_t newMaxVDist, uint16_t newMinEntanglingY,
                         uint16_t newMaxEntanglingY);
  [[nodiscard]] NASolver(const NASolver& other) = default;
  [[nodiscard]] auto operator=(const NASolver& other) -> NASolver& = default;
  virtual ~NASolver() = default;

  /// This struct wraps the result of the solver
  struct Result {
  public:
    /// The types for the members of the result is chosen to be compatible with
    /// what Z3 returns by default
    struct Qubit {
    public:
      /// discrete x-coordinate of the site the atom is located in
      uint32_t x;
      /// discrete y-coordinate of the site the atom is located in
      uint32_t y;
      /// boolean variable to indicate whether the atom is loaded in an AOD,
      /// SLM otherwise
      bool a;
      /// if the atom is loaded in an AOD, this is the index of the AOD column,
      /// otherwise it has no meaning
      uint32_t c;
      /// if the atom is loaded in an AOD, this is the index of the AOD row,
      /// otherwise it has no meaning
      uint32_t r;
      /// denotes the horizontal offset from the SLM trap if the atom is loaded
      /// in an AOD
      int32_t h;
      /// denotes the vertical offset from the SLM trap if the atom is loaded in
      /// an AOD
      int32_t v;

      [[nodiscard]] static auto fromJSON(const nlohmann::json& json) -> Qubit;

      [[nodiscard]] auto json() const -> nlohmann::json;
      [[nodiscard]] auto operator==(const Qubit& other) const -> bool;
    };

    struct Gate {
    public:
      uint16_t stage = 0;
      std::pair<qc::Qubit, qc::Qubit> qubits;

      [[nodiscard]] static auto fromJSON(const nlohmann::json& json) -> Gate;

      [[nodiscard]] auto json() const -> nlohmann::json;

      [[nodiscard]] auto operator==(const Gate& other) const -> bool;
    };

    struct Stage {
    public:
      bool rydberg = true;
      std::vector<Qubit> qubits;
      std::vector<Gate> gates;

      [[nodiscard]] static auto fromJSON(const nlohmann::json& json) -> Stage;

      [[nodiscard]] auto json() const -> nlohmann::json;

      [[nodiscard]] auto operator==(const Stage& other) const -> bool;
    };

    bool sat = false;
    std::vector<Stage> stages;
    // Attributes required for the CodeGenerator to reconstruct the abstraction
    // used by the solver
    uint16_t minEntanglingY = 0;
    uint16_t maxEntanglingY = 0;
    uint16_t maxHOffset = 0;
    uint16_t maxVOffset = 0;

    [[nodiscard]] static auto fromJSON(const nlohmann::json& json) -> Result;

    [[nodiscard]] auto json() const -> nlohmann::json;

    [[nodiscard]] auto operator==(const Result& other) const -> bool;
  };

  /**
   * @brief The core function of the solver that solves one instance of the
   * problem.
   * @details The solver takes a list of operations and returns a list of stages
   * where each stage contains the location of all atoms and the gates that
   * should be executed in this stage. The solver takes several parameters to
   * configure the problem that are explained in the following.
   * @param ops a list of entangling operations represented as a list of qubit
   * pairs
   * @param newNumQubits the overall number of qubits in the quantum circuit
   * @param newNumStages the exact number of stages the computation should be
   * divided into
   * @param newNumTransfers the number of stages that are transfer stages. This
   * is an optional parameter and if not set, the number of transfer stages is
   * variable and not fixed.
   * @param mindOpsOrder if true, the solver schedules the operations in the
   * order they are given in the list. If false, the solver is free to choose
   * the order of the operations.
   * @param shieldIdleQubits if true, the solver ensures that qubits that are
   * not involved in an operation are shielded from the Rydberg beam, i.e.,
   * moved to one storage zone. If there is no storage zone and this parameter
   * is true, the solver will return an exception.
   * @throws illegal_argument if there is no storage zone and shieldIdleQubits
   * is true
   */
  [[nodiscard]] auto
  solve(const std::vector<std::pair<qc::Qubit, qc::Qubit>>& ops,
        uint16_t newNumQubits, uint16_t newNumStages,
        std::optional<uint16_t> newNumTransfers = std::nullopt,
        bool mindOpsOrder = false, bool shieldIdleQubits = true) -> Result;

  /**
   * @brief Get the list of entangling operations that the solver takes as
   * input.
   *
   * @details The solver only considers the entangling operations of a circuit.
   * For that it receives a list of qubit pairs that represent each one
   * entangling operation. This function generates this list from a given
   * QuantumComputation and a FullOpType that specifies the entangling
   * operation.
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
   * @param circ
   * @param opType
   * @param ctrls
   * @param quiet
   * @return
   */
  [[nodiscard]] static auto
  getOpsForSolver(const qc::QuantumComputation& circ, qc::OpType opType,
                  std::size_t ctrls, bool quiet = false)
      -> std::vector<std::pair<qc::Qubit, qc::Qubit>>;
};

struct ExprHash {
  auto operator()(const expr& e) const -> uint32_t { return e.hash(); }
};
} // namespace na
