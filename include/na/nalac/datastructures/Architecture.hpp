#pragma once

#include "NADefinitions.hpp"
#include "ir/Definitions.hpp"
#include "ir/operations/OpType.hpp"
#include "na/nalac/datastructures/Configuration.hpp"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <istream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace na::nalac {

/// The scope of an operation (Global or Local)
enum class Scope : uint8_t { Global, Local };

/**
 * @brief Get the Scope of a gate from a string
 *
 * @param s the name
 * @return Type
 */
inline auto getScopeOfString(const std::string& s) -> Scope {
  std::string sLowerCase = s;
  std::transform(sLowerCase.begin(), sLowerCase.end(), sLowerCase.begin(),
                 ::tolower);
  if (sLowerCase == "global") {
    return Scope::Global;
  }

  if (sLowerCase == "local") {
    return Scope::Local;
  }

  std::stringstream ss;
  ss << "The scope " << s << " is not supported.";
  throw std::invalid_argument(ss.str());
}

/// For Indices
using Index = std::size_t;
/// For distance, those cannot be negative
using Distance = std::size_t;
/// The zones are just stored as int
using ZoneId = Index;
/// Any double-valued property
using Value = qc::fp;
/// Any information on numbers of something
using Number = std::int64_t;

class Architecture {
public:
  /**
   * @brief Struct to store the decoherence times of a neutral atom
   * architecture
   * @details
   * The decoherence times of a neutral atom architecture are:
   * - T1 [µs]
   * - T2 [µs]
   * - effective decoherence time [µs]
   */
  struct DecoherenceTimes {
    Value t1 = 0;
    Value t2 = 0;

    [[nodiscard]] auto tEff() const -> Value {
      if (t1 == 0 && t2 == 0) {
        return 0;
      }
      return t1 * t2 / (t1 + t2);
    }
    explicit operator double() const { return tEff(); }
  };
  /**
   * @brief Struct to store the properties of an operation.
   * @details Times are in µs, fidelities are in [0,1].
   */
  struct OperationProperties {
    Scope scope;                      // local or global
    std::unordered_set<ZoneId> zones; // the zones where the gate can be applied
    Value time;     // the time the gate takes to be applied in µs
    Value fidelity; // the fidelity of the gate
  };
  /**
   * @brief Struct to store the properties of a Shuttling operation (i.e. of the
   * AOD).
   * @details Times are in µs, fidelities are in [0,1], and velocities are in
   * µm/µs.
   */
  struct ShuttlingProperties {
    Index rows = 0;          // maximum number of rows in one AOD
    Index cols = 0;          // maximum number of columns in one AOD
    Number minX = 0;         // minimum x position of the AOD
    Number maxX = 0;         // maximum x position of the AOD
    Number minY = 0;         // minimum y position of the AOD
    Number maxY = 0;         // maximum y position of the AOD
    Value speed = 0;         // speed of the AOD in µm/µs
    Value fidelity = 1;      // fidelity during the shuttling
    Value loadTime = 0;      // time to activate the AOD in µs
    Value loadFidelity = 1;  // fidelity of the load
    Value storeTime = 0;     // time to deactivate the AOD in µs
    Value storeFidelity = 1; // fidelity of the store
  };
  struct ZoneProperties {
    std::string name;   // the name of the zone
    Number minX = 0;    // minimum x dimension
    Number maxX = 0;    // maximum x dimension
    Number minY = 0;    // minimum y dimension
    Number maxY = 0;    // maximum y dimension
    Value fidelity = 1; // fidelity during idling
  };

protected:
  std::string name; // the name of the architecture
  std::vector<ZoneProperties>
      zones; // a mapping from zones (int) to their name from the config
  std::vector<Point> sites; // a vector of sites
  std::unordered_map<std::pair<qc::OpType, std::size_t>, OperationProperties>
      gateSet; // all possible operations by their type, i.e. gate set
  DecoherenceTimes decoherenceTimes;          // the decoherence characteristic
  std::vector<ShuttlingProperties> shuttling; // all properties regarding AODs
  Distance minAtomDistance =
      0; // minimal distance that must be kept between atoms
  Distance interactionRadius = 0; // the Rydberg radius
  Distance noInteractionRadius =
      0; // sufficient radius to avoid Rydberg interaction
  std::vector<ZoneId> initialZones; // the zones where the atoms are initially

public:
  Architecture() = default;

  /**
   * @brief Import a new architecture from a file.
   *
   * @param jsonFn The path to the JSON file
   * @param csvFn The path to the CSV file
   */
  Architecture(const std::string& jsonFn, const std::string& csvFn) {
    fromFile(jsonFn, csvFn);
  }
  Architecture(std::istream& jsonS, std::istream& csvS) {
    fromFileStream(jsonS, csvS);
  }

  auto fromFile(const std::string& jsonFn, const std::string& csvFn) -> void;
  auto fromFileStream(std::istream& jsonS, std::istream& csvS) -> void;
  [[nodiscard]] auto getName() const -> const std::string& { return name; }
  [[nodiscard]] auto getNZones() const -> Index { return zones.size(); }
  [[nodiscard]] auto getZoneLabel(const Index& i) const -> const std::string& {
    return zones[i].name;
  }
  [[nodiscard]] auto getInitialZones() const -> const std::vector<ZoneId>& {
    return initialZones;
  }
  [[nodiscard]] auto getNSites() const -> Index { return sites.size(); }
  [[nodiscard]] auto getPositionOfSite(const Index& i) const -> const Point& {
    return sites[i];
  }
  [[nodiscard]] auto getDecoherenceTimes() const -> const DecoherenceTimes& {
    return decoherenceTimes;
  }
  [[nodiscard]] auto getNShuttlingUnits() const -> Index {
    return shuttling.size();
  }
  [[nodiscard]] auto getPropertiesOfShuttlingUnit(const Index& i) const
      -> const ShuttlingProperties& {
    return shuttling[i];
  }
  [[nodiscard]] auto getMinAtomDistance() const -> Distance {
    return minAtomDistance;
  }
  [[nodiscard]] auto getInteractionRadius() const -> Distance {
    return interactionRadius;
  }
  [[nodiscard]] auto getNoInteractionRadius() const -> Distance {
    return noInteractionRadius;
  }
  [[nodiscard]] auto getPropertiesOfZone(const ZoneId& zone) const
      -> const ZoneProperties& {
    return zones[zone];
  }
  [[nodiscard]] auto getPropertiesOfOperation(const qc::OpType t,
                                              const std::size_t ctrls) const
      -> const OperationProperties& {
    if (const auto& it = gateSet.find({t, ctrls}); it != gateSet.end()) {
      return it->second;
    }
    std::stringstream ss;
    ss << "The operation " << t << " is not supported.";
    throw std::invalid_argument(ss.str());
  }
  /**
   * @brief Returns the distance between two sites.
   *
   * @param i address of first site
   * @param j address of second site
   * @return the distance in µm
   */
  [[nodiscard]] auto getDistance(const Index& i, const Index& j) const
      -> uint64_t {
    return (getPositionOfSite(j) - getPositionOfSite(i)).length();
  }
  [[nodiscard]] auto getZoneAt(const Point& p) const -> ZoneId;
  [[nodiscard]] auto getZoneOfSite(const Index& i) const -> ZoneId {
    return getZoneAt(getPositionOfSite(i));
  }
  /// Checks whether the gate can be applied at all.
  [[nodiscard]] auto isAllowedLocally(qc::OpType t, std::size_t ctrls) const
      -> bool;
  /// Checks whether the gate can be applied (locally) in this zone.
  [[nodiscard]] auto isAllowedLocally(qc::OpType t, std::size_t ctrls,
                                      const ZoneId& zone) const -> bool;
  /// Checks whether the gate can be applied (locally) on this qubit.
  [[nodiscard]] auto isAllowedLocallyAt(qc::OpType t, std::size_t ctrls,
                                        const Point& p) const -> bool;
  /// Checks whether the gate is a global gate for this ZoneId.
  [[nodiscard]] auto isAllowedGlobally(qc::OpType t, std::size_t ctrls) const
      -> bool;
  [[nodiscard]] auto isAllowedGlobally(qc::OpType t, std::size_t ctrls,
                                       const ZoneId& zone) const -> bool;
  [[nodiscard]] auto getNrowsInZone(const ZoneId& z) const -> Index;
  [[nodiscard]] auto getNColsInZone(const ZoneId& z) const -> Index;
  [[nodiscard]] auto getSitesInRow(const ZoneId& z, const Index& row) const
      -> std::vector<Index>;
  [[nodiscard]] auto getNearestXLeft(const Number& x, const ZoneId& z,
                                     bool proper = true) const -> Number;
  [[nodiscard]] auto getNearestXRight(const Number& x, const ZoneId& z,
                                      bool proper = true) const -> Number;
  [[nodiscard]] auto hasSiteLeft(const Point& p, bool proper = false,
                                 bool sameZoneId = false) const
      -> std::pair<std::vector<Point>::const_reverse_iterator, bool>;
  [[nodiscard]] auto hasSiteRight(const Point& p, bool proper = false,
                                  bool sameZoneId = false) const
      -> std::pair<std::vector<Point>::const_iterator, bool>;
  [[nodiscard]] auto hasSiteUp(const Point& p, bool proper = false,
                               bool sameZoneId = false) const
      -> std::pair<std::vector<Point>::const_reverse_iterator, bool>;
  [[nodiscard]] auto hasSiteDown(const Point& p, bool proper = false,
                                 bool sameZoneId = false) const
      -> std::pair<std::vector<Point>::const_iterator, bool>;
  [[nodiscard]] auto getNearestSiteLeft(const Point& p, bool proper = false,
                                        bool sameZoneId = false) const
      -> std::optional<Index>;
  [[nodiscard]] auto getNearestSiteRight(const Point& p, bool proper = false,
                                         bool sameZoneId = false) const
      -> std::optional<Index>;
  [[nodiscard]] auto getNearestSiteUp(const Point& p, bool proper = false,
                                      bool sameZoneId = false) const
      -> std::optional<Index>;
  [[nodiscard]] auto getNearestSiteDown(const Point& p, bool proper = false,
                                        bool sameZoneId = false) const
      -> std::optional<Index>;
  [[nodiscard]] auto getNearestSiteUpRight(const Point& p, bool proper = false,
                                           bool sameZoneId = false) const
      -> std::optional<Index>;
  [[nodiscard]] auto getNearestSiteUpLeft(const Point& p, bool proper = false,
                                          bool sameZoneId = false) const
      -> std::optional<Index>;
  [[nodiscard]] auto getNearestSiteDownLeft(const Point& p, bool proper = false,
                                            bool sameZoneId = false) const
      -> std::optional<Index>;
  [[nodiscard]] auto getNearestSiteDownRight(const Point& p,
                                             bool proper = false,
                                             bool sameZoneId = false) const
      -> std::optional<Index>;
  [[nodiscard]] auto getSiteAt(const Point& p) const -> std::optional<Index>;
  [[nodiscard]] auto getSitesInZone(const ZoneId& z) const
      -> std::vector<Index>;
  [[nodiscard]] auto withConfig(const Configuration& config) const
      -> Architecture;
  [[nodiscard]] auto getPositionOffsetBy(const Point& p, const Number& rows,
                                         const Number& cols) const -> Point;

private:
  [[nodiscard]] auto getRowsInZone(const ZoneId& z) const
      -> std::vector<Number>;
  [[nodiscard]] auto getColsInZone(const ZoneId& z) const
      -> std::vector<Number>;
};
} // namespace na::nalac
