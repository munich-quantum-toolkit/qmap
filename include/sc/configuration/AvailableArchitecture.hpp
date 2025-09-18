/*
 * Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
 * Copyright (c) 2025 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#pragma once

#include <cstdint>
#include <map>
#include <stdexcept>
#include <string>

enum class AvailableArchitecture : std::uint8_t {
  IbmQx4,
  IbmQx5,
  IbmqYorktown,
  IbmqLondon,
  IbmqBogota,
  IbmqCasablanca,
  IbmqTokyo,
  RigettiAgave,
  RigettiAspen
};

[[maybe_unused]] static inline std::string
toString(const AvailableArchitecture architecture) {
  switch (architecture) {
  case AvailableArchitecture::IbmQx4:
    return "IBM_QX4";
  case AvailableArchitecture::IbmQx5:
    return "IBM_QX5";
  case AvailableArchitecture::IbmqYorktown:
    return "IBMQ_Yorktown";
  case AvailableArchitecture::IbmqLondon:
    return "IBMQ_London";
  case AvailableArchitecture::IbmqBogota:
    return "IBMQ_Bogota";
  case AvailableArchitecture::IbmqCasablanca:
    return "IBMQ_Casablanca";
  case AvailableArchitecture::IbmqTokyo:
    return "IBMQ_Tokyo";
  case AvailableArchitecture::RigettiAgave:
    return "Rigetti_Agave";
  case AvailableArchitecture::RigettiAspen:
    return "Rigetti_Aspen";
  }
  return " ";
}

[[maybe_unused]] static AvailableArchitecture
architectureFromString(const std::string& architecture) {
  if (architecture == "IBM_QX4" || architecture == "0") {
    return AvailableArchitecture::IbmQx4;
  }
  if (architecture == "IBM_QX5" || architecture == "1") {
    return AvailableArchitecture::IbmQx5;
  }
  if (architecture == "IBMQ_Yorktown" || architecture == "2") {
    return AvailableArchitecture::IbmqYorktown;
  }
  if (architecture == "IBMQ_London" || architecture == "3") {
    return AvailableArchitecture::IbmqLondon;
  }
  if (architecture == "IBMQ_Bogota" || architecture == "4") {
    return AvailableArchitecture::IbmqBogota;
  }
  if (architecture == "IBMQ_Casablanca" || architecture == "5") {
    return AvailableArchitecture::IbmqCasablanca;
  }
  if (architecture == "IBMQ_Tokyo" || architecture == "6") {
    return AvailableArchitecture::IbmqTokyo;
  }
  if (architecture == "Rigetti_Agave" || architecture == "7") {
    return AvailableArchitecture::RigettiAgave;
  }
  if (architecture == "Rigetti_Aspen" || architecture == "8") {
    return AvailableArchitecture::RigettiAspen;
  }
  throw std::invalid_argument("Invalid architecture value: " + architecture);
}

[[maybe_unused]] static std::string
getCouplingMapSpecification(AvailableArchitecture architecture) {
  static std::map<AvailableArchitecture, std::string> architectureMap{
      {AvailableArchitecture::IbmQx4, "5\n1 0\n2 0\n2 1\n3 2\n3 4\n2 4"},
      {AvailableArchitecture::IbmQx5,
       "16\n1 0\n15 0\n1 2\n2 3\n15 2\n3 4\n3 14\n5 4\n13 4\n6 5\n12 5\n6 7\n6 "
       "11\n8 7\n7 10\n9 8\n9 10\n11 10\n12 11\n12 13\n13 14\n15 14"},
      {AvailableArchitecture::IbmqYorktown,
       "5\n0 1\n1 0\n0 2\n2 0\n1 2\n2 1\n2 3\n3 2\n3 4\n4 3\n2 4\n4 2"},
      {AvailableArchitecture::IbmqLondon,
       "5\n0 1\n1 0\n1 2\n2 1\n1 3\n3 1\n3 4\n4 3"},
      {AvailableArchitecture::IbmqBogota,
       "5\n0 1\n1 0\n1 2\n2 1\n2 3\n3 2\n3 4\n4 3"},
      {AvailableArchitecture::IbmqCasablanca,
       "7\n0 1\n1 0\n1 2\n2 1\n1 3\n3 1\n3 5\n5 3\n5 4\n4 5\n5 6\n6 5"},
      {AvailableArchitecture::IbmqTokyo,
       "20\n0 1\n1 0\n1 2\n2 1\n2 3\n3 2\n3 4\n4 3\n5 6\n6 5\n6 7\n7 6\n7 8\n8 "
       "7\n8 9\n9 8\n10 11\n11 10\n11 12\n12 11\n12 13\n13 12\n13 14\n14 "
       "13\n15 16\n16 15\n16 17\n17 16\n17 18\n18 17\n18 19\n19 18\n0 5\n5 "
       "0\n5 10\n10 5\n10 15\n15 10\n1 6\n6 1\n6 11\n11 6\n11 16\n16 11\n2 "
       "7\n7 2\n7 12\n12 7\n12 17\n17 12\n3 8\n8 3\n8 13\n13 8\n13 18\n18 "
       "13\n4 9\n9 4\n9 14\n14 9\n14 19\n19 14\n5 11\n11 5\n11 17\n17 11\n1 "
       "7\n7 1\n7 13\n13 7\n13 9\n9 13\n3 9\n9 3\n2 6\n6 2\n6 10\n10 6\n4 8\n8 "
       "4\n8 12\n12 8\n12 16\n16 12\n14 18\n18 14"},
      {AvailableArchitecture::RigettiAgave,
       "8\n1 0\n0 1\n0 7\n7 0\n7 6\n6 7\n6 5\n5 6\n5 4\n4 5\n4 3\n3 4\n3 2\n2 "
       "3\n2 1\n1 2"},
      {AvailableArchitecture::RigettiAspen,
       "16\n0 1\n1 0\n1 14\n14 1\n14 15\n15 14\n15 0\n0 15\n0 7\n7 0\n7 6\n6 "
       "7\n6 5\n5 6\n5 4\n4 5\n4 3\n3 4\n3 2\n2 3\n2 1\n1 2\n14 13\n13 14\n13 "
       "12\n12 13\n12 11\n11 12\n11 10\n10 11\n10 9\n9 10\n9 8\n8 9\n8 15\n15 "
       "8"}};
  return architectureMap.at(architecture);
}
