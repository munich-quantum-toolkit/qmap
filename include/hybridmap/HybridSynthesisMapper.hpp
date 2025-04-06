//
// This file is part of the MQT QMAP library released under the MIT license.
// See README.md or go to https://github.com/cda-tum/qmap for more information.
//

#pragma once

#include "Definitions.hpp"
#include "HybridNeutralAtomMapper.hpp"
#include "NeutralAtomArchitecture.hpp"
#include "NeutralAtomUtils.hpp"
#include "hybridmap/NeutralAtomDefinitions.hpp"
#include "ir/QuantumComputation.hpp"

#include <cstddef>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace na {

/**
 * @brief Class to manage information exchange between the neutral atom mapper
 * and the ZX extraction.
 * @deteils This class is derived from the HybridNeutralAtomMapper and stores
 * all the information about the neutral atom hardware and the current status of
 * the mapping. The ZX (or another synthesis algorithm) can propose different
 * possible next synthesis steps, which are then evaluated by the
 * HybridNeutralAtomMapper regarding the "effort" to map this synthesis step. It
 * also provides additional functionality to exchange information between the ZX
 * and the HybridNeutralAtomMapper.
 *
 */
class HybridSynthesisMapper : public NeutralAtomMapper {
  using qcs = std::vector<qc::QuantumComputation>;

  qc::QuantumComputation synthesizedQc;
  Mapping originalMapping;
  /**
   * @brief Evaluates a single synthesis step proposed by the ZX extraction.
   * @details The effort is calculated by the NeutralAtomMapper, taking into
   * account the number of SWAP gates or shuttling moves and the time needed to
   * execute the mapped synthesis step.
   * @param qc The synthesis step to be evaluated.
   * @param completeRemap
   * @return The cost/effort to map the synthesis step.
   */
  qc::fp evaluateSynthesisStep(qc::QuantumComputation& qc,
                               bool completeRemap = false) const;

public:
  // Constructors
  HybridSynthesisMapper() = delete;
  explicit HybridSynthesisMapper(
      const NeutralAtomArchitecture& arch,
      const MapperParameters& params = MapperParameters())
      : NeutralAtomMapper(arch, params) {}

  // Functions

  /**
   * @brief Initializes the mapping with the given number of qubits and the
   * initial mapping.
   * @param nQubits The number of qubits to be mapped.
   * @param initialMapping The initial mapping to be used.
   */
  void initMapping(size_t nQubits,
                   InitialMapping initialMapping = InitialMapping::Identity) {
    mappedQc = qc::QuantumComputation(arch->getNpositions());
    synthesizedQc = qc::QuantumComputation(nQubits);
    mapping = Mapping(nQubits, initialMapping);
    originalMapping = mapping;
  }

  /**
   * @brief Returns the mapped QuantumComputation.
   * @return The mapped QuantumComputation.
   */
  void completeRemap() {
    this->map(synthesizedQc, originalMapping);
    this->convertToAod();
  }

  /**
   * @brief Returns the synthesized QuantumComputation with all gates but not
   * mapped to the hardware.
   * @return The synthesized QuantumComputation.
   */
  [[nodiscard]] qc::QuantumComputation getSynthesizedQc() const {
    return this->synthesizedQc;
  }

  /**
   * @brief Returns the synthesized QuantumComputation as a string.
   * @return The synthesized QuantumComputation as a string.
   */
  [[maybe_unused]] std::string getSynthesizedQcQASM() {
    std::stringstream ss;
    synthesizedQc.dumpOpenQASM(ss, false);
    return ss.str();
  }

  /**
   * @brief Saves the synthesized QuantumComputation to the given file.
   * @param filename The file to save the synthesized QuantumComputation to.
   */
  [[maybe_unused]] void saveSynthesizedQc(const std::string& filename) {
    std::ofstream ofs(filename);
    synthesizedQc.dumpOpenQASM(ofs, false);
    ofs.close();
  }

  /**
   * @brief Evaluates the synthesis steps proposed by the ZX extraction.
   * @param synthesisSteps The synthesis steps proposed by the ZX extraction.
   * @param completeRemap
   * @param alsoMap If true, the best synthesis step is directly mapped to the
   * hardware.
   * @return Returns a list of fidelities of the mapped synthesis steps.
   */
  std::vector<qc::fp> evaluateSynthesisSteps(qcs& synthesisSteps,
                                             bool completeRemap = false,
                                             bool alsoMap = false);

  /**
   * @brief Directly maps the given QuantumComputation to the hardware NOT
   * inserting SWAP gates or shuttling move operations.
   * @param qc The gates (QuantumComputation) to be mapped.
   */
  void appendWithoutMapping(const qc::QuantumComputation& qc);

  /**
   * @brief Appends the given QuantumComputation to the synthesized
   * QuantumComputation and maps the gates to the hardware.
   * @param qc The gates (QuantumComputation) to be appended and mapped.
   * @param completeRemap
   */
  void appendWithMapping(qc::QuantumComputation& qc,
                         bool completeRemap = false);

  /**
   * @brief Returns the current adjacency matrix of the neutral atom hardware.
   * @return The current adjacency matrix of the neutral atom hardware.
   */
  [[nodiscard]] AdjacencyMatrix getCircuitAdjacencyMatrix() const;

  /**
   * @brief Returns the maximum gate size of the neutral atom hardware.
   * @return The maximum gate size of the neutral atom hardware.
   */
  [[nodiscard]] size_t getMaxGateSize() const {
    return this->arch->getMaxGateSize();
  }
};
} // namespace na
