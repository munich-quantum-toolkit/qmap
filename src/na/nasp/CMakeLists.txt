set(MQT_QMAP_NASP_TARGET_NAME ${MQT_QMAP_TARGET_NAME}-nasp)

if(NOT TARGET ${MQT_QMAP_NASP_TARGET_NAME})
  file(GLOB NASP_HEADERS ${MQT_QMAP_INCLUDE_BUILD_DIR}/na/nasp/*.hpp)
  file(GLOB NASP_SOURCES *.cpp)

  add_library(${MQT_QMAP_NASP_TARGET_NAME} ${NASP_HEADERS} ${NASP_SOURCES})

  target_include_directories(${MQT_QMAP_NASP_TARGET_NAME}
                             PUBLIC $<BUILD_INTERFACE:${MQT_QMAP_INCLUDE_BUILD_DIR}>)

  target_link_libraries(
    ${MQT_QMAP_NASP_TARGET_NAME}
    PUBLIC MQT::CoreNA nlohmann_json::nlohmann_json z3::z3lib
    PRIVATE MQT::CoreDS MQT::CoreCircuitOptimizer MQT::ProjectOptions MQT::ProjectWarnings)

  add_library(MQT::NASP ALIAS ${MQT_QMAP_NASP_TARGET_NAME})
endif()
