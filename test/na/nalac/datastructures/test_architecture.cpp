/*
 * Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
 * Copyright (c) 2025 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "ir/operations/OpType.hpp"
#include "na/nalac/datastructures/Architecture.hpp"
#include "na/nalac/datastructures/Configuration.hpp"
#include "na/nalac/datastructures/NADefinitions.hpp"

#include <cstdint>
#include <cstdio>
#include <fstream>
#include <gtest/gtest.h>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>

class NAArchitecture : public testing::Test {
protected:
  na::nalac::Architecture arch;
  void SetUp() override {
    // write content to a file
    std::istringstream archIS(R"({
      "name": "Nature",
      "initialZones": [
          "storage"
      ],
      "zones": [
          {
              "name": "entangling",
              "xmin": -300,
              "xmax": 656,
              "ymin": -20,
              "ymax": 46,
              "fidelity": 0.9959
          },
          {
              "name": "storage",
              "xmin": -300,
              "xmax": 656,
              "ymin": 47,
              "ymax": 121,
              "fidelity": 1
          },
          {
              "name": "readout",
              "xmin": -300,
              "xmax": 656,
              "ymin": 122,
              "ymax": 156,
              "fidelity": 0.99
          }
      ],
      "operations": [
          {
              "name": "rz",
              "type": "local",
              "zones": [
                  "entangling",
                  "storage",
                  "readout"
              ],
              "time": 0.5,
              "fidelity": 0.999
          },
          {
              "name": "ry",
              "type": "global",
              "zones": [
                  "entangling",
                  "storage",
                  "readout"
              ],
              "time": 0.5,
              "fidelity": 0.999
          },
          {
              "name": "cz",
              "type": "global",
              "zones": [
                  "entangling"
              ],
              "time": 0.2,
              "fidelity": 0.9959
          },
          {
              "name": "measure",
              "type": "global",
              "zones": [
                  "readout"
              ],
              "time": 0.2,
              "fidelity": 0.95
          }
      ],
      "decoherence": {
          "t1": 100000000,
          "t2": 1500000
      },
      "interactionRadius": 2,
      "noInteractionRadius": 5,
      "minAtomDistance": 1,
      "shuttling": [
          {
              "rows": 5,
              "columns": 5,
              "xmin": -2.5,
              "xmax": 2.5,
              "ymin": -2.5,
              "ymax": 2.5,
              "move": {
                  "speed": 0.55,
                  "fidelity": 1
              },
              "load": {
                  "time": 20,
                  "fidelity": 1
              },
              "store": {
                  "time": 20,
                  "fidelity": 1
              }
          }
      ]
  })");
    std::stringstream gridSS;
    gridSS << "x,y\n";
    // entangling zone (4 x 36 = 144 sites)
    for (std::size_t y = 0; y <= 36; y += 12) {
      for (std::size_t x = 3; x <= 353; x += 10) {
        gridSS << x << "," << y << "\n";
      }
    }
    // storage zone (12 x 72 = 864 sites)
    for (std::size_t y = 56; y <= 111; y += 5) {
      for (std::size_t x = 0; x <= 355; x += 5) {
        gridSS << x << "," << y << "\n";
      }
    }
    // readout zone (4 x 72 = 288 sites)
    for (std::size_t y = 131; y <= 146; y += 5) {
      for (std::size_t x = 0; x <= 355; x += 5) {
        gridSS << x << "," << y << "\n";
      }
    }
    // total: 1296 sites
    arch.fromFileStream(archIS, gridSS);
  }
};

TEST_F(NAArchitecture, ScopeString) {
  EXPECT_EQ(na::nalac::getScopeOfString("local"), na::nalac::Scope::Local);
  EXPECT_EQ(na::nalac::getScopeOfString("gLoBaL"), // spellchecker:disable-line
            na::nalac::Scope::Global);
  EXPECT_THROW(na::nalac::getScopeOfString(""), std::invalid_argument);
}

TEST_F(NAArchitecture, Import) {
  EXPECT_EQ(arch.getNZones(), 3);
  EXPECT_EQ(arch.getNSites(), 1296);
  EXPECT_EQ(arch.getName(), "Nature");
  EXPECT_EQ(arch.getZoneLabel(arch.getZoneOfSite(0)), "entangling");
  EXPECT_ANY_THROW(na::nalac::Architecture("file_does_not_Exist.json",
                                           "file_does_not_Exist.csv"));
  std::ofstream tmp("temp.json");
  tmp.close();
  EXPECT_ANY_THROW(
      na::nalac::Architecture("temp.json", "file_does_not_Exist.csv"));
  std::remove("temp.json");
  {
    std::istringstream archIS("{}");
    std::istringstream gridIS("x,y\n0;0");
    EXPECT_ANY_THROW(na::nalac::Architecture(archIS, gridIS));
  }
  {
    std::istringstream archIS("{");
    std::istringstream gridIS("x,y\n0,0");
    EXPECT_ANY_THROW(na::nalac::Architecture(archIS, gridIS));
  }
  {
    std::istringstream archIS("{ }");
    std::istringstream gridIS("x,y\n0,0");
    EXPECT_ANY_THROW(na::nalac::Architecture(archIS, gridIS));
  }
}

TEST_F(NAArchitecture, GateApplicability) {
  EXPECT_TRUE(arch.isAllowedGlobally(qc::OpType::RY, 0, 1));
  EXPECT_TRUE(arch.isAllowedGlobally(qc::OpType::Z, 1, 0));
  EXPECT_TRUE(arch.isAllowedLocally(qc::OpType::RZ, 0, 1));
}

TEST_F(NAArchitecture, GateProperty) {
  EXPECT_EQ(arch.getPropertiesOfOperation(qc::OpType::RY, 0).scope,
            na::nalac::Scope::Global);
  EXPECT_EQ(arch.getPropertiesOfOperation(qc::OpType::Z, 1).fidelity, 0.9959);
  EXPECT_THROW(std::ignore = arch.getPropertiesOfOperation(qc::OpType::RX, 0),
               std::invalid_argument);
}

TEST_F(NAArchitecture, Getter) {
  EXPECT_EQ(arch.getNrowsInZone(0), 4);
  EXPECT_EQ(arch.getNColsInZone(0), 36);
}

TEST_F(NAArchitecture, WithConfiguration) {
  na::nalac::Configuration const config(2, 3);
  const auto modArch = arch.withConfig(config);
  EXPECT_EQ(modArch.getNSites(), 216);
}

TEST_F(NAArchitecture, SiteAt) {
  EXPECT_FALSE(arch.getSiteAt({-1000, -1000}).has_value());
}

TEST_F(NAArchitecture, ZoneAt) {
  EXPECT_EQ(arch.getZoneAt({0, 0}), 0);
  EXPECT_EQ(arch.getZoneAt({0, 50}), 1);
  EXPECT_THROW(std::ignore = arch.getZoneAt({0, -1000}), std::invalid_argument);
}

TEST_F(NAArchitecture, LocallyAllowed) {
  EXPECT_TRUE(arch.isAllowedLocally(qc::RZ, 0));
  EXPECT_FALSE(arch.isAllowedLocally(qc::RY, 0));
  EXPECT_TRUE(arch.isAllowedLocally(qc::RZ, 0, 1));
  EXPECT_TRUE(arch.isAllowedLocallyAt(qc::RZ, 0, {0, 50}));
  EXPECT_THROW(std::ignore = arch.isAllowedLocallyAt(qc::RZ, 0, {0, -1000}),
               std::invalid_argument);
}

TEST_F(NAArchitecture, GloballyAllowed) {
  EXPECT_FALSE(arch.isAllowedGlobally(qc::RZ, 0));
  EXPECT_TRUE(arch.isAllowedGlobally(qc::RY, 0));
  EXPECT_TRUE(arch.isAllowedGlobally(qc::RY, 0, 1));
}

TEST_F(NAArchitecture, SitesInZone) {
  EXPECT_EQ(arch.getSitesInZone(arch.getZoneAt({0, 0})).size(), 144);
}

TEST_F(NAArchitecture, SiteUp) {
  EXPECT_TRUE(arch.hasSiteUp({3, 3}, false, true).second);
  EXPECT_FALSE(arch.hasSiteUp({3, 0}, true, true).second);
  EXPECT_EQ(arch.getPositionOfSite(*arch.getNearestSiteUp({3, 3}, true, true)),
            (na::nalac::Point{3, 0}));
  EXPECT_EQ(arch.getNearestSiteUp({3, 0}, true, true), std::nullopt);
}

TEST_F(NAArchitecture, SiteDown) {
  EXPECT_FALSE(arch.hasSiteDown({0, 3}, false, true).second);
  EXPECT_TRUE(arch.hasSiteDown({3, 0}, true, true).second);
  EXPECT_EQ(arch.getNearestSiteDown({0, 3}, false, true), std::nullopt);
  EXPECT_EQ(
      arch.getPositionOfSite(*arch.getNearestSiteDown({3, 0}, true, true)),
      (na::nalac::Point{3, 12}));
}

TEST_F(NAArchitecture, SiteLeft) {
  EXPECT_TRUE(arch.hasSiteLeft({3, 0}, false, true).second);
  EXPECT_FALSE(arch.hasSiteLeft({3, 0}, true, true).second);
  EXPECT_EQ(
      arch.getPositionOfSite(*arch.getNearestSiteLeft({3, 0}, false, true)),
      (na::nalac::Point{3, 0}));
  EXPECT_EQ(arch.getNearestSiteLeft({3, 0}, true, true), std::nullopt);
}

TEST_F(NAArchitecture, SiteRight) {
  EXPECT_TRUE(arch.hasSiteRight({3, 0}, false, true).second);
  EXPECT_FALSE(arch.hasSiteRight({3, 3}, true, true).second);
  EXPECT_EQ(
      arch.getPositionOfSite(*arch.getNearestSiteRight({3, 0}, true, true)),
      (na::nalac::Point{13, 0}));
  EXPECT_EQ(arch.getNearestSiteRight({3, 3}, true, true), std::nullopt);
}

TEST_F(NAArchitecture, SiteDownRight) {
  EXPECT_TRUE(arch.getNearestSiteDownRight({3, 0}, true, true).has_value());
  EXPECT_EQ(arch.getPositionOfSite(
                arch.getNearestSiteDownRight({3, 0}, true, true).value()),
            (na::nalac::Point{13, 12}));
  EXPECT_TRUE(arch.getNearestSiteDownRight({353, 36}, false, true).has_value());
  EXPECT_FALSE(arch.getNearestSiteDownRight({353, 36}, true, true).has_value());
}

TEST_F(NAArchitecture, SiteDownLeft) {
  EXPECT_TRUE(arch.getNearestSiteDownLeft({353, 0}, true, true).has_value());
  EXPECT_EQ(arch.getPositionOfSite(
                arch.getNearestSiteDownLeft({353, 0}, true, true).value()),
            (na::nalac::Point{343, 12}));
  EXPECT_TRUE(arch.getNearestSiteDownLeft({3, 36}, false, true).has_value());
  EXPECT_FALSE(arch.getNearestSiteDownLeft({3, 36}, true, true).has_value());
}

TEST_F(NAArchitecture, SiteOffsetBy) {
  EXPECT_EQ(arch.getPositionOffsetBy({13, 12}, 0, 1),
            (na::nalac::Point{23, 12}));
  EXPECT_EQ(arch.getPositionOffsetBy({13, 12}, -1, 1),
            (na::nalac::Point{23, 0}));
  EXPECT_EQ(arch.getPositionOffsetBy({13, 12}, -1, 0),
            (na::nalac::Point{13, 0}));
  EXPECT_EQ(arch.getPositionOffsetBy({13, 12}, -1, -1),
            (na::nalac::Point{3, 0}));
  EXPECT_EQ(arch.getPositionOffsetBy({13, 12}, 0, -1),
            (na::nalac::Point{3, 12}));
  EXPECT_EQ(arch.getPositionOffsetBy({13, 12}, 1, -1),
            (na::nalac::Point{3, 24}));
  EXPECT_EQ(arch.getPositionOffsetBy({13, 12}, 1, 0),
            (na::nalac::Point{13, 24}));
  EXPECT_EQ(arch.getPositionOffsetBy({13, 12}, 1, 1),
            (na::nalac::Point{23, 24}));
  EXPECT_EQ(arch.getPositionOffsetBy({3, -2}, 1, 1),
            (na::nalac::Point{13, 10}));
  EXPECT_EQ(arch.getPositionOffsetBy({355, 0}, 1, -1),
            (na::nalac::Point{345, 12}));
  EXPECT_EQ(arch.getPositionOffsetBy({350, -2}, 1, -1),
            (na::nalac::Point{340, 10}));
  EXPECT_EQ(arch.getPositionOffsetBy({3, 25}, -1, 1),
            (na::nalac::Point{13, 13}));
  EXPECT_EQ(arch.getPositionOffsetBy({0, 24}, -1, 1),
            (na::nalac::Point{10, 12}));
  EXPECT_EQ(arch.getPositionOffsetBy({355, 24}, -1, -1),
            (na::nalac::Point{345, 12}));
  EXPECT_EQ(arch.getPositionOffsetBy({353, 25}, -1, -1),
            (na::nalac::Point{343, 13}));
  const auto d = static_cast<std::int64_t>(arch.getNoInteractionRadius());
  EXPECT_EQ(arch.getPositionOffsetBy({-40, -20}, 1, 1),
            (na::nalac::Point{-40 + d, -20 + d}));
  EXPECT_EQ(arch.getPositionOffsetBy({-10, -10}, 1, 1),
            (na::nalac::Point{-10 + d, -10 + d}));
  EXPECT_EQ(arch.getPositionOffsetBy({13, 12}, -2, -2),
            (na::nalac::Point{3 - d, 0 - d}));
}
