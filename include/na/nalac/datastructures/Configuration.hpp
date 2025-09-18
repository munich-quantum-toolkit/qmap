#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <istream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace na::nalac {

enum class NAMappingMethod : std::uint8_t {
  Naive,
  MaximizeParallelismHeuristic
};
static const std::unordered_map<std::string, NAMappingMethod> STRING_TO_METHOD =
    {{"naive", NAMappingMethod::Naive},
     {"maximize parallelism", NAMappingMethod::MaximizeParallelismHeuristic}};
[[nodiscard]] inline auto getMethodOfString(const std::string& method)
    -> NAMappingMethod {
  std::string methodLowerCase = method;
  std::transform(methodLowerCase.begin(), methodLowerCase.end(),
                 methodLowerCase.begin(),
                 [](const auto c) { return std::tolower(c); });
  if (const auto it = STRING_TO_METHOD.find(methodLowerCase);
      it != STRING_TO_METHOD.end()) {
    return it->second;
  }
  std::stringstream ss;
  ss << "The method " << method << " is not supported.";
  throw std::invalid_argument(ss.str());
}
class Configuration {
private:
  std::size_t patchRows = 1;
  std::size_t patchCols = 1;
  NAMappingMethod method = NAMappingMethod::MaximizeParallelismHeuristic;

public:
  Configuration() = default;
  explicit Configuration(const NAMappingMethod mappingMethod)
      : method(mappingMethod) {};
  explicit Configuration(const std::size_t rows, const std::size_t cols)
      : patchRows(rows), patchCols(cols) {};
  explicit Configuration(const std::size_t rows, const std::size_t cols,
                         const NAMappingMethod mappingMethod)
      : patchRows(rows), patchCols(cols), method(mappingMethod) {};
  explicit Configuration(const std::string& filename);
  explicit Configuration(std::istream& fs);
  virtual ~Configuration() = default;

  [[nodiscard]] auto getPatchRows() const -> std::size_t { return patchRows; }
  [[nodiscard]] auto getPatchCols() const -> std::size_t { return patchCols; }
  [[nodiscard]] auto getMethod() const -> NAMappingMethod { return method; }
};
} // namespace na::nalac
