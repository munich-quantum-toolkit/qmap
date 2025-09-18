#include "na/nalac/datastructures/Architecture.hpp"

#include "ir/Definitions.hpp"
#include "ir/operations/OpType.hpp"
#include "na/nalac/datastructures/Configuration.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <fstream>
#include <istream>
#include <iterator>
#include <limits>
#include <map>
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace na::nalac {

auto Architecture::fromFile(const std::string& jsonFn, const std::string& csvFn)
    -> void {
  std::ifstream jsonS(jsonFn);
  if (!jsonS.good()) {
    std::stringstream ss;
    ss << "Could not open JSON file " << jsonFn << ".";
    throw std::runtime_error(ss.str());
  }
  std::ifstream csvS(csvFn);
  if (!csvS.good()) {
    std::stringstream ss;
    ss << "Could not open CSV file " << csvFn << ".";
    throw std::runtime_error(ss.str());
  }
  *this = Architecture(jsonS, csvS);
}

auto Architecture::fromFileStream(std::istream& jsonS, std::istream& csvS)
    -> void {
  nlohmann::basic_json data;
  // load CSV
  // auxiliary variables
  std::string line;
  std::size_t lineno = 1;
  try {
    csvS >> line; // skip first line, i.e. header
    while (csvS >> line) {
      ++lineno;
      std::stringstream lineStream(line); // make a stream of this line
      std::string sX;
      std::string sY;

      std::getline(lineStream, sX, ',');
      std::getline(lineStream, sY);

      sites.emplace_back(
          std::stoi(sX),
          std::stoi(sY)); // make point from x and y (convert to int)
    }
  } catch (std::exception& e) {
    std::stringstream ss;
    ss << "While parsing the CSV file, the following error occurred in line "
       << lineno << "(" << line << ")"
       << ": " << e.what();
    throw std::runtime_error(ss.str());
  }
  // load JSON
  try {
    jsonS >> data;
  } catch (std::exception& e) {
    throw std::runtime_error(
        "While parsing the JSON file, the following error occurred: " +
        std::string(e.what()));
  }
  try {
    // load rest of JSON
    name = data["name"];
    std::map<std::string, ZoneId> nameToZone;
    for (const auto& zone : data["zones"]) {
      nameToZone[zone["name"]] = zones.size();
      const ZoneProperties zp{zone["name"], zone["xmin"], zone["xmax"],
                              zone["ymin"], zone["ymax"], zone["fidelity"]};
      zones.emplace_back(zp);
    }
    for (auto const& zone : data["initialZones"]) {
      initialZones.emplace_back(nameToZone.find(zone)->second);
    }
    for (auto const& op : data["operations"]) {
      const std::string opName = op["name"];
      const std::pair ty = {qc::opTypeFromString(opName),
                            opName.find_first_not_of('c')};
      const Scope sc = getScopeOfString(op["type"]);
      std::unordered_set<ZoneId> zo = {};
      for (auto const& zs : op["zones"]) {
        zo.emplace(nameToZone.find(zs)->second);
      }
      const Value fi = op["fidelity"];
      const Value ti = op["time"];
      OperationProperties const o = {sc, zo, ti, fi};
      gateSet.emplace(ty, o);
    }
    decoherenceTimes = {data["decoherence"]["t1"], data["decoherence"]["t2"]};
    for (const auto& sh : data["shuttling"]) {
      const ShuttlingProperties sp{
          sh["rows"],          sh["columns"],
          sh["xmin"],          sh["xmax"],
          sh["ymin"],          sh["ymax"],
          sh["move"]["speed"], sh["move"]["fidelity"],
          sh["load"]["time"],  sh["load"]["fidelity"],
          sh["store"]["time"], sh["store"]["fidelity"]};
      shuttling.emplace_back(sp);
    }
    minAtomDistance = data["minAtomDistance"];
    interactionRadius = data["interactionRadius"];
    noInteractionRadius = data["noInteractionRadius"];
  } catch (std::exception& e) {
    throw std::runtime_error(
        "While reading the JSON data, the following error occurred: " +
        std::string(e.what()));
  }
}

auto Architecture::getZoneAt(const Point& p) const -> ZoneId {
  const auto& it =
      std::find_if(zones.cbegin(), zones.cend(), [&](const auto& zProp) {
        return p.x >= zProp.minX && p.x <= zProp.maxX && p.y >= zProp.minY &&
               p.y <= zProp.maxY;
      });
  if (it == zones.cend()) {
    std::stringstream ss;
    ss << "The point " << p << " is not in any zone.";
    throw std::invalid_argument(ss.str());
  }
  return static_cast<ZoneId>(std::distance(zones.cbegin(), it));
}

auto Architecture::isAllowedLocally(const qc::OpType t,
                                    const std::size_t ctrls) const -> bool {
  const auto it = gateSet.find({t, ctrls});
  return it != gateSet.end() && it->second.scope == Scope::Local;
}
auto Architecture::isAllowedLocally(const qc::OpType t, const std::size_t ctrls,
                                    const ZoneId& zone) const -> bool {
  if (!isAllowedLocally(t, ctrls)) {
    return false; // gate not supported at all
  }
  const auto it = gateSet.find({t, ctrls});
  if (it == gateSet.end()) {
    qc::unreachable(); // please the clang-tidy null dereference checker
  }
  const auto& gateZones = it->second.zones;
  // zone exists in gateZones
  return gateZones.find(zone) != gateZones.end();
}

auto Architecture::isAllowedLocallyAt(const qc::OpType t,
                                      const std::size_t ctrls,
                                      const Point& p) const -> bool {
  const auto& it =
      std::find_if(zones.cbegin(), zones.cend(), [&](const auto& zProp) {
        return p.x >= zProp.minX && p.x <= zProp.maxX && p.y >= zProp.minY &&
               p.y <= zProp.maxY;
      });
  if (it == zones.cend()) {
    std::stringstream ss;
    ss << "The point " << p << " is not in any zone.";
    throw std::invalid_argument(ss.str());
  }
  return isAllowedLocally(
      t, ctrls, static_cast<ZoneId>(std::distance(zones.cbegin(), it)));
}

auto Architecture::isAllowedGlobally(const qc::OpType t,
                                     const std::size_t ctrls) const -> bool {
  const auto it = gateSet.find({t, ctrls});
  return it != gateSet.end() && it->second.scope == Scope::Global;
}

auto Architecture::isAllowedGlobally(const qc::OpType t,
                                     const std::size_t ctrls,
                                     const ZoneId& zone) const -> bool {
  if (!isAllowedGlobally(t, ctrls)) {
    return false; // gate not supported at all
  }
  const auto it = gateSet.find({t, ctrls});
  if (it == gateSet.end()) {
    qc::unreachable(); // please the clang-tidy null dereference checker
  }
  const auto& gateZones = it->second.zones;
  // zone exists in gateZones
  return gateZones.find(zone) != gateZones.end();
}
auto Architecture::getRowsInZone(const ZoneId& z) const -> std::vector<Number> {
  std::unordered_set<Number> rows;
  std::for_each(sites.cbegin(), sites.cend(), [&](const auto& s) {
    if (s.x >= zones[z].minX && s.x <= zones[z].maxX and s.y >= zones[z].minY &&
        s.y <= zones[z].maxY) {
      rows.insert(s.y);
    }
  });
  std::vector<Number> result(rows.cbegin(), rows.cend());
  std::sort(result.begin(), result.end());
  return result;
}
auto Architecture::getColsInZone(const ZoneId& z) const -> std::vector<Number> {
  std::unordered_set<Number> cols;
  std::for_each(sites.cbegin(), sites.cend(), [&](const auto& s) {
    if (s.x >= zones[z].minX && s.x <= zones[z].maxX and
        s.y >= zones[z].minY and s.y <= zones[z].maxY) {
      cols.insert(s.x);
    }
  });
  std::vector<Number> result(cols.cbegin(), cols.cend());
  std::sort(result.begin(), result.end());
  return result;
}
auto Architecture::getNColsInZone(const ZoneId& z) const -> Index {
  return getColsInZone(z).size();
}
auto Architecture::getNrowsInZone(const ZoneId& z) const -> Index {
  return getRowsInZone(z).size();
}
auto Architecture::getSitesInRow(const ZoneId& z, const Index& row) const
    -> std::vector<Index> {
  const auto y = Architecture::getRowsInZone(z)[row];
  std::vector<Index> atoms;
  for (Index i = 0; i < sites.size(); ++i) {
    const auto& s = sites[i];
    if (s.y == y && s.x >= zones[z].minX && s.x <= zones[z].maxX) {
      atoms.emplace_back(i);
    }
  }
  return atoms;
}
auto Architecture::getSitesInZone(const ZoneId& z) const -> std::vector<Index> {
  std::vector<Index> atoms;
  for (Index i = 0; i < sites.size(); ++i) {
    const auto& s = sites[i];
    if (s.x >= zones[z].minX && s.x <= zones[z].maxX && s.y >= zones[z].minY &&
        s.y <= zones[z].maxY) {
      atoms.emplace_back(i);
    }
  }
  return atoms;
}
auto Architecture::getNearestXLeft(const Number& x, const ZoneId& z,
                                   const bool proper) const -> Number {
  const auto& cols = getColsInZone(z);
  if (!std::any_of(cols.cbegin(), cols.cend(),
                   [&](const auto& c) { return proper ? c < x : c <= x; })) {
    return x;
  }

  Number result = std::numeric_limits<Number>::min();
  for (const auto& c : cols) {
    if ((proper ? c < x : c <= x) && c > result) {
      result = c;
    }
  }
  return result;
}

auto Architecture::getNearestXRight(const Number& x, const ZoneId& z,
                                    const bool proper) const -> Number {
  const auto& cols = getColsInZone(z);
  if (!std::any_of(cols.cbegin(), cols.cend(),
                   [&](const auto& c) { return proper ? c > x : c >= x; })) {
    return x;
  }

  Number result = std::numeric_limits<Number>::max();
  for (const auto& c : cols) {
    if ((proper ? c > x : c >= x) && c < result) {
      result = c;
    }
  }
  return result;
}
auto Architecture::hasSiteLeft(const Point& p, bool proper, bool sameZone) const
    -> std::pair<std::vector<Point>::const_reverse_iterator, bool> {
  const auto& zone = getZoneAt(p);
  const auto& it =
      std::find_if(sites.crbegin(), sites.crend(), [&](const auto& s) {
        return s.y == p.y and (proper ? s.x < p.x : s.x <= p.x) and
               (not sameZone or getZoneAt(s) == zone);
      });
  return {it, it != sites.crend()};
}
auto Architecture::hasSiteRight(const Point& p, bool proper,
                                bool sameZone) const
    -> std::pair<std::vector<Point>::const_iterator, bool> {
  const auto& zone = getZoneAt(p);
  const auto& it =
      std::find_if(sites.cbegin(), sites.cend(), [&](const auto& s) {
        return s.y == p.y and (proper ? s.x > p.x : s.x >= p.x) and
               (not sameZone or getZoneAt(s) == zone);
      });
  return {it, it != sites.cend()};
}
auto Architecture::hasSiteUp(const Point& p, bool proper, bool sameZone) const
    -> std::pair<std::vector<Point>::const_reverse_iterator, bool> {
  const auto& zone = getZoneAt(p);
  const auto& it =
      std::find_if(sites.crbegin(), sites.crend(), [&](const auto& s) {
        return s.x == p.x and (proper ? s.y < p.y : s.y <= p.y) and
               (not sameZone or getZoneAt(s) == zone);
      });
  return {it, it != sites.crend()};
}
auto Architecture::hasSiteDown(const Point& p, bool proper, bool sameZone) const
    -> std::pair<std::vector<Point>::const_iterator, bool> {
  const auto& zone = getZoneAt(p);
  const auto& it =
      std::find_if(sites.cbegin(), sites.cend(), [&](const auto& s) {
        return s.x == p.x and (proper ? s.y > p.y : s.y >= p.y) and
               (not sameZone or getZoneAt(s) == zone);
      });
  return {it, it != sites.cend()};
}
auto Architecture::getNearestSiteLeft(const Point& p, const bool proper,
                                      const bool sameZone) const
    -> std::optional<Index> {
  const auto& [it, success] = hasSiteLeft(p, proper, sameZone);
  if (not success) {
    return std::nullopt;
  }
  return sites.size() -
         static_cast<std::size_t>(std::distance(sites.crbegin(), it)) - 1;
}
auto Architecture::getNearestSiteRight(const Point& p, const bool proper,
                                       const bool sameZone) const
    -> std::optional<Index> {
  const auto& [it, success] = hasSiteRight(p, proper, sameZone);
  if (not success) {
    return std::nullopt;
  }
  return static_cast<std::size_t>(std::distance(sites.cbegin(), it));
}
auto Architecture::getNearestSiteUp(const Point& p, const bool proper,
                                    const bool sameZone) const
    -> std::optional<Index> {
  const auto& [it, success] = hasSiteUp(p, proper, sameZone);
  if (not success) {
    return std::nullopt;
  }
  return sites.size() -
         static_cast<std::size_t>(std::distance(sites.crbegin(), it)) - 1;
}
auto Architecture::getNearestSiteDown(const Point& p, const bool proper,
                                      const bool sameZone) const
    -> std::optional<Index> {
  const auto& [it, success] = hasSiteDown(p, proper, sameZone);
  if (not success) {
    return std::nullopt;
  }
  return static_cast<std::size_t>(std::distance(sites.cbegin(), it));
}
auto Architecture::getNearestSiteUpRight(const Point& p, const bool proper,
                                         const bool sameZone) const
    -> std::optional<Index> {
  const auto& zone = getZoneAt(p);
  std::optional<Index> opt;
  for (std::size_t i = 0; i < sites.size(); ++i) {
    const auto& s = sites[i];
    if ((proper ? s.x > p.x and s.y < p.y : s.x >= p.x and s.y <= p.y) and
        (not sameZone or getZoneAt(s) == zone)) {
      if (!opt or (s - p).length() < (getPositionOfSite(*opt) - p).length()) {
        opt = i;
      }
    }
  }
  return opt;
}
auto Architecture::getNearestSiteUpLeft(const Point& p, const bool proper,
                                        const bool sameZone) const
    -> std::optional<Index> {
  const auto& zone = getZoneAt(p);
  std::optional<Index> opt;
  for (std::size_t i = 0; i < sites.size(); ++i) {
    const auto& s = sites[i];
    if ((proper ? s.x < p.x and s.y < p.y : s.x <= p.x and s.y <= p.y) and
        (not sameZone or getZoneAt(s) == zone)) {
      if (!opt or (s - p).length() < (getPositionOfSite(*opt) - p).length()) {
        opt = i;
      }
    }
  }
  return opt;
}
auto Architecture::getNearestSiteDownLeft(const Point& p, const bool proper,
                                          const bool sameZone) const
    -> std::optional<Index> {
  const auto& zone = getZoneAt(p);
  std::optional<Index> opt;
  for (std::size_t i = 0; i < sites.size(); ++i) {
    const auto& s = sites[i];
    if ((proper ? s.x < p.x and s.y > p.y : s.x <= p.x and s.y >= p.y) and
        (not sameZone or getZoneAt(s) == zone)) {
      if (!opt or (s - p).length() < (getPositionOfSite(*opt) - p).length()) {
        opt = i;
      }
    }
  }
  return opt;
}
auto Architecture::getNearestSiteDownRight(const Point& p, const bool proper,
                                           const bool sameZone) const
    -> std::optional<Index> {
  const auto& zone = getZoneAt(p);
  std::optional<Index> opt;
  for (std::size_t i = 0; i < sites.size(); ++i) {
    const auto& s = sites[i];
    if ((proper ? s.x > p.x and s.y > p.y : s.x >= p.x and s.y >= p.y) and
        (not sameZone or getZoneAt(s) == zone)) {
      if (!opt or (s - p).length() < (getPositionOfSite(*opt) - p).length()) {
        opt = i;
      }
    }
  }
  return opt;
}
auto Architecture::getSiteAt(const Point& p) const -> std::optional<Index> {
  auto it = std::find_if(sites.begin(), sites.end(),
                         [&](const auto& site) { return site == p; });
  if (it == sites.end()) {
    return std::nullopt;
  }
  return std::distance(sites.begin(), it);
}
auto Architecture::withConfig(const Configuration& config) const
    -> Architecture {
  Architecture result = *this;
  result.sites.clear();
  std::vector<bool> usedSites(sites.size(), false);
  for (const auto& zone : zones) {
    for (std::size_t i = 0; i < sites.size(); ++i) {
      const auto& s = sites[i];
      if (s.x >= zone.minX && s.x <= zone.maxX && s.y >= zone.minY &&
          s.y <= zone.maxY) {
        // s lays in the zone
        if (!usedSites[i]) {
          // s is not used yet
          bool patchFittable = true;
          auto row = i;
          for (std::size_t r = 1; r < config.getPatchRows(); ++r) {
            auto col = row;
            for (std::size_t c = 1; c < config.getPatchCols(); ++c) {
              auto nextCol =
                  getNearestSiteRight(getPositionOfSite(col), true, true);
              if (!nextCol.has_value()) {
                patchFittable = false;
                break;
              }
              col = nextCol.value();
            }
            if (!patchFittable) {
              break;
            }

            auto nextRow =
                getNearestSiteDown(getPositionOfSite(row), true, true);
            if (!nextRow.has_value()) {
              patchFittable = false;
              break;
            }
            row = nextRow.value();
          }
          if (patchFittable) {
            result.sites.emplace_back(s);
            auto rowOpt = std::optional<Index>(i);
            for (std::size_t r = 0; r < config.getPatchRows(); ++r) {
              auto colOpt = rowOpt;
              for (std::size_t c = 0; c < config.getPatchCols(); ++c) {
                usedSites[*colOpt] = true;
                colOpt =
                    getNearestSiteRight(getPositionOfSite(*colOpt), true, true);
              }
              rowOpt =
                  getNearestSiteDown(getPositionOfSite(*rowOpt), true, true);
            }
          }
        }
      }
    }
  }
  return result;
}
auto Architecture::getPositionOffsetBy(const Point& p, const Number& rows,
                                       const Number& cols) const -> Point {
  const auto d = static_cast<std::int64_t>(getNoInteractionRadius());
  // get nearest site to p
  // checks in all directions except the one pointed to by rows and cols
  std::optional<Index> nearestSiteOpt;
  if (rows >= 0) {
    if (cols >= 0) {
      nearestSiteOpt = getNearestSiteUpLeft(p, false, true);
      if (!nearestSiteOpt.has_value()) {
        nearestSiteOpt = getNearestSiteUpRight(p, false, true);
      }
      if (!nearestSiteOpt.has_value()) {
        nearestSiteOpt = getNearestSiteDownLeft(p, false, true);
      }
    } else {
      nearestSiteOpt = getNearestSiteUpRight(p, false, true);
      if (!nearestSiteOpt.has_value()) {
        nearestSiteOpt = getNearestSiteUpLeft(p, false, true);
      }
      if (!nearestSiteOpt.has_value()) {
        nearestSiteOpt = getNearestSiteDownRight(p, false, true);
      }
    }
  } else {
    if (cols >= 0) {
      nearestSiteOpt = getNearestSiteDownLeft(p, false, true);
      if (!nearestSiteOpt.has_value()) {
        nearestSiteOpt = getNearestSiteUpLeft(p, false, true);
      }
      if (!nearestSiteOpt.has_value()) {
        nearestSiteOpt = getNearestSiteDownRight(p, false, true);
      }
    } else {
      nearestSiteOpt = getNearestSiteDownRight(p, false, true);
      if (!nearestSiteOpt.has_value()) {
        nearestSiteOpt = getNearestSiteDownLeft(p, false, true);
      }
      if (!nearestSiteOpt.has_value()) {
        nearestSiteOpt = getNearestSiteUpRight(p, false, true);
      }
    }
  }

  if (!nearestSiteOpt.has_value()) {
    return {p.x + rows * d, p.y + cols * d};
  }

  // get position of nearest site
  const auto nearestSitePos = getPositionOfSite(nearestSiteOpt.value());
  const auto dx = p.x - nearestSitePos.x;
  const auto dy = p.y - nearestSitePos.y;
  auto anchorSitePos = nearestSitePos;
  auto r = std::abs(rows);
  auto c = std::abs(cols);

  for (; r > 0; --r) {
    auto nextSiteOpt = rows >= 0 ? getNearestSiteDown(anchorSitePos, true, true)
                                 : getNearestSiteUp(anchorSitePos, true, true);
    if (!nextSiteOpt.has_value()) {
      break;
    }
    const auto& newAnchorSitePos = getPositionOfSite(nextSiteOpt.value());
    // The following check is used to decide whether the patch is located
    // outside of the zone Patches that overlap the border of the zone are not
    // possible with this approach yet
    if (std::abs(newAnchorSitePos.y - anchorSitePos.y) < std::abs(dy)) {
      break;
    }
    anchorSitePos = newAnchorSitePos;
  }

  for (; c > 0; --c) {
    auto nextSiteOpt = cols >= 0
                           ? getNearestSiteRight(anchorSitePos, true, true)
                           : getNearestSiteLeft(anchorSitePos, true, true);
    if (!nextSiteOpt.has_value()) {
      break;
    }
    const auto& newAnchorSitePos = getPositionOfSite(nextSiteOpt.value());
    // The following check is used to decide whether the patch is located
    // outside of the zone Patches that overlap the border of the zone are not
    // possible with this approach yet
    if (std::abs(newAnchorSitePos.x - anchorSitePos.x) < std::abs(dx)) {
      break;
    }
    anchorSitePos = newAnchorSitePos;
  }
  anchorSitePos.x =
      cols >= 0 ? anchorSitePos.x + c * d : anchorSitePos.x - c * d;
  anchorSitePos.y =
      rows >= 0 ? anchorSitePos.y + r * d : anchorSitePos.y - r * d;
  return {anchorSitePos.x + dx, anchorSitePos.y + dy};
}

} // namespace na::nalac
