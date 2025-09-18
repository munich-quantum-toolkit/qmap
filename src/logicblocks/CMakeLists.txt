add_library(
  mqt-logic-blocks
  ${MQT_QMAP_INCLUDE_BUILD_DIR}/logicblocks/Encodings.hpp
  ${MQT_QMAP_INCLUDE_BUILD_DIR}/logicblocks/Model.hpp
  ${MQT_QMAP_INCLUDE_BUILD_DIR}/logicblocks/Logic.hpp
  ${MQT_QMAP_INCLUDE_BUILD_DIR}/logicblocks/LogicBlock.hpp
  ${MQT_QMAP_INCLUDE_BUILD_DIR}/logicblocks/LogicTerm.hpp
  ${MQT_QMAP_INCLUDE_BUILD_DIR}/logicblocks/Z3Logic.hpp
  ${MQT_QMAP_INCLUDE_BUILD_DIR}/logicblocks/Z3Model.hpp
  ${MQT_QMAP_INCLUDE_BUILD_DIR}/logicblocks/util_logicblock.hpp
  Encodings.cpp
  LogicBlock.cpp
  LogicTerm.cpp
  Z3Logic.cpp
  Z3Model.cpp)

# set include directories
target_include_directories(mqt-logic-blocks
                           PUBLIC $<BUILD_INTERFACE:${MQT_QMAP_INCLUDE_BUILD_DIR}/logicblocks/>)

# add link libraries
target_link_libraries(
  mqt-logic-blocks
  PUBLIC plog::plog z3::z3lib
  PRIVATE MQT::ProjectOptions MQT::ProjectWarnings)

# set required C++ standard
target_compile_features(mqt-logic-blocks PUBLIC cxx_std_17)

# add MQT alias
add_library(MQT::LogicBlocks ALIAS mqt-logic-blocks)
