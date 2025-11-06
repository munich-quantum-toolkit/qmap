/*
 * Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
 * Copyright (c) 2025 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#ifndef HYBRIDMAP_HYBRID_ANIMATION_HPP
#define HYBRIDMAP_HYBRID_ANIMATION_HPP

#include "NeutralAtomArchitecture.hpp"
#include "NeutralAtomDefinitions.hpp"
#include "ir/Definitions.hpp"
#include "ir/operations/Operation.hpp"

#include <map>
#include <memory>
#include <string>
#include <utility>

namespace na {
class AnimationAtoms {
protected:
  std::map<CoordIndex, HwQubit> coordIdxToId;
  std::map<HwQubit, std::pair<qc::fp, qc::fp>> idToCoord;
  const NeutralAtomArchitecture* arch;

  void initPositions(const std::map<HwQubit, CoordIndex>& initHwPos,
                     const std::map<HwQubit, CoordIndex>& initFaPos);

public:
  AnimationAtoms(const std::map<HwQubit, CoordIndex>& initHwPos,
                 const std::map<HwQubit, CoordIndex>& initFaPos,
                 const NeutralAtomArchitecture& architecture)
      : arch(&architecture) {
    initPositions(initHwPos, initFaPos);
  }

  std::string placeInitAtoms();
  std::string opToNaViz(const std::unique_ptr<qc::Operation>& op,
                        qc::fp startTime);
};

} // namespace na

#endif // HYBRIDMAP_HYBRID_ANIMATION_HPP
