//
// This file is part of the MQT QMAP library released under the MIT license.
// See README.md or go to https://github.com/cda-tum/qmap for more information.
//

#pragma once

#include "configuration/AvailableArchitecture.hpp"
#include "ir/operations/OpType.hpp"
#include "utils.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <map>
#include <nlohmann/json.hpp>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

constexpr std::uint8_t GATES_OF_BIDIRECTIONAL_SWAP = 3U;
constexpr std::uint8_t GATES_OF_UNIDIRECTIONAL_SWAP = 7U;
constexpr std::uint8_t GATES_OF_DIRECTION_REVERSE = 4U;

constexpr std::uint32_t COST_SINGLE_QUBIT_GATE = 1;
constexpr std::uint32_t COST_CNOT_GATE = 10;
constexpr std::uint32_t COST_MEASUREMENT = 10;
constexpr std::uint32_t COST_UNIDIRECTIONAL_SWAP =
    3 * COST_CNOT_GATE + 4 * COST_SINGLE_QUBIT_GATE;
constexpr std::uint32_t COST_BIDIRECTIONAL_SWAP = 3 * COST_CNOT_GATE;
constexpr std::uint32_t COST_DIRECTION_REVERSE = 4 * COST_SINGLE_QUBIT_GATE;

class Architecture {
public:
  class Properties {
  protected:
    template <class KeyType, class ValueType> class Property {
    public:
      Property() = default;

      [[nodiscard]] auto& get(const KeyType& key) { return props[key]; }
      [[nodiscard]] const auto& get(const KeyType& key) const {
        return props.at(key);
      }
      [[nodiscard]] const auto& get() const { return props; }
      void set(const KeyType& key, const ValueType& value) {
        props[key] = value;
      }
      [[nodiscard, gnu::pure]] bool available(const KeyType& key) const {
        return props.find(key) != props.end();
      }
      void clear() { props.clear(); }
      [[nodiscard]] bool empty() const { return props.empty(); }

    protected:
      std::map<KeyType, ValueType> props{};
    };

  public:
    Properties() = default;

    [[nodiscard]] std::string getName() const { return name; }
    void setName(const std::string& propertiesName) { name = propertiesName; }

    [[nodiscard]] std::uint16_t getNqubits() const { return nq; }
    void setNqubits(std::uint16_t nqs) { nq = nqs; }

    Property<std::uint16_t, Property<qc::OpType, double>> singleQubitErrorRate;
    Property<std::uint16_t,
             Property<std::uint16_t, Property<qc::OpType, double>>>
        twoQubitErrorRate;
    Property<std::uint16_t, double> readoutErrorRate;
    Property<std::uint16_t, double> t1Time;
    Property<std::uint16_t, double> t2Time;
    Property<std::uint16_t, double> qubitFrequency;
    Property<std::uint16_t, std::string> calibrationDate;

    // convenience functions
    void setSingleQubitErrorRate(std::uint16_t qubit,
                                 const std::string& operation,
                                 double errorRate) {
      singleQubitErrorRate.get(qubit).set(qc::opTypeFromString(operation),
                                          errorRate);
    }
    [[nodiscard]] double
    getSingleQubitErrorRate(std::uint16_t qubit,
                            const std::string& operation) const {
      return singleQubitErrorRate.get(qubit).get(
          qc::opTypeFromString(operation));
    }
    [[nodiscard]] double
    getAverageSingleQubitErrorRate(const std::uint16_t qubit) const {
      double avgErrorRate = 0.0;
      for (const auto& [opType, error] :
           singleQubitErrorRate.get(qubit).get()) {
        avgErrorRate += error;
      }
      return avgErrorRate /
             static_cast<double>(singleQubitErrorRate.get(qubit).get().size());
    }

    void setTwoQubitErrorRate(std::uint16_t qubit1, std::uint16_t qubit2,
                              double errorRate,
                              const std::string& operation = "cx") {
      twoQubitErrorRate.get(qubit1).get(qubit2).set(
          qc::opTypeFromString(operation), errorRate);
    }
    [[nodiscard]] double
    getTwoQubitErrorRate(std::uint16_t qubit1, std::uint16_t qubit2,
                         const std::string& operation = "cx") const {
      return twoQubitErrorRate.get(qubit1).get(qubit2).get(
          qc::opTypeFromString(operation));
    }
    [[nodiscard]] bool
    twoQubitErrorRateAvailable(std::uint16_t qubit1, std::uint16_t qubit2,
                               const std::string& operation = "cx") const {
      return twoQubitErrorRate.available(qubit1) &&
             twoQubitErrorRate.get(qubit1).available(qubit2) &&
             twoQubitErrorRate.get(qubit1).get(qubit2).available(
                 qc::opTypeFromString(operation));
    }

    void clear() {
      singleQubitErrorRate.clear();
      twoQubitErrorRate.clear();
      readoutErrorRate.clear();
      t1Time.clear();
      t2Time.clear();
      qubitFrequency.clear();
      calibrationDate.clear();
    }

    [[nodiscard]] bool empty() const {
      return singleQubitErrorRate.empty() && twoQubitErrorRate.empty() &&
             readoutErrorRate.empty() && t1Time.empty() && t2Time.empty() &&
             qubitFrequency.empty() && calibrationDate.empty();
    }

    [[nodiscard]] nlohmann::basic_json<> json() const {
      nlohmann::basic_json json;
      if (empty()) {
        return json;
      }

      json["name"] = name;
      json["qubits"] = {};
      for (std::uint16_t i = 0U; i < nq; ++i) {
        auto& qubitProperties = json["qubits"][std::to_string(i)];

        if (singleQubitErrorRate.available(i)) {
          auto& singleQubitErrorRates =
              qubitProperties["single_qubit_error_rate"];
          for (const auto& [operation, error] :
               singleQubitErrorRate.get(i).get()) {
            singleQubitErrorRates[qc::toString(operation)] = error;
          }
        }

        if (t1Time.available(i)) {
          qubitProperties["t1_time"] = t1Time.get(i);
        }
        if (t2Time.available(i)) {
          qubitProperties["t2_time"] = t2Time.get(i);
        }
        if (qubitFrequency.available(i)) {
          qubitProperties["frequency"] = qubitFrequency.get(i);
        }
        if (calibrationDate.available(i)) {
          qubitProperties["calibration_date"] = calibrationDate.get(i);
        }
        if (readoutErrorRate.available(i)) {
          qubitProperties["readout_error_rate"] = readoutErrorRate.get(i);
        }

        if (twoQubitErrorRate.available(i)) {
          auto& twoQubitErrorRates = qubitProperties["two_qubit_error_rate"];
          for (const auto& [qubit2, errorRates] :
               twoQubitErrorRate.get(i).get()) {
            const std::string pair =
                '(' + std::to_string(i) + ',' + std::to_string(qubit2) + ')';
            auto& qubits = twoQubitErrorRates[pair];
            for (const auto& [operation, error] : errorRates.get()) {
              qubits[qc::toString(operation)] = error;
            }
          }
        }
      }

      return json;
    }
    [[nodiscard]] std::string toString() const { return json().dump(2); }

  protected:
    std::string name;
    std::uint16_t nq{};
  };

  void loadCouplingMap(std::istream& is);
  void loadCouplingMap(const std::string& filename);
  void loadCouplingMap(std::uint16_t nQ, const CouplingMap& cm);
  void loadCouplingMap(AvailableArchitecture architecture);
  void loadProperties(std::istream& is);
  void loadProperties(const std::string& filename);
  void loadProperties(const Properties& props);

  Architecture() = default;
  explicit Architecture(const std::string& cmFilename) {
    loadCouplingMap(cmFilename);
  }
  Architecture(const std::string& cmFilename, const std::string& propsFilename)
      : Architecture(cmFilename) {
    loadProperties(propsFilename);
  }

  Architecture(std::uint16_t nQ, const CouplingMap& cm);
  Architecture(std::uint16_t nQ, const CouplingMap& cm,
               const Properties& props);

  [[nodiscard]] std::uint16_t getNqubits() const { return nqubits; }
  void setNqubits(std::uint16_t nQ) { nqubits = nQ; }

  [[nodiscard]] const std::string& getName() const { return name; }
  void setName(const std::string& architectureName) { name = architectureName; }

  [[nodiscard]] const CouplingMap& getCouplingMap() const {
    return couplingMap;
  }
  [[nodiscard]] CouplingMap& getCouplingMap() { return couplingMap; }

  void setCouplingMap(const CouplingMap& cm) {
    couplingMap = cm;
    createDistanceTable();
  }

  [[nodiscard]] bool
  isEdgeConnected(const Edge& edge, const bool considerDirection = true) const {
    if (considerDirection) {
      return couplingMap.find(edge) != couplingMap.end();
    }
    return couplingMap.find(edge) != couplingMap.end() ||
           couplingMap.find({edge.second, edge.first}) != couplingMap.end();
  }

  [[nodiscard]] bool isEdgeBidirectional(const Edge& edge) const {
    return couplingMap.find(edge) != couplingMap.end() &&
           couplingMap.find({edge.second, edge.first}) != couplingMap.end();
  }

  [[nodiscard]] const Matrix&
  getDistanceTable(bool includeReversalCost = true) const {
    if (includeReversalCost) {
      return distanceTableReversals;
    }
    return distanceTable;
  }

  [[nodiscard]] const Properties& getProperties() const { return properties; }

  [[nodiscard]] Properties& getProperties() { return properties; }

  void setProperties(const Properties& props) {
    properties = props;
    createFidelityTable();
  }

  [[nodiscard]] bool isFidelityAvailable() const { return fidelityAvailable; }

  [[nodiscard]] const std::vector<Matrix>& getFidelityDistanceTables() const {
    if (!fidelityAvailable) {
      throw QMAPException("No fidelity data available.");
    }
    return fidelityDistanceTables;
  }

  [[nodiscard]] const Matrix&
  getFidelityDistanceTable(std::size_t skipEdges) const {
    if (!fidelityAvailable) {
      throw QMAPException("No fidelity data available.");
    }
    if (skipEdges >= fidelityDistanceTables.size()) {
      const static Matrix DEFAULT_MATRIX(nqubits,
                                         std::vector<double>(nqubits, 0.0));
      return DEFAULT_MATRIX;
    }
    return fidelityDistanceTables.at(skipEdges);
  }

  [[nodiscard]] const Matrix& getFidelityDistanceTable() const {
    return getFidelityDistanceTable(0);
  }

  [[nodiscard]] double fidelityDistance(std::uint16_t q1, std::uint16_t q2,
                                        std::size_t skipEdges) const {
    if (!fidelityAvailable) {
      throw QMAPException("No fidelity data available.");
    }
    if (q1 >= nqubits) {
      throw QMAPException("Qubit out of range.");
    }
    if (q2 >= nqubits) {
      throw QMAPException("Qubit out of range.");
    }
    if (skipEdges >= fidelityDistanceTables.size()) {
      return 0.;
    }
    return fidelityDistanceTables.at(skipEdges).at(q1).at(q2);
  }

  [[nodiscard]] double fidelityDistance(std::uint16_t q1,
                                        std::uint16_t q2) const {
    return fidelityDistance(q1, q2, 0);
  }

  [[nodiscard]] const Matrix& getFidelityTable() const {
    if (!fidelityAvailable) {
      throw QMAPException("No fidelity data available.");
    }
    return fidelityTable;
  }

  [[nodiscard]] const std::vector<double>& getSingleQubitFidelities() const {
    if (!fidelityAvailable) {
      throw QMAPException("No fidelity data available.");
    }
    return singleQubitFidelities;
  }

  [[nodiscard]] const std::vector<double>& getSingleQubitFidelityCosts() const {
    if (!fidelityAvailable) {
      throw QMAPException("No fidelity data available.");
    }
    return singleQubitFidelityCosts;
  }

  [[nodiscard]] double getSingleQubitFidelityCost(std::uint16_t qbit) const {
    if (!fidelityAvailable) {
      throw QMAPException("No fidelity data available.");
    }
    if (qbit >= nqubits) {
      throw QMAPException("Qubit out of range.");
    }
    return singleQubitFidelityCosts.at(qbit);
  }

  [[nodiscard]] const Matrix& getTwoQubitFidelityCosts() const {
    if (!fidelityAvailable) {
      throw QMAPException("No fidelity data available.");
    }
    return twoQubitFidelityCosts;
  }

  [[nodiscard]] double getTwoQubitFidelityCost(std::uint16_t q1,
                                               std::uint16_t q2) const {
    if (!fidelityAvailable) {
      throw QMAPException("No fidelity data available.");
    }
    if (q1 >= nqubits) {
      throw QMAPException("Qubit out of range.");
    }
    if (q2 >= nqubits) {
      throw QMAPException("Qubit out of range.");
    }
    return twoQubitFidelityCosts.at(q1).at(q2);
  }

  [[nodiscard]] const Matrix& getSwapFidelityCosts() const {
    if (!fidelityAvailable) {
      throw QMAPException("No fidelity data available.");
    }
    return swapFidelityCosts;
  }

  [[nodiscard]] double getSwapFidelityCost(std::uint16_t q1,
                                           std::uint16_t q2) const {
    if (!fidelityAvailable) {
      throw QMAPException("No fidelity data available.");
    }
    if (q1 >= nqubits) {
      throw QMAPException("Qubit out of range.");
    }
    if (q2 >= nqubits) {
      throw QMAPException("Qubit out of range.");
    }
    return swapFidelityCosts.at(q1).at(q2);
  }

  /** true if the coupling map contains no unidirectional edges */
  [[nodiscard]] bool bidirectional() const { return isBidirectional; }

  /** true if the coupling map contains no bidirectional edges */
  [[nodiscard]] bool unidirectional() const { return isUnidirectional; }

  [[nodiscard]] bool isArchitectureAvailable() const {
    return !(name.empty()) && nqubits != 0;
  }
  [[nodiscard]] bool isCalibrationDataAvailable() const {
    return !(name.empty()) && !properties.empty();
  }

  void reset() {
    name = "";
    nqubits = 0;
    couplingMap.clear();
    distanceTable.clear();
    distanceTableReversals.clear();
    isBidirectional = true;
    isUnidirectional = true;
    properties.clear();
    fidelityAvailable = false;
    fidelityTable.clear();
    singleQubitFidelities.clear();
    singleQubitFidelityCosts.clear();
    twoQubitFidelityCosts.clear();
    swapFidelityCosts.clear();
    fidelityDistanceTables.clear();
  }

  [[nodiscard]] double distance(std::uint16_t control, std::uint16_t target,
                                bool includeReversalCost = true) const {
    if (includeReversalCost) {
      return distanceTableReversals.at(control).at(target);
    }
    return distanceTable.at(control).at(target);
  }

  [[nodiscard]] std::set<std::uint16_t> getQubitSet() const {
    std::set<std::uint16_t> result{};
    for (std::uint16_t i = 0; i < nqubits; ++i) {
      result.insert(result.end(),
                    i); // should be constant with gcc, or at most O(nqubits)
    }
    return result;
  }

  std::uint64_t minimumNumberOfSwaps(std::vector<std::uint16_t>& permutation,
                                     std::int64_t limit = -1);
  void minimumNumberOfSwaps(std::vector<std::uint16_t>& permutation,
                            std::vector<Edge>& swaps);

  struct Node {
    std::uint64_t nswaps = 0U;
    std::vector<Edge> swaps;
    std::unordered_map<std::uint16_t, std::uint16_t> permutation;

    void print(std::ostream& out) {
      out << swaps.size() << ": ";
      for (const auto& p : permutation) {
        out << p.first << "->" << p.second << " ";
      }
      out << " | ";
      for (const auto& swap : swaps) {
        out << swap.first << "<->" << swap.second << " ";
      }
      out << '\n';
    }
  };

  [[nodiscard]] std::size_t getCouplingLimit() const;
  [[nodiscard]] std::size_t
  getCouplingLimit(const std::set<std::uint16_t>& qubitChoice) const;

  void getHighestFidelityCouplingMap(std::uint16_t subsetSize,
                                     CouplingMap& reducedMap) const;
  [[nodiscard]] std::vector<QubitSubset>
  getAllConnectedSubsets(std::uint16_t subsetSize) const;
  void getReducedCouplingMaps(std::uint16_t subsetSize,
                              std::vector<CouplingMap>& couplingMaps) const;
  void getReducedCouplingMap(const QubitSubset& qubitChoice,
                             CouplingMap& reducedMap) const;
  [[nodiscard]] static double
  getAverageArchitectureFidelity(const CouplingMap& cm,
                                 const QubitSubset& qubitChoice,
                                 const Properties& props);

  [[nodiscard]] static QubitSubset getQubitSet(const CouplingMap& cm);
  [[nodiscard]] static std::vector<std::uint16_t>
  getQubitList(const CouplingMap& cm) {
    const auto qubitSet = getQubitSet(cm);
    return {qubitSet.begin(), qubitSet.end()};
  }

  static bool isConnected(const QubitSubset& qubitChoice,
                          const CouplingMap& reducedCouplingMap);

  static void printCouplingMap(const CouplingMap& cm, std::ostream& os);

protected:
  std::string name;
  std::uint16_t nqubits = 0;
  CouplingMap couplingMap;

  /** true if the coupling map contains no unidirectional edges */
  bool isBidirectional = true;
  /** true if the coupling map contains no bidirectional edges */
  bool isUnidirectional = true;
  // by this definition the empty coupling map is both bidirectional and
  // unidirectional, and coupling maps containing both bidirectional and
  // unidirectional edges are neither bidirectional nor unidirectional

  Matrix distanceTable;
  Matrix distanceTableReversals;
  std::vector<std::pair<std::int16_t, std::int16_t>> teleportationQubits;
  Properties properties;
  bool fidelityAvailable = false;
  Matrix fidelityTable;
  std::vector<double> singleQubitFidelities;
  std::vector<double> singleQubitFidelityCosts;
  Matrix twoQubitFidelityCosts;
  Matrix swapFidelityCosts;
  std::vector<Matrix> fidelityDistanceTables;

  void createDistanceTable();
  void createFidelityTable();

  // added for teleportation

  static std::size_t findCouplingLimit(const CouplingMap& cm,
                                       std::uint16_t nQubits);
  static std::size_t
  findCouplingLimit(const CouplingMap& cm, std::uint16_t nQubits,
                    const std::set<std::uint16_t>& qubitChoice);
  static void findCouplingLimit(
      std::uint16_t node, std::uint16_t curSum,
      const std::vector<std::unordered_set<std::uint16_t>>& connections,
      std::vector<std::uint16_t>& d, std::vector<bool>& visited);
};
