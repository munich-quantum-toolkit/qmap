#include "na/nalac/datastructures/Configuration.hpp"

#include <exception>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <sstream>
#include <stdexcept>
#include <string>

using json = nlohmann::basic_json<>;

namespace na::nalac {

Configuration::Configuration(const std::string& filename) {
  std::ifstream fs(filename);
  if (!fs.good()) {
    std::stringstream ss;
    ss << "Could not open JSON file " << filename << ".";
    throw std::runtime_error(ss.str());
  }
  *this = Configuration(fs);
}

Configuration::Configuration(std::istream& fs) {
  json data;
  try {
    fs >> data;
  } catch (const std::exception& e) {
    std::stringstream ss;
    ss << "Error parsing JSON: " << e.what() << '\n';
    throw std::runtime_error(ss.str());
  }

  if (data.contains("patch") && data["patch"].is_object()) {
    auto& patch = data["patch"];
    if (patch.contains("rows") && patch["rows"].is_number_unsigned()) {
      patchRows = patch["rows"];
    }
    if (patch.contains("cols") && patch["cols"].is_number_unsigned()) {
      patchCols = patch["cols"];
    }
  }
  if (data.contains("method") && data["method"].is_string()) {
    method = getMethodOfString(data["method"]);
  }
}
} // namespace na::nalac
