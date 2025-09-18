#
# This file is part of the MQT QMAP library released under the MIT license. See README.md or go to
# https://github.com/cda-tum/mqt-qmap for more information.
#

set(MQT_QMAP_HYBRIDMAP_TARGET_NAME ${MQT_QMAP_TARGET_NAME}-hybridmap)

if(NOT TARGET ${MQT_QMAP_HYBRIDMAP_TARGET_NAME})
  # collect headers and source files
  file(GLOB_RECURSE HYBRIDMAP_HEADERS ${MQT_QMAP_INCLUDE_BUILD_DIR}/hybridmap/*.hpp)
  file(GLOB_RECURSE HYBRIDMAP_SOURCES **.cpp)

  # add HybridMap Package library
  add_library(${MQT_QMAP_HYBRIDMAP_TARGET_NAME} ${HYBRIDMAP_HEADERS} ${HYBRIDMAP_SOURCES})

  # set include directories
  target_include_directories(${MQT_QMAP_HYBRIDMAP_TARGET_NAME}
                             PUBLIC $<BUILD_INTERFACE:${MQT_QMAP_INCLUDE_BUILD_DIR}>)

  # link to the MQT::Core libraries
  target_link_libraries(
    ${MQT_QMAP_HYBRIDMAP_TARGET_NAME}
    PUBLIC MQT::CoreIR MQT::CoreNA nlohmann_json::nlohmann_json
    PRIVATE MQT::ProjectWarnings MQT::ProjectOptions MQT::CoreCircuitOptimizer)

  # add MQT alias
  add_library(MQT::QMapHybrid ALIAS ${MQT_QMAP_HYBRIDMAP_TARGET_NAME})
endif()
