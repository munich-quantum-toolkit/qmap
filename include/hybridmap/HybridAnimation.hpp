//
// This file is part of the MQT QMAP library released under the MIT license.
// See README.md or go to https://github.com/cda-tum/qmap for more information.
//

#pragma once

#include "NeutralAtomArchitecture.hpp"
#include "NeutralAtomDefinitions.hpp"
#include "ir/Definitions.hpp"
#include "ir/operations/Operation.hpp"

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <utility>

namespace na {
class AnimationAtoms {
protected:
  std::map<CoordIndex, HwQubit> coordIdxToId;
  std::map<HwQubit, std::pair<qc::fp, qc::fp>> idToCoord;
  const NeutralAtomArchitecture& arch;

  void initPositions(const std::map<HwQubit, CoordIndex>& initHwPos,
                     const std::map<HwQubit, CoordIndex>& initFaPos);

public:
  AnimationAtoms(const std::map<HwQubit, CoordIndex>& initHwPos,
                 const std::map<HwQubit, CoordIndex>& initFaPos,
                 const NeutralAtomArchitecture& arch)
      : arch(arch) {
    initPositions(initHwPos, initFaPos);
  }

  std::string placeInitAtoms();
  std::string opToNaViz(const std::unique_ptr<qc::Operation>& op,
                        qc::fp startTime);
};

} // namespace na
